#include "render/ui/rules_decision_ui_draw_list.h"
#include "render/modern/camera_projection.h"
#include "render/ui/sdf_font_atlas.h"
#include "rules/rules_decision_ui_model.h"
#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::rules;
using namespace billiardgl::rules::ui;
using namespace billiardgl::render;
using namespace billiardgl::render::ui;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

GroupChoiceDecisionOffer groupOffer()
{
    GroupChoiceDecisionOffer offer;
    offer.decisionId = "rules.group_choice.42.1";
    offer.authorityStateRevision = 42;
    offer.actorPlayerId = 0;
    return offer;
}

BallInHandPlacementOffer placementOffer(BallInHandPlacementScope scope)
{
    BallInHandPlacementOffer offer;
    offer.decisionId = scope == BallInHandPlacementScope::FullTable ?
        "rules.ball_in_hand.full_table.42.1" :
        "rules.ball_in_hand.behind_head_string.42.1";
    offer.authorityStateRevision = 42;
    offer.actorPlayerId = 0;
    offer.scope = scope;
    if (scope == BallInHandPlacementScope::BehindHeadString) {
        offer.offeredActionIds[0] =
            kConfirmBehindHeadStringPlacementActionId;
        offer.sourceClause = kBehindHeadStringSourceClause;
        offer.sourcePdfPage = 8;
        offer.headStringWorldZCm = -63.5;
    }
    return offer;
}

EightOnBreakDecisionOffer eightOffer(EightOnBreakBranch branch)
{
    EightOnBreakDecisionOffer offer;
    offer.decisionId = "rules.eight_on_break.42.1";
    offer.authorityStateRevision = 42;
    offer.actorPlayerId = 0;
    offer.branch = branch;
    offer.offeredActionIds = branch == EightOnBreakBranch::LegalBreak ?
        std::vector<std::string>{kEightOnBreakSpotContinueActionId,
            kEightOnBreakRerackOriginalActionId} :
        std::vector<std::string>{kEightOnBreakSpotBallInHandActionId,
            kEightOnBreakRerackOpponentActionId,
            kEightOnBreakRerackOriginalActionId};
    return offer;
}

BreakFoulDecisionOffer breakOffer(BreakFoulBranch branch)
{
    BreakFoulDecisionOffer offer;
    offer.decisionId = "rules.break_foul.42.1";
    offer.authorityStateRevision = 42;
    offer.actorPlayerId = 0;
    offer.branch = branch;
    if (branch == BreakFoulBranch::IllegalBreak) {
        offer.sourceClause = kIllegalBreakSourceClause;
        offer.offeredActionIds = {kBreakFoulAcceptTableActionId,
            kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId};
    } else if (branch == BreakFoulBranch::NoRackContactMiscue) {
        offer.sourceClause = kBreakNoRackContactSourceClause;
        offer.sourcePdfPage = 8;
        offer.offeredActionIds = {kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId};
    } else {
        offer.sourceClause = kBreakShotFoulSourceClause;
        offer.offeredActionIds = {kBreakFoulBallInHandActionId,
            kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId};
    }
    offer.sourcePdfPage = 7;
    return offer;
}

DecisionAckIdentity acceptedAck(const DecisionSubmissionIntent& request)
{
    DecisionAckIdentity ack;
    ack.accepted = true;
    ack.decisionId = request.offer.decisionId;
    ack.actionId = request.actionId;
    ack.idempotencyKey = request.idempotencyKey;
    ack.actorPlayerId = request.offer.actorPlayerId;
    ack.previousStateRevision = request.offer.authorityStateRevision;
    ack.authoritativeStateRevision = ack.previousStateRevision + 1;
    ack.authoritativeStateHash = std::string(64, 'a');
    return ack;
}

void testStateAndAuthorityBindings()
{
    RulesDecisionUiModel model;
    expect(model.present(groupOffer(), 0), "group offer is accepted");
    expect(model.snapshot().state == DecisionUiState::DecisionOpenUnselected &&
        model.snapshot().armedActionId.empty() &&
        model.snapshot().visibleActionIds ==
            std::vector<std::string>{kGroupChoiceSolidsActionId,
                kGroupChoiceStripesActionId},
        "group UI opens unselected with exact authority order");
    expect(model.focusAction(kGroupChoiceSolidsActionId) &&
        model.snapshot().armedActionId.empty(),
        "focus never arms an action");
    expect(model.armAction(kGroupChoiceSolidsActionId) &&
        model.snapshot().state == DecisionUiState::ActionArmedNotSubmitted,
        "first activation arms without submission");
    model.escape();
    expect(model.snapshot().state == DecisionUiState::DecisionOpenUnselected &&
        model.snapshot().armedActionId.empty(),
        "Escape clears only the arm and keeps blocking decision open");
    expect(model.armAction(kGroupChoiceStripesActionId),
        "second group action can be armed explicitly");
    const auto submission = model.confirmArmedAction("ui-test-key-1");
    expect(submission && submission->offer.rulesetId ==
        kRulesDecisionUiRulesetId && submission->offer.capabilityId ==
        kGroupChoiceCapabilityId && submission->offer.flowId ==
        kGroupChoiceFlowId && submission->actionId ==
        kGroupChoiceStripesActionId && submission->payloadCanonical == "{}",
        "confirm produces an exact transaction envelope");
    const auto typedGroup = groupChoiceRequest(*submission);
    expect(typedGroup && typedGroup->decisionId == submission->offer.decisionId &&
        typedGroup->expectedAuthorityStateRevision == 42 &&
        typedGroup->actionId == kGroupChoiceStripesActionId &&
        typedGroup->idempotencyKey == "ui-test-key-1",
        "generic UI intent binds exactly to stable group authority request");
    expect(!model.confirmArmedAction("ui-test-key-2"),
        "submitting cannot double-submit");
    expect(model.retryExactSubmission() &&
        model.retryExactSubmission()->idempotencyKey == "ui-test-key-1",
        "retry preserves the byte-identical transaction and key");
    model.beginAckVerification();
    expect(model.acceptAck(acceptedAck(*submission)) &&
        model.snapshot().state == DecisionUiState::CompletedAuthoritative &&
        !model.snapshot().shotInputBlocked,
        "only an exact accepted ACK completes the UI transaction");

    auto malformed = groupOffer();
    malformed.offeredActionIds[1] = "rules.foundation.rerack";
    expect(!model.present(malformed, 0) &&
        model.snapshot().state == DecisionUiState::AuthorityErrorBlocking &&
        model.snapshot().visibleActionIds.empty() &&
        model.snapshot().localAuthorityMutationCount == 0,
        "unknown/foundation inventory fails closed and is never visible");

    expect(model.present(groupOffer(), 1) &&
        model.snapshot().state == DecisionUiState::AiOrRemotePending &&
        model.snapshot().visibleActionIds.empty() &&
        model.snapshot().shotInputBlocked,
        "non-local actor renders pending with no interactive actions");

    expect(model.present(eightOffer(EightOnBreakBranch::LegalBreak), 0) &&
        model.snapshot().visibleActionIds ==
            std::vector<std::string>{kEightOnBreakSpotContinueActionId,
                kEightOnBreakRerackOriginalActionId},
        "legal eight-on-break action inventory is exact");
    expect(model.present(eightOffer(EightOnBreakBranch::FoulBreak), 0) &&
        model.snapshot().visibleActionIds ==
            std::vector<std::string>{kEightOnBreakSpotBallInHandActionId,
                kEightOnBreakRerackOpponentActionId,
                kEightOnBreakRerackOriginalActionId},
        "foul eight-on-break action inventory is exact");
    expect(model.present(breakOffer(BreakFoulBranch::IllegalBreak), 0) &&
        model.snapshot().visibleActionIds.front() ==
            kBreakFoulAcceptTableActionId,
        "illegal-break action inventory binds to clause 6C");
    expect(model.present(breakOffer(BreakFoulBranch::BreakShotFoul), 0) &&
        model.snapshot().visibleActionIds.front() ==
            kBreakFoulBallInHandActionId,
        "break-shot-foul action inventory binds to clause 6E");
    expect(model.present(breakOffer(BreakFoulBranch::NoRackContactMiscue), 0) &&
        model.snapshot().visibleActionIds == std::vector<std::string>{
            kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId},
        "no-rack-contact UI framework exposes only clause 6G rerack actions");

    auto wrongClause = breakOffer(BreakFoulBranch::BreakShotFoul);
    wrongClause.sourceClause = kIllegalBreakSourceClause;
    expect(!model.present(wrongClause, 0) &&
        model.snapshot().visibleActionIds.empty(),
        "branch/source-clause drift blocks instead of remapping actions");
    auto noRackWithBallInHand =
        breakOffer(BreakFoulBranch::NoRackContactMiscue);
    noRackWithBallInHand.offeredActionIds.insert(
        noRackWithBallInHand.offeredActionIds.begin(),
        kBreakFoulBallInHandActionId);
    expect(!model.present(noRackWithBallInHand, 0) &&
        model.snapshot().visibleActionIds.empty(),
        "clause 6G framework rejects ball-in-hand inventory expansion");
}

void testPlacementAndFailClosedBehavior()
{
    RulesDecisionUiModel model;
    expect(model.present(placementOffer(
        BallInHandPlacementScope::FullTable), 0),
        "full-table placement offer is accepted");
    BallInHandPlacementPosition candidate{12.25, -36.5};
    BallInHandPlacementValidation legal;
    legal.ok = true;
    legal.authoritativePosition = candidate;
    expect(model.setPlacementPreview(candidate, legal,
        "wrong-decision", 42) &&
        model.snapshot().state ==
            DecisionUiState::PlacementPreviewLocalInvalid &&
        !model.armAction(kConfirmFullTablePlacementActionId),
        "stale validation never enables placement confirm");
    expect(model.setPlacementPreview(candidate, legal,
        model.snapshot().offer.decisionId, 42) &&
        model.snapshot().state == DecisionUiState::PlacementPreviewLocalValid &&
        model.armAction(kConfirmFullTablePlacementActionId),
        "fresh exact validation enables arm only");
    const auto intent = model.confirmArmedAction("placement-key-1");
    expect(intent && intent->hasPlacement &&
        intent->payloadCanonical == "x=12.25;z=-36.5" &&
        model.snapshot().localAuthorityMutationCount == 0,
        "placement submission retains exact local candidate without mutation");
    const auto typedPlacement = ballInHandPlacementRequest(*intent);
    expect(typedPlacement && typedPlacement->rulesetId ==
        kBallInHandRulesetId && typedPlacement->flowId ==
        kFullTableBallInHandFlowId &&
        typedPlacement->position.worldX == 12.25 &&
        typedPlacement->position.worldZ == -36.5,
        "placement intent binds exactly to stable placement request");
    auto badAck = acceptedAck(*intent);
    badAck.actionId = kConfirmBehindHeadStringPlacementActionId;
    expect(!model.acceptAck(badAck) &&
        model.snapshot().state == DecisionUiState::StateDriftRefreshing &&
        model.snapshot().visibleActionIds.empty() &&
        model.snapshot().localAuthorityMutationCount == 0,
        "ACK mismatch clears local intent and refreshes without effects");

    expect(model.present(placementOffer(
        BallInHandPlacementScope::BehindHeadString), 0),
        "behind-head-string offer is accepted");
    BallInHandPlacementValidation illegal;
    illegal.errorCode = "rules_placement_outside_head_string_area";
    expect(model.setPlacementPreview(candidate, illegal,
        model.snapshot().offer.decisionId, 42),
        "invalid preview remains local and visible");
    model.escape();
    expect(!model.snapshot().placementPreviewVisible &&
        model.snapshot().state == DecisionUiState::DecisionOpenUnselected &&
        model.snapshot().shotInputBlocked,
        "placement Escape clears preview but preserves entitlement overlay");
    model.authorityError("future_unknown_authority_code");
    model.escape();
    expect(model.snapshot().state == DecisionUiState::AuthorityErrorBlocking &&
        model.snapshot().blockingCode == "future_unknown_authority_code" &&
        !model.retryExactSubmission(),
        "unknown authority error remains visible and blocking");
}

void testRequestAdaptersAndErrorInventory()
{
    RulesDecisionUiModel model;
    expect(model.present(eightOffer(EightOnBreakBranch::LegalBreak), 0) &&
        model.armAction(kEightOnBreakSpotContinueActionId),
        "eight action arms for adapter test");
    const auto eightIntent = model.confirmArmedAction("eight-key-1");
    const auto typedEight = eightOnBreakRequest(*eightIntent);
    expect(typedEight && typedEight->rulesetId == kEightOnBreakRulesetId &&
        typedEight->capabilityId == kEightOnBreakCapabilityId &&
        typedEight->flowId == kEightOnBreakFlowId &&
        typedEight->payloadCanonical == "{}",
        "eight-on-break intent binds exactly to stable authority request");

    expect(model.present(breakOffer(BreakFoulBranch::IllegalBreak), 0) &&
        model.armAction(kBreakFoulAcceptTableActionId),
        "break-foul action arms for adapter test");
    const auto breakIntent = model.confirmArmedAction("break-key-1");
    const auto typedBreak = breakFoulRequest(*breakIntent);
    expect(typedBreak && typedBreak->rulesetId == kBreakFoulRulesetId &&
        typedBreak->capabilityId == kBreakFoulCapabilityId &&
        typedBreak->flowId == kBreakFoulFlowId &&
        typedBreak->actionId == kBreakFoulAcceptTableActionId,
        "break-foul intent binds exactly to stable authority request");

    expect(isKnownDecisionErrorCode("rules_decision_stale") &&
        decisionErrorDisplayGroup("rules_decision_stale") ==
            DecisionErrorDisplayGroup::StaleOrDrift &&
        decisionErrorDisplayGroup("rules_decision_actor_mismatch") ==
            DecisionErrorDisplayGroup::WrongActorOrOffer &&
        decisionErrorDisplayGroup("rules_placement_overlap") ==
            DecisionErrorDisplayGroup::PlacementInvalid &&
        decisionErrorDisplayGroup("rules_decision_idempotency_conflict") ==
            DecisionErrorDisplayGroup::TransactionConflict &&
        decisionErrorDisplayGroup("rules_respot_no_legal_position") ==
            DecisionErrorDisplayGroup::NoLegalResult &&
        decisionErrorDisplayGroup("future_unknown_authority_code") ==
            DecisionErrorDisplayGroup::UnknownBlocking,
        "gate error inventory maps to fail-closed display groups");
}

void testPolicyBlockers()
{
    RulesDecisionUiModel model;
    model.presentSection6gClassificationPending();
    expect(model.snapshot().state ==
        DecisionUiState::ClassificationPendingBlocked &&
        model.snapshot().blockingCode == kSection6gPendingCode &&
        model.snapshot().visibleActionIds.empty() &&
        model.snapshot().localAuthorityMutationCount == 0,
        "section 6G stays a non-interactive classification blocker");
    model.presentOffTablePolicyUnresolved();
    expect(model.snapshot().state == DecisionUiState::PolicyUnresolvedBlocked &&
        model.snapshot().blockingCode == kOffTablePolicyId &&
        model.snapshot().visibleActionIds.empty() &&
        model.snapshot().localAuthorityMutationCount == 0,
        "12.3 versus 17(e) stays unresolved and action-free");
}

struct MachineCase {
    std::string id;
    std::string state;
    std::string locale;
    std::string palette;
    std::string viewportDensity;
    float uiScale = 1.0f;
};

std::string semanticSignature(const RulesDecisionDrawList& drawList)
{
    std::ostringstream output;
    output << drawList.stateId << '|' << drawList.armedActionId << '|';
    for (const auto& action : drawList.offeredActionIds) output << action << ',';
    output << '|';
    for (const auto& item : drawList.items)
        output << static_cast<int>(item.kind) << ':' << item.actionId << ':'
               << item.enabled << ':' << item.armed << ';';
    return output.str();
}

std::string jsonEscape(const std::string& input)
{
    std::string output;
    for (char value : input) {
        switch (value) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\n': output += "\\n"; break;
        default: output += value; break;
        }
    }
    return output;
}

std::vector<std::uint32_t> requiredCodepoints()
{
    const auto hud = requiredHudCodepoints();
    std::set<std::uint32_t> unique(hud.begin(), hud.end());
    for (const std::string& text : allRulesDecisionUiStrings()) {
        std::vector<std::uint32_t> decoded;
        std::string error;
        if (!decodeUtf8(text, decoded, error))
            throw std::runtime_error("fixture UTF-8 invalid: " + error);
        unique.insert(decoded.begin(), decoded.end());
    }
    return {unique.begin(), unique.end()};
}

std::vector<MachineCase> runMachineMatrix(const std::string& fontPath,
    SdfFontAtlas& atlas)
{
    FontAtlasConfig config;
    config.fontPath = fontPath;
    config.nominalPixelSize = 36;
    config.atlasWidth = 4096;
    config.atlasHeight = 4096;
    config.sdfSpread = 6;
    const auto initialized = atlas.initialize(config, requiredCodepoints());
    expect(initialized.ok, "shipping SDF backend initializes all en/zh_cn glyphs: " +
        initialized.errorCode);
    if (!initialized.ok) return {};
    const RulesDecisionTextMeasure measure = [&atlas](const std::string& text,
        float pixels) {
        const auto value = atlas.measureUtf8(text, pixels, false);
        return RulesDecisionTextMetrics{value.ok, value.width, value.height};
    };
    const std::array<RulesDecisionDisplayProfile, 3> profiles{{
        {1280, 720, 1280, 720, 1.0f, 1.0f},
        {1920, 1080, 1920, 1080, 1.0f, 1.0f},
        {1280, 720, 2560, 1440, 2.0f, 1.0f},
    }};
    const std::array<const char*, 3> profileIds{{
        "1280x720_density1", "1920x1080_density1",
        "1280x720_retina_density2"}};
    const std::array<float, 3> scales{{0.85f, 1.0f, 1.35f}};
    const std::array<RulesDecisionLocale, 2> locales{{
        RulesDecisionLocale::En, RulesDecisionLocale::ZhCn}};
    const std::array<RulesDecisionPalette, 2> palettes{{
        RulesDecisionPalette::Standard,
        RulesDecisionPalette::ColorVisionSafe}};
    std::vector<MachineCase> cases;
    for (const std::string& state : rulesDecisionCanonicalStateIds()) {
        const auto fixture = rulesDecisionCanonicalFixture(state);
        for (RulesDecisionLocale locale : locales) {
            for (std::size_t profileIndex = 0;
                 profileIndex < profiles.size(); ++profileIndex) {
                for (float scale : scales) {
                    std::string standardSignature;
                    for (RulesDecisionPalette palette : palettes) {
                        auto profile = profiles[profileIndex];
                        profile.uiScale = scale;
                        CameraPresentation camera;
                        camera.eye = {0.0f, 300.0f, 260.0f};
                        camera.target = {0.0f,
                            kTableModelPlaySurfaceHeightCm, -20.0f};
                        camera.fieldOfViewDegrees = 48.0f;
                        const SharedCameraProjection projection =
                            buildSharedCameraProjection(camera,
                                profile.physicalWidth,
                                profile.physicalHeight);
                        const float halfWidth =
                            kChinesePlayfieldWidthCm * 0.5f;
                        const float halfLength =
                            kChinesePlayfieldLengthCm * 0.5f;
                        const std::array<Point3, 4> world{{
                            {-halfWidth, kTableModelPlaySurfaceHeightCm,
                                -halfLength},
                            {halfWidth, kTableModelPlaySurfaceHeightCm,
                                -halfLength},
                            {halfWidth, kTableModelPlaySurfaceHeightCm,
                                halfLength},
                            {-halfWidth, kTableModelPlaySurfaceHeightCm,
                                halfLength}}};
                        profile.projectedTableAvailable = projection.valid;
                        for (std::size_t index = 0;
                                index < world.size() &&
                                profile.projectedTableAvailable; ++index) {
                            PhysicalScreenPoint screen;
                            profile.projectedTableAvailable =
                                clipToPhysicalScreen(projection,
                                    transformWorldToClip(projection,
                                        world[index]), screen);
                            profile.projectedTableCorners[index] = {
                                static_cast<float>(screen.x /
                                    profile.density),
                                static_cast<float>(screen.y /
                                    profile.density)};
                        }
                        const auto drawList = buildRulesDecisionDrawList(fixture,
                            locale, palette, profile, measure);
                        const auto validation =
                            validateRulesDecisionDrawList(drawList);
                        std::ostringstream caseName;
                        caseName << state << "__"
                                 << (locale == RulesDecisionLocale::En ?
                                     "en" : "zh_cn") << "__"
                                 << (palette == RulesDecisionPalette::Standard ?
                                     "standard" : "color_vision_safe") << "__"
                                 << profileIds[profileIndex] << "__ui"
                                 << std::fixed << std::setprecision(2) << scale;
                        if (!validation.ok) {
                            std::ostringstream detail;
                            for (const auto& error : validation.errors)
                                detail << error << ',';
                            expect(false, caseName.str() + ":" + detail.str());
                        }
                        const std::string signature = semanticSignature(drawList);
                        if (palette == RulesDecisionPalette::Standard)
                            standardSignature = signature;
                        else
                            expect(signature == standardSignature,
                                caseName.str() +
                                ": standard/CVD semantic inventory differs");
                        cases.push_back({caseName.str(), state,
                            locale == RulesDecisionLocale::En ? "en" : "zh_cn",
                            palette == RulesDecisionPalette::Standard ?
                                "standard" : "color_vision_safe",
                            profileIds[profileIndex], scale});
                    }
                }
            }
        }
    }
    const std::size_t expectedCases = rulesDecisionCanonicalStateIds().size() *
        locales.size() * profiles.size() * scales.size() * palettes.size();
    expect(cases.size() == expectedCases,
        "exhaustive matrix contains every current canonical case");
    return cases;
}

void writeReport(const std::string& path, const std::string& fontPath,
    const SdfFontAtlas& atlas, const std::vector<MachineCase>& cases)
{
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot open report: " + path);
    output << "{\n"
           << "  \"schema\": \"phase5_rules_decision_ui_machine_evidence_v1\",\n"
           << "  \"schema_version\": 1,\n"
           << "  \"design_gate_sha256\": \"26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08\",\n"
           << "  \"status\": \"MACHINE_PASS_ONLY_NOT_GPU_NOT_HUMAN_NOT_PRODUCT_PASS\",\n"
           << "  \"product_exposed\": false,\n"
           << "  \"profile_complete\": false,\n"
           << "  \"rules_owner_signature_present\": false,\n"
           << "  \"off_table_12_3_vs_17_e_status\": \"UNRESOLVED\",\n"
           << "  \"font_backend\": {\"backend\": \"existing_freetype_sdf_atlas\", "
           << "\"font_path\": \"" << jsonEscape(fontPath) << "\", "
           << "\"glyph_rasterizations\": " << atlas.rasterizationCount()
           << ", \"actual_glyph_advance_measurement\": true},\n"
           << "  \"totals\": {\"machine_required\": 540, "
           << "\"machine_executed\": " << cases.size() << ", "
           << "\"machine_passed\": " << cases.size() << ", "
           << "\"actual_gpu_required\": 60, \"actual_gpu_completed\": 0, "
           << "\"manual_required\": 60, \"manual_completed\": 0, "
           << "\"beta_required\": 12, \"beta_completed\": 0},\n"
           << "  \"evidence_boundaries\": {\"actual_gpu_claimed\": false, "
           << "\"manual_review_claimed\": false, \"beta_claimed\": false, "
           << "\"product_pass_claimed\": false},\n"
           << "  \"per_case_checks\": [\n"
           << "    \"zero_truncated_text_runs\",\n"
           << "    \"zero_glyphs_below_profile_minimum\",\n"
           << "    \"zero_overlapping_action_or_focus_bounds\",\n"
           << "    \"zero_safe_area_violations\",\n"
           << "    \"offered_actions_exact_and_ordered\",\n"
           << "    \"zero_unoffered_or_foundation_actions\",\n"
           << "    \"no_default_selected_or_armed_action\",\n"
           << "    \"focus_count_zero_or_one\",\n"
           << "    \"standard_cvd_semantic_inventory_equal\",\n"
           << "    \"local_preview_authority_invariant\",\n"
           << "    \"blocking_paths_block_shot_input\"\n"
           << "  ],\n  \"cases\": [\n";
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto& item = cases[index];
        output << "    {\"case_id\": \"" << jsonEscape(item.id)
               << "\", \"state\": \"" << item.state
               << "\", \"locale\": \"" << item.locale
               << "\", \"palette\": \"" << item.palette
               << "\", \"viewport_density\": \""
               << item.viewportDensity << "\", \"ui_scale\": "
               << std::fixed << std::setprecision(2) << item.uiScale
               << ", \"passed\": true}";
        output << (index + 1 == cases.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "usage: rules_decision_ui_tests FONT_PATH REPORT_PATH\n";
        return 2;
    }
    testStateAndAuthorityBindings();
    testPlacementAndFailClosedBehavior();
    testPolicyBlockers();
    testRequestAdaptersAndErrorInventory();
    SdfFontAtlas atlas;
    const auto cases = runMachineMatrix(argv[1], atlas);
    if (failures != 0) {
        std::cerr << failures << " rules decision UI test(s) failed\n";
        return 1;
    }
    try {
        writeReport(argv[2], argv[1], atlas, cases);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Phase5 rules decision UI PASS: state/authority=PASS "
              << "machine=540/540 actual_gpu=0/60 manual=0/60 beta=0/12 "
              << "product_exposed=false off_table_policy=UNRESOLVED\n";
    return 0;
}
