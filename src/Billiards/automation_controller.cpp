#include "automation_controller.h"
#include "physics_scenario.h"
#include "render_state.h"
#include "training/preview_runtime.h"

#include <algorithm>
#include <cmath>
#include <exception>

namespace billiardgl {
namespace {

double numberParam(const json::Value& params, const char* name)
{
    if (!params.has(name) || !params.at(name).isNumber()) throw std::runtime_error(std::string(name) + " must be a number");
    const double value = params.at(name).asNumber();
    if (!std::isfinite(value)) throw std::runtime_error(std::string(name) + " must be finite");
    return value;
}

int intParam(const json::Value& params, const char* name)
{
    if (!params.has(name)) throw std::runtime_error(std::string(name) + " is required");
    return params.at(name).asInt();
}

std::uint64_t tickParam(const json::Value& params, const char* name, std::uint64_t fallback)
{
    if (!params.has(name)) return fallback;
    if (!params.at(name).isNumber()) {
        throw std::runtime_error(std::string(name) + " must be a number");
    }
    const double value = params.at(name).asNumber();
    constexpr double kMaximumExactJsonInteger = 9007199254740991.0;
    if (!std::isfinite(value) || value < 0.0 || value > kMaximumExactJsonInteger ||
        std::floor(value) != value) {
        throw std::runtime_error(std::string(name) + " must be a nonnegative exact integer");
    }
    return static_cast<std::uint64_t>(value);
}

std::string stringParam(const json::Value& params, const char* name)
{
    if (!params.has(name) || !params.at(name).isString()) throw std::runtime_error(std::string(name) + " must be a string");
    return params.at(name).asString();
}

json::Value commandList(const std::vector<std::string>& commands)
{
    json::Value result = json::Value::array();
    for (const std::string& command : commands) result.asArray().push_back(json::Value(command));
    return result;
}

Point3 pointParam(const json::Value& params, const char* name)
{
    if (!params.has(name) || !params.at(name).isObject()) throw std::runtime_error(std::string(name) + " must be an object");
    const json::Value& value = params.at(name);
    return Point3{static_cast<float>(numberParam(value, "x")), static_cast<float>(numberParam(value, "y")), static_cast<float>(numberParam(value, "z"))};
}

bool eventConditionMet(const GameRuntime& runtime, const std::string& condition, std::uint64_t after)
{
    if (condition == "balls_stopped") return !runtime.state().ballsMoving;
    for (const RuntimeEvent& event : runtime.eventsSince(after)) if (event.name == condition) return true;
    return false;
}

const char* cameraStateName(camera::SemanticState state)
{
    switch (state) {
    case camera::SemanticState::ObserveFree: return "observe_free";
    case camera::SemanticState::AimPlayer: return "aim_player";
    case camera::SemanticState::AimOverhead: return "aim_overhead";
    case camera::SemanticState::ShotFollow: return "shot_follow";
    case camera::SemanticState::ReplayFree: return "replay_free";
    case camera::SemanticState::ReplayBroadcast: return "replay_broadcast";
    }
    return "unknown";
}

const char* cameraPresetName(camera::Preset preset)
{
    switch (preset) {
    case camera::Preset::PlayerLow: return "player_low";
    case camera::Preset::Elevated: return "elevated";
    case camera::Preset::Overhead: return "overhead";
    case camera::Preset::Broadcast: return "broadcast";
    }
    return "unknown";
}

const char* assistName(AssistLevel level)
{
    switch (level) {
    case AssistLevel::Off: return "off";
    case AssistLevel::Basic: return "basic";
    case AssistLevel::Training: return "training";
    }
    return "unknown";
}

bool difficultyParam(const std::string& id, ai::Difficulty& value)
{
    if (id == "rookie") value = ai::Difficulty::Rookie;
    else if (id == "club") value = ai::Difficulty::Club;
    else if (id == "expert") value = ai::Difficulty::Expert;
    else return false;
    return true;
}

const char* difficultyName(ai::Difficulty value)
{
    switch (value) {
    case ai::Difficulty::Rookie: return "rookie";
    case ai::Difficulty::Club: return "club";
    case ai::Difficulty::Expert: return "expert";
    }
    return "unknown";
}

const char* aiStatusName(ai::PlayerPollStatus status)
{
    switch (status) {
    case ai::PlayerPollStatus::Disabled: return "disabled";
    case ai::PlayerPollStatus::WaitingForTurn: return "waiting_for_turn";
    case ai::PlayerPollStatus::Thinking: return "thinking";
    case ai::PlayerPollStatus::Applied: return "applied";
    case ai::PlayerPollStatus::NoShot: return "no_shot";
    case ai::PlayerPollStatus::StaleAuthority: return "stale_authority";
    case ai::PlayerPollStatus::Rejected: return "rejected";
    case ai::PlayerPollStatus::MatchComplete: return "match_complete";
    }
    return "unknown";
}

json::Value aiStateValue(const ai::PlayerState& state)
{
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["enabled"] = json::Value(state.configuration.enabled);
    value["player"] = json::Value(state.configuration.player);
    value["difficulty"] = json::Value(
        difficultyName(state.configuration.difficulty));
    value["deterministic_seed"] = json::Value(
        static_cast<double>(state.configuration.deterministicSeed));
    value["status"] = json::Value(aiStatusName(state.status));
    value["thinking"] = json::Value(state.thinking);
    value["jobs_started"] = json::Value(
        static_cast<double>(state.jobsStarted));
    value["error"] = json::Value(state.error);
    return value;
}

json::Value presentationPointValue(const PresentationPoint& point)
{
    json::Value value = json::Value::object();
    value["x"] = json::Value(point.x);
    value["y"] = json::Value(point.y);
    value["z"] = json::Value(point.z);
    return value;
}

json::Value presentationStateValue(const GameRuntime& runtime)
{
    const PresentationSnapshot snapshot = buildPresentationSnapshot(
        runtime.state(), runtime.state(), 1.0, runtime.shotControl(),
        runtime.shotControlCapabilities(), runtime.cameraFrame(),
        buildRulesPresentation(runtime));
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["tick"] = json::Value(static_cast<double>(runtime.tick()));
    value["balls_moving"] = json::Value(snapshot.ballsMoving);

    json::Value shot = json::Value::object();
    shot["schema_version"] = json::Value(static_cast<int>(snapshot.shotControl.schemaVersion));
    shot["yaw_radians"] = json::Value(snapshot.shotControl.yawRadians);
    json::Value tip = json::Value::object();
    tip["x"] = json::Value(snapshot.shotControl.tipOffsetX);
    tip["y"] = json::Value(snapshot.shotControl.tipOffsetY);
    shot["tip_offset_radius"] = tip;
    shot["power_mode"] = json::Value(
        snapshot.shotControl.powerMode == static_cast<std::uint32_t>(ShotPowerMode::StrokeSpeedCmS)
            ? "stroke_speed_cm_s" : "requested_power");
    shot["requested_power"] = json::Value(snapshot.shotControl.requestedPower);
    shot["stroke_speed_cm_s"] = json::Value(snapshot.shotControl.strokeSpeedCmS);
    shot["cue_elevation_degrees"] = json::Value(snapshot.shotControl.cueElevationDegrees);
    shot["assist_level"] = json::Value(assistName(runtime.shotControl().assistLevel));
    shot["cue_elevation_visible"] = json::Value(snapshot.hud.showCueElevation);
    shot["chalk_state_visible"] = json::Value(snapshot.hud.showChalkState);
    value["shot_control"] = shot;

    json::Value cameraValue = json::Value::object();
    cameraValue["state"] = json::Value(cameraStateName(runtime.cameraFrame().state));
    cameraValue["preset"] = json::Value(cameraPresetName(runtime.cameraFrame().preset));
    cameraValue["eye_cm"] = presentationPointValue(snapshot.camera.eye);
    cameraValue["target_cm"] = presentationPointValue(snapshot.camera.target);
    cameraValue["distance_cm"] = json::Value(snapshot.camera.zoom);
    cameraValue["fov_degrees"] = json::Value(snapshot.camera.fieldOfViewDegrees);
    cameraValue["transitioning"] = json::Value(snapshot.camera.transitioning);
    value["camera"] = cameraValue;

    json::Value cue = json::Value::object();
    cue["direction"] = presentationPointValue(snapshot.cue.direction);
    cue["shot_power"] = json::Value(snapshot.cue.shotPower);
    cue["tip_offset_x"] = json::Value(snapshot.cue.tipOffsetX);
    cue["tip_offset_y"] = json::Value(snapshot.cue.tipOffsetY);
    value["cue"] = cue;

    json::Value hud = json::Value::object();
    hud["shot_power"] = json::Value(snapshot.hud.shotPower);
    hud["tip_offset_x"] = json::Value(snapshot.hud.tipOffsetX);
    hud["tip_offset_y"] = json::Value(snapshot.hud.tipOffsetY);
    hud["assist_level"] = json::Value(assistName(runtime.shotControl().assistLevel));
    hud["show_cue_elevation"] = json::Value(snapshot.hud.showCueElevation);
    hud["show_chalk_state"] = json::Value(snapshot.hud.showChalkState);
    value["hud"] = hud;

    json::Value rulesValue = json::Value::object();
    rulesValue["ball_in_hand"] = json::Value(snapshot.rules.ballInHand);
    rulesValue["continue_turn"] = json::Value(snapshot.rules.continueTurn);
    rulesValue["current_player"] = json::Value(snapshot.rules.currentPlayer);
    rulesValue["enabled"] = json::Value(snapshot.rules.enabled);
    rulesValue["foul"] = json::Value(snapshot.rules.foul);
    rulesValue["game_over"] = json::Value(snapshot.rules.gameOver);
    rulesValue["has_adjudication"] = json::Value(snapshot.rules.hasAdjudication);
    rulesValue["phase"] = json::Value(snapshot.rules.phase);
    rulesValue["ruleset_id"] = json::Value(snapshot.rules.rulesetId);
    rulesValue["winner"] = json::Value(snapshot.rules.winner);
    json::Value groupsValue = json::Value::array();
    for (const std::string& group : snapshot.rules.groups)
        groupsValue.asArray().push_back(json::Value(group));
    rulesValue["groups"] = groupsValue;
    json::Value reasonValues = json::Value::array();
    for (const std::string& reason : snapshot.rules.reasonCodes)
        reasonValues.asArray().push_back(json::Value(reason));
    rulesValue["reason_codes"] = reasonValues;
    json::Value textKeyValues = json::Value::array();
    for (const std::string& key : snapshot.rules.textKeys)
        textKeyValues.asArray().push_back(json::Value(key));
    rulesValue["text_keys"] = textKeyValues;
    json::Value optionValues = json::Value::array();
    for (const std::string& option : snapshot.rules.decisionOptions)
        optionValues.asArray().push_back(json::Value(option));
    rulesValue["decision_options"] = optionValues;
    value["rules"] = rulesValue;

    json::Value support = json::Value::object();
    support["shot_control_v2"] = json::Value(true);
    support["cue_elevation"] = json::Value(runtime.shotControlCapabilities().cueElevation);
    support["chalk_state"] = json::Value(runtime.shotControlCapabilities().chalkState);
    support["stroke_input"] = json::Value(false);
    value["capabilities"] = support;
    return value;
}

json::Value trainingStateValue(const training::Session& session)
{
    json::Value value = json::Value::object();
    value["active"] = json::Value(session.active());
    value["schema_version"] = json::Value(1);
    if (session.active()) {
        value["scenario_id"] = json::Value(session.scenario().id);
        value["score_enabled"] = json::Value(session.scenario().scoreEnabled);
        json::Value missing = json::Value::array();
        for (const std::string& capability : session.lastLoad().missingCapabilities)
            missing.asArray().push_back(json::Value(capability));
        value["missing_capabilities"] = missing;
    }
    return value;
}

json::Value replayStateValue(const replay::Player& player)
{
    json::Value value = json::Value::object();
    value["document_format_version"] =
        json::Value(player.documentFormatVersion());
    value["final_hash_matches"] = json::Value(player.finalHashMatches());
    value["final_state_sha256"] = json::Value(player.finalStateSha256());
    value["finished"] = json::Value(player.finished());
    value["paused"] = json::Value(player.paused());
    value["playback_rate"] = json::Value(player.playbackRate());
    value["rules_effect_verified"] =
        json::Value(player.rulesEffectVerified());
    value["rules_receipt_count"] = json::Value(
        static_cast<double>(player.rulesReceiptCount()));
    value["schema_version"] = json::Value(1);
    value["tick"] = json::Value(static_cast<double>(player.runtime().tick()));
    const RulesRackTerminalState& terminal =
        player.runtime().rulesRackTerminalState();
    json::Value rackTerminal = json::Value::object();
    rackTerminal["active"] = json::Value(terminal.active);
    rackTerminal["reason"] = json::Value(terminal.reason ==
            RulesRackTerminalReason::MobileDeviceRepeatViolation ?
        "MOBILE_DEVICE_REPEAT_VIOLATION" : "NONE");
    rackTerminal["losing_player"] = json::Value(terminal.losingPlayer);
    rackTerminal["winning_player"] = json::Value(terminal.winningPlayer);
    rackTerminal["rack_identity"] = json::Value(terminal.rackIdentity);
    rackTerminal["authority_decision_id"] =
        json::Value(terminal.authorityDecisionId);
    value["rack_terminal"] = rackTerminal;
    return value;
}

json::Value replayRecordingStateValue(const replay::Recorder& recorder)
{
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["active"] = json::Value(recorder.active());
    value["finished"] = json::Value(recorder.finished());
    value["shot_count"] = json::Value(recorder.shotCount());
    value["document_format_version"] =
        json::Value(recorder.document().formatVersion);
    value["rules_receipt_count"] = json::Value(
        static_cast<double>(recorder.document().rulesReceipts.size()));
    value["action_count"] = json::Value(
        static_cast<int>(recorder.document().actions.size()));
    if (recorder.active() || recorder.finished()) {
        value["initial_tick"] = json::Value(0);
        value["physics_profile_id"] = json::Value(
            recorder.document().physicsProfileId);
        value["content_sha256"] = json::Value(
            recorder.document().contentSha256);
        value["seed"] = json::Value(
            static_cast<double>(recorder.document().seed));
    }
    if (recorder.finished()) {
        value["final_tick"] = json::Value(static_cast<double>(
            recorder.document().finalSummary.tick));
        value["final_state_sha256"] = json::Value(
            recorder.document().finalSummary.stateSha256);
    }
    return value;
}

json::Value overlayTrackValue(const training::OverlayTrack& track)
{
    json::Value value = json::Value::object();
    value["visible"] = json::Value(track.visible);
    value["sample_count"] = json::Value(
        static_cast<int>(track.samples.size()));
    value["endpoint_visible"] = json::Value(track.endpointVisible);
    json::Value endpoint = json::Value::object();
    endpoint["x"] = json::Value(track.endpoint.x);
    endpoint["y"] = json::Value(track.endpoint.y);
    endpoint["z"] = json::Value(track.endpoint.z);
    value["endpoint_cm"] = endpoint;
    value["rail_collision_events"] =
        json::Value(track.railCollisionEvents);
    value["style"] = json::Value(track.encoding.lineStyle ==
            training::TrajectoryLineStyle::Dashed ?
        "dashed" : "solid_with_perpendicular_ticks");
    value["marker"] = json::Value(track.encoding.marker);
    value["label"] = json::Value(track.encoding.legendLabel);
    return value;
}

json::Value trajectoryOverlayStateValue(
    const training::TrajectoryOverlaySnapshot& snapshot)
{
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["predicted"] = overlayTrackValue(snapshot.predicted);
    value["actual"] = overlayTrackValue(snapshot.actual);
    value["advanced_expanded"] = json::Value(snapshot.advancedExpanded);
    value["authority_role"] = json::Value("presentation_only");
    value["projection_contract"] = json::Value(
        "shared_camera_homogeneous_clip_physical_viewport");
    value["gpu_evidence_status"] = json::Value("NOT_RUN");
    value["manual_review_status"] = json::Value("NOT_RUN");
    return value;
}

bool unrecordedAuthorityMutation(const std::string& command)
{
    return command == "reset_game" || command == "set_ball" ||
        command == "set_player_state" || command == "load_scenario" ||
        command == "set_ruleset" || command == "set_called_pocket" ||
        command == "adjudicate_rules_shot" ||
        command == "rules.submit_simultaneous_group_choice" ||
        command == "rules.confirm_ball_in_hand_placement" ||
        command == "rules.submit_break_foul_choice" ||
        command == "rules.submit_eight_on_break_choice" ||
        command == "rules.unintentional_break_miscue_propose" ||
        command == "rules.unintentional_break_miscue_final" ||
        command == "rules.unintentional_break_miscue_cancel" ||
        command == "training_load" ||
        command == "training_reset" || command == "training_undo" ||
        command == "training_retry" || command == "training_edit_commit" ||
        command == "replay_load" || command == "replay_step_tick" ||
        command == "replay_seek_shot" || command == "configure_ai_player" ||
        command == "poll_ai" || command == "key_down" ||
        command == "special_key" || command == "mouse_move" ||
        command == "mouse_button" || command == "mouse_wheel";
}

const char* rulesPhaseName(rules::MatchPhase phase)
{
    switch (phase) {
    case rules::MatchPhase::Break: return "break";
    case rules::MatchPhase::OpenTable: return "open_table";
    case rules::MatchPhase::GroupsAssigned: return "groups_assigned";
    case rules::MatchPhase::Complete: return "complete";
    }
    return "unknown";
}

const char* rulesGroupName(rules::BallGroup group)
{
    switch (group) {
    case rules::BallGroup::Open: return "open";
    case rules::BallGroup::Solids: return "solids";
    case rules::BallGroup::Stripes: return "stripes";
    }
    return "unknown";
}

const char* ballInHandScopeName(rules::BallInHandPlacementScope scope)
{
    switch (scope) {
    case rules::BallInHandPlacementScope::None: return "none";
    case rules::BallInHandPlacementScope::FullTable: return "full_table";
    case rules::BallInHandPlacementScope::BehindHeadString:
        return "behind_head_string";
    }
    return "unknown";
}

const char* eightOnBreakBranchName(rules::EightOnBreakBranch branch)
{
    return branch == rules::EightOnBreakBranch::LegalBreak ?
        "legal_break" : "foul_break";
}

const char* breakFoulBranchName(rules::BreakFoulBranch branch)
{
    return branch == rules::BreakFoulBranch::IllegalBreak ?
        "illegal_break" : "break_shot_foul";
}

const char* breakHeadStringOutcomeName(
    rules::BreakHeadStringOffenseOutcome outcome)
{
    switch (outcome) {
    case rules::BreakHeadStringOffenseOutcome::Compliant:
        return "compliant";
    case rules::BreakHeadStringOffenseOutcome::Reminder:
        return "reminder";
    case rules::BreakHeadStringOffenseOutcome::Foul:
        return "foul";
    }
    return "unknown";
}

json::Value rulesStateValue(const GameRuntime& runtime)
{
    json::Value value = json::Value::object();
    value["enabled"] = json::Value(
        runtime.rulesMode() == RulesMode::ChineseEightBallV1);
    value["ruleset_id"] = json::Value(
        runtime.rulesMode() == RulesMode::ChineseEightBallV1 ?
            "wpa_heyball_2025_08_16_v1" : "legacy");
    value["schema_version"] = json::Value(1);
    value["authority_state_revision"] = json::Value(
        static_cast<double>(runtime.rulesAuthorityRevision()));
    value["authoritative_state_hash"] = json::Value(
        runtime.rulesAuthorityStateHash());
    const rules::MatchState& state = runtime.matchState();
    value["ball_in_hand"] = json::Value(state.ballInHand);
    value["ball_in_hand_scope"] = json::Value(
        ballInHandScopeName(runtime.ballInHandPlacementScope()));
    value["ball_in_hand_placement_pending"] = json::Value(
        runtime.ballInHandPlacementPending());
    value["eight_on_break_decision_pending"] = json::Value(
        runtime.eightOnBreakDecisionPending());
    value["break_foul_decision_pending"] = json::Value(
        runtime.breakFoulDecisionPending());
    value["current_player"] = json::Value(state.currentPlayer);
    json::Value groups = json::Value::array();
    groups.asArray().push_back(json::Value(rulesGroupName(state.groups[0])));
    groups.asArray().push_back(json::Value(rulesGroupName(state.groups[1])));
    value["groups"] = groups;
    value["group_choice_pending"] =
        json::Value(state.groupChoicePending);
    value["phase"] = json::Value(rulesPhaseName(state.phase));
    value["winner"] = json::Value(state.winner);
    value["collector_active"] = json::Value(runtime.shotFactCollectorActive());
    json::Value headString = json::Value::object();
    headString["sample_event"] =
        json::Value("accepted_break_shot_release");
    headString["position_source"] =
        json::Value("frozen_committed_cue_ball_center");
    headString["history_scope"] = json::Value("match");
    headString["pending"] =
        json::Value(runtime.breakHeadStringReleasePending());
    headString["quantization_grid_world_units"] = json::Value(
        rules::kBreakHeadStringQuantizationGridWorldUnits);
    json::Value counts = json::Value::array();
    counts.asArray().push_back(json::Value(
        static_cast<int>(runtime.breakHeadStringOffenseCount(0))));
    counts.asArray().push_back(json::Value(
        static_cast<int>(runtime.breakHeadStringOffenseCount(1))));
    headString["per_player_offense_counts"] = counts;
    const auto lastHeadString = runtime.lastBreakHeadStringOffense();
    headString["has_last"] = json::Value(lastHeadString.has_value());
    headString["visible"] = json::Value(lastHeadString &&
        lastHeadString->outcome !=
            rules::BreakHeadStringOffenseOutcome::Compliant);
    if (lastHeadString) {
        headString["shot_identity"] = json::Value(
            static_cast<double>(lastHeadString->shotIdentity));
        headString["actor_player_id"] =
            json::Value(lastHeadString->actorPlayerId);
        headString["frozen_cue_ball_center_world_x_cm"] =
            json::Value(lastHeadString->frozenCueBallCenterWorldXCm);
        headString["frozen_cue_ball_center_world_z_cm"] =
            json::Value(lastHeadString->frozenCueBallCenterWorldZCm);
        headString["head_string_world_z_cm"] =
            json::Value(lastHeadString->headStringWorldZCm);
        headString["quantized_signed_crossing_cm"] =
            json::Value(lastHeadString->quantizedSignedCrossingCm);
        headString["outcome"] = json::Value(
            breakHeadStringOutcomeName(lastHeadString->outcome));
        headString["player_offense_count"] = json::Value(
            static_cast<int>(lastHeadString->playerOffenseCount));
        headString["text_key"] = json::Value(lastHeadString->textKey);
    }
    value["break_head_string_offense"] = headString;
    if (runtime.groupChoiceDecisionOffer()) {
        const rules::GroupChoiceDecisionOffer offer =
            *runtime.groupChoiceDecisionOffer();
        json::Value pending = json::Value::object();
        pending["decision_id"] = json::Value(offer.decisionId);
        pending["authority_state_revision"] = json::Value(
            static_cast<double>(offer.authorityStateRevision));
        pending["actor_player_id"] = json::Value(offer.actorPlayerId);
        pending["flow_id"] = json::Value("simultaneous_group_choice");
        json::Value actions = json::Value::array();
        for (const std::string& action : offer.offeredActionIds)
            actions.asArray().push_back(json::Value(action));
        pending["offered_action_ids"] = actions;
        value["pending_decision"] = pending;
    }
    if (runtime.ballInHandPlacementOffer()) {
        const rules::BallInHandPlacementOffer offer =
            *runtime.ballInHandPlacementOffer();
        json::Value pending = json::Value::object();
        pending["decision_id"] = json::Value(offer.decisionId);
        pending["authority_state_revision"] = json::Value(
            static_cast<double>(offer.authorityStateRevision));
        pending["actor_player_id"] = json::Value(offer.actorPlayerId);
        pending["capability_id"] = json::Value(
            rules::kFreeBallPlacementCapabilityId);
        pending["flow_id"] = json::Value(offer.scope ==
                rules::BallInHandPlacementScope::BehindHeadString ?
            rules::kBehindHeadStringBallInHandFlowId :
            rules::kFullTableBallInHandFlowId);
        pending["ball_in_hand_scope"] = json::Value(
            ballInHandScopeName(offer.scope));
        if (offer.scope ==
            rules::BallInHandPlacementScope::BehindHeadString) {
            pending["head_string_world_z_cm"] = json::Value(
                offer.headStringWorldZCm);
            pending["source_clause"] = json::Value(offer.sourceClause);
            pending["source_pdf_page"] = json::Value(
                offer.sourcePdfPage);
            pending["head_string_definition_source_clause"] =
                json::Value(offer.headStringDefinitionSourceClause);
            pending["head_string_definition_pdf_page"] = json::Value(
                offer.headStringDefinitionPdfPage);
        }
        json::Value actions = json::Value::array();
        for (const std::string& action : offer.offeredActionIds)
            actions.asArray().push_back(json::Value(action));
        pending["offered_action_ids"] = actions;
        value["pending_decision"] = pending;
    }
    if (runtime.breakFoulDecisionOffer()) {
        const rules::BreakFoulDecisionOffer offer =
            *runtime.breakFoulDecisionOffer();
        json::Value pending = json::Value::object();
        pending["decision_id"] = json::Value(offer.decisionId);
        pending["authority_state_revision"] = json::Value(
            static_cast<double>(offer.authorityStateRevision));
        pending["actor_player_id"] = json::Value(offer.actorPlayerId);
        pending["capability_id"] = json::Value(
            rules::kBreakFoulCapabilityId);
        pending["flow_id"] = json::Value(rules::kBreakFoulFlowId);
        pending["branch"] = json::Value(
            breakFoulBranchName(offer.branch));
        pending["source_clause"] = json::Value(offer.sourceClause);
        pending["source_pdf_page"] = json::Value(offer.sourcePdfPage);
        pending["original_breaker_player_id"] = json::Value(
            offer.originalBreakerPlayerId);
        pending["frozen_next_player"] = json::Value(
            offer.frozenNextPlayer);
        json::Value actions = json::Value::array();
        for (const std::string& action : offer.offeredActionIds)
            actions.asArray().push_back(json::Value(action));
        pending["offered_action_ids"] = actions;
        value["pending_decision"] = pending;
    }
    if (runtime.eightOnBreakDecisionOffer()) {
        const rules::EightOnBreakDecisionOffer offer =
            *runtime.eightOnBreakDecisionOffer();
        json::Value pending = json::Value::object();
        pending["decision_id"] = json::Value(offer.decisionId);
        pending["authority_state_revision"] = json::Value(
            static_cast<double>(offer.authorityStateRevision));
        pending["actor_player_id"] = json::Value(offer.actorPlayerId);
        pending["capability_id"] = json::Value(
            rules::kEightOnBreakCapabilityId);
        pending["flow_id"] = json::Value(rules::kEightOnBreakFlowId);
        pending["branch"] = json::Value(
            eightOnBreakBranchName(offer.branch));
        pending["original_breaker_player_id"] = json::Value(
            offer.originalBreakerPlayerId);
        pending["frozen_next_player"] = json::Value(
            offer.frozenNextPlayer);
        pending["frozen_continue_turn"] = json::Value(
            offer.frozenContinueTurn);
        json::Value actions = json::Value::array();
        for (const std::string& action : offer.offeredActionIds)
            actions.asArray().push_back(json::Value(action));
        pending["offered_action_ids"] = actions;
        value["pending_decision"] = pending;
    }
    value["has_last_adjudication"] = json::Value(runtime.hasLastAdjudication());
    if (runtime.hasLastAdjudication()) {
        const rules::AdjudicationResult& adjudication = runtime.lastAdjudication();
        json::Value last = json::Value::object();
        last["accepted"] = json::Value(adjudication.accepted);
        last["ball_in_hand"] = json::Value(adjudication.ballInHand);
        last["continue_turn"] = json::Value(adjudication.continueTurn);
        last["foul"] = json::Value(adjudication.foul);
        last["game_over"] = json::Value(adjudication.gameOver);
        last["group_choice_required"] =
            json::Value(adjudication.groupChoiceRequired);
        last["eight_ball_must_be_spotted"] =
            json::Value(adjudication.eightBallMustBeSpotted);
        last["rerack_allowed"] = json::Value(adjudication.rerackAllowed);
        last["next_player"] = json::Value(adjudication.nextPlayer);
        last["winner"] = json::Value(adjudication.winner);
        json::Value reasons = json::Value::array();
        for (rules::FoulReason reason : adjudication.reasons)
            reasons.asArray().push_back(json::Value(rules::foulReasonCode(reason)));
        last["reasons"] = reasons;
        json::Value textKeys = json::Value::array();
        for (const std::string& key : adjudication.textKeys)
            textKeys.asArray().push_back(json::Value(key));
        last["text_keys"] = textKeys;
        json::Value options = json::Value::array();
        for (const std::string& option : adjudication.decisionOptions)
            options.asArray().push_back(json::Value(option));
        last["decision_options"] = options;
        value["last_adjudication"] = last;
    }
    if (runtime.hasLastShotFacts()) {
        const rules::ShotFacts& facts = runtime.lastShotFacts();
        json::Value last = json::Value::object();
        last["any_rail_after_first_contact"] =
            json::Value(facts.anyRailAfterFirstContact);
        last["break_shot"] = json::Value(facts.breakShot);
        last["called_pocket"] = json::Value(facts.calledPocket);
        last["cue_ball_off_table"] = json::Value(facts.cueBallOffTable);
        last["cue_ball_pocketed"] = json::Value(facts.cueBallPocketed);
        last["cue_ball_scratch_evidence_origin"] = json::Value(
            facts.cueBallScratchEvidenceOrigin ==
                    rules::CueBallScratchEvidenceOrigin::
                        AuthoritativeShotFactCollector
                ? "authoritative_shot_fact_collector"
                : "direct_or_synthetic");
        last["cue_ball_scratch_evidence_complete"] =
            json::Value(facts.cueBallScratchEvidenceComplete);
        json::Value cueCaptures = json::Value::array();
        for (const auto& capture : facts.cueBallPocketCaptures) {
            json::Value item = json::Value::object();
            item["canonical_event_ordinal"] =
                json::Value(capture.canonicalEventOrdinal);
            item["capture_sequence"] = json::Value(
                static_cast<double>(capture.captureSequence));
            item["physics_contact_ordinal"] =
                json::Value(capture.physicsContactOrdinal);
            item["physics_tick"] = json::Value(
                static_cast<double>(capture.physicsTick));
            item["pocket_id"] = json::Value(capture.pocketId);
            item["time_of_impact_seconds"] =
                json::Value(capture.timeOfImpactSeconds);
            cueCaptures.asArray().push_back(item);
        }
        last["cue_ball_pocket_captures"] = cueCaptures;
        last["distinct_object_balls_to_rail"] =
            json::Value(facts.distinctObjectBallsToRail);
        last["eight_ball_pocket"] = json::Value(facts.eightBallPocket);
        last["first_contact_ball"] = json::Value(facts.firstContactBall);
        json::Value firstContacts = json::Value::array();
        for (int ball : facts.firstContactBalls)
            firstContacts.asArray().push_back(json::Value(ball));
        last["first_contact_balls"] = firstContacts;
        json::Value offTable = json::Value::array();
        for (int ball : facts.objectBallsOffTable)
            offTable.asArray().push_back(json::Value(ball));
        last["object_balls_off_table"] = offTable;
        json::Value pockets = json::Value::array();
        for (int ball : facts.pocketedBallsInOrder)
            pockets.asArray().push_back(json::Value(ball));
        last["pocketed_balls_in_order"] = pockets;
        last["shooter"] = json::Value(facts.shooter);
        last["solids_remaining_before"] =
            json::Value(facts.solidsRemainingBefore);
        last["stripes_remaining_before"] =
            json::Value(facts.stripesRemainingBefore);
        value["last_shot_facts"] = last;
    }
    return value;
}

json::Value refereeAuthorityStateValue(const GameRuntime& runtime)
{
    const rules::RefereeAuthorityState& state =
        runtime.refereeAuthorityState();
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["protocol_id"] = json::Value(state.protocolId);
    value["rack_identity"] = json::Value(state.rackIdentity);
    value["reset_generation"] = json::Value(
        static_cast<double>(state.resetGeneration));
    value["authority_revision"] = json::Value(
        static_cast<double>(state.authorityRevision));
    value["authority_state_hash"] = json::Value(
        state.authorityStateHash);
    value["audit_digest"] = json::Value(state.auditDigest);
    value["audit_entry_count"] = json::Value(
        static_cast<int>(state.journal.size()));
    value["proposal_pending"] = json::Value(
        state.liveProposal.has_value());
    value["final_declaration_committed"] = json::Value(
        state.latestFinalDeclaration.has_value());
    if (state.liveProposal) {
        json::Value proposal = json::Value::object();
        proposal["declaration_kind"] = json::Value(
            state.liveProposal->declarationKind);
        proposal["decision_id"] = json::Value(
            state.liveProposal->decisionId);
        proposal["authority_role"] = json::Value(
            rules::refereeAuthorityRoleName(
                state.liveProposal->authorityRole));
        proposal["authority_subject_id"] = json::Value(
            state.liveProposal->authoritySubjectId);
        proposal["expires_at_authority_tick"] = json::Value(
            static_cast<double>(
                state.liveProposal->expiresAtAuthorityTick));
        proposal["payload_sha256"] = json::Value(
            state.liveProposal->payloadSha256);
        value["proposal"] = proposal;
    }
    if (state.latestFinalDeclaration) {
        json::Value final = json::Value::object();
        final["declaration_kind"] = json::Value(
            state.latestFinalDeclaration->declarationKind);
        final["decision_id"] = json::Value(
            state.latestFinalDeclaration->decisionId);
        final["authority_role"] = json::Value(
            rules::refereeAuthorityRoleName(
                state.latestFinalDeclaration->authorityRole));
        final["authority_subject_id"] = json::Value(
            state.latestFinalDeclaration->authoritySubjectId);
        final["payload_sha256"] = json::Value(
            state.latestFinalDeclaration->payloadSha256);
        value["latest_final_declaration"] = final;
    }
    return value;
}

const char* stalematePhaseName(rules::StalematePhase phase)
{
    switch (phase) {
    case rules::StalematePhase::Inactive: return "INACTIVE";
    case rules::StalematePhase::ProposalOpen: return "PROPOSAL_OPEN";
    case rules::StalematePhase::ContinuationActive:
        return "CONTINUATION_ACTIVE";
    case rules::StalematePhase::AwaitingRefereeFinal:
        return "AWAITING_REFEREE_FINAL";
    case rules::StalematePhase::RerackCommitted:
        return "RERACK_COMMITTED";
    case rules::StalematePhase::Cancelled: return "CANCELLED";
    }
    return "UNKNOWN";
}

json::Value stalemateAuthorityStateValue(const GameRuntime& runtime)
{
    const rules::StalemateState& state = runtime.stalemateState();
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    const rules::RefereeAuthorityState& protocol =
        runtime.stalemateProtocolState();
    value["protocol_id"] = json::Value(protocol.protocolId);
    value["authority_revision"] = json::Value(
        static_cast<double>(protocol.authorityRevision));
    value["authority_state_hash"] =
        json::Value(protocol.authorityStateHash);
    value["audit_digest"] = json::Value(protocol.auditDigest);
    value["phase"] = json::Value(stalematePhaseName(state.phase));
    value["rack_identity"] = json::Value(state.rackIdentity);
    value["original_breaker_player_id"] =
        json::Value(state.originalBreakerPlayerId);
    json::Value order = json::Value::array();
    order.asArray().push_back(json::Value(state.originalShootingOrder[0]));
    order.asArray().push_back(json::Value(state.originalShootingOrder[1]));
    value["original_shooting_order"] = order;
    value["order_snapshot_sha256"] =
        json::Value(state.orderSnapshotSha256);
    value["proposal_decision_id"] =
        json::Value(state.proposalDecisionId);
    value["proposal_payload_sha256"] =
        json::Value(state.proposalPayloadSha256);
    value["referee_subject_id"] = json::Value(state.refereeSubjectId);
    value["directive_kind"] = json::Value(state.directiveKind);
    value["per_player_shot_allowance"] =
        json::Value(state.perPlayerShotAllowance);
    value["window_start_authority_revision"] = json::Value(
        static_cast<double>(state.windowStartAuthorityRevision));
    json::Value counts = json::Value::array();
    counts.asArray().push_back(json::Value(state.completedShots[0]));
    counts.asArray().push_back(json::Value(state.completedShots[1]));
    value["per_player_completed_shots"] = counts;
    json::Value last = json::Value::array();
    last.asArray().push_back(json::Value(
        state.lastCountedShotIdentity[0]));
    last.asArray().push_back(json::Value(
        state.lastCountedShotIdentity[1]));
    value["last_counted_shot_identities"] = last;
    value["counter_snapshot_sha256"] =
        json::Value(state.counterSnapshotSha256);
    json::Value refusals = json::Value::array();
    for (int actor : state.refusalHistory)
        refusals.asArray().push_back(json::Value(actor));
    value["refusal_actors"] = refusals;
    json::Value refusalDigests = json::Value::array();
    for (const std::string& digest : state.refusalAuditDigests)
        refusalDigests.asArray().push_back(json::Value(digest));
    value["refusal_audit_digests"] = refusalDigests;
    value["final_authorized"] = json::Value(state.finalAuthorized);
    value["core_e_rerack_committed"] =
        json::Value(state.rerackCommitted);
    value["credential_material_exposed"] = json::Value(false);
    return value;
}

const char* groupConfusionPhaseName(rules::GroupConfusionPhase phase)
{
    switch (phase) {
    case rules::GroupConfusionPhase::Inactive: return "INACTIVE";
    case rules::GroupConfusionPhase::TimelyWindowOpen:
        return "TIMELY_WINDOW_OPEN";
    case rules::GroupConfusionPhase::RealizationProposed:
        return "REALIZATION_PROPOSED_RACK_STOPPED";
    case rules::GroupConfusionPhase::RerackCommitting:
        return "RERACK_COMMITTING";
    case rules::GroupConfusionPhase::RerackCommitted:
        return "RERACK_COMMITTED";
    case rules::GroupConfusionPhase::WindowExpired:
        return "WINDOW_EXPIRED";
    }
    return "UNKNOWN";
}

json::Value groupConfusionAuthorityStateValue(const GameRuntime& runtime)
{
    const auto& state = runtime.groupConfusionState();
    const auto& protocol = runtime.groupConfusionProtocolState();
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["protocol_id"] = json::Value(protocol.protocolId);
    value["phase"] = json::Value(groupConfusionPhaseName(state.phase));
    value["authority_revision"] = json::Value(
        static_cast<double>(protocol.authorityRevision));
    value["authority_state_hash"] =
        json::Value(protocol.authorityStateHash);
    value["audit_digest"] = json::Value(protocol.auditDigest);
    value["rack_identity"] = json::Value(state.window.rackIdentity);
    value["original_breaker_player_id"] =
        json::Value(state.window.originalBreakerPlayerId);
    json::Value order = json::Value::array();
    order.asArray().push_back(
        json::Value(state.window.originalShootingOrder[0]));
    order.asArray().push_back(
        json::Value(state.window.originalShootingOrder[1]));
    value["original_shooting_order"] = order;
    value["rack_order_snapshot_sha256"] =
        json::Value(state.rackOrderSnapshotSha256);
    value["assignment_snapshot_sha256"] =
        json::Value(state.window.assignmentSnapshotSha256);
    value["completed_shot_identity"] =
        json::Value(state.window.completedShotIdentity);
    value["completed_shot_ordinal"] = json::Value(
        static_cast<double>(state.window.completedShotOrdinal));
    value["completed_shooter"] =
        json::Value(state.window.completedShooter);
    value["window_generation"] = json::Value(
        static_cast<double>(state.windowGeneration));
    value["proposal_decision_id"] =
        json::Value(state.proposalDecisionId);
    value["proposal_actor_role"] = json::Value(
        rules::refereeAuthorityRoleName(state.proposalActorRole));
    value["proposal_actor_subject_id"] =
        json::Value(state.proposalActorSubjectId);
    value["proposal_payload_sha256"] =
        json::Value(state.proposalPayloadSha256);
    value["final_authorized"] = json::Value(state.finalAuthorized);
    value["core_e_rerack_committed"] =
        json::Value(state.rerackCommitted);
    value["credential_material_exposed"] = json::Value(false);
    return value;
}

const char* frozenDeclarationPhaseName(
    rules::FrozenBallDeclarationPhase phase)
{
    switch(phase){
    case rules::FrozenBallDeclarationPhase::Inactive:return "INACTIVE";
    case rules::FrozenBallDeclarationPhase::ProposalOpen:return "PROPOSAL_OPEN";
    case rules::FrozenBallDeclarationPhase::Active:return "DECLARATION_ACTIVE";
    case rules::FrozenBallDeclarationPhase::Consumed:return "DECLARATION_CONSUMED";
    case rules::FrozenBallDeclarationPhase::Cancelled:return "CANCELLED";
    case rules::FrozenBallDeclarationPhase::Invalidated:return "INVALIDATED";
    }
    return "UNKNOWN";
}

json::Value frozenBallDeclarationStateValue(const GameRuntime& runtime)
{
    const auto& state=runtime.frozenBallDeclarationState();
    const auto& protocol=runtime.frozenBallDeclarationProtocolState();
    json::Value value=json::Value::object();
    value["schema_version"]=json::Value(1);
    value["protocol_id"]=json::Value(protocol.protocolId);
    value["phase"]=json::Value(frozenDeclarationPhaseName(state.phase));
    value["authority_revision"]=json::Value(static_cast<double>(protocol.authorityRevision));
    value["authority_state_hash"]=json::Value(protocol.authorityStateHash);
    value["audit_digest"]=json::Value(protocol.auditDigest);
    value["rack_identity"]=json::Value(state.rackIdentity);
    value["cue_ball_id"]=json::Value(state.context.cueBallId);
    value["object_ball_id"]=json::Value(state.context.objectBallId);
    value["parent_rules_revision"]=json::Value(static_cast<double>(state.context.parentRevision));
    value["parent_rules_state_hash"]=json::Value(state.context.parentStateHash);
    value["proposal_decision_id"]=json::Value(state.proposalDecisionId);
    value["referee_subject_id"]=json::Value(state.refereeSubjectId);
    value["proposal_payload_sha256"]=json::Value(state.proposalPayloadSha256);
    value["final_payload_sha256"]=json::Value(state.finalPayloadSha256);
    value["committed"]=json::Value(state.committed);
    value["consumed_shot_identity"]=json::Value(state.consumedShotIdentity);
    value["consumed_authority_tick"]=json::Value(static_cast<double>(state.consumedAuthorityTick));
    value["credential_material_exposed"]=json::Value(false);
    return value;
}

const char* frozenCushionPhaseName(
    rules::FrozenCushionDeclarationPhase phase)
{
    switch (phase) {
    case rules::FrozenCushionDeclarationPhase::Inactive: return "INACTIVE";
    case rules::FrozenCushionDeclarationPhase::ProposalOpen:
        return "PROPOSAL_OPEN";
    case rules::FrozenCushionDeclarationPhase::Active:
        return "DECLARATION_ACTIVE";
    case rules::FrozenCushionDeclarationPhase::Consumed:
        return "DECLARATION_CONSUMED";
    case rules::FrozenCushionDeclarationPhase::Cancelled: return "CANCELLED";
    case rules::FrozenCushionDeclarationPhase::Invalidated:
        return "INVALIDATED";
    }
    return "UNKNOWN";
}

json::Value frozenCushionDeclarationStateValue(const GameRuntime& runtime)
{
    const auto& state = runtime.frozenCushionDeclarationState();
    const auto& protocol = runtime.frozenCushionDeclarationProtocolState();
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["protocol_id"] = json::Value(protocol.protocolId);
    value["phase"] = json::Value(frozenCushionPhaseName(state.phase));
    value["authority_revision"] = json::Value(
        static_cast<double>(protocol.authorityRevision));
    value["authority_state_hash"] = json::Value(protocol.authorityStateHash);
    value["audit_digest"] = json::Value(protocol.auditDigest);
    value["rack_identity"] = json::Value(state.rackIdentity);
    value["object_ball_id"] = json::Value(state.context.objectBallId);
    value["straight_cushion_id"] = json::Value(
        rules::straightCushionIdName(state.context.cushionId));
    value["parent_rules_revision"] = json::Value(
        static_cast<double>(state.context.parentRevision));
    value["parent_rules_state_hash"] =
        json::Value(state.context.parentStateHash);
    value["proposal_decision_id"] = json::Value(state.proposalDecisionId);
    value["referee_subject_id"] = json::Value(state.refereeSubjectId);
    value["proposal_payload_sha256"] =
        json::Value(state.proposalPayloadSha256);
    value["final_payload_sha256"] = json::Value(state.finalPayloadSha256);
    value["committed"] = json::Value(state.committed);
    value["consumed_shot_identity"] =
        json::Value(state.consumedShotIdentity);
    value["consumed_authority_tick"] = json::Value(
        static_cast<double>(state.consumedAuthorityTick));
    value["credential_material_exposed"] = json::Value(false);
    value["section_15_2_result_classified"] = json::Value(false);
    return value;
}

const char* doubleHitCloseCasePhaseName(rules::DoubleHitCloseCasePhase phase)
{
    switch(phase){
    case rules::DoubleHitCloseCasePhase::Inactive:return "INACTIVE";
    case rules::DoubleHitCloseCasePhase::ProposalOpen:return "PROPOSAL_OPEN";
    case rules::DoubleHitCloseCasePhase::Active:return "CLOSE_CASE_ACTIVE";
    case rules::DoubleHitCloseCasePhase::Consumed:return "CLOSE_CASE_CONSUMED";
    case rules::DoubleHitCloseCasePhase::Cancelled:return "CANCELLED";
    case rules::DoubleHitCloseCasePhase::Invalidated:return "INVALIDATED";
    }
    return "UNKNOWN";
}

json::Value doubleHitCloseCaseStateValue(const GameRuntime& runtime)
{
    const auto& state=runtime.doubleHitCloseCaseState();
    const auto& protocol=runtime.doubleHitCloseCaseProtocolState();
    json::Value value=json::Value::object();
    value["schema_version"]=json::Value(1);
    value["protocol_id"]=json::Value(protocol.protocolId);
    value["phase"]=json::Value(doubleHitCloseCasePhaseName(state.phase));
    value["authority_revision"]=json::Value(
        static_cast<double>(protocol.authorityRevision));
    value["authority_state_hash"]=json::Value(protocol.authorityStateHash);
    value["audit_digest"]=json::Value(protocol.auditDigest);
    value["rack_identity"]=json::Value(state.rackIdentity);
    value["cue_ball_id"]=json::Value(state.context.cueBallId);
    value["object_ball_id"]=json::Value(state.context.objectBallId);
    value["parent_rules_revision"]=json::Value(
        static_cast<double>(state.context.parentRevision));
    value["parent_rules_state_hash"]=json::Value(state.context.parentStateHash);
    value["proposal_decision_id"]=json::Value(state.proposalDecisionId);
    value["referee_subject_id"]=json::Value(state.refereeSubjectId);
    value["proposal_payload_sha256"]=json::Value(state.proposalPayloadSha256);
    value["final_payload_sha256"]=json::Value(state.finalPayloadSha256);
    value["committed"]=json::Value(state.committed);
    value["review_needed"]=json::Value(state.reviewNeeded);
    value["double_hit_classified"]=json::Value(false);
    value["foul_classified"]=json::Value(false);
    value["consumed_shot_identity"]=json::Value(state.consumedShotIdentity);
    value["consumed_authority_tick"]=json::Value(
        static_cast<double>(state.consumedAuthorityTick));
    value["credential_material_exposed"]=json::Value(false);
    return value;
}
const char*pushReviewPhaseName(rules::PushReviewPhase p){switch(p){
case rules::PushReviewPhase::Inactive:return "INACTIVE";case rules::PushReviewPhase::ProposalOpen:return "PROPOSAL_OPEN";case rules::PushReviewPhase::Active:return "PUSH_REVIEW_ACTIVE";case rules::PushReviewPhase::Consumed:return "PUSH_REVIEW_CONSUMED";case rules::PushReviewPhase::Cancelled:return "CANCELLED";case rules::PushReviewPhase::Invalidated:return "INVALIDATED";}return "UNKNOWN";}
json::Value pushReviewStateValue(const GameRuntime&r){auto&s=r.pushReviewState();auto&p=r.pushReviewProtocolState();json::Value v=json::Value::object();v["schema_version"]=json::Value(1);v["phase"]=json::Value(pushReviewPhaseName(s.phase));v["authority_revision"]=json::Value(static_cast<double>(p.authorityRevision));v["authority_state_hash"]=json::Value(p.authorityStateHash);v["rack_identity"]=json::Value(s.rackIdentity);v["cue_ball_id"]=json::Value(s.context.cueBallId);v["parent_rules_revision"]=json::Value(static_cast<double>(s.context.parentRevision));v["parent_rules_state_hash"]=json::Value(s.context.parentStateHash);v["proposal_decision_id"]=json::Value(s.proposalDecisionId);v["referee_subject_id"]=json::Value(s.refereeSubjectId);v["committed"]=json::Value(s.committed);v["review_needed"]=json::Value(s.reviewNeeded);v["measurement_complete"]=json::Value(false);v["push_classified"]=json::Value(false);v["foul_classified"]=json::Value(false);v["consumed_shot_identity"]=json::Value(s.consumedShotIdentity);v["credential_material_exposed"]=json::Value(false);return v;}
const char* nonphysicalGeneralFoulPhaseName(
    rules::NonphysicalGeneralFoulPhase p){switch(p){
case rules::NonphysicalGeneralFoulPhase::Inactive:return "INACTIVE";
case rules::NonphysicalGeneralFoulPhase::ProposalOpen:return "PROPOSAL_OPEN";
case rules::NonphysicalGeneralFoulPhase::Committed:return "COMMITTED";
case rules::NonphysicalGeneralFoulPhase::Cancelled:return "CANCELLED";
case rules::NonphysicalGeneralFoulPhase::Invalidated:return "INVALIDATED";}
return "UNKNOWN";}
json::Value nonphysicalGeneralFoulStateValue(const GameRuntime&r){
 auto&s=r.nonphysicalGeneralFoulState();auto&p=r.nonphysicalGeneralFoulProtocolState();
 json::Value v=json::Value::object();v["schema_version"]=json::Value(1);
 v["phase"]=json::Value(nonphysicalGeneralFoulPhaseName(s.phase));
 v["authority_revision"]=json::Value(static_cast<double>(p.authorityRevision));
 v["authority_state_hash"]=json::Value(p.authorityStateHash);
 v["rack_identity"]=json::Value(s.rackIdentity);
 v["event_kind"]=json::Value(rules::nonphysicalGeneralFoulKindName(s.eventKind));
 v["offending_player"]=json::Value(s.context.offendingPlayer);
 v["opponent_player"]=json::Value(s.context.opponentPlayer);
 v["shot_identity"]=json::Value(s.context.shotIdentity);
 v["parent_rules_revision"]=json::Value(static_cast<double>(s.context.parentRevision));
 v["parent_rules_state_hash"]=json::Value(s.context.parentStateHash);
 v["proposal_decision_id"]=json::Value(s.proposalDecisionId);
 v["referee_subject_id"]=json::Value(s.refereeSubjectId);
 v["committed"]=json::Value(s.committed);
 v["full_wpa_ii_17_complete"]=json::Value(false);
 v["d5_complete"]=json::Value(false);v["d6_complete"]=json::Value(false);
 v["credential_material_exposed"]=json::Value(false);return v;}

const char* unintentionalBreakMiscuePhaseName(
    rules::UnintentionalBreakMiscueDeclarationPhase phase)
{
    using Phase = rules::UnintentionalBreakMiscueDeclarationPhase;
    switch (phase) {
    case Phase::Inactive: return "INACTIVE";
    case Phase::ProposalOpen: return "PROPOSAL_OPEN";
    case Phase::Active: return "ACTIVE";
    case Phase::Consumed: return "CONSUMED";
    case Phase::Cancelled: return "CANCELLED";
    case Phase::Invalidated: return "INVALIDATED";
    }
    return "UNKNOWN";
}

json::Value unintentionalBreakMiscueStateValue(const GameRuntime& runtime)
{
    const auto& state = runtime.unintentionalBreakMiscueDeclarationState();
    const auto& protocol =
        runtime.unintentionalBreakMiscueDeclarationProtocolState();
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(1);
    value["phase"] = json::Value(
        unintentionalBreakMiscuePhaseName(state.phase));
    value["authority_revision"] = json::Value(
        static_cast<double>(protocol.authorityRevision));
    value["authority_state_hash"] = json::Value(
        protocol.authorityStateHash);
    value["rack_identity"] = json::Value(state.rackIdentity);
    value["actor_player_id"] = json::Value(state.context.actorPlayerId);
    value["parent_rules_revision"] = json::Value(
        static_cast<double>(state.context.parentRevision));
    value["parent_rules_state_hash"] = json::Value(
        state.context.parentStateHash);
    value["proposal_decision_id"] = json::Value(
        state.proposalDecisionId);
    value["referee_subject_id"] = json::Value(state.refereeSubjectId);
    value["committed"] = json::Value(state.committed);
    value["consumed_shot_identity"] = json::Value(
        state.consumedShotIdentity);
    value["consumed_authority_tick"] = json::Value(
        static_cast<double>(state.consumedAuthorityTick));
    value["credential_material_exposed"] = json::Value(false);
    return value;
}

rules::RefereeAuthorityRole refereeRoleParam(const json::Value& params)
{
    const std::string role = stringParam(params, "authority_role");
    if (role == "REFEREE") return rules::RefereeAuthorityRole::Referee;
    if (role == "PLAYER_0") return rules::RefereeAuthorityRole::Player0;
    if (role == "PLAYER_1") return rules::RefereeAuthorityRole::Player1;
    throw std::runtime_error("authority_role is invalid");
}

rules::RefereeAuthorityOperation refereeOperationParam(
    const json::Value& params)
{
    const std::string operation = stringParam(params, "operation");
    if (operation == "PROPOSE")
        return rules::RefereeAuthorityOperation::Propose;
    if (operation == "FINAL_DECLARE")
        return rules::RefereeAuthorityOperation::FinalDeclare;
    if (operation == "CANCEL_PROPOSAL")
        return rules::RefereeAuthorityOperation::CancelProposal;
    throw std::runtime_error("operation is invalid");
}

rules::RefereeAuthorityRequest refereeRequestParam(
    const json::Value& params)
{
    if (!params.isObject() || params.asObject().size() != 17 ||
        !params.has("payload") || !params.at("payload").isObject())
        throw std::runtime_error(
            "referee request must contain the exact 17-field schema");
    rules::RefereeAuthorityRequest value;
    value.protocolId = stringParam(params, "protocol_id");
    value.schemaVersion = intParam(params, "schema_version");
    value.operation = refereeOperationParam(params);
    value.declarationKind = stringParam(params, "declaration_kind");
    value.decisionId = stringParam(params, "decision_id");
    value.rackIdentity = stringParam(params, "rack_identity");
    value.expectedAuthorityRevision = tickParam(
        params, "authority_revision", 0);
    value.expectedAuthorityStateHash = stringParam(
        params, "authority_state_hash");
    value.authorityRole = refereeRoleParam(params);
    value.authoritySubjectId = stringParam(params, "authority_subject_id");
    value.credentialId = stringParam(params, "credential_id");
    value.keyVersion = stringParam(params, "key_version");
    value.authenticationNonce = stringParam(
        params, "authentication_nonce");
    value.expiresAtAuthorityTick = tickParam(
        params, "expires_at_authority_tick", 0);
    value.idempotencyKey = stringParam(params, "idempotency_key");
    value.payloadCanonical = json::stringify(params.at("payload"));
    value.authenticationProof = stringParam(
        params, "authentication_proof");
    return value;
}

json::Value refereeAckValue(const rules::RefereeAuthorityAck& ack)
{
    json::Value value = json::Value::object();
    value["decision_id"] = json::Value(ack.decisionId);
    value["idempotency_key"] = json::Value(ack.idempotencyKey);
    value["authority_revision"] = json::Value(
        static_cast<double>(ack.authoritativeRevision));
    value["authority_state_hash"] = json::Value(
        ack.authoritativeStateHash);
    value["payload_sha256"] = json::Value(ack.payloadSha256);
    return value;
}

json::Value groupChoiceAckValue(const rules::GroupChoiceDecisionAck& ack)
{
    json::Value value = json::Value::object();
    value["accepted"] = json::Value(ack.accepted);
    value["decision_id"] = json::Value(ack.decisionId);
    value["action_id"] = json::Value(ack.actionId);
    value["idempotency_key"] = json::Value(ack.idempotencyKey);
    value["actor_player_id"] = json::Value(ack.actorPlayerId);
    value["previous_state_revision"] = json::Value(
        static_cast<double>(ack.previousStateRevision));
    value["authoritative_state_revision"] = json::Value(
        static_cast<double>(ack.authoritativeStateRevision));
    value["authoritative_state_hash"] = json::Value(
        ack.authoritativeStateHash);
    json::Value effects = json::Value::object();
    effects["current_player"] = json::Value(ack.effects.currentPlayer);
    effects["next_player"] = json::Value(ack.effects.nextPlayer);
    effects["turn_continues"] = json::Value(ack.effects.turnContinues);
    effects["ball_in_hand"] = json::Value(ack.effects.ballInHand);
    effects["ball_in_hand_scope"] = json::Value(
        ack.effects.ballInHand ? "table" : "none");
    json::Value groups = json::Value::array();
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[0])));
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[1])));
    effects["groups"] = groups;
    effects["phase"] = json::Value(rulesPhaseName(ack.effects.phase));
    effects["winner"] = json::Value(ack.effects.winner);
    effects["group_choice_pending"] =
        json::Value(ack.effects.groupChoicePending);
    value["effects"] = effects;
    return value;
}

json::Value ballInHandPlacementAckValue(
    const rules::BallInHandPlacementAck& ack)
{
    json::Value value = json::Value::object();
    value["accepted"] = json::Value(ack.accepted);
    value["decision_id"] = json::Value(ack.decisionId);
    value["action_id"] = json::Value(ack.actionId);
    value["idempotency_key"] = json::Value(ack.idempotencyKey);
    value["actor_player_id"] = json::Value(ack.actorPlayerId);
    value["previous_state_revision"] = json::Value(
        static_cast<double>(ack.previousStateRevision));
    value["authoritative_state_revision"] = json::Value(
        static_cast<double>(ack.authoritativeStateRevision));
    value["authoritative_state_hash"] = json::Value(
        ack.authoritativeStateHash);
    json::Value chosen = json::Value::object();
    chosen["world_x_cm"] = json::Value(
        ack.effects.cueBallPosition.worldX);
    chosen["world_z_cm"] = json::Value(
        ack.effects.cueBallPosition.worldZ);
    value["chosen_position"] = chosen;
    json::Value effects = json::Value::object();
    effects["current_player"] = json::Value(ack.effects.currentPlayer);
    effects["next_player"] = json::Value(ack.effects.nextPlayer);
    effects["turn_continues"] = json::Value(ack.effects.turnContinues);
    effects["ball_in_hand"] = json::Value(ack.effects.ballInHand);
    effects["ball_in_hand_scope"] = json::Value(
        ballInHandScopeName(ack.effects.ballInHandScope));
    effects["ball_in_hand_placement_pending"] =
        json::Value(ack.effects.placementPending);
    json::Value groups = json::Value::array();
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[0])));
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[1])));
    effects["groups"] = groups;
    effects["phase"] = json::Value(rulesPhaseName(ack.effects.phase));
    effects["winner"] = json::Value(ack.effects.winner);
    effects["cue_ball_position"] = chosen;
    json::Value ballPositions = json::Value::object();
    ballPositions["cue_ball"] = chosen;
    effects["ball_positions"] = ballPositions;
    value["effects"] = effects;
    return value;
}

json::Value eightOnBreakAckValue(
    const rules::EightOnBreakDecisionAck& ack)
{
    json::Value value = json::Value::object();
    value["accepted"] = json::Value(ack.accepted);
    value["decision_id"] = json::Value(ack.decisionId);
    value["action_id"] = json::Value(ack.actionId);
    value["idempotency_key"] = json::Value(ack.idempotencyKey);
    value["actor_player_id"] = json::Value(ack.actorPlayerId);
    value["previous_state_revision"] = json::Value(
        static_cast<double>(ack.previousStateRevision));
    value["authoritative_state_revision"] = json::Value(
        static_cast<double>(ack.authoritativeStateRevision));
    value["authoritative_state_hash"] = json::Value(
        ack.authoritativeStateHash);
    json::Value effects = json::Value::object();
    effects["branch"] = json::Value(
        eightOnBreakBranchName(ack.effects.branch));
    effects["eight_ball_respotted"] = json::Value(
        ack.effects.eightBallRespotted);
    effects["rack_rebuilt"] = json::Value(ack.effects.rackRebuilt);
    effects["original_breaker_player_id"] = json::Value(
        ack.effects.originalBreakerPlayerId);
    effects["breaker_player_id"] = json::Value(
        ack.effects.breakerPlayerId);
    effects["current_player"] = json::Value(ack.effects.currentPlayer);
    effects["next_player"] = json::Value(ack.effects.nextPlayer);
    effects["turn_continues"] = json::Value(ack.effects.turnContinues);
    effects["ball_in_hand"] = json::Value(ack.effects.ballInHand);
    effects["ball_in_hand_scope"] = json::Value(
        ballInHandScopeName(ack.effects.ballInHandScope));
    effects["decision_pending"] = json::Value(
        ack.effects.decisionPending);
    json::Value groups = json::Value::array();
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[0])));
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[1])));
    effects["groups"] = groups;
    effects["phase"] = json::Value(rulesPhaseName(ack.effects.phase));
    effects["winner"] = json::Value(ack.effects.winner);
    json::Value position = json::Value::object();
    position["world_x_cm"] = json::Value(
        ack.effects.eightBallPosition.x);
    position["world_y_cm"] = json::Value(
        ack.effects.eightBallPosition.y);
    position["world_z_cm"] = json::Value(
        ack.effects.eightBallPosition.z);
    effects["eight_ball_position"] = position;
    value["effects"] = effects;
    return value;
}

json::Value breakFoulAckValue(const rules::BreakFoulDecisionAck& ack)
{
    json::Value value = json::Value::object();
    value["accepted"] = json::Value(ack.accepted);
    value["decision_id"] = json::Value(ack.decisionId);
    value["action_id"] = json::Value(ack.actionId);
    value["idempotency_key"] = json::Value(ack.idempotencyKey);
    value["actor_player_id"] = json::Value(ack.actorPlayerId);
    value["previous_state_revision"] = json::Value(
        static_cast<double>(ack.previousStateRevision));
    value["authoritative_state_revision"] = json::Value(
        static_cast<double>(ack.authoritativeStateRevision));
    value["authoritative_state_hash"] = json::Value(
        ack.authoritativeStateHash);
    json::Value effects = json::Value::object();
    effects["branch"] = json::Value(
        breakFoulBranchName(ack.effects.branch));
    effects["table_accepted"] = json::Value(
        ack.effects.tableAccepted);
    effects["rack_rebuilt"] = json::Value(ack.effects.rackRebuilt);
    effects["original_breaker_player_id"] = json::Value(
        ack.effects.originalBreakerPlayerId);
    effects["breaker_player_id"] = json::Value(
        ack.effects.breakerPlayerId);
    effects["current_player"] = json::Value(ack.effects.currentPlayer);
    effects["next_player"] = json::Value(ack.effects.nextPlayer);
    effects["ball_in_hand"] = json::Value(ack.effects.ballInHand);
    effects["ball_in_hand_scope"] = json::Value(
        ballInHandScopeName(ack.effects.ballInHandScope));
    effects["child_decision_pending"] = json::Value(
        ack.effects.childDecisionPending);
    json::Value groups = json::Value::array();
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[0])));
    groups.asArray().push_back(json::Value(
        rulesGroupName(ack.effects.groups[1])));
    effects["groups"] = groups;
    effects["phase"] = json::Value(rulesPhaseName(ack.effects.phase));
    effects["winner"] = json::Value(ack.effects.winner);
    value["effects"] = effects;
    return value;
}

void requireSchema(const json::Value& params, int expected)
{
    const int schema = intParam(params, "schema_version");
    if (schema != expected)
        throw std::runtime_error("schema_version must be " + std::to_string(expected));
}

}  // namespace

std::vector<std::string> AutomationController::capabilities() const
{
    std::vector<std::string> values = {
        "clear_events", "get_capabilities", "get_events", "get_state", "key_down", "key_up",
        "clear_physics_trace", "get_physics_trace", "load_scenario", "mouse_button", "mouse_move", "mouse_wheel", "orbit_camera", "pan_camera",
        "physics_scenario_v1", "physics_scenario_v2_cue_input", "ping", "quit", "reset_game", "resize", "run_until", "set_aim_yaw", "set_ball",
        "set_player_state", "set_shot_power", "shoot", "special_key", "start_physics_trace", "step", "stop_physics_trace", "toggle_aim",
        "toggle_help", "zoom_camera",
        "get_presentation_state", "presentation_state_v1", "set_tip_offset",
        "set_camera_preset", "set_assist_level", "shot_control_v2",
        "begin_stroke_unsupported", "update_stroke_unsupported",
        "release_stroke_unsupported", "cue_elevation_unsupported",
        "chalk_state_unsupported",
        "training_scenario_v1", "training_load", "training_get_state",
        "training_score", "training_reset", "training_undo", "training_retry",
        "training_save", "training_preview", "training_edit_begin",
        "training_get_overlay_state", "training_set_overlay_advanced",
        "training_edit_set_ball", "training_edit_commit", "training_edit_cancel",
        "replay_v1", "replay_v2_rules_receipts", "replay_load",
        "replay_get_state", "replay_pause",
        "replay_set_speed", "replay_step_tick", "replay_seek_shot",
        "replay_record_v1", "replay_record_begin", "replay_record_get_state",
        "replay_record_finish", "replay_record_save"
        , "chinese_eight_ball_rules_v1",
        "chinese_eight_ball_first_contact_set_v1",
        "simultaneous_contact_and_group_choice",
        "free_ball_placement",
        "break_foul_opponent_choices",
        "eight_on_break_spot_or_rerack",
        "get_rules_state", "set_ruleset",
        "set_called_pocket", "adjudicate_rules_shot",
        "rules.submit_simultaneous_group_choice",
        "rules.confirm_ball_in_hand_placement",
        "rules.submit_break_foul_choice",
        "rules.submit_eight_on_break_choice",
        "authenticated_referee_authority_v1",
        "get_referee_authority_state",
        "wpa_ii_21_stalemate_referee_consumer_v1",
        "rules.stalemate_referee_propose",
        "rules.stalemate_player_refuse",
        "rules.stalemate_referee_final",
        "rules.stalemate_referee_cancel",
        "get_stalemate_authority_state",
        "wpa_ii_19_group_confusion_referee_consumer_v1",
        "rules.group_confusion_propose",
        "rules.group_confusion_final",
        "get_group_confusion_authority_state"
        , "wpa_ii_15_1_signed_frozen_declaration_v1",
        "rules.frozen_cue_object_propose",
        "rules.frozen_cue_object_final",
        "rules.frozen_cue_object_cancel",
        "get_frozen_cue_object_declaration_state"
        , "wpa_ii_15_2_signed_frozen_cushion_declaration_v1",
        "rules.frozen_object_cushion_propose",
        "rules.frozen_object_cushion_final",
        "rules.frozen_object_cushion_cancel",
        "get_frozen_object_cushion_declaration_state"
        , "wpa_ii_12_5_6_signed_close_case_declaration_v1",
        "rules.double_hit_close_case_propose",
        "rules.double_hit_close_case_final",
        "rules.double_hit_close_case_cancel",
        "get_double_hit_close_case_state"
        ,"wpa_ii_12_7_signed_push_review_declaration_v1",
        "rules.push_review_propose","rules.push_review_final",
        "rules.push_review_cancel","get_push_review_state"
        ,"wpa_ii_17_exact_nonphysical_referee_foul_subset_v1",
        "rules.nonphysical_general_foul_propose",
        "rules.nonphysical_general_foul_final",
        "rules.nonphysical_general_foul_cancel",
        "get_nonphysical_general_foul_state"
        ,"wpa_ii_06_g_signed_unintentional_break_miscue_v1",
        "rules.unintentional_break_miscue_propose",
        "rules.unintentional_break_miscue_final",
        "rules.unintentional_break_miscue_cancel",
        "get_unintentional_break_miscue_state"
    };
    if (aiControl_ != nullptr) {
        values.push_back("configure_ai_player");
        values.push_back("get_ai_state");
        values.push_back("poll_ai");
    }
    if (mode_ == AutomationMode::Rendered) values.push_back("screenshot");
    if (mode_ == AutomationMode::Rendered)
        values.push_back("collect_render_performance_v1");
    std::sort(values.begin(), values.end());
    return values;
}

ControllerResult AutomationController::success(int id, const json::Value& result) const
{
    ControllerResult value;
    value.response = automationSuccessResponse(id, result);
    return value;
}

ControllerResult AutomationController::failure(int id, const std::string& code, const std::string& message) const
{
    ControllerResult value;
    value.response = automationErrorResponse(id, code, message);
    return value;
}

ControllerResult AutomationController::handle(const AutomationRequest& request)
{
    try {
        const std::string& command = request.command;
        const json::Value& params = request.params;
        if (replayRecorder_.active() && unrecordedAuthorityMutation(command))
            return failure(request.id, "replay_recording_mutation_unsupported",
                "mutation cannot be represented by Replay v1 while recording");
        if (command == "ping") { json::Value value = json::Value::object(); value["pong"] = json::Value(true); return success(request.id, value); }
        if (command == "collect_render_performance_v1") {
            if (mode_ != AutomationMode::Rendered)
                return failure(request.id, "render_performance_unavailable",
                    "render performance collection requires rendered automation");
            const int warmup = intParam(params, "warmup_frames");
            const int samples = intParam(params, "sample_frames");
            if (warmup < 120 || samples < 300)
                return failure(request.id, "render_performance_sample_contract",
                    "formal collection requires at least 120 warmup and 300 sample frames");
            ControllerResult value = success(request.id);
            value.renderPerformanceRequested = true;
            value.performanceWarmupFrames = warmup;
            value.performanceSampleFrames = samples;
            return value;
        }
        if (command == "get_capabilities") { json::Value value = json::Value::object(); value["commands"] = commandList(capabilities()); value["protocol_version"] = json::Value(1); return success(request.id, value); }
        if (command == "get_state") return success(request.id, serializeAutomationState(runtime_));
        if (command == "get_presentation_state")
            return success(request.id, presentationStateValue(runtime_));
        if (command == "get_rules_state")
            return success(request.id, rulesStateValue(runtime_));
        if (command == "get_referee_authority_state")
            return success(request.id,
                refereeAuthorityStateValue(runtime_));
        if (command == "get_stalemate_authority_state")
            return success(request.id,
                stalemateAuthorityStateValue(runtime_));
        if (command == "get_group_confusion_authority_state")
            return success(request.id,
                groupConfusionAuthorityStateValue(runtime_));
        if (command == "get_frozen_cue_object_declaration_state")
            return success(request.id,
                frozenBallDeclarationStateValue(runtime_));
        if (command == "get_frozen_object_cushion_declaration_state")
            return success(request.id,
                frozenCushionDeclarationStateValue(runtime_));
        if(command=="get_double_hit_close_case_state")
            return success(request.id,doubleHitCloseCaseStateValue(runtime_));
        if(command=="get_push_review_state")return success(request.id,pushReviewStateValue(runtime_));
        if(command=="get_nonphysical_general_foul_state")
            return success(request.id,nonphysicalGeneralFoulStateValue(runtime_));
        if(command=="get_unintentional_break_miscue_state")
            return success(request.id,
                unintentionalBreakMiscueStateValue(runtime_));
        if (command == "get_ai_state") {
            if (aiControl_ == nullptr)
                return failure(request.id, "ai_unavailable",
                    "AI player control is unavailable");
            return success(request.id, aiStateValue(aiControl_->state()));
        }
        if (command == "poll_ai") {
            if (aiControl_ == nullptr)
                return failure(request.id, "ai_unavailable",
                    "AI player control is unavailable");
            return success(request.id,
                aiStateValue(aiControl_->poll(runtime_)));
        }
        if (command == "configure_ai_player") {
            if (aiControl_ == nullptr)
                return failure(request.id, "ai_unavailable",
                    "AI player control is unavailable");
            requireSchema(params, 1);
            if (!params.has("enabled") || !params.at("enabled").isBool())
                throw std::runtime_error("enabled must be a boolean");
            ai::PlayerConfiguration configuration;
            configuration.enabled = params.at("enabled").asBool();
            configuration.player = params.has("player") ?
                params.at("player").asInt() : 1;
            const std::string difficulty = params.has("difficulty") ?
                stringParam(params, "difficulty") : "club";
            if (!difficultyParam(difficulty, configuration.difficulty))
                return failure(request.id, "ai_difficulty_unknown",
                    "difficulty must be rookie, club, or expert");
            configuration.deterministicSeed = tickParam(
                params, "deterministic_seed", 1);
            const ActionResult applied = aiControl_->configure(
                runtime_, configuration);
            if (!applied.ok) return failure(request.id, applied.errorCode,
                "AI player configuration was rejected");
            return success(request.id, aiStateValue(aiControl_->state()));
        }
        if (command == "set_ruleset") {
            requireSchema(params, 1);
            const std::string id = stringParam(params, "ruleset_id");
            RulesMode mode;
            if (id == "legacy") mode = RulesMode::Legacy;
            else if (id == "wpa_heyball_2025_08_16_v1")
                mode = RulesMode::ChineseEightBallV1;
            else return failure(request.id, "ruleset_unknown",
                "unknown ruleset id");
            const ActionResult applied = runtime_.setRulesMode(mode);
            if (!applied.ok) return failure(request.id, applied.errorCode,
                "ruleset change was rejected");
            return success(request.id, rulesStateValue(runtime_));
        }
        if (command == "set_called_pocket") {
            requireSchema(params, 1);
            const ActionResult applied = runtime_.setCalledPocket(
                intParam(params, "pocket"));
            if (!applied.ok) return failure(request.id, applied.errorCode,
                "called pocket was rejected");
            return success(request.id, rulesStateValue(runtime_));
        }
        if (command == "adjudicate_rules_shot") {
            requireSchema(params, 1);
            rules::ShotFacts facts;
            facts.shooter = intParam(params, "shooter");
            facts.breakShot = params.has("break_shot") &&
                params.at("break_shot").asBool();
            facts.firstContactBall = params.has("first_contact_ball") ?
                params.at("first_contact_ball").asInt() : -1;
            if (params.has("first_contact_balls")) {
                for (const json::Value& ball :
                    params.at("first_contact_balls").asArray())
                    facts.firstContactBalls.push_back(ball.asInt());
                if (!params.has("first_contact_ball") &&
                    !facts.firstContactBalls.empty())
                    facts.firstContactBall = facts.firstContactBalls.front();
            }
            facts.distinctObjectBallsToRail =
                params.has("distinct_object_balls_to_rail") ?
                params.at("distinct_object_balls_to_rail").asInt() : 0;
            facts.anyRailAfterFirstContact =
                params.has("any_rail_after_first_contact") &&
                params.at("any_rail_after_first_contact").asBool();
            facts.cueBallPocketed = params.has("cue_ball_pocketed") &&
                params.at("cue_ball_pocketed").asBool();
            facts.cueBallOffTable = params.has("cue_ball_off_table") &&
                params.at("cue_ball_off_table").asBool();
            facts.solidsRemainingBefore = params.has("solids_remaining_before") ?
                params.at("solids_remaining_before").asInt() : 7;
            facts.stripesRemainingBefore = params.has("stripes_remaining_before") ?
                params.at("stripes_remaining_before").asInt() : 7;
            facts.calledPocket = params.has("called_pocket") ?
                params.at("called_pocket").asInt() : -1;
            facts.eightBallPocket = params.has("eight_ball_pocket") ?
                params.at("eight_ball_pocket").asInt() : -1;
            if (params.has("object_balls_off_table")) {
                for (const json::Value& ball :
                    params.at("object_balls_off_table").asArray())
                    facts.objectBallsOffTable.push_back(ball.asInt());
            }
            if (params.has("pocketed_balls_in_order")) {
                for (const json::Value& ball :
                    params.at("pocketed_balls_in_order").asArray())
                    facts.pocketedBallsInOrder.push_back(ball.asInt());
            }
            const ActionResult applied = runtime_.adjudicateRulesShot(facts);
            if (!applied.ok) return failure(request.id, applied.errorCode,
                "rules shot was rejected");
            return success(request.id, rulesStateValue(runtime_));
        }
        if (command == "rules.submit_simultaneous_group_choice") {
            requireSchema(params, 1);
            if (stringParam(params, "ruleset_id") !=
                    "wpa_heyball_2025_08_16_v1" ||
                stringParam(params, "capability_id") !=
                    "simultaneous_contact_and_group_choice" ||
                stringParam(params, "flow_id") !=
                    "simultaneous_group_choice")
                return failure(request.id, "rules_decision_context_mismatch",
                    "rules decision context does not match the authority");
            if (!params.has("payload") || !params.at("payload").isObject())
                return failure(request.id, "invalid_argument",
                    "payload must be an object");
            rules::GroupChoiceDecisionRequest decision;
            decision.rulesetId = stringParam(params, "ruleset_id");
            decision.capabilityId = stringParam(params, "capability_id");
            decision.flowId = stringParam(params, "flow_id");
            decision.decisionId = stringParam(params, "decision_id");
            decision.actionId = stringParam(params, "action_id");
            decision.idempotencyKey = stringParam(params, "idempotency_key");
            decision.actorPlayerId = intParam(params, "actor_player_id");
            decision.expectedAuthorityStateRevision = tickParam(
                params, "authority_state_revision", 0);
            if (!params.has("authority_state_revision"))
                return failure(request.id, "invalid_argument",
                    "authority_state_revision is required");
            decision.payloadCanonical = json::stringify(params.at("payload"));
            const rules::GroupChoiceDecisionResult applied =
                runtime_.submitGroupChoiceDecision(decision);
            if (!applied.ok)
                return failure(request.id, applied.errorCode,
                    "rules group choice was rejected");
            return success(request.id, groupChoiceAckValue(applied.ack));
        }
        if (command == "rules.confirm_ball_in_hand_placement") {
            requireSchema(params, 1);
            if (!params.has("payload") || !params.at("payload").isObject())
                return failure(request.id, "invalid_argument",
                    "payload must be an object");
            const json::Value& payload = params.at("payload");
            if (payload.asObject().size() != 2 ||
                !payload.has("world_x_cm") ||
                !payload.has("world_z_cm"))
                return failure(request.id, "invalid_argument",
                    "payload must contain only world_x_cm and world_z_cm");
            rules::BallInHandPlacementRequest placement;
            placement.rulesetId = stringParam(params, "ruleset_id");
            placement.capabilityId = stringParam(params, "capability_id");
            placement.flowId = stringParam(params, "flow_id");
            placement.decisionId = stringParam(params, "decision_id");
            placement.actionId = stringParam(params, "action_id");
            placement.idempotencyKey = stringParam(
                params, "idempotency_key");
            placement.actorPlayerId = intParam(params, "actor_player_id");
            if (!params.has("authority_state_revision"))
                return failure(request.id, "invalid_argument",
                    "authority_state_revision is required");
            placement.expectedAuthorityStateRevision = tickParam(
                params, "authority_state_revision", 0);
            placement.position.worldX = numberParam(payload, "world_x_cm");
            placement.position.worldZ = numberParam(payload, "world_z_cm");
            const rules::BallInHandPlacementResult applied =
                runtime_.submitBallInHandPlacement(placement);
            if (!applied.ok)
                return failure(request.id, applied.errorCode,
                    "ball-in-hand placement was rejected");
            return success(request.id,
                ballInHandPlacementAckValue(applied.ack));
        }
        if (command == "rules.submit_break_foul_choice") {
            requireSchema(params, 1);
            if (!params.has("payload") ||
                !params.at("payload").isObject() ||
                !params.at("payload").asObject().empty())
                return failure(request.id, "invalid_argument",
                    "payload must be an empty object");
            rules::BreakFoulDecisionRequest decision;
            decision.rulesetId = stringParam(params, "ruleset_id");
            decision.capabilityId = stringParam(params, "capability_id");
            decision.flowId = stringParam(params, "flow_id");
            decision.decisionId = stringParam(params, "decision_id");
            decision.actionId = stringParam(params, "action_id");
            decision.idempotencyKey = stringParam(
                params, "idempotency_key");
            decision.actorPlayerId = intParam(params, "actor_player_id");
            if (!params.has("authority_state_revision"))
                return failure(request.id, "invalid_argument",
                    "authority_state_revision is required");
            decision.expectedAuthorityStateRevision = tickParam(
                params, "authority_state_revision", 0);
            decision.payloadCanonical =
                json::stringify(params.at("payload"));
            const rules::BreakFoulDecisionResult applied =
                runtime_.submitBreakFoulDecision(decision);
            if (!applied.ok)
                return failure(request.id, applied.errorCode,
                    "break-foul decision was rejected");
            return success(request.id, breakFoulAckValue(applied.ack));
        }
        if (command == "rules.submit_eight_on_break_choice") {
            requireSchema(params, 1);
            if (!params.has("payload") ||
                !params.at("payload").isObject() ||
                !params.at("payload").asObject().empty())
                return failure(request.id, "invalid_argument",
                    "payload must be an empty object");
            rules::EightOnBreakDecisionRequest decision;
            decision.rulesetId = stringParam(params, "ruleset_id");
            decision.capabilityId = stringParam(params, "capability_id");
            decision.flowId = stringParam(params, "flow_id");
            decision.decisionId = stringParam(params, "decision_id");
            decision.actionId = stringParam(params, "action_id");
            decision.idempotencyKey = stringParam(
                params, "idempotency_key");
            decision.actorPlayerId = intParam(params, "actor_player_id");
            if (!params.has("authority_state_revision"))
                return failure(request.id, "invalid_argument",
                    "authority_state_revision is required");
            decision.expectedAuthorityStateRevision = tickParam(
                params, "authority_state_revision", 0);
            decision.payloadCanonical =
                json::stringify(params.at("payload"));
            const rules::EightOnBreakDecisionResult applied =
                runtime_.submitEightOnBreakDecision(decision);
            if (!applied.ok)
                return failure(request.id, applied.errorCode,
                    "8-ball-on-break decision was rejected");
            return success(request.id,
                eightOnBreakAckValue(applied.ack));
        }
        if (command == "rules.stalemate_referee_propose" ||
            command == "rules.stalemate_player_refuse" ||
            command == "rules.stalemate_referee_final" ||
            command == "rules.stalemate_referee_cancel") {
            const rules::RefereeAuthorityRequest authority =
                refereeRequestParam(params);
            rules::StalemateResult applied;
            if (command == "rules.stalemate_referee_propose")
                applied = runtime_.submitStalemateProposal(authority);
            else if (command == "rules.stalemate_player_refuse")
                applied = runtime_.submitStalemateRefusal(authority);
            else if (command == "rules.stalemate_referee_final")
                applied = runtime_.submitStalemateFinal(authority);
            else
                applied = runtime_.submitStalemateCancel(authority);
            if (!applied.ok)
                return failure(request.id, applied.errorCode,
                    "stalemate authority request was rejected");
            return success(request.id, refereeAckValue(applied.ack));
        }
        if (command == "rules.group_confusion_propose" ||
            command == "rules.group_confusion_final") {
            const rules::RefereeAuthorityRequest authority =
                refereeRequestParam(params);
            const rules::GroupConfusionResult applied =
                command == "rules.group_confusion_propose" ?
                    runtime_.submitGroupConfusionProposal(authority) :
                    runtime_.submitGroupConfusionFinal(authority);
            if (!applied.ok)
                return failure(request.id, applied.errorCode,
                    "group confusion authority request was rejected");
            return success(request.id, refereeAckValue(applied.ack));
        }
        if (command == "rules.frozen_cue_object_propose" ||
            command == "rules.frozen_cue_object_final" ||
            command == "rules.frozen_cue_object_cancel") {
            const rules::RefereeAuthorityRequest authority=
                refereeRequestParam(params);
            rules::FrozenBallDeclarationResult applied;
            if(command=="rules.frozen_cue_object_propose")
                applied=runtime_.submitFrozenBallDeclarationProposal(authority);
            else if(command=="rules.frozen_cue_object_final")
                applied=runtime_.submitFrozenBallDeclarationFinal(authority);
            else applied=runtime_.submitFrozenBallDeclarationCancel(authority);
            if(!applied.ok)return failure(request.id,applied.errorCode,
                "frozen ball declaration request was rejected");
            return success(request.id,refereeAckValue(applied.ack));
        }
        if (command == "rules.frozen_object_cushion_propose" ||
            command == "rules.frozen_object_cushion_final" ||
            command == "rules.frozen_object_cushion_cancel") {
            const rules::RefereeAuthorityRequest authority =
                refereeRequestParam(params);
            rules::FrozenCushionDeclarationResult applied;
            if (command == "rules.frozen_object_cushion_propose")
                applied = runtime_.submitFrozenCushionDeclarationProposal(
                    authority);
            else if (command == "rules.frozen_object_cushion_final")
                applied = runtime_.submitFrozenCushionDeclarationFinal(
                    authority);
            else
                applied = runtime_.submitFrozenCushionDeclarationCancel(
                    authority);
            if (!applied.ok)
                return failure(request.id, applied.errorCode,
                    "frozen cushion declaration request was rejected");
            return success(request.id, refereeAckValue(applied.ack));
        }
        if(command=="rules.double_hit_close_case_propose"||
            command=="rules.double_hit_close_case_final"||
            command=="rules.double_hit_close_case_cancel"){
            const rules::RefereeAuthorityRequest authority=
                refereeRequestParam(params);
            rules::DoubleHitCloseCaseResult applied;
            if(command=="rules.double_hit_close_case_propose")
                applied=runtime_.submitDoubleHitCloseCaseProposal(authority);
            else if(command=="rules.double_hit_close_case_final")
                applied=runtime_.submitDoubleHitCloseCaseFinal(authority);
            else applied=runtime_.submitDoubleHitCloseCaseCancel(authority);
            if(!applied.ok)return failure(request.id,applied.errorCode,
                "double hit close case request was rejected");
            return success(request.id,refereeAckValue(applied.ack));
        }
        if(command=="rules.push_review_propose"||command=="rules.push_review_final"||command=="rules.push_review_cancel"){
            auto authority=refereeRequestParam(params);rules::PushReviewResult applied;
            if(command=="rules.push_review_propose")applied=runtime_.submitPushReviewProposal(authority);
            else if(command=="rules.push_review_final")applied=runtime_.submitPushReviewFinal(authority);
            else applied=runtime_.submitPushReviewCancel(authority);
            if(!applied.ok)return failure(request.id,applied.errorCode,"push review rejected");
            return success(request.id,refereeAckValue(applied.ack));}
        if(command=="rules.nonphysical_general_foul_propose"||
           command=="rules.nonphysical_general_foul_final"||
           command=="rules.nonphysical_general_foul_cancel"){
            auto authority=refereeRequestParam(params);
            rules::NonphysicalGeneralFoulResult applied;
            if(command=="rules.nonphysical_general_foul_propose")
                applied=runtime_.submitNonphysicalGeneralFoulProposal(authority);
            else if(command=="rules.nonphysical_general_foul_final")
                applied=runtime_.submitNonphysicalGeneralFoulFinal(authority);
            else applied=runtime_.submitNonphysicalGeneralFoulCancel(authority);
            if(!applied.ok)return failure(request.id,applied.errorCode,
                "nonphysical general foul request rejected");
            return success(request.id,refereeAckValue(applied.ack));}
        if(command=="rules.unintentional_break_miscue_propose"||
           command=="rules.unintentional_break_miscue_final"||
           command=="rules.unintentional_break_miscue_cancel"){
            const auto authority=refereeRequestParam(params);
            rules::UnintentionalBreakMiscueDeclarationResult applied;
            if(command=="rules.unintentional_break_miscue_propose")
                applied=runtime_.
                    submitUnintentionalBreakMiscueDeclarationProposal(
                        authority);
            else if(command=="rules.unintentional_break_miscue_final")
                applied=runtime_.
                    submitUnintentionalBreakMiscueDeclarationFinal(authority);
            else applied=runtime_.
                    submitUnintentionalBreakMiscueDeclarationCancel(authority);
            if(!applied.ok)return failure(request.id,applied.errorCode,
                "unintentional break miscue declaration was rejected");
            return success(request.id,refereeAckValue(applied.ack));}
        if (command == "training_get_state")
            return success(request.id, trainingStateValue(trainingSession_));
        if (command == "training_get_overlay_state")
            return success(request.id,
                trajectoryOverlayStateValue(trajectoryOverlay_.snapshot()));
        if (command == "training_set_overlay_advanced") {
            requireSchema(params, 1);
            if (!params.has("expanded") || !params.at("expanded").isBool())
                throw std::runtime_error("expanded must be a bool");
            trajectoryOverlay_.setAdvancedExpanded(
                params.at("expanded").asBool());
            return success(request.id,
                trajectoryOverlayStateValue(trajectoryOverlay_.snapshot()));
        }
        if (command == "training_load") {
            requireSchema(params, 1);
            const std::set<std::string> runtimeCapabilities = {
                "training_scenario_v1",
                "authoritative_horizontal_tip_offset_v1",
                "authoritative_longitudinal_tip_offset_v1"};
            const training::LoadResult loaded = trainingSession_.load(
                stringParam(params, "path"), runtimeCapabilities);
            if (!loaded.ok) return failure(request.id, loaded.errorCode,
                loaded.errorMessage);
            editTransaction_.reset();
            return success(request.id, trainingStateValue(trainingSession_));
        }
        if (command == "training_reset" || command == "training_undo" ||
            command == "training_retry") {
            const bool ok = command == "training_reset" ? trainingSession_.reset() :
                command == "training_undo" ? trainingSession_.undo() :
                trainingSession_.retry();
            if (!ok) return failure(request.id, "training_operation_unavailable",
                "training session operation is unavailable");
            return success(request.id, trainingStateValue(trainingSession_));
        }
        if (command == "training_save") {
            requireSchema(params, 1);
            std::string error;
            if (!trainingSession_.save(stringParam(params, "path"), error))
                return failure(request.id, error, "training save failed");
            return success(request.id, trainingStateValue(trainingSession_));
        }
        if (command == "training_score") {
            requireSchema(params, 1);
            training::AttemptFacts facts;
            facts.state = runtime_.state();
            facts.shotCount = params.has("shot_count") ?
                params.at("shot_count").asInt() : 0;
            facts.elapsedTicks = params.has("elapsed_ticks") ?
                static_cast<std::uint64_t>(params.at("elapsed_ticks").asNumber()) :
                runtime_.tick();
            facts.numericalFailure = params.has("numerical_failure") &&
                params.at("numerical_failure").asBool();
            facts.firstContactBall = params.has("first_contact_ball") ?
                params.at("first_contact_ball").asInt() : -1;
            if (params.has("cue_ball_position_at_first_contact_cm")) {
                const auto& point = params.at(
                    "cue_ball_position_at_first_contact_cm").asArray();
                if (point.size() != 3)
                    throw std::runtime_error(
                        "cue_ball_position_at_first_contact_cm must contain 3 entries");
                facts.cueBallPositionAtFirstContactCm = {
                    static_cast<float>(point[0].asNumber()),
                    static_cast<float>(point[1].asNumber()),
                    static_cast<float>(point[2].asNumber())};
                facts.hasCueBallPositionAtFirstContact = true;
            }
            facts.distinctObjectBallsWithRailContact =
                params.has("distinct_object_balls_with_rail_contact") ?
                params.at("distinct_object_balls_with_rail_contact").asInt() : 0;
            if (params.has("rail_contacts")) {
                const json::Value::Array& contacts = params.at("rail_contacts").asArray();
                if (contacts.size() != kBallCount)
                    throw std::runtime_error("rail_contacts must contain 16 entries");
                for (int index = 0; index < kBallCount; ++index)
                    facts.railContacts[index] = contacts[index].asInt();
            }
            if (params.has("pocket_order")) {
                for (const json::Value& ball : params.at("pocket_order").asArray())
                    facts.pocketOrder.push_back(ball.asInt());
            }
            const training::Evaluation evaluated = trainingSession_.score(facts);
            json::Value value = json::Value::object();
            value["failed"] = json::Value(evaluated.failed);
            value["reason"] = json::Value(evaluated.reason);
            value["score"] = json::Value(evaluated.score);
            value["score_eligible"] = json::Value(evaluated.scoreEligible);
            value["succeeded"] = json::Value(evaluated.succeeded);
            return success(request.id, value);
        }
        if (command == "training_edit_begin") {
            if (!trainingSession_.active()) return failure(request.id,
                "training_session_inactive", "load a training scenario first");
            editTransaction_ = std::make_unique<training::BallEditTransaction>(
                runtime_.state(), runtime_.physicsProfile());
            return success(request.id);
        }
        if (command == "training_edit_set_ball") {
            if (!editTransaction_) return failure(request.id,
                "training_edit_inactive", "begin an edit transaction first");
            const training::EditResult staged = editTransaction_->setBall(
                intParam(params, "index"), pointParam(params, "position"),
                params.has("pocketed") && params.at("pocketed").asBool());
            if (!staged.ok) return failure(request.id, staged.errorCode,
                "ball edit was rejected");
            return success(request.id);
        }
        if (command == "training_edit_commit") {
            if (!editTransaction_) return failure(request.id,
                "training_edit_inactive", "begin an edit transaction first");
            const training::EditResult committed = trainingSession_.commit(
                *editTransaction_);
            if (!committed.ok) return failure(request.id, committed.errorCode,
                "edit transaction validation failed");
            editTransaction_.reset();
            return success(request.id, trainingStateValue(trainingSession_));
        }
        if (command == "training_edit_cancel") {
            editTransaction_.reset();
            return success(request.id);
        }
        if (command == "training_preview") {
            requireSchema(params, 1);
            training::PreviewBudget budget;
            if (params.has("maximum_ticks")) budget.maximumTicks =
                params.at("maximum_ticks").asInt();
            if (params.has("maximum_wall_milliseconds"))
                budget.maximumWallMilliseconds =
                    params.at("maximum_wall_milliseconds").asInt();
            if (params.has("maximum_balls")) budget.maximumBalls =
                params.at("maximum_balls").asInt();
            const training::PreviewResult preview =
                training::previewTrainingAssist(runtime_, budget);
            if (!preview.ok) return failure(request.id, preview.errorCode,
                "training preview failed");
            lastTrainingPreview_ = preview;
            trajectoryOverlay_.setPredicted(lastTrainingPreview_);
            json::Value value = json::Value::object();
            value["events_emitted"] = json::Value(preview.eventsEmitted);
            value["final_state_sha256"] = json::Value(preview.finalStateSha256);
            value["sample_count"] = json::Value(
                static_cast<int>(preview.samples.size()));
            value["simulated_ticks"] = json::Value(preview.simulatedTicks);
            value["tick_budget_reached"] = json::Value(preview.tickBudgetReached);
            value["wall_budget_reached"] = json::Value(preview.wallBudgetReached);
            return success(request.id, value);
        }
        if (command == "replay_get_state")
            return success(request.id, replayStateValue(replayPlayer_));
        if (command == "replay_record_get_state")
        {
            if (replayRecorder_.active()) {
                const replay::ValidationResult captured =
                    replayRecorder_.captureApplied(runtime_);
                if (!captured.ok) return failure(request.id,
                    captured.errorCode, "semantic actions could not be recorded");
            }
            return success(request.id,
                replayRecordingStateValue(replayRecorder_));
        }
        if (command == "replay_record_begin") {
            requireSchema(params, 1);
            const std::string contentSha256 = params.has("content_sha256") ?
                stringParam(params, "content_sha256") :
                trainingSession_.active() ?
                    trainingSession_.scenario().scenarioSha256 : std::string();
            const std::uint64_t seed = tickParam(params, "seed", 1);
            if (seed > 0xffffffffull)
                return failure(request.id, "replay_seed_out_of_range",
                    "seed must fit uint32");
            const replay::ValidationResult begun = replayRecorder_.begin(
                runtime_, stringParam(params, "executable_version"),
                contentSha256, static_cast<std::uint32_t>(seed));
            if (!begun.ok) return failure(request.id, begun.errorCode,
                "replay recording could not begin");
            return success(request.id,
                replayRecordingStateValue(replayRecorder_));
        }
        if (command == "replay_record_finish") {
            requireSchema(params, 1);
            const replay::ValidationResult finished = replayRecorder_.finish(
                runtime_, replayRecorder_.shotCount());
            if (!finished.ok) return failure(request.id, finished.errorCode,
                "replay recording could not finish");
            return success(request.id,
                replayRecordingStateValue(replayRecorder_));
        }
        if (command == "replay_record_save") {
            requireSchema(params, 1);
            const replay::ValidationResult saved = replayRecorder_.saveAtomic(
                stringParam(params, "path"));
            if (!saved.ok) return failure(request.id, saved.errorCode,
                "replay recording could not be saved");
            return success(request.id,
                replayRecordingStateValue(replayRecorder_));
        }
        if (command == "replay_load") {
            requireSchema(params, 1);
            replay::Document document;
            const replay::ValidationResult parsed = replay::parse(
                params.at("replay"), document);
            if (!parsed.ok) return failure(request.id, parsed.errorCode,
                "replay document is invalid");
            const replay::ValidationResult loaded = replayPlayer_.load(
                document, runtime_.physicsProfile(),
                stringParam(params, "executable_version"),
                stringParam(params, "content_sha256"));
            if (!loaded.ok) return failure(request.id, loaded.errorCode,
                "replay binding was rejected");
            return success(request.id, replayStateValue(replayPlayer_));
        }
        if (command == "replay_pause") {
            if (!params.has("paused") || !params.at("paused").isBool())
                throw std::runtime_error("paused must be boolean");
            replayPlayer_.setPaused(params.at("paused").asBool());
            return success(request.id, replayStateValue(replayPlayer_));
        }
        if (command == "replay_set_speed") {
            const replay::ValidationResult result = replayPlayer_.setPlaybackRate(
                numberParam(params, "rate"));
            if (!result.ok) return failure(request.id, result.errorCode,
                "replay speed was rejected");
            return success(request.id, replayStateValue(replayPlayer_));
        }
        if (command == "replay_step_tick") {
            const replay::ValidationResult result = replayPlayer_.stepTick();
            if (!result.ok) return failure(request.id, result.errorCode,
                "replay tick failed");
            return success(request.id, replayStateValue(replayPlayer_));
        }
        if (command == "replay_seek_shot") {
            const int ordinal = intParam(params, "shot_ordinal");
            if (ordinal < 0) return failure(request.id, "invalid_argument",
                "shot_ordinal must be nonnegative");
            const replay::ValidationResult result = replayPlayer_.seekShot(
                static_cast<std::size_t>(ordinal));
            if (!result.ok) return failure(request.id, result.errorCode,
                "replay seek failed");
            return success(request.id, replayStateValue(replayPlayer_));
        }
        if (command == "get_events") {
            const std::uint64_t after = params.has("after_sequence") ? static_cast<std::uint64_t>(params.at("after_sequence").asNumber()) : 0;
            json::Value list = json::Value::array(); for (const RuntimeEvent& event : runtime_.eventsSince(after)) list.asArray().push_back(serializeRuntimeEvent(event));
            json::Value value = json::Value::object(); value["events"] = list; return success(request.id, value);
        }
        if (command == "reset_game") { runtime_.reset(); return success(request.id); }
        if (command == "clear_events") { runtime_.clearEvents(); return success(request.id); }
        if (command == "start_physics_trace") { runtime_.setPhysicsTraceEnabled(true); return success(request.id); }
        if (command == "stop_physics_trace") { runtime_.setPhysicsTraceEnabled(false); return success(request.id); }
        if (command == "clear_physics_trace") { runtime_.clearPhysicsTrace(); return success(request.id); }
        if (command == "get_physics_trace") {
            const std::uint64_t afterTick = tickParam(params, "after_tick", 0);
            const int limit = params.has("limit") ? params.at("limit").asInt() : 1000;
            if (limit < 1 || limit > 1000) return failure(request.id, "invalid_argument", "limit must be between 1 and 1000");

            json::Value frames = json::Value::array();
            bool hasMore = false;
            for (const PhysicsFrame& frame : runtime_.physicsTrace().frames()) {
                if (frame.tick <= afterTick) continue;
                if (static_cast<int>(frames.asArray().size()) == limit) {
                    hasMore = true;
                    break;
                }
                frames.asArray().push_back(serializePhysicsFrame(frame));
            }
            json::Value value = json::Value::object();
            value["dropped_frames"] = json::Value(
                static_cast<double>(runtime_.physicsTrace().droppedFrames()));
            value["frames"] = frames;
            value["has_more"] = json::Value(hasMore);
            return success(request.id, value);
        }
        if (command == "quit") { ControllerResult value = success(request.id); value.quitRequested = true; return value; }

        if (command == "set_tip_offset") {
            requireSchema(params, 2);
            const ActionResult applied = runtime_.setTipOffset(
                static_cast<float>(numberParam(params, "x")),
                static_cast<float>(numberParam(params, "y")));
            if (!applied.ok) return failure(request.id, applied.errorCode,
                "tip offset was rejected");
            if (replayRecorder_.active()) {
                const replay::ValidationResult recorded =
                    replayRecorder_.captureApplied(runtime_);
                if (!recorded.ok) return failure(request.id, recorded.errorCode,
                    "applied tip offset could not be recorded");
            }
            return success(request.id, presentationStateValue(runtime_));
        }
        if (command == "set_assist_level") {
            requireSchema(params, 2);
            const std::string level = stringParam(params, "level");
            AssistLevel assist;
            if (level == "off") assist = AssistLevel::Off;
            else if (level == "basic") assist = AssistLevel::Basic;
            else if (level == "training") assist = AssistLevel::Training;
            else throw std::runtime_error("level must be off, basic, or training");
            const ActionResult applied = runtime_.setAssistLevel(assist);
            if (!applied.ok) return failure(request.id, applied.errorCode,
                "assist level was rejected");
            return success(request.id, presentationStateValue(runtime_));
        }
        if (command == "set_camera_preset") {
            requireSchema(params, 1);
            const std::string presetName = stringParam(params, "preset");
            camera::Preset preset;
            camera::SemanticState state;
            if (presetName == "player_low") { preset = camera::Preset::PlayerLow; state = camera::SemanticState::AimPlayer; }
            else if (presetName == "elevated") { preset = camera::Preset::Elevated; state = camera::SemanticState::ObserveFree; }
            else if (presetName == "overhead") { preset = camera::Preset::Overhead; state = camera::SemanticState::AimOverhead; }
            else if (presetName == "broadcast") { preset = camera::Preset::Broadcast; state = camera::SemanticState::ReplayBroadcast; }
            else throw std::runtime_error("preset must be player_low, elevated, overhead, or broadcast");
            bool instant = false;
            if (params.has("instant")) {
                if (!params.at("instant").isBool())
                    throw std::runtime_error("instant must be boolean");
                instant = params.at("instant").asBool();
            }
            const ActionResult applied = instant ?
                runtime_.setCameraPresetInstant(state, preset) :
                runtime_.setCameraPreset(state, preset);
            if (!applied.ok) return failure(request.id, applied.errorCode,
                "camera preset was rejected");
            return success(request.id, presentationStateValue(runtime_));
        }
        if (command == "begin_stroke" || command == "update_stroke" ||
            command == "release_stroke") {
            return failure(request.id, "unsupported_capability",
                "stroke input capability is not promoted");
        }

        if (command == "set_ball") {
            const int index = intParam(params, "index");
            if (index < 0 || index >= kBallCount) return failure(request.id, "invalid_argument", "ball index must be between 0 and 15");
            BallState ball = runtime_.state().balls[index];
            if (params.has("position")) ball.position = pointParam(params, "position");
            if (params.has("velocity")) ball.velocity = pointParam(params, "velocity");
            if (params.has("angular_velocity")) ball.angularVelocity = pointParam(params, "angular_velocity");
            if (params.has("rotation_axis")) ball.rotationAxis = pointParam(params, "rotation_axis");
            if (params.has("rotation_angle")) ball.rotationAngle = static_cast<float>(numberParam(params, "rotation_angle"));
            if (params.has("pocketed")) { if (!params.at("pocketed").isBool()) throw std::runtime_error("pocketed must be boolean"); ball.pocketed = params.at("pocketed").asBool(); }
            ball.speed = std::sqrt(ball.velocity.x*ball.velocity.x + ball.velocity.y*ball.velocity.y + ball.velocity.z*ball.velocity.z);
            runtime_.setBall(index, ball); return success(request.id);
        }
        if (command == "set_player_state") {
            GameState state = runtime_.state();
            if (params.has("current_player")) state.players.currentPlayer = params.at("current_player").asInt();
            if (params.has("next_player")) state.players.nextPlayer = params.at("next_player").asInt();
            if ((state.players.currentPlayer < 0 || state.players.currentPlayer > 1) || (state.players.nextPlayer < 0 || state.players.nextPlayer > 1)) throw std::runtime_error("player index must be 0 or 1");
            if (params.has("illegal_shot")) state.players.illegalShot = params.at("illegal_shot").asBool();
            runtime_.replaceState(state); return success(request.id);
        }
        if (command == "load_scenario") {
            if (params.has("scenario")) {
                const PhysicsScenarioResult parsed = parsePhysicsScenario(params.at("scenario"));
                if (!parsed.ok) return failure(request.id, parsed.errorCode, parsed.errorMessage);
                const ActionResult applied = applyPhysicsScenario(runtime_, parsed.scenario);
                if (!applied.ok) return failure(request.id, applied.errorCode, "scenario was rejected");
                return success(request.id);
            }
            if (!params.has("balls") || !params.at("balls").isArray() || params.at("balls").asArray().size() != kBallCount) throw std::runtime_error("balls must contain exactly 16 entries");
            GameState state = runtime_.state();
            for (int index=0; index<kBallCount; ++index) {
                const json::Value& item=params.at("balls").asArray()[index]; if (!item.isObject()) throw std::runtime_error("each ball must be an object");
                BallState ball=state.balls[index]; ball.position=pointParam(item,"position"); ball.velocity=pointParam(item,"velocity");
                ball.pocketed=item.has("pocketed") ? item.at("pocketed").asBool() : false;
                ball.trajectoryGeneration = 0;
                if (item.has("trajectory_generation") &&
                    (!item.at("trajectory_generation").isString() ||
                        !parseCanonicalTrajectoryGeneration(
                            item.at("trajectory_generation").asString(),
                            ball.trajectoryGeneration)))
                    throw std::runtime_error(
                        "trajectory_generation must be a canonical unsigned 64-bit decimal string");
                ball.speed=std::sqrt(ball.velocity.x*ball.velocity.x+ball.velocity.y*ball.velocity.y+ball.velocity.z*ball.velocity.z); state.balls[index]=ball;
            }
            runtime_.replaceState(state); return success(request.id);
        }

        GameAction action;
        bool hasAction = true;
        if (command == "toggle_aim") action.type = ActionType::ToggleAim;
        else if (command == "toggle_help") action.type = ActionType::ToggleHelp;
        else if (command == "shoot") action.type = ActionType::Shoot;
        else if (command == "set_aim_yaw") { action.type = ActionType::SetAimYaw; action.first = static_cast<float>(numberParam(params, "yaw")); }
        else if (command == "set_shot_power") { action.type = ActionType::SetShotPower; action.first = static_cast<float>(numberParam(params, "power")); }
        else if (command == "key_down" || command == "key_up") { action.type = command == "key_down" ? ActionType::KeyDown : ActionType::KeyUp; const std::string key = stringParam(params, "key"); if (key.size() != 1) throw std::runtime_error("key must contain one character"); action.firstInt = static_cast<unsigned char>(key[0]); }
        else if (command == "special_key") { action.type = ActionType::SpecialKey; const std::string key = stringParam(params, "key"); if (key == "left") action.firstInt = 0; else if (key == "right") action.firstInt = 1; else if (key == "up") action.firstInt = 2; else if (key == "down") action.firstInt = 3; else throw std::runtime_error("unknown special key"); }
        else if (command == "mouse_move") { action.type = ActionType::MouseMove; action.firstInt = intParam(params, "x"); action.secondInt = intParam(params, "y"); }
        else if (command == "mouse_wheel") { action.type = ActionType::MouseWheel; action.firstInt = intParam(params, "direction"); }
        else if (command == "mouse_button") { action.type = ActionType::MouseButton; const std::string button = stringParam(params, "button"); const std::string state = stringParam(params, "state"); action.firstInt = button == "left" ? 0 : button == "right" ? 1 : button == "other" ? 2 : -1; if (action.firstInt < 0 || (state != "down" && state != "up")) throw std::runtime_error("invalid mouse button or state"); action.secondInt = state == "down" ? 0 : 1; action.thirdInt = params.has("x") ? params.at("x").asInt() : 0; action.first = static_cast<float>(params.has("y") ? params.at("y").asInt() : 0); }
        else if (command == "resize") { action.type = ActionType::Resize; action.firstInt = intParam(params, "width"); action.secondInt = intParam(params, "height"); }
        else if (command == "orbit_camera") { action.type = ActionType::OrbitCamera; action.first = static_cast<float>(numberParam(params, "yaw_delta")); action.second = static_cast<float>(numberParam(params, "pitch_delta")); }
        else if (command == "pan_camera") { action.type = ActionType::PanCamera; action.first = static_cast<float>(numberParam(params, "x_delta")); action.second = static_cast<float>(numberParam(params, "z_delta")); }
        else if (command == "zoom_camera") { action.type = ActionType::ZoomCamera; action.first = static_cast<float>(numberParam(params, "delta")); }
        else hasAction = false;

        if (hasAction) {
            const GameState beforeAction = runtime_.state();
            const ActionResult result = runtime_.dispatch(action);
            if (!result.ok) return failure(request.id, result.errorCode, "game action was rejected");
            if (action.type == ActionType::Shoot && trainingSession_.active())
                trainingSession_.markShotStart(beforeAction);
            if (replayRecorder_.active()) {
                const replay::ValidationResult recorded =
                    replayRecorder_.captureApplied(runtime_);
                if (!recorded.ok) return failure(request.id,
                    recorded.errorCode,
                    "applied semantic action could not be recorded");
            }
            return success(request.id);
        }
        if (command == "step") {
            const int ticks = intParam(params, "ticks");
            if (ticks < 0 || ticks > 100000) return failure(request.id, "invalid_argument", "ticks must be between 0 and 100000");
            const ActionResult result = runtime_.step(ticks);
            if (!result.ok) return failure(request.id, result.errorCode,
                std::string("physics step failed and was rolled back") +
                (runtime_.hasRecentPhysicsStep() &&
                 !runtime_.latestPhysicsStep().telemetry.failureDetail.empty() ?
                    ": " + runtime_.latestPhysicsStep().telemetry.failureDetail : ""));
            json::Value value = json::Value::object(); value["tick"] = json::Value(static_cast<double>(runtime_.tick())); return success(request.id, value);
        }
        if (command == "run_until") {
            const std::string condition=stringParam(params,"condition"); const int maxSteps=params.has("max_steps") ? params.at("max_steps").asInt() : 10000;
            if (maxSteps < 0 || maxSteps > 1000000) return failure(request.id,"invalid_argument","max_steps must be between 0 and 1000000");
            const std::uint64_t sequence = runtime_.events().empty() ? 0 : runtime_.events().back().sequence;
            int steps=0; while (steps<maxSteps && !eventConditionMet(runtime_,condition,sequence)) {
                const ActionResult result = runtime_.step(1);
                if (!result.ok) return failure(request.id, result.errorCode,
                    std::string("physics step failed and was rolled back") +
                    (runtime_.hasRecentPhysicsStep() &&
                     !runtime_.latestPhysicsStep().telemetry.failureDetail.empty() ?
                        ": " + runtime_.latestPhysicsStep().telemetry.failureDetail : ""));
                ++steps;
            }
            json::Value value=json::Value::object(); value["tick"]=json::Value(static_cast<double>(runtime_.tick())); value["steps"]=json::Value(steps); value["balls_moving"]=json::Value(runtime_.state().ballsMoving);
            if (!eventConditionMet(runtime_,condition,sequence)) { ControllerResult result=failure(request.id,"condition_not_met","condition was not met before max_steps"); result.response["result"]=value; return result; }
            return success(request.id,value);
        }
        if (command == "screenshot") {
            if (mode_ != AutomationMode::Rendered) return failure(request.id, "unsupported_in_mode", "screenshot requires rendered mode");
            json::Value details=json::Value::object(); details["tick"]=json::Value(static_cast<double>(runtime_.tick()));
            ControllerResult value = success(request.id,details); value.screenshotRequested = true; value.screenshotPath = stringParam(params, "path"); return value;
        }
        return failure(request.id, "unknown_command", "unknown command: " + command);
    } catch (const std::exception& error) {
        return failure(request.id, "invalid_argument", error.what());
    }
}

}  // namespace billiardgl
