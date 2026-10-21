#include "training/training_product_controller.h"

#include "automation_json.h"
#include "deterministic_hash.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <set>
#include <stdexcept>
#include <utility>

namespace billiardgl::training {
namespace {

TrainingProductResult success() { return {true, ""}; }
TrainingProductResult failure(const std::string& code) { return {false, code}; }

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("training_pack_io");
    return std::string(std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

bool validHash(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
        [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
}

std::string referenceActionLogSha256(const replay::Document& document)
{
    const json::Value serialized = replay::serialize(document);
    return sha256Hex(json::stringify(serialized.at("semantic_actions")) + "\n");
}

void appendReferenceTraceState(std::string& trace,
    const replay::Player& player)
{
    trace += std::to_string(player.runtime().tick());
    trace += ':';
    trace += replay::replayAuthorityStateSha256(player.runtime().state());
    trace += '\n';
}

void consumeReferenceTelemetry(const PhysicsStepTelemetry& telemetry,
    AttemptFacts& facts, std::array<bool, kBallCount>& pocketRecorded)
{
    if (telemetry.stepStatus == PhysicsStepStatus::Failed)
        facts.numericalFailure = true;
    for (const PhysicsContactRecord& contact : telemetry.contacts) {
        if (contact.kind == PhysicsContactKind::BallBall &&
            facts.firstContactBall < 0) {
            if (contact.firstBall == 0 && contact.secondBall > 0) {
                facts.firstContactBall = contact.secondBall;
                facts.cueBallPositionAtFirstContactCm =
                    contact.firstPositionAtContactCm;
                facts.hasCueBallPositionAtFirstContact = true;
            } else if (contact.secondBall == 0 && contact.firstBall > 0) {
                facts.firstContactBall = contact.firstBall;
                facts.cueBallPositionAtFirstContactCm =
                    contact.secondPositionAtContactCm;
                facts.hasCueBallPositionAtFirstContact = true;
            }
        } else if (contact.kind == PhysicsContactKind::Rail &&
            contact.firstBall >= 0 && contact.firstBall < kBallCount) {
            ++facts.railContacts[contact.firstBall];
        } else if (contact.kind == PhysicsContactKind::Pocket &&
            contact.firstBall >= 0 && contact.firstBall < kBallCount &&
            contact.pocketPhaseAfter == PocketInteractionPhase::Captured &&
            !pocketRecorded[contact.firstBall]) {
            pocketRecorded[contact.firstBall] = true;
            facts.pocketOrder.push_back(contact.firstBall);
            facts.pocketIds[contact.firstBall] = contact.pocketId;
        }
    }
}

std::uint64_t referenceTickLimit(const Scenario& scenario)
{
    for (const json::Value& condition : scenario.failure.asArray()) {
        if (condition.isObject() && condition.has("predicate") &&
            condition.at("predicate").isString() &&
            condition.at("predicate").asString() == "timeout_ticks" &&
            condition.has("maximum") && condition.at("maximum").isNumber()) {
            const int maximum = condition.at("maximum").asInt();
            return maximum >= 0 ? static_cast<std::uint64_t>(maximum) : 0;
        }
    }
    return 0;
}

TrainingReferenceReplayAudit auditReferenceReplayImpl(
    const replay::Document& document, const Scenario& scenario,
    const PhysicsProfile& profile, const std::string& executableVersion,
    const std::string& contentSha256)
{
    TrainingReferenceReplayAudit audit;
    audit.actionLogSha256 = referenceActionLogSha256(document);
    const std::uint64_t tickLimit = referenceTickLimit(scenario);
    if (tickLimit == 0 || document.finalSummary.tick > tickLimit) {
        audit.result = failure("training_reference_tick_limit_exceeded");
        return audit;
    }

    GameRuntime expectedInitialRuntime;
    const ActionResult initialApplied = applyPhysicsScenario(
        expectedInitialRuntime, scenario.initialScenario);
    if (!initialApplied.ok) {
        audit.result = failure("training_reference_initial_state_invalid");
        return audit;
    }
    if (gameStateSha256(document.initialState) !=
            gameStateSha256(expectedInitialRuntime.state()) ||
        replay::replayAuthorityStateSha256(document.initialState) !=
            replay::replayAuthorityStateSha256(
                expectedInitialRuntime.state())) {
        audit.result = failure("training_reference_initial_state_mismatch");
        return audit;
    }

    replay::Player player;
    const replay::ValidationResult loaded = player.load(document, profile,
        executableVersion, contentSha256);
    if (!loaded.ok) {
        audit.result = failure(loaded.errorCode);
        return audit;
    }

    AttemptFacts facts;
    std::array<bool, kBallCount> pocketRecorded{};
    std::string trace = "billiardgl_training_reference_trace_v1\n";
    appendReferenceTraceState(trace, player);
    while (!player.finished()) {
        const replay::ValidationResult stepped = player.stepTick();
        if (!stepped.ok) {
            audit.result = failure(stepped.errorCode);
            return audit;
        }
        if (!player.runtime().hasRecentPhysicsStep()) {
            audit.result = failure("training_reference_trace_incomplete");
            return audit;
        }
        consumeReferenceTelemetry(
            player.runtime().latestPhysicsStep().telemetry,
            facts, pocketRecorded);
        appendReferenceTraceState(trace, player);
    }
    if (!player.finalHashMatches()) {
        audit.result = failure("replay_final_hash_mismatch");
        return audit;
    }
    audit.traceSha256 = sha256Hex(trace);

    facts.state = player.runtime().state();
    facts.shotCount = document.finalSummary.shotCount;
    facts.elapsedTicks = document.finalSummary.tick;
    facts.traceSha256 = audit.traceSha256;
    for (int ball = 1; ball < kBallCount; ++ball) {
        if (facts.railContacts[ball] > 0)
            ++facts.distinctObjectBallsWithRailContact;
    }
    audit.evaluation = evaluate(scenario, facts);
    audit.result = audit.evaluation.succeeded ? success() :
        failure("training_reference_success_not_met");
    return audit;
}

bool isContainedBy(const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    const auto relative = candidate.lexically_relative(root);
    if (relative.empty()) return candidate == root;
    return *relative.begin() != "..";
}

std::string optionalString(const json::Value& value, const char* key)
{
    return value.has(key) && value.at(key).isString() ?
        value.at(key).asString() : std::string{};
}

float requiredFiniteNumber(const json::Value& value, const char* key)
{
    if (!value.has(key) || !value.at(key).isNumber())
        throw std::runtime_error("training_teaching_hints_invalid");
    const double number = value.at(key).asNumber();
    if (!std::isfinite(number))
        throw std::runtime_error("training_teaching_hints_invalid");
    const float narrowed = static_cast<float>(number);
    if (!std::isfinite(narrowed))
        throw std::runtime_error("training_teaching_hints_invalid");
    return narrowed;
}

std::array<float, 2> requiredPoint2(const json::Value& value, const char* key)
{
    if (!value.has(key) || !value.at(key).isArray() ||
        value.at(key).asArray().size() != 2)
        throw std::runtime_error("training_teaching_hints_invalid");
    const auto& point = value.at(key).asArray();
    if (!point[0].isNumber() || !point[1].isNumber() ||
        !std::isfinite(point[0].asNumber()) ||
        !std::isfinite(point[1].asNumber()))
        throw std::runtime_error("training_teaching_hints_invalid");
    return {static_cast<float>(point[0].asNumber()),
            static_cast<float>(point[1].asNumber())};
}

TrainingTeachingHints parseTeachingHintValue(const json::Value& scenario, const json::Value& hints)
{
    TrainingTeachingHints result;
    if (!hints.isObject())
        throw std::runtime_error("training_teaching_hints_invalid");
    result.goalEn = optionalString(hints, "goal_en");
    if (hints.has("target_pocket_enabled"))
        result.targetPocketEnabled = hints.at("target_pocket_enabled").asBool();
    if (hints.has("aim_at_pocket")) result.aimAtPocket = hints.at("aim_at_pocket").asBool();
    const json::Value& power = hints.at("recommended_power");
    const json::Value& tip = hints.at("recommended_tip");
    const json::Value& aim = hints.at("aim");
    const json::Value& targetZone = hints.at("cue_ball_target_zone");
    const json::Value& shotLine = hints.at("shot_line");
    if (!power.isObject() || !tip.isObject() || !aim.isObject() ||
        !targetZone.isObject() || !shotLine.isObject())
        throw std::runtime_error("training_teaching_hints_invalid");
    result.minimumPowerPercent = requiredFiniteNumber(power, "minimum_percent");
    result.maximumPowerPercent = requiredFiniteNumber(power, "maximum_percent");
    result.defaultPowerPercent = requiredFiniteNumber(power, "default_percent");
    result.tipXBallRadii = requiredFiniteNumber(tip, "x_ball_radii");
    result.tipYBallRadii = requiredFiniteNumber(tip, "y_ball_radii");
    result.tipLabel = optionalString(tip, "label");
    result.aimYawRadians = requiredFiniteNumber(aim, "yaw_radians");
    result.aimToleranceDegrees = requiredFiniteNumber(aim, "tolerance_degrees");
    result.targetPocketId = optionalString(hints, "target_pocket_id");
    result.targetPocketCenterCm = requiredPoint2(
        hints, "target_pocket_center_cm");
    result.cueBallTargetCenterCm = requiredPoint2(targetZone, "center_cm");
    result.cueBallTargetRadiusCm = requiredFiniteNumber(targetZone, "radius_cm");
    if (!targetZone.has("visible") || !targetZone.at("visible").isBool())
        throw std::runtime_error("training_teaching_hints_invalid");
    result.cueBallTargetEnabled = targetZone.at("visible").asBool();
    for (const auto& condition : scenario.at("success").at("all").asArray()) {
        const auto name = optionalString(condition, "predicate");
        if (name == "cue_ball_final_near_first_contact_point")
            result.cueBallTargetTracksFirstContact = true;
        if (name == "rail_contact_count_lte" &&
            condition.at("ball_index").asInt() == 0 &&
            condition.at("maximum").asInt() == 0)
            result.cueBallMustAvoidRails = true;
    }
    result.cueBallIndex = shotLine.at("cue_ball_index").asInt();
    result.objectBallIndex = shotLine.at("object_ball_index").asInt();
    result.shotLineStartCm = requiredPoint2(shotLine, "start_cm");
    result.shotLineObjectCenterCm = requiredPoint2(
        shotLine, "object_center_cm");
    if (result.minimumPowerPercent < 0.0f ||
        result.maximumPowerPercent < result.minimumPowerPercent ||
        result.maximumPowerPercent > 100.0f ||
        result.defaultPowerPercent < result.minimumPowerPercent ||
        result.defaultPowerPercent > result.maximumPowerPercent ||
        std::fabs(result.tipXBallRadii) > 1.0f ||
        std::fabs(result.tipYBallRadii) > 1.0f ||
        result.tipLabel.empty() || result.aimToleranceDegrees <= 0.0f ||
        std::fabs(result.aimYawRadians) > 3.14159265358979323846f ||
        (result.targetPocketEnabled && result.targetPocketId.empty()) || result.cueBallTargetRadiusCm <= 0.0f ||
        result.cueBallIndex < 0 || result.cueBallIndex >= kBallCount ||
        result.objectBallIndex < 0 || result.objectBallIndex >= kBallCount ||
        result.cueBallIndex == result.objectBallIndex)
        throw std::runtime_error("training_teaching_hints_invalid");
    result.available = true;
    return result;
}

TrainingTeachingHints parseTeachingHints(const json::Value& scenario)
{
    if (!scenario.has("teaching_hints")) return {};
    return parseTeachingHintValue(scenario, scenario.at("teaching_hints"));
}

std::string predicateSummary(const json::Value& conditions)
{
    if (!conditions.isArray()) return {};
    std::ostringstream output;
    bool first = true;
    for (const json::Value& condition : conditions.asArray()) {
        if (!condition.isObject() || !condition.has("predicate") ||
            !condition.at("predicate").isString())
            continue;
        if (!first) output << ", ";
        first = false;
        output << condition.at("predicate").asString();
    }
    return output.str();
}

}  // namespace

TrainingReferenceReplayAudit auditReferenceReplay(
    const replay::Document& document, const Scenario& scenario,
    const PhysicsProfile& profile, const std::string& executableVersion,
    const std::string& contentSha256)
{
    return auditReferenceReplayImpl(document, scenario, profile,
        executableVersion, contentSha256);
}

TrainingProductController::TrainingProductController(GameRuntime& runtime,
    std::string executableVersion)
    : runtime_(runtime), session_(runtime),
      configuredExecutableVersion_(std::move(executableVersion))
{
}

void TrainingProductController::setExecutableVersion(
    std::string executableVersion)
{
    if (state_ == TrainingProductState::AttemptActive) return;
    configuredExecutableVersion_ = std::move(executableVersion);
}

TrainingProductResult TrainingProductController::loadPack(
    const std::string& manifestPath,
    const std::set<std::string>& capabilities)
{
    if (state_ == TrainingProductState::AttemptActive)
        return failure("training_attempt_active");
    try {
        const std::filesystem::path manifest =
            std::filesystem::weakly_canonical(manifestPath);
        const std::filesystem::path root = manifest.parent_path();
        const json::ParseResult parsed = json::parse(readFile(manifest));
        if (!parsed.ok || !parsed.value.isObject())
            return failure("training_pack_json");
        const json::Value& value = parsed.value;
        if (value.at("schema").asString() != "training_pack_v1" ||
            value.at("schema_version").asInt() != 1 ||
            value.at("scenario_schema").asString() != "training_scenario_v1")
            return failure("training_pack_schema");

        std::vector<TrainingScenarioCard> cards;
        std::set<std::string> ids;
        for (const json::Value& item : value.at("scenarios").asArray()) {
            TrainingScenarioCard card;
            card.id = item.at("id").asString();
            card.category = item.at("category").asString();
            card.fileSha256 = item.at("sha256").asString();
            if (card.id.empty() || card.category.empty() ||
                !validHash(card.fileSha256) || !ids.insert(card.id).second)
                return failure("training_pack_entry_invalid");
            const std::filesystem::path scenarioPath =
                std::filesystem::weakly_canonical(root /
                    item.at("file").asString());
            if (!isContainedBy(root, scenarioPath))
                return failure("training_pack_path_escape");
            const std::string bytes = readFile(scenarioPath);
            if (sha256Hex(bytes) != card.fileSha256)
                return failure("training_pack_hash_mismatch");
            const LoadResult checked = loadScenarioFile(
                scenarioPath.string(), capabilities);
            if (!checked.ok) return failure(checked.errorCode);
            if (checked.scenario.id != card.id ||
                checked.scenario.category != card.category)
                return failure("training_pack_identity_mismatch");
            card.filePath = scenarioPath.string();
            card.scoreEnabled = checked.scenario.scoreEnabled;
            card.missingCapabilities = checked.missingCapabilities;
            card.physicsProfile = checked.scenario.initialScenario.physicsProfile;
            const json::ParseResult scenarioJson = json::parse(bytes);
            if (!scenarioJson.ok || !scenarioJson.value.isObject())
                return failure("training_pack_json");
            const json::Value& scenario = scenarioJson.value;
            card.teachingHints = parseTeachingHints(scenario);
            if (scenario.has("teaching_hints") && scenario.at("teaching_hints").has("shot_steps")) {
                const auto& steps = scenario.at("teaching_hints").at("shot_steps").asArray();
                if (steps.empty() || steps.size() > 16)
                    throw std::runtime_error("training_teaching_steps_invalid");
                for (const auto& step : steps)
                    card.teachingSteps.push_back(parseTeachingHintValue(scenario, step));
                card.teachingHints = card.teachingSteps.front();
            }
            if (checked.scenario.version == 2 &&
                !card.teachingHints.available)
                return failure("training_teaching_hints_invalid");
            const json::Value& controls = scenario.at("allowed_controls");
            if (!controls.isObject() || !controls.at("actions").isArray() ||
                !controls.at("tip_offset").isObject())
                return failure("training_pack_entry_invalid");
            for (const json::Value& action : controls.at("actions").asArray()) {
                if (!action.isString()) continue;
                card.powerAdjustmentAllowed |=
                    action.asString() == "shot_power";
                card.aimAdjustmentAllowed |= action.asString() == "aim_yaw";
            }
            card.tipAdjustmentAllowed =
                controls.at("tip_offset").at("enabled").asBool();
            card.tipAdjustmentAxis = optionalString(
                controls.at("tip_offset"), "axis");
            if (scenario.has("locale") && scenario.at("locale").isObject()) {
                const json::Value& locale = scenario.at("locale");
                card.titleEn = optionalString(locale, "title_en");
                card.titleZhCn = optionalString(locale, "title_zh_cn");
                card.descriptionEn = optionalString(locale, "description_en");
                card.descriptionZhCn = optionalString(locale, "description_zh_cn");
            }
            if (scenario.has("assist") && scenario.at("assist").isObject())
                card.recommendedAssist = optionalString(
                    scenario.at("assist"), "default");
            if (scenario.has("success") && scenario.at("success").isObject()) {
                const json::Value& successValue = scenario.at("success");
                if (successValue.has("all"))
                    card.successSummary = predicateSummary(successValue.at("all"));
                card.successEvaluation = optionalString(
                    successValue, "evaluation");
            }
            if (scenario.has("failure") && scenario.at("failure").isObject())
                card.failureEvaluation = optionalString(
                    scenario.at("failure"), "evaluation");
            if (scenario.has("reset") && scenario.at("reset").isObject()) {
                const json::Value& reset = scenario.at("reset");
                card.resetMode = optionalString(reset, "mode");
                if (reset.has("attempt_seed")) {
                    const int seed = reset.at("attempt_seed").asInt();
                    if (seed < 0) return failure("training_pack_entry_invalid");
                    card.attemptSeed = static_cast<std::uint32_t>(seed);
                }
                if (reset.has("preserve") && reset.at("preserve").isArray()) {
                    for (const json::Value& preserved :
                            reset.at("preserve").asArray())
                        card.resetPreserve.push_back(preserved.asString());
                }
            }
            if (!scenario.has("reference_replay") ||
                !scenario.at("reference_replay").isObject())
                return failure("training_reference_binding_invalid");
            const json::Value& reference = scenario.at("reference_replay");
            const std::string referenceStatus = optionalString(
                reference, "status");
            if (referenceStatus == "bound") {
                if (!reference.has("binding_rule") ||
                    !reference.at("binding_rule").isObject())
                    return failure("training_reference_binding_invalid");
                const json::Value& binding = reference.at("binding_rule");
                card.referenceReplay.bound = true;
                card.referenceReplay.id = optionalString(reference, "id");
                card.referenceReplay.physicsProfileId = optionalString(
                    binding, "physics_profile_id");
                card.referenceReplay.actionLogSha256 = optionalString(
                    binding, "action_log_sha256");
                card.referenceReplay.traceSha256 = optionalString(
                    binding, "trace_sha256");
                card.referenceReplay.reviewStatus = optionalString(
                    binding, "review_status");
                const std::string relativePath = optionalString(
                    reference, "file");
                if (card.referenceReplay.id.empty() || relativePath.empty() ||
                    card.referenceReplay.physicsProfileId.empty() ||
                    !validHash(card.referenceReplay.actionLogSha256) ||
                    !validHash(card.referenceReplay.traceSha256) ||
                    card.referenceReplay.reviewStatus !=
                        "automated_verified" ||
                    !item.has("reference_replay") ||
                    !item.at("reference_replay").isObject())
                    return failure("training_reference_binding_invalid");
                const json::Value& manifestReference =
                    item.at("reference_replay");
                card.referenceReplay.fileSha256 = optionalString(
                    manifestReference, "sha256");
                if (optionalString(manifestReference, "file") != relativePath ||
                    !validHash(card.referenceReplay.fileSha256))
                    return failure("training_reference_binding_invalid");
                const std::filesystem::path replayPath =
                    std::filesystem::weakly_canonical(root / relativePath);
                if (!isContainedBy(root, replayPath))
                    return failure("training_reference_path_escape");
                const std::string replayBytes = readFile(replayPath);
                if (sha256Hex(replayBytes) !=
                    card.referenceReplay.fileSha256)
                    return failure("training_reference_file_hash_mismatch");
                const json::ParseResult replayJson = json::parse(replayBytes);
                replay::Document replayDocument;
                const replay::ValidationResult replayParsed = replayJson.ok ?
                    replay::parse(replayJson.value, replayDocument) :
                    replay::ValidationResult{false, "replay_format_invalid"};
                if (!replayParsed.ok)
                    return failure(replayParsed.errorCode);
                if (replayDocument.contentSha256 != card.fileSha256 ||
                    replayDocument.physicsProfileId !=
                        card.referenceReplay.physicsProfileId ||
                    referenceActionLogSha256(replayDocument) !=
                        card.referenceReplay.actionLogSha256)
                    return failure("training_reference_binding_mismatch");
                card.referenceReplay.filePath = replayPath.string();
            } else if (referenceStatus != "placeholder_unbound") {
                return failure("training_reference_binding_invalid");
            }
            cards.push_back(card);
        }
        if (cards.empty()) return failure("training_pack_empty");

        scenarios_ = cards;
        packId_ = value.at("id").asString();
        capabilities_ = capabilities;
        currentScenarioId_.clear();
        evaluation_ = Evaluation{};
        recorder_ = replay::Recorder{};
        resetAttemptAccumulator();
        state_ = TrainingProductState::PackReady;
        return success();
    } catch (const std::exception& error) {
        const std::string code = error.what();
        if (code == "training_pack_io" ||
            code == "training_teaching_hints_invalid") return failure(code);
        return failure("training_pack_format");
    }
}

const TrainingScenarioCard* TrainingProductController::findScenario(
    const std::string& id) const
{
    const auto found = std::find_if(scenarios_.begin(), scenarios_.end(),
        [&](const TrainingScenarioCard& card) { return card.id == id; });
    return found == scenarios_.end() ? nullptr : &*found;
}

TrainingProductResult TrainingProductController::startScenario(
    const std::string& scenarioId, const std::string& executableVersion,
    std::uint32_t seed)
{
    if (state_ == TrainingProductState::Empty)
        return failure("training_pack_not_loaded");
    if (state_ == TrainingProductState::AttemptActive)
        return failure("training_attempt_active");
    if (runtime_.state().ballsMoving)
        return failure("training_requires_stopped_state");
    const TrainingScenarioCard* card = findScenario(scenarioId);
    if (card == nullptr) return failure("training_scenario_not_found");
    const LoadResult loaded = session_.load(card->filePath, capabilities_);
    if (!loaded.ok) return failure(loaded.errorCode);

    currentScenarioId_ = scenarioId;
    executableVersion_ = configuredExecutableVersion_.empty() ?
        executableVersion : configuredExecutableVersion_;
    contentSha256_ = card->fileSha256;
    // The scenario's reset contract is authoritative. The legacy caller seed
    // remains accepted for source compatibility but cannot override it.
    (void)seed;
    seed_ = card->attemptSeed;
    evaluation_ = Evaluation{};
    resetAttemptAccumulator();
    const TrainingProductResult recording = restartRecorder();
    if (!recording.ok) {
        state_ = TrainingProductState::PackReady;
        return recording;
    }
    const TrainingProductResult defaults = applyTeachingDefaults();
    if (!defaults.ok) {
        session_.abandon();
        currentScenarioId_.clear();
        state_ = TrainingProductState::PackReady;
        return defaults;
    }
    state_ = TrainingProductState::AttemptActive;
    return success();
}

TrainingProductResult TrainingProductController::startScenario(
    const std::string& scenarioId)
{
    if (configuredExecutableVersion_.empty())
        return failure("training_executable_version_missing");
    return startScenario(scenarioId, configuredExecutableVersion_, 0);
}

TrainingProductResult TrainingProductController::restartRecorder()
{
    recorder_ = replay::Recorder{};
    const replay::ValidationResult result = recorder_.begin(runtime_,
        executableVersion_, contentSha256_, seed_);
    return result.ok ? success() : failure(result.errorCode);
}

TrainingProductResult TrainingProductController::applyTeachingDefaults()
{
    const TrainingScenarioCard* card = currentCard();
    if (card == nullptr || !card->teachingHints.available) return success();
    const TrainingTeachingHints hints = currentTeachingHints();

    // The camera looks back toward its target, so its yaw is aim + pi.  Set
    // this while ObserveFree still owns orbit, then enter Aim with orientation
    // preservation.  A later Observe->Aim toggle therefore reconstructs the
    // same authoritative shot yaw instead of replacing the teaching default.
    constexpr float kPi = 3.14159265358979323846f;
    const auto wrap = [](float value) {
        constexpr float twoPi = 6.28318530717958647692f;
        return std::remainder(value, twoPi);
    };
    const float targetCameraYaw = wrap(hints.aimYawRadians + kPi);
    // Preparation is an explicit user action. Orbit is disabled in Aim, so
    // enter Observe first; otherwise a later Tab would restore the old yaw.
    if (!runtime_.observeFromCurrentCameraPose().ok)
        return failure("training_teaching_default_observe_failed");
    GameAction orbit;
    orbit.type = ActionType::OrbitCamera;
    orbit.first = wrap(targetCameraYaw - runtime_.cameraFrame().pose.yawRadians);
    if (!runtime_.dispatch(orbit).ok)
        return failure("training_teaching_default_camera_failed");
    if (runtime_.state().aim.mode != AimMode::Aim) {
        GameAction toggle;
        toggle.type = ActionType::ToggleAim;
        if (!runtime_.dispatch(toggle).ok)
            return failure("training_teaching_default_aim_mode_failed");
    }

    GameAction aim;
    aim.type = ActionType::SetAimYaw;
    aim.first = hints.aimYawRadians;
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = hints.defaultPowerPercent * 2.0f;
    if (!runtime_.dispatch(aim).ok || !runtime_.dispatch(power).ok ||
        !runtime_.setTipOffset(
            hints.tipXBallRadii, hints.tipYBallRadii).ok)
        return failure("training_teaching_default_controls_failed");
    return success();
}

int TrainingProductController::currentTeachingStepIndex() const
{
    if (shotPrepared_ || shotInProgress_ ||
        (state_ == TrainingProductState::AttemptEvaluated && inFlightTeachingHints_.available))
        return inFlightTeachingStep_;
    const auto* card = currentCard();
    if (card == nullptr || card->teachingSteps.empty()) return 0;
    std::size_t index = 0;
    while (index + 1 < card->teachingSteps.size() &&
        runtime_.state().balls[card->teachingSteps[index].objectBallIndex].pocketed)
        ++index;
    return static_cast<int>(index);
}

TrainingTeachingHints TrainingProductController::currentTeachingHints() const
{
    if (shotPrepared_ || shotInProgress_ ||
        (state_ == TrainingProductState::AttemptEvaluated && inFlightTeachingHints_.available))
        return inFlightTeachingHints_;
    const auto* card = currentCard();
    if (card == nullptr) return {};
    auto hints = card->teachingSteps.empty() ? card->teachingHints :
        card->teachingSteps[currentTeachingStepIndex()];
    if (hints.available && hints.aimAtPocket) {
        const auto& cue = runtime_.state().balls[hints.cueBallIndex].position;
        const auto& object = runtime_.state().balls[hints.objectBallIndex].position;
        hints.shotLineStartCm = {cue.x, cue.z};
        hints.shotLineObjectCenterCm = {object.x, object.z};
        const float dx = hints.targetPocketCenterCm[0] - object.x;
        const float dz = hints.targetPocketCenterCm[1] - object.z;
        const float length = std::hypot(dx, dz);
        if (length > 1e-5f) {
            const float radius = card->physicsProfile.ball.radiusCm;
            hints.aimYawRadians = std::atan2(object.z - 2.0f * radius * dz / length - cue.z,
                object.x - 2.0f * radius * dx / length - cue.x);
        }
    }
    return hints;
}

TrainingProductResult TrainingProductController::prepareTeachingShot()
{
    if (state_ != TrainingProductState::AttemptActive || runtime_.state().ballsMoving ||
        shotPrepared_ || shotInProgress_) return failure("training_requires_stopped_active_attempt");
    const auto result = applyTeachingDefaults();
    return result.ok ? captureAppliedActions() : result;
}

TrainingProductResult TrainingProductController::captureAppliedActions()
{
    if (state_ != TrainingProductState::AttemptActive)
        return failure("training_attempt_inactive");
    const replay::ValidationResult result = recorder_.captureApplied(runtime_);
    return result.ok ? success() : failure(result.errorCode);
}

TrainingProductResult TrainingProductController::markShotStart()
{
    if (state_ != TrainingProductState::AttemptActive)
        return failure("training_attempt_inactive");
    if (runtime_.state().ballsMoving)
        return failure("training_requires_stopped_state");
    if (shotPrepared_ || shotInProgress_)
        return failure("training_shot_already_started");
    session_.markShotStart();
    inFlightTeachingStep_ = currentTeachingStepIndex();
    inFlightTeachingHints_ = currentTeachingHints();
    retryShotControl_ = runtime_.shotControl();
    hasRetryShotControl_ = true;
    shotPrepared_ = true;
    return success();
}

TrainingProductResult TrainingProductController::confirmShotDispatched()
{
    if (state_ != TrainingProductState::AttemptActive)
        return failure("training_attempt_inactive");
    if (!shotPrepared_) return failure("training_shot_not_prepared");
    const auto& actions = runtime_.appliedSemanticActions();
    if (actions.empty() ||
        actions.back().type != AppliedSemanticActionType::Shoot) {
        shotPrepared_ = false;
        return failure("training_shot_not_applied");
    }
    shotPrepared_ = false;
    shotInProgress_ = true;
    preview_ = PreviewResult{};
    previewAuthorityUnchanged_ = false;
    return success();
}

void TrainingProductController::cancelShotStart() noexcept
{
    shotPrepared_ = false;
}

void TrainingProductController::consumeTelemetry(
    const PhysicsStepTelemetry& telemetry)
{
    if (telemetry.stepStatus == PhysicsStepStatus::Failed)
        attemptFacts_.numericalFailure = true;
    for (const PhysicsContactRecord& contact : telemetry.contacts) {
        if (contact.kind == PhysicsContactKind::BallBall &&
            attemptFacts_.firstContactBall < 0) {
            if (contact.firstBall == 0 && contact.secondBall > 0) {
                attemptFacts_.firstContactBall = contact.secondBall;
                attemptFacts_.cueBallPositionAtFirstContactCm =
                    contact.firstPositionAtContactCm;
                attemptFacts_.hasCueBallPositionAtFirstContact = true;
            } else if (contact.secondBall == 0 && contact.firstBall > 0) {
                attemptFacts_.firstContactBall = contact.firstBall;
                attemptFacts_.cueBallPositionAtFirstContactCm =
                    contact.secondPositionAtContactCm;
                attemptFacts_.hasCueBallPositionAtFirstContact = true;
            }
        } else if (contact.kind == PhysicsContactKind::Rail &&
            contact.firstBall >= 0 && contact.firstBall < kBallCount) {
            ++attemptFacts_.railContacts[contact.firstBall];
        } else if (contact.kind == PhysicsContactKind::Pocket &&
            contact.firstBall >= 0 && contact.firstBall < kBallCount &&
            contact.pocketPhaseAfter == PocketInteractionPhase::Captured &&
            !pocketRecorded_[contact.firstBall]) {
            pocketRecorded_[contact.firstBall] = true;
            attemptFacts_.pocketOrder.push_back(contact.firstBall);
            attemptFacts_.pocketIds[contact.firstBall] = contact.pocketId;
        }
    }
}

TrainingProductResult TrainingProductController::finishIfTerminal()
{
    attemptFacts_.state = runtime_.state();
    attemptFacts_.distinctObjectBallsWithRailContact = 0;
    for (int ball = 1; ball < kBallCount; ++ball) {
        if (attemptFacts_.railContacts[ball] > 0)
            ++attemptFacts_.distinctObjectBallsWithRailContact;
    }
    evaluation_ = session_.score(attemptFacts_);
    if (!evaluation_.succeeded && !evaluation_.failed) {
        const auto* card = currentCard();
        if (card != nullptr && card->teachingSteps.size() > 1 &&
            attemptFacts_.shotCount >= static_cast<int>(card->teachingSteps.size())) {
            evaluation_.failed = true;
            evaluation_.reason = "training_sequence_shots_exhausted";
            evaluation_.feedback = "Sequence incomplete - select Restart drill";
        }
    }
    if (!evaluation_.succeeded && !evaluation_.failed) return success();
    const replay::ValidationResult finished = recorder_.finish(
        runtime_, attemptFacts_.shotCount);
    if (!finished.ok) return failure(finished.errorCode);
    state_ = TrainingProductState::AttemptEvaluated;
    return success();
}

TrainingProductResult TrainingProductController::consumeAuthoritySteps()
{
    if (state_ != TrainingProductState::AttemptActive) return success();
    const replay::ValidationResult captured = recorder_.captureApplied(runtime_);
    if (!captured.ok) return failure(captured.errorCode);
    const std::uint64_t generation = runtime_.eventJournalGeneration();
    if (!hasPhysicsJournalGeneration_ ||
        generation != physicsJournalGeneration_) {
        hasPhysicsJournalGeneration_ = true;
        physicsJournalGeneration_ = generation;
        lastPhysicsStepAttemptSequence_ = 0;
    }
    bool consumedShotStep = false;
    for (const RuntimePhysicsStepRecord& record : runtime_.recentPhysicsSteps()) {
        if (record.journalGeneration != generation ||
            record.attemptSequence <= lastPhysicsStepAttemptSequence_)
            continue;
        lastPhysicsStepAttemptSequence_ = record.attemptSequence;
        if (!shotInProgress_) continue;
        consumedShotStep = true;
        if (record.telemetry.stepStatus == PhysicsStepStatus::Succeeded)
            ++attemptFacts_.elapsedTicks;
        consumeTelemetry(record.telemetry);
    }
    if (shotInProgress_ && consumedShotStep &&
        (!runtime_.state().ballsMoving || attemptFacts_.numericalFailure)) {
        shotInProgress_ = false;
        ++attemptFacts_.shotCount;
        return finishIfTerminal();
    }
    return success();
}

TrainingProductResult TrainingProductController::refreshPreview(
    const PreviewBudget& budget)
{
    if (state_ != TrainingProductState::AttemptActive)
        return failure("training_attempt_inactive");
    if (runtime_.state().ballsMoving || shotPrepared_ || shotInProgress_)
        return failure("training_requires_stopped_state");
    const std::string beforeHash = gameStateSha256(runtime_.state());
    const std::uint64_t beforeTick = runtime_.tick();
    const std::size_t beforeEvents = runtime_.events().size();
    preview_ = previewTrainingAssist(runtime_, budget);
    previewAuthorityUnchanged_ =
        beforeHash == gameStateSha256(runtime_.state()) &&
        beforeTick == runtime_.tick() &&
        beforeEvents == runtime_.events().size();
    if (!preview_.ok) return failure(preview_.errorCode);
    if (!previewAuthorityUnchanged_)
        return failure("training_preview_mutated_authority");
    return success();
}

TrainingProductResult TrainingProductController::finishAttempt(
    const AttemptFacts& facts)
{
    if (state_ != TrainingProductState::AttemptActive)
        return failure("training_attempt_inactive");
    if (runtime_.state().ballsMoving)
        return failure("training_requires_stopped_state");
    const replay::ValidationResult captured = recorder_.captureApplied(runtime_);
    if (!captured.ok) return failure(captured.errorCode);
    evaluation_ = session_.score(facts);
    attemptFacts_ = facts;
    if (!evaluation_.succeeded && !evaluation_.failed) return success();
    const replay::ValidationResult finished = recorder_.finish(
        runtime_, recorder_.shotCount());
    if (!finished.ok) return failure(finished.errorCode);
    state_ = TrainingProductState::AttemptEvaluated;
    return success();
}

TrainingProductResult TrainingProductController::abandonAttempt()
{
    session_.abandon();
    currentScenarioId_.clear();
    executableVersion_.clear();
    contentSha256_.clear();
    seed_ = 0;
    evaluation_ = Evaluation{};
    recorder_ = replay::Recorder{};
    resetAttemptAccumulator();
    state_ = scenarios_.empty() ? TrainingProductState::Empty :
        TrainingProductState::PackReady;
    return success();
}

TrainingProductResult TrainingProductController::resetAttempt()
{
    if (state_ != TrainingProductState::AttemptActive &&
        state_ != TrainingProductState::AttemptEvaluated)
        return failure("training_attempt_inactive");
    if (runtime_.state().ballsMoving)
        return failure("training_requires_stopped_state");
    if (!session_.reset()) return failure("training_reset_failed");
    evaluation_ = Evaluation{};
    resetAttemptAccumulator();
    const TrainingProductResult result = restartRecorder();
    if (!result.ok) return result;
    const TrainingProductResult defaults = applyTeachingDefaults();
    if (defaults.ok) state_ = TrainingProductState::AttemptActive;
    return defaults;
}

TrainingProductResult TrainingProductController::retryAttempt()
{
    // Multi-shot lessons restart as a whole: a fresh recorder must not claim
    // earlier pockets/rail counts from a different partial attempt.
    if (const auto* card = currentCard(); card != nullptr && card->teachingSteps.size() > 1) {
        const auto reset = resetAttempt();
        if (!reset.ok) return reset;
        return runtime_.observeFromCurrentCameraPose().ok ? success() :
            failure("training_retry_observe_failed");
    }
    if (state_ != TrainingProductState::AttemptActive &&
        state_ != TrainingProductState::AttemptEvaluated)
        return failure("training_attempt_inactive");
    if (runtime_.state().ballsMoving)
        return failure("training_requires_stopped_state");
    const ShotControlModel retryControls = retryShotControl_;
    const bool restoreShotControls = hasRetryShotControl_;
    if (!session_.retry()) return failure("training_retry_failed");
    if (restoreShotControls) {
        GameAction aim;
        aim.type = ActionType::SetAimYaw;
        aim.first = retryControls.yawRadians;
        GameAction power;
        power.type = ActionType::SetShotPower;
        power.first = retryControls.requestedPower;
        if (!runtime_.dispatch(aim).ok || !runtime_.dispatch(power).ok ||
            !runtime_.setTipOffset(retryControls.tipOffsetX,
                retryControls.tipOffsetY).ok)
            return failure("training_retry_controls_failed");
    }
    // Retrying restores the shot, but must not leave the table armed to fire.
    // Preserve the current camera pose and require an explicit return to Aim.
    if (!runtime_.observeFromCurrentCameraPose().ok)
        return failure("training_retry_observe_failed");
    // Scenario restoration can reset the camera independently of shot yaw.
    // Keep Observe aligned with the restored shot so Tab does not undo aim.
    constexpr float kPi = 3.14159265358979323846f;
    GameAction orbit;
    orbit.type = ActionType::OrbitCamera;
    orbit.first = std::remainder(runtime_.shotControl().yawRadians + kPi -
        runtime_.cameraFrame().pose.yawRadians, 2.0f * kPi);
    if (!runtime_.dispatch(orbit).ok)
        return failure("training_retry_camera_failed");
    evaluation_ = Evaluation{};
    resetAttemptAccumulator();
    const TrainingProductResult result = restartRecorder();
    if (result.ok) state_ = TrainingProductState::AttemptActive;
    return result;
}

TrainingProductResult TrainingProductController::saveReplay(
    const std::string& path) const
{
    if (state_ != TrainingProductState::AttemptEvaluated ||
        !recorder_.finished())
        return failure("training_replay_not_ready");
    const replay::ValidationResult result = recorder_.saveAtomic(path);
    return result.ok ? success() : failure(result.errorCode);
}

TrainingProductResult TrainingProductController::loadReferenceReplay(
    replay::ReplayTransportModel& transport) const
{
    const TrainingScenarioCard* card = currentCard();
    const Scenario* scenario = currentScenario();
    if (card == nullptr || scenario == nullptr)
        return failure("training_attempt_inactive");
    if (!card->referenceReplay.bound)
        return failure("training_reference_unavailable");
    try {
        const std::string bytes = readFile(card->referenceReplay.filePath);
        if (sha256Hex(bytes) != card->referenceReplay.fileSha256)
            return failure("training_reference_file_hash_mismatch");
        const json::ParseResult decoded = json::parse(bytes);
        if (!decoded.ok) return failure("replay_format_invalid");
        replay::Document document;
        const replay::ValidationResult parsed = replay::parse(
            decoded.value, document);
        if (!parsed.ok) return failure(parsed.errorCode);
        const TrainingReferenceReplayAudit audit = auditReferenceReplay(
            document, *scenario, runtime_.physicsProfile(),
            executableVersion_, card->fileSha256);
        if (!audit.result.ok) return audit.result;
        if (audit.actionLogSha256 !=
                card->referenceReplay.actionLogSha256)
            return failure("training_reference_action_hash_mismatch");
        if (audit.traceSha256 != card->referenceReplay.traceSha256)
            return failure("training_reference_trace_hash_mismatch");
        const replay::ValidationResult loaded = transport.load(document,
            runtime_.physicsProfile(), executableVersion_, card->fileSha256);
        return loaded.ok ? success() : failure(loaded.errorCode);
    } catch (const std::exception&) {
        return failure("training_reference_read_failed");
    }
}

const Scenario* TrainingProductController::currentScenario() const
{
    return session_.active() && !currentScenarioId_.empty() ?
        &session_.scenario() : nullptr;
}

const TrainingScenarioCard* TrainingProductController::currentCard() const
{
    return currentScenarioId_.empty() ? nullptr :
        findScenario(currentScenarioId_);
}

const replay::Document* TrainingProductController::replayDocument() const
{
    return recorder_.finished() ? &recorder_.document() : nullptr;
}

const PreviewResult* TrainingProductController::previewResult() const
{
    return preview_.ok ? &preview_ : nullptr;
}

void TrainingProductController::resetAttemptAccumulator()
{
    attemptFacts_ = AttemptFacts{};
    attemptFacts_.state = runtime_.state();
    shotPrepared_ = false;
    shotInProgress_ = false;
    hasPhysicsJournalGeneration_ = true;
    inFlightTeachingHints_ = {};
    inFlightTeachingStep_ = 0;
    physicsJournalGeneration_ = runtime_.eventJournalGeneration();
    lastPhysicsStepAttemptSequence_ = 0;
    pocketRecorded_.fill(false);
    preview_ = PreviewResult{};
    previewAuthorityUnchanged_ = false;
}

TrainingProductSnapshot TrainingProductController::snapshot() const
{
    TrainingProductSnapshot result;
    result.state = state_;
    result.packId = packId_;
    result.scenarioId = currentScenarioId_;
    result.scenarioCount = scenarios_.size();
    result.replayReady = recorder_.finished();
    result.shotPrepared = shotPrepared_;
    result.shotInProgress = shotInProgress_;
    result.executableVersion = executableVersion_;
    result.scenarioFileSha256 = contentSha256_;
    result.attemptSeed = seed_;
    result.attemptFacts = attemptFacts_;
    result.previewReady = preview_.ok;
    result.previewSampleCount = static_cast<int>(preview_.samples.size());
    result.previewAuthorityUnchanged = previewAuthorityUnchanged_;
    if (const TrainingScenarioCard* card = currentCard()) {
        result.teachingStepIndex = currentTeachingStepIndex();
        result.teachingStepCount = card->teachingSteps.empty() ? 1 : static_cast<int>(card->teachingSteps.size());
        result.teachingHints = currentTeachingHints();
        if (attemptFacts_.hasCueBallPositionAtFirstContact &&
            result.teachingHints.cueBallTargetEnabled &&
            result.teachingHints.cueBallTargetTracksFirstContact) {
            result.teachingHints.cueBallTargetCenterCm = {
                attemptFacts_.cueBallPositionAtFirstContactCm.x,
                attemptFacts_.cueBallPositionAtFirstContactCm.z};
        }
        result.powerAdjustmentAllowed = card->powerAdjustmentAllowed;
        result.tipAdjustmentAllowed = card->tipAdjustmentAllowed;
        result.aimAdjustmentAllowed = card->aimAdjustmentAllowed;
    }
    result.evaluation = evaluation_;
    return result;
}

}  // namespace billiardgl::training
