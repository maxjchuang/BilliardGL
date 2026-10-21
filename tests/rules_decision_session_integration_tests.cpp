#include "render/modern/modern_product_session.h"
#include "deterministic_hash.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

class Product06GAuthenticator final :
    public rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const rules::RefereeAuthenticationInput& input)
        const override
    {
        return input.credentialId == "credential" &&
            input.keyVersion == "v1" &&
            input.authenticationProof == sha256Hex(
                "secret|" + input.authenticatedBytes);
    }
};

rules::RefereeAuthorityRequest product06GRequest(
    const GameRuntime& runtime, rules::RefereeAuthorityOperation operation,
    const std::string& key, const std::string& payload)
{
    rules::RefereeAuthorityRequest request;
    request.protocolId = rules::kRefereeAuthorityProtocolId;
    request.schemaVersion = 1;
    request.operation = operation;
    request.declarationKind =
        rules::kUnintentionalBreakMiscueDeclarationKind;
    request.decisionId = "product.unintentional_break_miscue.1";
    request.rackIdentity =
        runtime.unintentionalBreakMiscueDeclarationState().rackIdentity;
    request.expectedAuthorityRevision =
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityRevision;
    request.expectedAuthorityStateHash =
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityStateHash;
    request.authorityRole = rules::RefereeAuthorityRole::Referee;
    request.authoritySubjectId = "referee.06g";
    request.credentialId = "credential";
    request.keyVersion = "v1";
    request.authenticationNonce = key + ".nonce";
    request.expiresAtAuthorityTick = 1000;
    request.idempotencyKey = key;
    request.payloadCanonical = payload;
    request.authenticationProof = sha256Hex("secret|" +
        rules::canonicalRefereeAuthenticatedBytes(request));
    return request;
}

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("rules decision session: " + message);
}

bool close(double first, double second, double tolerance = 0.001)
{
    return std::fabs(first - second) <= tolerance;
}

PlatformInputEvent pointerEvent(PlatformInputKind kind, double x, double y)
{
    PlatformInputEvent event;
    event.kind = kind;
    event.control = "LeftButton";
    event.logicalX = x;
    event.logicalY = y;
    event.backingScale = 1.0;
    return event;
}

ModernProductEventResult clickRulesRect(ModernProductSession& session,
    const ui::RulesDecisionUiHitRect& hit)
{
    const double x = hit.physicalRect.x + hit.physicalRect.width * 0.5;
    const double y = hit.physicalRect.y + hit.physicalRect.height * 0.5;
    (void)session.handlePlatformInput(
        pointerEvent(PlatformInputKind::PointerButtonDown, x, y));
    return session.handlePlatformInput(
        pointerEvent(PlatformInputKind::PointerButtonUp, x, y));
}

std::optional<std::pair<double, double>> placementPointer(
    const CameraPresentation& camera, const GameRuntime& runtime, bool legal)
{
    TablePlanePointerProjectionRequest request;
    request.camera = camera;
    request.viewport = {1280, 720, 1280, 720};
    request.tablePlaneY = kTableHeight;
    for (int y = 72; y < 680; y += 12) {
        for (int x = 48; x < 1232; x += 12) {
            request.logicalX = x;
            request.logicalY = y;
            const auto projected = projectPointerToTablePlane(request);
            if (!projected.ok) continue;
            const rules::BallInHandPlacementValidation validation =
                runtime.validateBallInHandPlacement(
                    {projected.tablePoint.x, projected.tablePoint.z});
            if (validation.ok == legal)
                return std::make_pair(static_cast<double>(x),
                    static_cast<double>(y));
        }
    }
    return std::nullopt;
}

rules::ShotFacts facts(int shooter, int firstContact)
{
    rules::ShotFacts result;
    result.shooter = shooter;
    result.firstContactBall = firstContact;
    result.anyRailAfterFirstContact = true;
    return result;
}

void attestWeakBreakEvidence(rules::ShotFacts& shot)
{
    shot.breakEvidenceOrigin =
        rules::BreakEvidenceOrigin::AuthoritativeShotFactCollector;
    shot.breakEvidenceComplete = true;
    shot.breakObjectCushionContacts.clear();
    for (int index = 0; index < 3; ++index) {
        rules::BreakObjectCushionContact contact;
        contact.objectBallId = index + 1;
        contact.boundaryKind =
            rules::BreakCushionBoundaryKind::StraightCushion;
        contact.provenance =
            rules::BreakCushionContactProvenance::DiscretePhysics;
        contact.velocityImpulseApplied = true;
        contact.normalImpulseNs = 0.1;
        contact.physicsTick = 1;
        contact.canonicalContactOrdinal = index;
        contact.timeOfImpactSeconds = 0.001 * index;
        shot.breakObjectCushionContacts.push_back(contact);
    }
    shot.distinctObjectBallsToRail = 3;
}

void stageGroupChoice(GameRuntime& runtime)
{
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "rules mode stages");
    auto breakShot = facts(0, 1);
    breakShot.breakShot = true;
    breakShot.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(breakShot).ok,
        "legal break stages open table");
    auto choice = facts(1, 1);
    choice.firstContactBalls = {1, 9};
    choice.pocketedBallsInOrder = {1, 9};
    expect(runtime.adjudicateRulesShot(choice).ok &&
        runtime.groupChoiceDecisionOffer().has_value(),
        "simultaneous group authority offer stages");
}

void stageLegalEightOnBreak(GameRuntime& runtime)
{
    runtime.mutableState().balls[8].pocketed = true;
    runtime.mutableState().pocketedBallCount = 1;
    auto shot = facts(0, 1);
    shot.breakShot = true;
    shot.pocketedBallsInOrder = {8};
    expect(runtime.adjudicateRulesShot(shot).ok &&
        runtime.eightOnBreakDecisionOffer().has_value(),
        "legal 8-on-break stages the authoritative parent offer");
}

void stageFoulEightOnBreak(GameRuntime& runtime)
{
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "foul 8-on-break rules mode stages");
    runtime.mutableState().balls[8].pocketed = true;
    runtime.mutableState().pocketedBallCount = 1;
    auto shot = facts(0, 1);
    shot.breakShot = true;
    shot.pocketedBallsInOrder = {8};
    shot.cueBallPocketed = true;
    shot.cueBallScratchEvidenceOrigin =
        rules::CueBallScratchEvidenceOrigin::AuthoritativeShotFactCollector;
    shot.cueBallScratchEvidenceComplete = true;
    rules::CueBallPocketCaptureEvidence capture;
    capture.pocketId = 2;
    capture.captureSequence = 1;
    capture.physicsTick = 1;
    capture.physicsContactOrdinal = 0;
    capture.canonicalEventOrdinal = 0;
    capture.timeOfImpactSeconds = 0.01;
    shot.cueBallPocketCaptures = {capture};
    expect(runtime.adjudicateRulesShot(shot).ok &&
        runtime.eightOnBreakDecisionOffer().has_value() &&
        !runtime.matchState().ballInHand &&
        !runtime.ballInHandPlacementOffer().has_value(),
        "foul 8-on-break stages Core-F parent without early BIH");
}

void stageFullTablePlacement(GameRuntime& runtime)
{
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "full-table placement rules mode stages");
    auto breakShot = facts(0, 1);
    breakShot.breakShot = true;
    breakShot.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(breakShot).ok,
        "full-table placement legal break stages");
    auto assignment = facts(1, 1);
    assignment.pocketedBallsInOrder = {1};
    expect(runtime.adjudicateRulesShot(assignment).ok,
        "full-table placement group assignment stages");
    runtime.mutableState().balls[0].pocketed = true;
    auto foul = facts(1, 9);
    foul.anyRailAfterFirstContact = false;
    foul.cueBallOffTable = true;
    expect(runtime.adjudicateRulesShot(foul).ok &&
        runtime.ballInHandPlacementOffer().has_value() &&
        runtime.ballInHandPlacementScope() ==
            rules::BallInHandPlacementScope::FullTable,
        "ordinary foul stages full-table placement authority");
}

void stageBehindHeadStringPlacement(GameRuntime& runtime)
{
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "behind-head-string placement rules mode stages");
    runtime.mutableState().balls[0].pocketed = true;
    auto foul = facts(0, 1);
    foul.breakShot = true;
    foul.cueBallOffTable = true;
    expect(runtime.adjudicateRulesShot(foul).ok &&
        runtime.breakFoulDecisionOffer().has_value(),
        "break foul parent stages");
    const auto offer = *runtime.breakFoulDecisionOffer();
    rules::BreakFoulDecisionRequest request;
    request.rulesetId = rules::kBreakFoulRulesetId;
    request.capabilityId = rules::kBreakFoulCapabilityId;
    request.flowId = rules::kBreakFoulFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision = offer.authorityStateRevision;
    request.idempotencyKey = "product-stage-behind-parent";
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId = rules::kBreakFoulBallInHandActionId;
    request.payloadCanonical = "{}";
    expect(runtime.submitBreakFoulDecision(request).ok &&
        runtime.ballInHandPlacementOffer().has_value() &&
        runtime.ballInHandPlacementScope() ==
            rules::BallInHandPlacementScope::BehindHeadString,
        "break-foul parent commit stages scoped placement child");
}

void stageBreakShotFoulOffer(GameRuntime& runtime)
{
    runtime.mutableState().balls[0].pocketed = true;
    auto foul = facts(runtime.matchState().currentPlayer, 1);
    foul.breakShot = true;
    foul.cueBallOffTable = true;
    expect(runtime.adjudicateRulesShot(foul).ok &&
        runtime.breakFoulDecisionOffer().has_value() &&
        runtime.breakFoulDecisionOffer()->branch ==
            rules::BreakFoulBranch::BreakShotFoul,
        "break-shot foul stages frozen Core-H parent offer");
}

void stageIllegalBreakOffer(GameRuntime& runtime)
{
    auto weak = facts(runtime.matchState().currentPlayer, 1);
    weak.breakShot = true;
    attestWeakBreakEvidence(weak);
    expect(runtime.adjudicateRulesShot(weak).ok &&
        runtime.breakFoulDecisionOffer().has_value() &&
        runtime.breakFoulDecisionOffer()->branch ==
            rules::BreakFoulBranch::IllegalBreak,
        "weak authoritative break stages WPA II 6(c) offer");
}

std::filesystem::path settingsPath(const char* suffix)
{
    const auto nonce = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        (std::string("billiardgl-rules-ui-") + suffix + "-" +
            std::to_string(nonce) + ".json");
}

void cleanup(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path.string() + ".tmp", ignored);
}

void testDefaultProductSessionCannotExposeOrSubmit()
{
    const auto path = settingsPath("disabled");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    ModernProductSession session(runtime, ai, options);
    stageGroupChoice(runtime);
    session.syncRulesDecisionAuthority();
    const ModernProductSessionSnapshot snapshot = session.snapshot();
    expect(!snapshot.rulesDecisionUiImplementationEnabled &&
        !snapshot.rulesDecisionUiProductExposed &&
        snapshot.rulesDecision.state ==
            rules::ui::DecisionUiState::HiddenNoOffer,
        "default session keeps implementation and product exposure disabled");
    const auto rejected = session.armRulesDecisionAction(
        rules::kGroupChoiceSolidsActionId);
    const auto rejected06f =
        session.submitProductAuthorizedEightOnBreakChoice(
            rules::kEightOnBreakSpotContinueActionId, "disabled-06f");
    expect(!rejected.ok && rejected.errorCode ==
            "rules_decision_ui_capability_disabled" &&
        !rejected06f.ok && rejected06f.errorCode ==
            "rules_product_eight_on_break_capability_disabled" &&
        runtime.groupChoiceDecisionOffer().has_value() &&
        runtime.rulesAuthorityActions().empty(),
        "default session cannot arm or mutate authority");
    cleanup(path);

}

void testInternalCapabilityUsesExactRuntimeTransaction()
{
    const auto path = settingsPath("enabled");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.internalCapabilities.insert(
        kRulesDecisionUiImplementationCapability);
    // The simultaneous choice actor in this fixture is player 1.
    options.rulesDecisionLocalPlayerId = 1;
    ModernProductSession session(runtime, ai, options);
    stageGroupChoice(runtime);
    const std::uint64_t revisionBefore = runtime.rulesAuthorityRevision();
    session.syncRulesDecisionAuthority();
    ModernProductSessionSnapshot snapshot = session.snapshot();
    expect(snapshot.rulesDecisionUiImplementationEnabled &&
        !snapshot.rulesDecisionUiProductExposed &&
        snapshot.rulesDecision.state ==
            rules::ui::DecisionUiState::DecisionOpenUnselected &&
        snapshot.rulesDecision.visibleActionIds ==
            std::vector<std::string>{rules::kGroupChoiceSolidsActionId,
                rules::kGroupChoiceStripesActionId},
        "internal capability consumes exact stable offer without a default");
    expect(!session.armRulesDecisionAction("rules.foundation.rerack").ok &&
        runtime.rulesAuthorityRevision() == revisionBefore,
        "foundation action is rejected before runtime mutation");
    expect(session.armRulesDecisionAction(
            rules::kGroupChoiceStripesActionId).ok &&
        session.snapshot().rulesDecision.state ==
            rules::ui::DecisionUiState::ActionArmedNotSubmitted &&
        runtime.rulesAuthorityRevision() == revisionBefore,
        "first activation arms only");
    expect(session.confirmRulesDecisionAction("session-exact-key-1").ok,
        "distinct Confirm submits through stable runtime adapter");
    snapshot = session.snapshot();
    expect(snapshot.rulesDecision.state ==
            rules::ui::DecisionUiState::CompletedAuthoritative &&
        !snapshot.rulesDecisionUiProductExposed &&
        runtime.rulesAuthorityRevision() == revisionBefore + 1 &&
        !runtime.groupChoiceDecisionOffer().has_value() &&
        runtime.rulesAuthorityActions().size() == 1 &&
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kGroupChoiceStripesActionId &&
        runtime.rulesAuthorityActions().back().idempotencyKey ==
            "session-exact-key-1",
        "accepted exact ACK is the only commit and records exact identity");
    session.syncRulesDecisionAuthority();
    expect(session.snapshot().rulesDecision.state ==
            rules::ui::DecisionUiState::HiddenNoOffer,
        "next authority sync removes completed internal surface");
    cleanup(path);

}

void testProductAuthorizedEightOnBreakRespotParentSeam()
{
    const auto path = settingsPath("product-respot-parent");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
    ModernProductSession session(runtime, ai, options);

    const auto outsideMatch =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kEightOnBreakSpotContinueActionId, "outside-match");
    expect(!outsideMatch.ok && outsideMatch.handled &&
        outsideMatch.errorCode == "rules_product_respot_not_in_match" &&
        runtime.rulesAuthorityActions().empty(),
        "narrow product respot parent seam rejects outside Match");

    expect(session.shell().startSinglePlayer(17).ok,
        "product match begins through the official rack lifecycle");
    stageLegalEightOnBreak(runtime);
    const auto offer = *runtime.eightOnBreakDecisionOffer();
    const std::uint64_t revision = runtime.rulesAuthorityRevision();
    const std::size_t actions = runtime.rulesAuthorityActions().size();

    const auto foundation =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kFoundationRespotActionId, "foundation-forbidden");
    const auto coreH =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kBreakFoulRerackOriginalActionId,
            "core-h-forbidden");
    expect(!foundation.ok && !coreH.ok &&
        foundation.errorCode ==
            "rules_product_respot_action_not_authorized" &&
        coreH.errorCode ==
            "rules_product_respot_action_not_authorized" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions &&
        runtime.eightOnBreakDecisionOffer().has_value(),
        "foundation and Core-H sibling IDs cannot reach the narrow Core-F product seam");

    const auto malformed =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kEightOnBreakSpotContinueActionId, "");
    expect(!malformed.ok && malformed.errorCode ==
            "rules_decision_idempotency_key_invalid" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions,
        "bad parent idempotency input fails closed before mutation");

    const auto committed =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kEightOnBreakSpotContinueActionId,
            "product-respot-parent-1");
    const auto snapshot = session.snapshot();
    const double expectedZ =
        runtime.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    expect(committed.ok && committed.handled &&
        !runtime.eightOnBreakDecisionOffer().has_value() &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        runtime.rulesAuthorityActions().size() == actions + 1,
        "product respot parent commits exactly one authority revision/action");
    expect(
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kEightOnBreakSpotContinueActionId &&
        runtime.rulesAuthorityActions().back().decisionId == offer.decisionId &&
        runtime.rulesAuthorityActions().back().actorPlayerId ==
            offer.actorPlayerId &&
        runtime.rulesAuthorityActions().back().payloadCanonical == "{}" &&
        runtime.rulesAuthorityActions().back().idempotencyKey ==
            "product-respot-parent-1",
        "product respot parent records exact offer/action/actor/idempotency identity");
    expect(
        !runtime.state().balls[8].pocketed &&
        std::fabs(runtime.state().balls[8].position.x) < 0.000001 &&
        runtime.state().balls[8].position.z >= expectedZ &&
        !snapshot.rulesDecisionUiImplementationEnabled &&
        !snapshot.rulesDecisionUiProductExposed &&
        snapshot.rulesDecision.state ==
            rules::ui::DecisionUiState::HiddenNoOffer,
        "product seam respots on the official longitudinal search and keeps Rules UI hidden");

    const std::uint64_t committedRevision = runtime.rulesAuthorityRevision();
    const std::size_t committedActions = runtime.rulesAuthorityActions().size();
    const auto retry = session.submitProductAuthorizedEightOnBreakRespot(
        rules::kEightOnBreakSpotContinueActionId,
        "product-respot-parent-1");
    const auto conflict = session.submitProductAuthorizedEightOnBreakRespot(
        rules::kEightOnBreakRerackOriginalActionId,
        "product-respot-parent-1");
    const auto payloadConflict =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kEightOnBreakSpotContinueActionId,
            "product-respot-parent-1", "{\"branch\":\"foul\"}");
    expect(retry.ok && !conflict.ok && conflict.errorCode ==
            "rules_decision_idempotency_conflict" &&
        !payloadConflict.ok && payloadConflict.errorCode ==
            "rules_decision_invalid_payload" &&
        runtime.rulesAuthorityRevision() == committedRevision &&
        runtime.rulesAuthorityActions().size() == committedActions &&
        !runtime.matchState().ballInHand &&
        !runtime.ballInHandPlacementOffer().has_value(),
        "legal spot exact retry is idempotent and action/payload conflicts have zero delta");
    cleanup(path);
}

void testProductAuthorizedRespotRejectsWrongLocalActor()
{
    const auto path = settingsPath("product-respot-actor");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionLocalPlayerId = 1;
    options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
    ModernProductSession session(runtime, ai, options);
    expect(session.shell().startSinglePlayer(23).ok,
        "actor fixture starts product match");
    stageLegalEightOnBreak(runtime);
    ai::PlayerConfiguration remoteActor;
    remoteActor.enabled = true;
    remoteActor.player = runtime.eightOnBreakDecisionOffer()->actorPlayerId;
    remoteActor.deterministicSeed = 23;
    expect(ai.configure(runtime, remoteActor).ok,
        "actor fixture assigns the frozen decision actor to AI");
    const std::uint64_t revision = runtime.rulesAuthorityRevision();
    const std::size_t actions = runtime.rulesAuthorityActions().size();
    const auto rejected =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kEightOnBreakSpotContinueActionId,
            "wrong-local-actor");
    expect(!rejected.ok && rejected.errorCode ==
            "rules_decision_actor_mismatch" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions &&
        runtime.eightOnBreakDecisionOffer().has_value() &&
        !session.snapshot().rulesDecisionUiProductExposed,
        "parent respot requires exact current offer actor to equal the local product actor");
    cleanup(path);
}

void testProductAuthorizedEightOnBreakExactRerackActions()
{
    {
        const auto path = settingsPath("product-eight-legal-rerack");
        GameRuntime runtime;
        ai::AiPlayerController ai;
        ModernProductSessionOptions options;
        options.settingsPath = path.string();
        options.rulesDecisionLocalPlayerId = 0;
        options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
        ModernProductSession session(runtime, ai, options);
        expect(session.shell().startSinglePlayer(24).ok,
            "legal rerack fixture starts Match");
        stageLegalEightOnBreak(runtime);
        const auto offer = *runtime.eightOnBreakDecisionOffer();
        expect(offer.branch == rules::EightOnBreakBranch::LegalBreak &&
            offer.actorPlayerId == offer.originalBreakerPlayerId &&
            offer.offeredActionIds == std::vector<std::string>{
                rules::kEightOnBreakSpotContinueActionId,
                rules::kEightOnBreakRerackOriginalActionId},
            "legal Core-F offer freezes original-breaker actor and exact two actions");
        const auto rejectedFoulSpot =
            session.submitProductAuthorizedEightOnBreakRespot(
                rules::kEightOnBreakSpotBallInHandActionId,
                "legal-foul-action");
        const auto committed =
            session.submitProductAuthorizedEightOnBreakRespot(
                rules::kEightOnBreakRerackOriginalActionId,
                "legal-rerack-original");
        expect(!rejectedFoulSpot.ok && rejectedFoulSpot.errorCode ==
                "rules_decision_action_not_offered" &&
            committed.ok && runtime.matchState().currentPlayer == 0 &&
            runtime.state().players.currentPlayer == 0 &&
            runtime.matchState().phase == rules::MatchPhase::Break &&
            !runtime.matchState().ballInHand &&
            runtime.ballInHandPlacementScope() ==
                rules::BallInHandPlacementScope::None &&
            !runtime.ballInHandPlacementOffer().has_value() &&
            runtime.rulesAuthorityActions().back().actionId ==
                rules::kEightOnBreakRerackOriginalActionId,
            "legal rerack selects the frozen original breaker and never grants BIH");
        cleanup(path);
    }

    const std::vector<std::pair<std::string, int>> foulReracks{
        {rules::kEightOnBreakRerackOpponentActionId, 1},
        {rules::kEightOnBreakRerackOriginalActionId, 0}};
    for (std::size_t index = 0; index < foulReracks.size(); ++index) {
        const std::string suffix =
            "product-eight-foul-rerack-" + std::to_string(index);
        const auto path = settingsPath(suffix.c_str());
        GameRuntime runtime;
        ai::AiPlayerController ai;
        ModernProductSessionOptions options;
        options.settingsPath = path.string();
        options.rulesDecisionLocalPlayerId = 1;
        options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
        ModernProductSession session(runtime, ai, options);
        expect(session.shell().startSinglePlayer(25 + index).ok,
            "foul rerack fixture starts Match");
        ai::PlayerConfiguration localTwoPlayer;
        expect(ai.configure(runtime, localTwoPlayer).ok,
            "foul rerack component fixture makes the opposing actor local");
        stageFoulEightOnBreak(runtime);
        const auto offer = *runtime.eightOnBreakDecisionOffer();
        expect(offer.branch == rules::EightOnBreakBranch::FoulBreak &&
            offer.actorPlayerId == 1 - offer.originalBreakerPlayerId &&
            offer.offeredActionIds == std::vector<std::string>{
                rules::kEightOnBreakSpotBallInHandActionId,
                rules::kEightOnBreakRerackOpponentActionId,
                rules::kEightOnBreakRerackOriginalActionId} &&
            !runtime.matchState().ballInHand,
            "foul Core-F offer freezes opposing actor/exact three actions without BIH");
        const auto committed =
            session.submitProductAuthorizedEightOnBreakRespot(
                foulReracks[index].first,
                "foul-rerack-" + std::to_string(index));
        expect(committed.ok &&
            runtime.matchState().currentPlayer == foulReracks[index].second &&
            runtime.state().players.currentPlayer == foulReracks[index].second &&
            runtime.matchState().phase == rules::MatchPhase::Break &&
            !runtime.matchState().ballInHand &&
            runtime.ballInHandPlacementScope() ==
                rules::BallInHandPlacementScope::None &&
            !runtime.ballInHandPlacementOffer().has_value() &&
            runtime.rulesAuthorityActions().back().actionId ==
                foulReracks[index].first,
            "each foul rerack selects the official breaker and cannot grant or retain BIH: index=" +
                std::to_string(index) + " ok=" +
                std::to_string(committed.ok) + " error=" +
                committed.errorCode + " player=" +
                std::to_string(runtime.matchState().currentPlayer) +
                " expected=" + std::to_string(foulReracks[index].second) +
                " phase=" + std::to_string(static_cast<int>(
                    runtime.matchState().phase)) + " bih=" +
                std::to_string(runtime.matchState().ballInHand));
        cleanup(path);
    }
}

void testProductAuthorizedEightOnBreakFoulSpotParentChildAtomicity()
{
    const auto path = settingsPath("product-eight-foul-spot");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionLocalPlayerId = 1;
    options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
    ModernProductSession session(runtime, ai, options);
    expect(session.shell().startSinglePlayer(27).ok,
        "foul spot fixture starts Match");
    ai::PlayerConfiguration localTwoPlayer;
    expect(ai.configure(runtime, localTwoPlayer).ok,
        "foul spot component fixture makes the opposing actor local");
    stageFoulEightOnBreak(runtime);
    const auto offer = *runtime.eightOnBreakDecisionOffer();
    const auto beforeRevision = runtime.rulesAuthorityRevision();
    const auto wrongLegalAction =
        session.submitProductAuthorizedEightOnBreakRespot(
            rules::kEightOnBreakSpotContinueActionId,
            "foul-legal-action");
    expect(!wrongLegalAction.ok && wrongLegalAction.errorCode ==
            "rules_decision_action_not_offered" &&
        runtime.rulesAuthorityRevision() == beforeRevision &&
        !runtime.matchState().ballInHand &&
        !runtime.ballInHandPlacementOffer().has_value(),
        "legal-only spot action cannot enter the foul branch or grant early BIH");

    const auto committed = session.submitProductAuthorizedEightOnBreakRespot(
        rules::kEightOnBreakSpotBallInHandActionId, "foul-spot-bih");
    const auto child = runtime.ballInHandPlacementOffer();
    expect(committed.ok && runtime.rulesAuthorityRevision() ==
            beforeRevision + 1 && !runtime.eightOnBreakDecisionOffer() &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementScope() ==
            rules::BallInHandPlacementScope::BehindHeadString &&
        child.has_value() && child->actorPlayerId == offer.actorPlayerId &&
        child->offeredActionIds[0] ==
            rules::kConfirmBehindHeadStringPlacementActionId &&
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kEightOnBreakSpotBallInHandActionId &&
        !session.snapshot().rulesDecisionUiProductExposed,
        "foul spot parent commit atomically grants BehindHeadString BIH and one Core-G child");
    PlatformInputEvent postAckInput;
    postAckInput.kind = PlatformInputKind::KeyDown;
    postAckInput.control = "Tab";
    const auto aiGated = session.handlePlatformInput(postAckInput);
    expect(aiGated.ok && aiGated.errorCode.empty(),
        "local-two-player foul-spot ACK refreshes input without inventing an AI gate");
    const auto parentRevision = runtime.rulesAuthorityRevision();
    const auto parentActions = runtime.rulesAuthorityActions().size();
    const rules::BallInHandPlacementPosition invalid{
        runtime.state().balls[1].position.x,
        runtime.state().balls[1].position.z};
    const auto rejectedChild =
        session.previewProductAuthorizedBallInHandPlacement(invalid);
    expect(!rejectedChild.ok && runtime.rulesAuthorityRevision() ==
            parentRevision &&
        runtime.rulesAuthorityActions().size() == parentActions &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementOffer().has_value(),
        "rejected child preview preserves the committed parent entitlement atomically");
    cleanup(path);

    const auto failurePath = settingsPath("product-eight-foul-spot-failure");
    GameRuntime failureRuntime;
    ai::AiPlayerController failureAi;
    ModernProductSessionOptions failureOptions;
    failureOptions.settingsPath = failurePath.string();
    failureOptions.rulesDecisionLocalPlayerId = 1;
    failureOptions.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
    ModernProductSession failureSession(
        failureRuntime, failureAi, failureOptions);
    expect(failureSession.shell().startSinglePlayer(31).ok,
        "foul spot failure fixture starts Match");
    ai::PlayerConfiguration failureLocalTwoPlayer;
    expect(failureAi.configure(failureRuntime, failureLocalTwoPlayer).ok,
        "failure fixture makes the opposing actor local");
    failureRuntime.mutableState().balls[1].position.x =
        std::numeric_limits<float>::quiet_NaN();
    stageFoulEightOnBreak(failureRuntime);
    const auto failureRevision = failureRuntime.rulesAuthorityRevision();
    const auto failureActions = failureRuntime.rulesAuthorityActions().size();
    const auto failed =
        failureSession.submitProductAuthorizedEightOnBreakRespot(
            rules::kEightOnBreakSpotBallInHandActionId,
            "foul-spot-no-solution");
    expect(!failed.ok && failed.errorCode ==
            "rules_respot_no_legal_position" &&
        failureRuntime.rulesAuthorityRevision() == failureRevision &&
        failureRuntime.rulesAuthorityActions().size() == failureActions &&
        failureRuntime.eightOnBreakDecisionOffer().has_value() &&
        !failureRuntime.matchState().ballInHand &&
        failureRuntime.ballInHandPlacementScope() ==
            rules::BallInHandPlacementScope::None &&
        !failureRuntime.ballInHandPlacementOffer().has_value(),
        "failed foul spot foundation rolls back parent, BIH, child, revision and action state");
    cleanup(failurePath);
}

void testProductAuthorizedEightOnBreakDriftResetAndSiblingFailures()
{
    const auto driftPath = settingsPath("product-eight-drift");
    GameRuntime driftRuntime;
    ai::AiPlayerController driftAi;
    ModernProductSessionOptions driftOptions;
    driftOptions.settingsPath = driftPath.string();
    driftOptions.rulesDecisionLocalPlayerId = 0;
    driftOptions.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
    ModernProductSession driftSession(driftRuntime, driftAi, driftOptions);
    expect(driftSession.shell().startSinglePlayer(28).ok,
        "eight-on-break drift fixture starts Match");
    stageLegalEightOnBreak(driftRuntime);
    const auto revision = driftRuntime.rulesAuthorityRevision();
    const auto actions = driftRuntime.rulesAuthorityActions().size();
    const auto coreH = driftSession.submitProductAuthorizedEightOnBreakRespot(
        rules::kBreakFoulBallInHandActionId, "core-h-sibling");
    const auto foundation = driftSession.submitProductAuthorizedEightOnBreakRespot(
        rules::kFoundationRerackActionId, "core-e-foundation");
    driftRuntime.mutableState().balls[2].position.x += 0.5f;
    const auto drift = driftSession.submitProductAuthorizedEightOnBreakRespot(
        rules::kEightOnBreakRerackOriginalActionId, "eight-state-drift");
    expect(!coreH.ok && !foundation.ok && !drift.ok &&
        coreH.errorCode == "rules_product_respot_action_not_authorized" &&
        foundation.errorCode == "rules_product_respot_action_not_authorized" &&
        drift.errorCode == "rules_eight_on_break_state_changed" &&
        driftRuntime.rulesAuthorityRevision() == revision &&
        driftRuntime.rulesAuthorityActions().size() == actions &&
        driftRuntime.eightOnBreakDecisionOffer().has_value() &&
        !driftRuntime.matchState().ballInHand,
        "Core-H/Core-E and hash drift fail before Core-F mutation");
    cleanup(driftPath);

    const auto resetPath = settingsPath("product-eight-reset");
    GameRuntime resetRuntime;
    ai::AiPlayerController resetAi;
    ModernProductSessionOptions resetOptions;
    resetOptions.settingsPath = resetPath.string();
    resetOptions.rulesDecisionLocalPlayerId = 0;
    resetOptions.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
    ModernProductSession resetSession(resetRuntime, resetAi, resetOptions);
    expect(resetSession.shell().startSinglePlayer(29).ok,
        "eight-on-break reset fixture starts Match");
    stageLegalEightOnBreak(resetRuntime);
    expect(resetSession.submitProductAuthorizedEightOnBreakRespot(
        rules::kEightOnBreakRerackOriginalActionId,
        "eight-before-reset").ok,
        "reset fixture retains one verified Core-F request");
    resetRuntime.reset();
    const auto resetRevision = resetRuntime.rulesAuthorityRevision();
    const auto stale = resetSession.submitProductAuthorizedEightOnBreakRespot(
        rules::kEightOnBreakRerackOriginalActionId,
        "eight-before-reset");
    expect(!stale.ok && stale.errorCode == "rules_decision_stale" &&
        resetRuntime.rulesAuthorityRevision() == resetRevision &&
        !resetRuntime.matchState().ballInHand &&
        !resetRuntime.ballInHandPlacementOffer().has_value(),
        "reset invalidates retained Core-F retry without residual BIH/rerack effects");
    cleanup(resetPath);
}

void testProductAuthorizedGroupChoiceParentSeam()
{
    const auto path = settingsPath("product-group-choice-parent");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionLocalPlayerId = 1;
    ModernProductSession session(runtime, ai, options);

    const auto outsideMatch = session.submitProductAuthorizedGroupChoice(
        rules::kGroupChoiceSolidsActionId, "outside-match");
    expect(!outsideMatch.ok && outsideMatch.handled &&
        outsideMatch.errorCode ==
            "rules_product_group_choice_not_in_match" &&
        runtime.rulesAuthorityActions().empty(),
        "narrow group-choice seam rejects outside Match");

    expect(session.shell().startSinglePlayer(29).ok,
        "group-choice product fixture starts Match");
    // ProductShell currently exposes only Player-vs-AI match setup. This
    // component fixture disables that AI after the authoritative rack commit
    // to model a local two-player match; it is not claiming a shipping menu
    // path. The separate actor rejection fixture below keeps the real
    // single-player AI boundary locked.
    ai::PlayerConfiguration localTwoPlayer;
    localTwoPlayer.enabled = false;
    expect(ai.configure(runtime, localTwoPlayer).ok,
        "group-choice success fixture makes both authority actors local");
    stageGroupChoice(runtime);
    const auto offer = *runtime.groupChoiceDecisionOffer();
    const std::uint64_t revision = runtime.rulesAuthorityRevision();
    const std::size_t actions = runtime.rulesAuthorityActions().size();
    const auto foundation = session.submitProductAuthorizedGroupChoice(
        rules::kFoundationRespotActionId, "foundation-forbidden");
    expect(!foundation.ok && foundation.errorCode ==
            "rules_product_group_choice_action_not_authorized" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions &&
        runtime.groupChoiceDecisionOffer().has_value(),
        "foundation and non-group actions cannot reach the product seam");

    const auto malformed = session.submitProductAuthorizedGroupChoice(
        rules::kGroupChoiceStripesActionId, "");
    expect(!malformed.ok && malformed.errorCode ==
            "rules_decision_idempotency_key_invalid" &&
        runtime.rulesAuthorityRevision() == revision,
        "bad group-choice idempotency input fails without mutation");

    const auto committed = session.submitProductAuthorizedGroupChoice(
        rules::kGroupChoiceStripesActionId, "product-group-choice-1");
    expect(committed.ok && committed.handled &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        runtime.rulesAuthorityActions().size() == actions + 1 &&
        !runtime.groupChoiceDecisionOffer().has_value() &&
        runtime.matchState().groups[offer.actorPlayerId] ==
            rules::BallGroup::Stripes &&
        runtime.rulesAuthorityActions().back().decisionId ==
            offer.decisionId &&
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kGroupChoiceStripesActionId &&
        runtime.rulesAuthorityActions().back().actorPlayerId ==
            offer.actorPlayerId &&
        runtime.rulesAuthorityActions().back().idempotencyKey ==
            "product-group-choice-1",
        "offered product group choice commits one exact parent transaction");
    const auto afterCommit = session.snapshot();
    expect(!afterCommit.rulesDecisionUiImplementationEnabled &&
        !afterCommit.rulesDecisionUiProductExposed &&
        afterCommit.rulesDecision.state ==
            rules::ui::DecisionUiState::HiddenNoOffer,
        "narrow seam keeps the global Rules Decision UI hidden");

    const std::uint64_t committedRevision = runtime.rulesAuthorityRevision();
    const std::size_t committedActions = runtime.rulesAuthorityActions().size();
    const auto duplicate = session.submitProductAuthorizedGroupChoice(
        rules::kGroupChoiceStripesActionId, "product-group-choice-1");
    const auto conflict = session.submitProductAuthorizedGroupChoice(
        rules::kGroupChoiceSolidsActionId, "product-group-choice-1");
    expect(duplicate.ok && !conflict.ok && conflict.errorCode ==
            "rules_decision_idempotency_conflict" &&
        runtime.rulesAuthorityRevision() == committedRevision &&
        runtime.rulesAuthorityActions().size() == committedActions,
        "exact product retry returns the original ACK and conflicting retry is mutation-free");
    cleanup(path);
}

void testProductAuthorizedGroupChoiceRejectsActorAndStateDrift()
{
    const auto actorPath = settingsPath("product-group-choice-actor");
    GameRuntime actorRuntime;
    ai::AiPlayerController actorAi;
    ModernProductSessionOptions actorOptions;
    actorOptions.settingsPath = actorPath.string();
    actorOptions.rulesDecisionLocalPlayerId = 0;
    ModernProductSession actorSession(actorRuntime, actorAi, actorOptions);
    expect(actorSession.shell().startSinglePlayer(31).ok,
        "wrong-actor fixture starts Match");
    stageGroupChoice(actorRuntime);
    const std::uint64_t actorRevision = actorRuntime.rulesAuthorityRevision();
    const auto wrongActor = actorSession.submitProductAuthorizedGroupChoice(
        rules::kGroupChoiceSolidsActionId, "wrong-group-actor");
    expect(!wrongActor.ok && wrongActor.errorCode ==
            "rules_decision_actor_mismatch" &&
        actorRuntime.rulesAuthorityRevision() == actorRevision &&
        actorRuntime.groupChoiceDecisionOffer().has_value(),
        "product group choice requires the exact local offered actor");
    cleanup(actorPath);

    const auto driftPath = settingsPath("product-group-choice-drift");
    GameRuntime driftRuntime;
    ai::AiPlayerController driftAi;
    ModernProductSessionOptions driftOptions;
    driftOptions.settingsPath = driftPath.string();
    driftOptions.rulesDecisionLocalPlayerId = 1;
    ModernProductSession driftSession(driftRuntime, driftAi, driftOptions);
    expect(driftSession.shell().startSinglePlayer(37).ok,
        "state-drift fixture starts Match");
    ai::PlayerConfiguration localDriftPlayers;
    localDriftPlayers.enabled = false;
    expect(driftAi.configure(driftRuntime, localDriftPlayers).ok,
        "state-drift fixture makes the offered actor local before hash mutation");
    stageGroupChoice(driftRuntime);
    driftRuntime.mutableState().balls[2].position.x += 0.5f;
    const std::uint64_t driftRevision = driftRuntime.rulesAuthorityRevision();
    const std::size_t driftActions = driftRuntime.rulesAuthorityActions().size();
    const auto drift = driftSession.submitProductAuthorizedGroupChoice(
        rules::kGroupChoiceSolidsActionId, "group-state-drift");
    expect(!drift.ok && drift.errorCode ==
            "rules_group_choice_state_changed" &&
        driftRuntime.rulesAuthorityRevision() == driftRevision &&
        driftRuntime.rulesAuthorityActions().size() == driftActions &&
        driftRuntime.groupChoiceDecisionOffer().has_value(),
        "frozen authority hash drift rejects without decision mutation");
    cleanup(driftPath);
}

void testProductAuthorizedFullTablePlacementPreviewAndConfirm()
{
    const auto path = settingsPath("product-placement-full-table");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionLocalPlayerId = 0;
    ModernProductSession session(runtime, ai, options);
    const rules::BallInHandPlacementPosition legal{-20.0, -30.0};

    const auto outside = session.previewProductAuthorizedBallInHandPlacement(
        legal);
    expect(!outside.ok && outside.handled &&
        outside.errorCode == "rules_product_placement_not_in_match" &&
        runtime.rulesAuthorityActions().empty(),
        "narrow placement preview rejects outside Match");

    expect(session.shell().startSinglePlayer(41).ok,
        "full-table product placement fixture starts Match");
    stageFullTablePlacement(runtime);
    const auto offer = *runtime.ballInHandPlacementOffer();
    const std::uint64_t revision = runtime.rulesAuthorityRevision();
    const std::size_t actions = runtime.rulesAuthorityActions().size();
    const auto cueBefore = runtime.state().balls[0];

    const auto noPreview =
        session.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmFullTablePlacementActionId, legal,
            "placement-no-preview");
    expect(!noPreview.ok && noPreview.errorCode ==
            "rules_product_placement_preview_not_current" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions,
        "confirm requires a current legal product preview");

    const rules::BallInHandPlacementPosition overlap{
        runtime.state().balls[1].position.x,
        runtime.state().balls[1].position.z};
    const auto illegal = session.previewProductAuthorizedBallInHandPlacement(
        overlap);
    expect(!illegal.ok && illegal.errorCode == "rules_placement_overlap" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions &&
        runtime.state().balls[0].pocketed == cueBefore.pocketed &&
        runtime.state().balls[0].position.x == cueBefore.position.x &&
        runtime.state().balls[0].position.z == cueBefore.position.z,
        "illegal product preview is authority/read-model only");

    const auto preview = session.previewProductAuthorizedBallInHandPlacement(
        legal);
    expect(preview.ok && preview.handled &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions &&
        runtime.ballInHandPlacementOffer()->decisionId == offer.decisionId &&
        runtime.state().balls[0].pocketed == cueBefore.pocketed,
        "legal product preview stores intent without authority mutation");

    const auto wrongScope =
        session.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmBehindHeadStringPlacementActionId, legal,
            "placement-wrong-scope");
    expect(!wrongScope.ok && wrongScope.errorCode ==
            "rules_product_placement_action_not_authorized" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions,
        "full-table offer rejects the behind-head-string action");

    const auto malformed =
        session.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmFullTablePlacementActionId, legal, "");
    expect(!malformed.ok && malformed.errorCode ==
            "rules_decision_idempotency_key_invalid" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions &&
        runtime.ballInHandPlacementOffer().has_value(),
        "placement confirm preserves its legal preview when idempotency input fails");

    const auto committed =
        session.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmFullTablePlacementActionId, legal,
            "product-placement-full-1");
    expect(committed.ok && committed.handled &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        runtime.rulesAuthorityActions().size() == actions + 1 &&
        runtime.ballInHandPlacementOffer() &&
        runtime.ballInHandPlacementOffer()->decisionId != offer.decisionId &&
        runtime.matchState().ballInHand &&
        std::fabs(runtime.state().balls[0].position.x + 20.0f) < 0.00001f &&
        std::fabs(runtime.state().balls[0].position.z + 30.0f) < 0.00001f &&
        runtime.rulesAuthorityActions().back().decisionId == offer.decisionId &&
        runtime.rulesAuthorityActions().back().actorPlayerId ==
            offer.actorPlayerId &&
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kConfirmFullTablePlacementActionId &&
        runtime.rulesAuthorityActions().back().idempotencyKey ==
            "product-placement-full-1" &&
        runtime.rulesAuthorityActions().back().payloadCanonical ==
            "x=-20;z=-30",
        "full-table product confirm commits exactly one current offer");
    expect(!session.snapshot().rulesDecisionUiImplementationEnabled &&
        !session.snapshot().rulesDecisionUiProductExposed,
        "narrow placement seam leaves global Rules UI hidden");

    const std::uint64_t committedRevision = runtime.rulesAuthorityRevision();
    const std::size_t committedActions = runtime.rulesAuthorityActions().size();
    const auto duplicate =
        session.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmFullTablePlacementActionId, legal,
            "product-placement-full-1");
    auto changed = legal;
    changed.worldX += 1.0;
    const auto conflict =
        session.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmFullTablePlacementActionId, changed,
            "product-placement-full-1");
    expect(duplicate.ok && !conflict.ok && conflict.errorCode ==
            "rules_decision_idempotency_conflict" &&
        runtime.rulesAuthorityRevision() == committedRevision &&
        runtime.rulesAuthorityActions().size() == committedActions,
        "exact placement retry returns original ACK and coordinate conflict is mutation-free");
    const auto replacementOffer = *runtime.ballInHandPlacementOffer();
    const rules::BallInHandPlacementPosition secondLegal{-10.0, -35.0};
    const auto secondPreview =
        session.previewProductAuthorizedBallInHandPlacement(secondLegal);
    const auto secondCommit =
        session.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmFullTablePlacementActionId, secondLegal,
            "product-placement-full-2");
    expect(secondPreview.ok && secondCommit.ok &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementOffer() &&
        runtime.ballInHandPlacementOffer()->decisionId !=
            replacementOffer.decisionId &&
        std::fabs(runtime.state().balls[0].position.x + 10.0f) < 0.00001f,
        "product refresh reaches a second preview and coordinate commit on the replacement offer");
    cleanup(path);
}

void testProductAuthorizedPlacementRejectsActorStateAndScopeDrift()
{
    const auto actorPath = settingsPath("product-placement-actor");
    GameRuntime actorRuntime;
    ai::AiPlayerController actorAi;
    ModernProductSessionOptions actorOptions;
    actorOptions.settingsPath = actorPath.string();
    actorOptions.rulesDecisionLocalPlayerId = 0;
    ModernProductSession actorSession(actorRuntime, actorAi, actorOptions);
    expect(actorSession.shell().startSinglePlayer(43).ok,
        "behind-head-string actor fixture starts Match");
    stageBehindHeadStringPlacement(actorRuntime);
    const std::uint64_t actorRevision = actorRuntime.rulesAuthorityRevision();
    const auto wrongActor =
        actorSession.previewProductAuthorizedBallInHandPlacement(
            {-20.0, -90.0});
    expect(!wrongActor.ok && wrongActor.errorCode ==
            "rules_decision_actor_mismatch" &&
        actorRuntime.rulesAuthorityRevision() == actorRevision &&
        actorRuntime.ballInHandPlacementOffer().has_value(),
        "placement preview requires exact local offered actor");
    cleanup(actorPath);

    const auto behindPath = settingsPath("product-placement-behind");
    GameRuntime behindRuntime;
    ai::AiPlayerController behindAi;
    ModernProductSessionOptions behindOptions;
    behindOptions.settingsPath = behindPath.string();
    behindOptions.rulesDecisionLocalPlayerId = 1;
    ModernProductSession behindSession(behindRuntime, behindAi, behindOptions);
    expect(behindSession.shell().startSinglePlayer(47).ok,
        "behind-head-string fixture starts Match");
    ai::PlayerConfiguration localBehindPlayers;
    localBehindPlayers.enabled = false;
    expect(behindAi.configure(behindRuntime, localBehindPlayers).ok,
        "behind-head-string drift fixture makes both actors local");
    stageBehindHeadStringPlacement(behindRuntime);
    const auto behindOffer = *behindRuntime.ballInHandPlacementOffer();
    const std::uint64_t behindRevision = behindRuntime.rulesAuthorityRevision();
    const std::size_t behindActions =
        behindRuntime.rulesAuthorityActions().size();
    const auto wrongHalf =
        behindSession.previewProductAuthorizedBallInHandPlacement(
            {0.0, -30.0});
    expect(!wrongHalf.ok && wrongHalf.errorCode ==
            "rules_placement_outside_head_string_area" &&
        behindRuntime.rulesAuthorityRevision() == behindRevision,
        "behind-head-string preview rejects the wrong half without mutation");
    const rules::BallInHandPlacementPosition legal{-20.0, -90.0};
    expect(behindSession.previewProductAuthorizedBallInHandPlacement(legal).ok,
        "behind-head-string legal preview succeeds");
    behindRuntime.mutableState().balls[2].position.x += 0.5f;
    const auto drift =
        behindSession.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmBehindHeadStringPlacementActionId, legal,
            "placement-state-drift");
    expect(!drift.ok && drift.errorCode ==
            "rules_product_placement_preview_state_changed" &&
        behindRuntime.rulesAuthorityRevision() == behindRevision &&
        behindRuntime.rulesAuthorityActions().size() == behindActions &&
        behindRuntime.ballInHandPlacementOffer()->decisionId ==
            behindOffer.decisionId,
        "post-preview authority hash drift fails closed before Core G");
    cleanup(behindPath);

    const auto commitPath = settingsPath("product-placement-behind-commit");
    GameRuntime commitRuntime;
    ai::AiPlayerController commitAi;
    ModernProductSessionOptions commitOptions;
    commitOptions.settingsPath = commitPath.string();
    commitOptions.rulesDecisionLocalPlayerId = 1;
    ModernProductSession commitSession(commitRuntime, commitAi, commitOptions);
    expect(commitSession.shell().startSinglePlayer(53).ok,
        "behind-head-string commit fixture starts Match");
    ai::PlayerConfiguration localCommitPlayers;
    localCommitPlayers.enabled = false;
    expect(commitAi.configure(commitRuntime, localCommitPlayers).ok,
        "behind-head-string commit fixture makes both actors local");
    stageBehindHeadStringPlacement(commitRuntime);
    const auto commitOffer = *commitRuntime.ballInHandPlacementOffer();
    const std::uint64_t commitRevision = commitRuntime.rulesAuthorityRevision();
    expect(commitSession.previewProductAuthorizedBallInHandPlacement(legal).ok,
        "behind-head-string commit preview succeeds");
    const auto committed =
        commitSession.confirmProductAuthorizedBallInHandPlacement(
            rules::kConfirmBehindHeadStringPlacementActionId, legal,
            "product-placement-behind-1");
    expect(committed.ok &&
        commitRuntime.rulesAuthorityRevision() == commitRevision + 1 &&
        commitRuntime.matchState().ballInHand &&
        commitRuntime.ballInHandPlacementOffer() &&
        commitRuntime.ballInHandPlacementOffer()->decisionId !=
            commitOffer.decisionId &&
        commitRuntime.rulesAuthorityActions().back().actionId ==
            rules::kConfirmBehindHeadStringPlacementActionId,
        "behind-head-string product confirm commits its exact scoped child");
    cleanup(commitPath);
}

void testProductAuthorizedBreakFoulParentAndBehindHeadStringChild()
{
    const auto path = settingsPath("product-break-foul-parent");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionLocalPlayerId = 1;
    options.rulesDecisionProductCapabilities.breakShotFoulChoice = true;
    ModernProductSession session(runtime, ai, options);

    const auto outside = session.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId, "outside-match", "{}");
    expect(!outside.ok && outside.handled && outside.errorCode ==
            "rules_product_break_foul_not_in_match" &&
        runtime.rulesAuthorityActions().empty(),
        "narrow break-foul seam rejects outside Match");

    expect(session.shell().startSinglePlayer(61).ok,
        "break-foul product fixture starts Match");
    expect(runtime.setCameraPresetInstant(camera::SemanticState::ObserveFree,
            camera::Preset::Elevated).ok,
        "break-foul camera fixture starts from a restorable free pose");
    GameAction orbit;
    orbit.type = ActionType::OrbitCamera;
    orbit.first = 0.21f;
    orbit.second = -0.07f;
    expect(runtime.dispatch(orbit).ok,
        "break-foul camera fixture records a user-composed orbit");
    const camera::Frame cameraBeforeChild = runtime.cameraFrame();
    const float aimYawBeforeChild = runtime.shotControl().yawRadians;
    stageBreakShotFoulOffer(runtime);
    const auto offer = *runtime.breakFoulDecisionOffer();
    const std::vector<std::string> expectedActions{
        rules::kBreakFoulBallInHandActionId,
        rules::kBreakFoulRerackOpponentActionId,
        rules::kBreakFoulRerackOriginalActionId};
    expect(offer.actorPlayerId == 1 - offer.originalBreakerPlayerId &&
        offer.actorPlayerId == 1 &&
        offer.offeredActionIds == expectedActions &&
        !runtime.matchState().ballInHand &&
        !runtime.ballInHandPlacementOffer().has_value(),
        "frozen Core-H offer has exact opponent actor/action order and no early BIH");
    const std::uint64_t revision = runtime.rulesAuthorityRevision();
    const std::size_t actions = runtime.rulesAuthorityActions().size();
    const std::string hash = runtime.rulesAuthorityStateHash();
    ai::PlayerConfiguration aiConfiguration;
    aiConfiguration.enabled = true;
    aiConfiguration.player = offer.originalBreakerPlayerId;
    aiConfiguration.deterministicSeed = 61;
    expect(ai.configure(runtime, aiConfiguration).ok,
        "break-foul fixture keeps the frozen opposing actor local to the human");

    const auto sibling = session.submitProductAuthorizedBreakFoulChoice(
        rules::kEightOnBreakSpotBallInHandActionId, "sibling", "{}");
    const auto foundation = session.submitProductAuthorizedBreakFoulChoice(
        rules::kFoundationRerackActionId, "foundation", "{}");
    const auto wrongPayload = session.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId, "wrong-payload", "[]");
    const auto emptyKey = session.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId, "", "{}");
    expect(!sibling.ok && !foundation.ok && !wrongPayload.ok && !emptyKey.ok &&
        sibling.errorCode ==
            "rules_product_break_foul_action_not_authorized" &&
        foundation.errorCode ==
            "rules_product_break_foul_action_not_authorized" &&
        wrongPayload.errorCode == "rules_decision_invalid_payload" &&
        emptyKey.errorCode == "rules_decision_idempotency_key_invalid" &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.rulesAuthorityActions().size() == actions &&
        runtime.rulesAuthorityStateHash() == hash &&
        !runtime.matchState().ballInHand,
        "sibling/foundation IDs, noncanonical payload and malformed key fail before authority mutation");

    const auto committed = session.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId,
        "product-break-foul-bih-1", "{}");
    expect(committed.ok && committed.handled &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        runtime.rulesAuthorityActions().size() == actions + 1 &&
        !runtime.breakFoulDecisionOffer().has_value() &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementScope() ==
            rules::BallInHandPlacementScope::BehindHeadString &&
        runtime.ballInHandPlacementOffer().has_value() &&
        runtime.ballInHandPlacementOffer()->actorPlayerId == offer.actorPlayerId &&
        runtime.rulesAuthorityActions().back().decisionId == offer.decisionId &&
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kBreakFoulBallInHandActionId &&
        runtime.rulesAuthorityActions().back().actorPlayerId ==
            offer.actorPlayerId &&
        runtime.rulesAuthorityActions().back().payloadCanonical == "{}" &&
        runtime.rulesAuthorityActions().back().idempotencyKey ==
            "product-break-foul-bih-1",
        "accepted parent ACK atomically grants BIH and stages one exact BehindHeadString child");
    expect(!session.snapshot().rulesDecisionUiImplementationEnabled &&
        !session.snapshot().rulesDecisionUiProductExposed &&
        session.snapshot().rulesDecisionProductActive &&
        session.snapshot().rulesDecision.offer.flowId ==
            rules::kBehindHeadStringBallInHandFlowId,
        "typed 06E capability advances from the parent into its exact BIH child");
    expect(session.presentationCameraFrame().state ==
            camera::SemanticState::AimOverhead &&
        close(session.presentationCameraFrame().pose.target.x, 0.0) &&
        close(session.presentationCameraFrame().pose.target.z, 0.0) &&
        close(runtime.shotControl().yawRadians, aimYawBeforeChild) &&
        runtime.rulesAuthorityRevision() == revision + 1,
        "new BehindHeadString child frames the table beside the modal once "
        "without changing aim or authority");
    const float childCameraDistance =
        session.presentationCameraFrame().pose.distanceCm;
    PlatformInputEvent childZoom;
    childZoom.kind = PlatformInputKind::Scroll;
    childZoom.scrollY = 3.0;
    const auto zoomed = session.handlePlatformInput(childZoom);
    expect(zoomed.ok && zoomed.handled &&
        session.presentationCameraFrame().pose.distanceCm <=
            childCameraDistance - 29.0f &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        runtime.ballInHandPlacementOffer().has_value(),
        "BehindHeadString child permits presentation-only wheel zoom without authority mutation: " +
            zoomed.errorCode);
    const float userAdjustedChildDistance =
        session.presentationCameraFrame().pose.distanceCm;
    session.syncRulesDecisionAuthority();
    expect(close(session.presentationCameraFrame().pose.distanceCm,
            userAdjustedChildDistance),
        "same child refresh does not overwrite the user's camera adjustment");
    PlatformInputEvent postAckShot;
    postAckShot.kind = PlatformInputKind::KeyDown;
    postAckShot.control = "Tab";
    const auto localChildInput = session.handlePlatformInput(postAckShot);
    expect(localChildInput.ok && localChildInput.handled &&
        runtime.rulesAuthorityRevision() == revision + 1,
        "accepted ACK refreshes the AI/input gate without blocking the local offered actor's BIH child: " +
            localChildInput.errorCode);

    const std::uint64_t parentRevision = runtime.rulesAuthorityRevision();
    const std::size_t parentActions = runtime.rulesAuthorityActions().size();
    const auto exactRetry = session.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId,
        "product-break-foul-bih-1", "{}");
    const auto conflict = session.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulRerackOpponentActionId,
        "product-break-foul-bih-1", "{}");
    const auto payloadConflict = session.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId,
        "product-break-foul-bih-1", "{\"scope\":\"Anywhere\"}");
    expect(exactRetry.ok && !conflict.ok && conflict.errorCode ==
            "rules_decision_idempotency_conflict" &&
        !payloadConflict.ok && payloadConflict.errorCode ==
            "rules_decision_invalid_payload" &&
        runtime.rulesAuthorityRevision() == parentRevision &&
        runtime.rulesAuthorityActions().size() == parentActions &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementOffer().has_value(),
        "exact parent retry returns the frozen ACK and action/payload conflicts have zero child/authority delta");

    const rules::BallInHandPlacementPosition invalid{
        runtime.state().balls[1].position.x,
        runtime.state().balls[1].position.z};
    const auto invalidPreview =
        session.previewProductAuthorizedBallInHandPlacement(invalid);
    expect(!invalidPreview.ok &&
        runtime.rulesAuthorityRevision() == parentRevision &&
        runtime.rulesAuthorityActions().size() == parentActions &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementOffer().has_value(),
        "invalid BehindHeadString child preview rolls back without weakening the committed parent entitlement");
    const rules::BallInHandPlacementPosition legal{-20.0, -90.0};
    expect(session.previewProductAuthorizedBallInHandPlacement(legal).ok,
        "authorized BehindHeadString child accepts one legal preview");
    const auto child = session.confirmProductAuthorizedBallInHandPlacement(
        rules::kConfirmBehindHeadStringPlacementActionId, legal,
        "product-break-foul-child-1");
    expect(child.ok && runtime.rulesAuthorityRevision() == parentRevision + 1 &&
        runtime.rulesAuthorityActions().size() == parentActions + 1 &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementOffer().has_value() &&
        !session.snapshot().rulesDecisionProductActive &&
        session.snapshot().rulesDecision.offer.decisionId.empty(),
        "BehindHeadString coordinate child retains BIH for the stroke while closing its completed product modal");
    expect(session.presentationCameraFrame().state == cameraBeforeChild.state &&
        session.presentationCameraFrame().preset == cameraBeforeChild.preset &&
        close(session.presentationCameraFrame().pose.eye.x,
            cameraBeforeChild.pose.eye.x) &&
        close(session.presentationCameraFrame().pose.eye.y,
            cameraBeforeChild.pose.eye.y) &&
        close(session.presentationCameraFrame().pose.eye.z,
            cameraBeforeChild.pose.eye.z) &&
        close(runtime.shotControl().yawRadians, aimYawBeforeChild),
        "completed child restores the exact pre-placement camera without changing aim");
    cleanup(path);
}

void testProductAuthorizedBreakFoulRerackBranches()
{
    struct Case {
        const char* suffix;
        const char* action;
        int expectedBreaker;
    };
    const Case cases[] = {
        {"opponent", rules::kBreakFoulRerackOpponentActionId, 1},
        {"original", rules::kBreakFoulRerackOriginalActionId, 0},
    };
    for (const Case& current : cases) {
        const auto path = settingsPath(current.suffix);
        GameRuntime runtime;
        ai::AiPlayerController ai;
        ModernProductSessionOptions options;
        options.settingsPath = path.string();
        options.rulesDecisionLocalPlayerId = 1;
        options.rulesDecisionProductCapabilities.breakShotFoulChoice = true;
        ModernProductSession session(runtime, ai, options);
        expect(session.shell().startSinglePlayer(67).ok,
            "rerack fixture starts Match");
        stageBreakShotFoulOffer(runtime);
        const auto offer = *runtime.breakFoulDecisionOffer();
        ai::PlayerConfiguration originalBreakerAi;
        originalBreakerAi.enabled = true;
        originalBreakerAi.player = offer.originalBreakerPlayerId;
        originalBreakerAi.deterministicSeed = 67;
        expect(ai.configure(runtime, originalBreakerAi).ok,
            "rerack fixture keeps the opposing decision actor local");
        const std::uint64_t revision = runtime.rulesAuthorityRevision();
        const std::size_t actions = runtime.rulesAuthorityActions().size();
        const auto committed = session.submitProductAuthorizedBreakFoulChoice(
            current.action, std::string("product-break-rerack-") +
                current.suffix, "{}");
        expect(committed.ok &&
            runtime.rulesAuthorityRevision() == revision + 1 &&
            runtime.rulesAuthorityActions().size() == actions + 1 &&
            runtime.matchState().phase == rules::MatchPhase::Break &&
            runtime.matchState().currentPlayer == current.expectedBreaker &&
            !runtime.matchState().ballInHand &&
            !runtime.ballInHandPlacementOffer().has_value() &&
            !runtime.breakFoulDecisionOffer().has_value() &&
            runtime.rulesAuthorityActions().back().decisionId ==
                offer.decisionId &&
            runtime.rulesAuthorityActions().back().actionId == current.action,
            "each offered rerack branch atomically rebuilds with its exact frozen breaker and no BIH");
        cleanup(path);
    }
}

void testProductAuthorizedBreakFoulRejectsActorSiblingResetAndDrift()
{
    const auto actorPath = settingsPath("break-foul-wrong-actor");
    GameRuntime actorRuntime;
    ai::AiPlayerController actorAi;
    ModernProductSessionOptions actorOptions;
    actorOptions.settingsPath = actorPath.string();
    actorOptions.rulesDecisionLocalPlayerId = 0;
    actorOptions.rulesDecisionProductCapabilities.breakShotFoulChoice = true;
    ModernProductSession actorSession(actorRuntime, actorAi, actorOptions);
    expect(actorSession.shell().startSinglePlayer(71).ok,
        "wrong-actor fixture starts Match");
    stageBreakShotFoulOffer(actorRuntime);
    ai::PlayerConfiguration remoteActor;
    remoteActor.enabled = true;
    remoteActor.player = actorRuntime.breakFoulDecisionOffer()->actorPlayerId;
    remoteActor.deterministicSeed = 71;
    expect(actorAi.configure(actorRuntime, remoteActor).ok,
        "wrong-actor fixture assigns the decision actor to AI");
    const auto actorRevision = actorRuntime.rulesAuthorityRevision();
    const auto wrongActor = actorSession.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId, "wrong-actor", "{}");
    expect(!wrongActor.ok && wrongActor.errorCode ==
            "rules_decision_actor_mismatch" &&
        actorRuntime.rulesAuthorityRevision() == actorRevision &&
        !actorRuntime.matchState().ballInHand &&
        actorRuntime.breakFoulDecisionOffer().has_value(),
        "product consumer requires local actor to be the original breaker's opponent");
    cleanup(actorPath);

    const auto siblingPath = settingsPath("break-foul-illegal-sibling");
    GameRuntime siblingRuntime;
    ai::AiPlayerController siblingAi;
    ModernProductSessionOptions siblingOptions;
    siblingOptions.settingsPath = siblingPath.string();
    siblingOptions.rulesDecisionLocalPlayerId = 1;
    siblingOptions.rulesDecisionProductCapabilities.breakShotFoulChoice = true;
    ModernProductSession siblingSession(siblingRuntime, siblingAi,
        siblingOptions);
    expect(siblingSession.shell().startSinglePlayer(73).ok,
        "illegal-break sibling fixture starts Match");
    auto weak = facts(0, 1);
    weak.breakShot = true;
    attestWeakBreakEvidence(weak);
    expect(siblingRuntime.adjudicateRulesShot(weak).ok &&
        siblingRuntime.breakFoulDecisionOffer().has_value() &&
        siblingRuntime.breakFoulDecisionOffer()->branch ==
            rules::BreakFoulBranch::IllegalBreak,
        "upstream authority stages separate IllegalBreak sibling");
    const auto siblingRevision = siblingRuntime.rulesAuthorityRevision();
    const auto siblingRejected =
        siblingSession.submitProductAuthorizedBreakFoulChoice(
            rules::kBreakFoulRerackOpponentActionId,
            "illegal-sibling-overlap", "{}");
    expect(!siblingRejected.ok && siblingRejected.errorCode ==
            "rules_product_break_foul_context_not_authorized" &&
        siblingRuntime.rulesAuthorityRevision() == siblingRevision &&
        siblingRuntime.breakFoulDecisionOffer().has_value(),
        "overlapping rerack ID from IllegalBreak sibling cannot enter §6(e) consumer");
    cleanup(siblingPath);

    const auto driftPath = settingsPath("break-foul-drift");
    GameRuntime driftRuntime;
    ai::AiPlayerController driftAi;
    ModernProductSessionOptions driftOptions;
    driftOptions.settingsPath = driftPath.string();
    driftOptions.rulesDecisionLocalPlayerId = 1;
    driftOptions.rulesDecisionProductCapabilities.breakShotFoulChoice = true;
    ModernProductSession driftSession(driftRuntime, driftAi, driftOptions);
    expect(driftSession.shell().startSinglePlayer(79).ok,
        "state-drift fixture starts Match");
    stageBreakShotFoulOffer(driftRuntime);
    ai::PlayerConfiguration driftOriginalBreakerAi;
    driftOriginalBreakerAi.enabled = true;
    driftOriginalBreakerAi.player =
        driftRuntime.breakFoulDecisionOffer()->originalBreakerPlayerId;
    driftOriginalBreakerAi.deterministicSeed = 79;
    expect(driftAi.configure(driftRuntime, driftOriginalBreakerAi).ok,
        "break-foul drift fixture keeps the decision actor local");
    const auto driftRevision = driftRuntime.rulesAuthorityRevision();
    const auto driftActions = driftRuntime.rulesAuthorityActions().size();
    driftRuntime.mutableState().balls[2].position.x += 0.5f;
    const auto drift = driftSession.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId, "break-foul-drift", "{}");
    expect(!drift.ok && drift.errorCode ==
            "rules_break_foul_state_changed" &&
        driftRuntime.rulesAuthorityRevision() == driftRevision &&
        driftRuntime.rulesAuthorityActions().size() == driftActions &&
        !driftRuntime.matchState().ballInHand &&
        driftRuntime.breakFoulDecisionOffer().has_value(),
        "frozen state-hash drift fails closed before Core H mutation");
    cleanup(driftPath);

    const auto resetPath = settingsPath("break-foul-reset");
    GameRuntime resetRuntime;
    ai::AiPlayerController resetAi;
    ModernProductSessionOptions resetOptions;
    resetOptions.settingsPath = resetPath.string();
    resetOptions.rulesDecisionLocalPlayerId = 1;
    resetOptions.rulesDecisionProductCapabilities.breakShotFoulChoice = true;
    ModernProductSession resetSession(resetRuntime, resetAi, resetOptions);
    expect(resetSession.shell().startSinglePlayer(83).ok,
        "reset fixture starts Match");
    stageBreakShotFoulOffer(resetRuntime);
    ai::PlayerConfiguration resetOriginalBreakerAi;
    resetOriginalBreakerAi.enabled = true;
    resetOriginalBreakerAi.player =
        resetRuntime.breakFoulDecisionOffer()->originalBreakerPlayerId;
    resetOriginalBreakerAi.deterministicSeed = 83;
    expect(resetAi.configure(resetRuntime, resetOriginalBreakerAi).ok,
        "break-foul reset fixture keeps the decision actor local");
    const auto beforeReset = resetSession.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId, "before-reset", "{}");
    expect(beforeReset.ok && resetRuntime.matchState().ballInHand &&
        resetRuntime.ballInHandPlacementOffer().has_value(),
        "reset fixture first retains one verified parent request");
    resetRuntime.reset();
    const auto resetRevision = resetRuntime.rulesAuthorityRevision();
    const auto stale = resetSession.submitProductAuthorizedBreakFoulChoice(
        rules::kBreakFoulBallInHandActionId, "before-reset", "{}");
    expect(!stale.ok && stale.errorCode == "rules_decision_stale" &&
        resetRuntime.rulesAuthorityRevision() == resetRevision &&
        !resetRuntime.matchState().ballInHand &&
        !resetRuntime.ballInHandPlacementOffer().has_value(),
        "match reset invalidates the frozen offer and prevents stale parent/child effects");
    cleanup(resetPath);
}

void testProductIllegalBreakTypedCapabilityAndAuthorityAck()
{
    struct Case { const char* suffix; const char* action; bool rerack; };
    const Case cases[] = {
        {"accept", rules::kBreakFoulAcceptTableActionId, false},
        {"opponent", rules::kBreakFoulRerackOpponentActionId, true},
        {"original", rules::kBreakFoulRerackOriginalActionId, true},
    };
    for (const Case& current : cases) {
        const auto path = settingsPath(current.suffix);
        GameRuntime runtime;
        ai::AiPlayerController ai;
        ModernProductSessionOptions options;
        options.settingsPath = path.string();
        options.rulesDecisionProductCapabilities.illegalBreakChoice = true;
        ModernProductSession session(runtime, ai, options);
        expect(session.shell().startSinglePlayer(601).ok,
            "typed 06C fixture starts product Match");
        const GameState officialRackFoundation = runtime.state();
        ai::PlayerConfiguration localTwoPlayer;
        expect(ai.configure(runtime, localTwoPlayer).ok,
            "disabled AI makes both authoritative actors locally interactive");
        if (current.rerack) {
            // Product-fixture disturbance: prove that the product choice
            // rebuilds the rack rather than merely changing MatchPhase.
            runtime.mutableState().balls[1].position.x += 17.0f;
            runtime.mutableState().balls[8].position.z -= 11.0f;
            runtime.mutableState().balls[1].velocity = {3.0f, 0.0f, 2.0f};
            runtime.mutableState().balls[1].angularVelocity =
                {1.0f, 2.0f, 3.0f};
            runtime.mutableState().balls[1].speed = 3.6055512f;
            runtime.mutableState().balls[1].pocketInteraction.phase =
                PocketInteractionPhase::Approaching;
            runtime.mutableState().balls[1].pocketInteraction.pocketId = 4;
            runtime.mutableState().balls[1].pocketInteraction.captureSequence =
                41;
            runtime.mutableState().nextPocketCaptureSequence = 42;
            runtime.mutableState().input.hitRequested = true;
            runtime.mutableState().players.shotTaken = true;
            runtime.mutableState().players.updatedAfterShot = true;
            runtime.mutableState().events.ballCollision = true;
            runtime.mutableState().events.railCollision = true;
            runtime.mutableState().events.shotEnded = true;
        }
        stageIllegalBreakOffer(runtime);
        if (current.rerack)
            expect(runtime.state().balls[1].angularVelocity.x == 1.0f &&
                runtime.state().balls[1].pocketInteraction.phase ==
                    PocketInteractionPhase::Approaching &&
                runtime.state().balls[1].pocketInteraction.pocketId == 4 &&
                runtime.state().balls[1].pocketInteraction.captureSequence ==
                    41 &&
                runtime.state().nextPocketCaptureSequence == 42 &&
                runtime.state().input.hitRequested &&
                runtime.state().events.shotEnded,
                "component reset stress state survives offer staging before rerack commit");
        const auto offer = *runtime.breakFoulDecisionOffer();
        session.syncRulesDecisionAuthority();
        const auto visible = session.snapshot();
        expect(visible.rulesDecisionIllegalBreakProductEnabled &&
            visible.rulesDecisionProductActive &&
            !visible.rulesDecisionUiProductExposed &&
            visible.rulesDecision.offer.branchId == "illegal_break_6c" &&
            visible.rulesDecision.visibleActionIds.size() == 3,
            "typed capability exposes only exact IllegalBreak offer without global UI flag");
        const auto revision = runtime.rulesAuthorityRevision();
        const auto committed = session.submitProductAuthorizedIllegalBreakChoice(
            current.action, std::string("product-06c-") + current.suffix);
        const int expectedBreaker =
            current.action == rules::kBreakFoulRerackOpponentActionId ?
            1 - offer.originalBreakerPlayerId : offer.originalBreakerPlayerId;
        const bool exactRackFoundation = !current.rerack ||
            std::all_of(runtime.state().balls.begin(),
                runtime.state().balls.end(), [&](const BallState& ball) {
                    const std::size_t index = static_cast<std::size_t>(
                        &ball - runtime.state().balls.data());
                    const BallState& expected =
                        officialRackFoundation.balls[index];
                    return close(ball.position.x, expected.position.x) &&
                        close(ball.position.y, expected.position.y) &&
                        close(ball.position.z, expected.position.z) &&
                        close(ball.velocity.x, 0.0) &&
                        close(ball.velocity.y, 0.0) &&
                        close(ball.velocity.z, 0.0) &&
                        close(ball.angularVelocity.x, 0.0) &&
                        close(ball.angularVelocity.y, 0.0) &&
                        close(ball.angularVelocity.z, 0.0) &&
                        close(ball.speed, 0.0) && !ball.pocketed &&
                        !ball.outOfPlay &&
                        ball.physicalDomain == PhysicalBallDomain::ClothSupported &&
                        ball.pocketInteraction.phase ==
                            PocketInteractionPhase::Outside &&
                        ball.pocketInteraction.pocketId == -1;
                });
        const bool exactRerackOutcome = !current.rerack ||
            (exactRackFoundation && !runtime.state().ballsMoving &&
                runtime.state().pocketedBallCount == 0 &&
                runtime.state().nextPocketCaptureSequence == 1 &&
                !runtime.state().input.hitRequested &&
                !runtime.state().players.shotTaken &&
                !runtime.state().players.updatedAfterShot &&
                !runtime.state().players.illegalShot &&
                !runtime.state().events.ballCollision &&
                !runtime.state().events.railCollision &&
                !runtime.state().events.ballPocketed &&
                !runtime.state().events.cueBallPocketed &&
                !runtime.state().events.eightBallPocketed &&
                !runtime.state().events.shotEnded &&
                !runtime.shotFactCollectorActive() &&
                !runtime.hasLastShotFacts() &&
                !runtime.hasLastAdjudication() &&
                runtime.matchState().currentPlayer == expectedBreaker &&
                runtime.state().players.currentPlayer == expectedBreaker &&
                runtime.state().players.nextPlayer == expectedBreaker &&
                !runtime.matchState().ballInHand &&
                !runtime.groupChoiceDecisionOffer().has_value() &&
                runtime.ballInHandPlacementScope() ==
                    rules::BallInHandPlacementScope::None &&
                !runtime.ballInHandPlacementOffer().has_value() &&
                !runtime.eightOnBreakDecisionOffer().has_value() &&
                !runtime.rulesRackTerminalState().active &&
                runtime.matchState().groups == std::array<rules::BallGroup, 2>{
                    rules::BallGroup::Open, rules::BallGroup::Open} &&
                runtime.matchState().winner == -1);
        const auto& committedAction = runtime.rulesAuthorityActions().back();
        expect(committed.ok &&
            runtime.rulesAuthorityRevision() == revision + 1 &&
            !runtime.breakFoulDecisionOffer().has_value() &&
            committedAction.decisionId == offer.decisionId &&
            committedAction.actionId == current.action &&
            committedAction.actorPlayerId == offer.actorPlayerId &&
            committedAction.previousStateRevision == revision &&
            committedAction.authoritativeStateRevision == revision + 1 &&
            committedAction.authoritativeStateHash ==
                runtime.rulesAuthorityStateHash() &&
            committedAction.payloadCanonical == "{}" && exactRerackOutcome &&
            runtime.matchState().phase ==
                (current.rerack ? rules::MatchPhase::Break :
                    rules::MatchPhase::OpenTable),
            "each 06C choice commits one exact ACK and complete rack outcome");
        const auto retry = session.submitProductAuthorizedIllegalBreakChoice(
            current.action, std::string("product-06c-") + current.suffix);
        const auto conflict = session.submitProductAuthorizedIllegalBreakChoice(
            current.action == rules::kBreakFoulAcceptTableActionId ?
                rules::kBreakFoulRerackOpponentActionId :
                rules::kBreakFoulAcceptTableActionId,
            std::string("product-06c-") + current.suffix);
        expect(retry.ok && !conflict.ok && conflict.errorCode ==
                "rules_decision_idempotency_conflict" &&
            runtime.rulesAuthorityRevision() == revision + 1,
            "06C retry is idempotent and conflicting reuse has zero mutation");
        cleanup(path);
    }

    const auto path = settingsPath("06c-sibling-blocked");
    GameRuntime sibling;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionProductCapabilities.illegalBreakChoice = true;
    ModernProductSession session(sibling, ai, options);
    expect(session.shell().startSinglePlayer(602).ok,
        "unsupported sibling fixture starts Match");
    ai::PlayerConfiguration disabled;
    expect(ai.configure(sibling, disabled).ok, "sibling actor is local");
    stageBreakShotFoulOffer(sibling);
    const auto siblingRevision = sibling.rulesAuthorityRevision();
    const auto siblingActions = sibling.rulesAuthorityActions().size();
    session.syncRulesDecisionAuthority();
    const auto blocked = session.snapshot();
    expect(blocked.rulesDecisionProductActive &&
        blocked.rulesDecision.state ==
            rules::ui::DecisionUiState::AuthorityErrorBlocking &&
        blocked.rulesDecision.blockingCode ==
            "rules_decision_product_branch_unavailable" &&
        sibling.breakFoulDecisionOffer().has_value() &&
        sibling.rulesAuthorityRevision() == siblingRevision &&
        sibling.rulesAuthorityActions().size() == siblingActions,
        "unopened local sibling is explicitly blocked without authority mutation");
    cleanup(path);

    const auto remotePath = settingsPath("06c-ai-actor");
    GameRuntime remote;
    ai::AiPlayerController remoteAi;
    ModernProductSessionOptions remoteOptions;
    remoteOptions.settingsPath = remotePath.string();
    remoteOptions.rulesDecisionProductCapabilities.illegalBreakChoice = true;
    ModernProductSession remoteSession(remote, remoteAi, remoteOptions);
    expect(remoteSession.shell().startSinglePlayer(603).ok,
        "remote actor fixture starts single-player Match");
    stageIllegalBreakOffer(remote);
    remoteSession.syncRulesDecisionAuthority();
    const auto remoteSnapshot = remoteSession.snapshot();
    const auto remoteRevision = remote.rulesAuthorityRevision();
    const auto remoteRejected =
        remoteSession.submitProductAuthorizedIllegalBreakChoice(
            rules::kBreakFoulAcceptTableActionId, "remote-actor");
    expect(!remoteSnapshot.rulesDecisionProductActive &&
        !remoteRejected.ok && remoteRejected.errorCode ==
            "rules_decision_actor_mismatch" &&
        remote.rulesAuthorityRevision() == remoteRevision &&
        remote.breakFoulDecisionOffer().has_value(),
        "single-player AI actor remains remote and cannot be clicked by local input");
    cleanup(remotePath);
}

void testProduct06EParentPlacementAndTieRespotPointerChain()
{
    const auto path = settingsPath("product-06e-pointer-chain");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionProductCapabilities.breakShotFoulChoice = true;
    ModernProductSession session(runtime, ai, options);
    expect(session.shell().startSinglePlayer(606).ok,
        "06E pointer fixture starts Match");
    ai::PlayerConfiguration localTwoPlayer;
    expect(ai.configure(runtime, localTwoPlayer).ok,
        "06E pointer fixture makes either authority actor local");
    const double head = -runtime.physicsProfile().tableBoundary.
        playfieldLengthCm * 0.25;
    for (int ball = 1; ball < kBallCount; ++ball) {
        runtime.mutableState().balls[ball].pocketed = ball != 2 && ball != 3;
        runtime.mutableState().balls[ball].position.z =
            static_cast<float>(head - 30.0 - ball);
    }
    runtime.mutableState().balls[2].position = Point3{-12.0f,
        kTableHeight + runtime.physicsProfile().ball.radiusCm,
        static_cast<float>(head - 2.0)};
    runtime.mutableState().balls[3].position = Point3{12.0f,
        kTableHeight + runtime.physicsProfile().ball.radiusCm,
        static_cast<float>(head - 2.0)};
    stageBreakShotFoulOffer(runtime);
    session.setShellViewport({1280, 720, 1280, 720, 96.0f});
    const camera::Frame renderedFrame = session.presentationCameraFrame();
    CameraPresentation placementCamera;
    placementCamera.eye = {renderedFrame.pose.eye.x, renderedFrame.pose.eye.y,
        renderedFrame.pose.eye.z};
    placementCamera.target = {renderedFrame.pose.target.x,
        renderedFrame.pose.target.y, renderedFrame.pose.target.z};
    placementCamera.fieldOfViewDegrees =
        renderedFrame.pose.fieldOfViewDegrees;
    placementCamera.projectionPan = {3.5f, 0.0f, -2.25f};
    session.setRulesDecisionProjectionCamera(placementCamera);
    session.syncRulesDecisionAuthority();
    const auto parent = session.snapshot();
    expect(parent.rulesDecisionBreakShotFoulProductEnabled &&
        parent.rulesDecisionProductActive &&
        parent.rulesDecision.offer.branchId == "break_shot_foul_6e" &&
        parent.rulesDecision.visibleActionIds.size() == 3,
        "typed 06E capability exposes only its exact local parent offer");

    const ui::RulesDecisionUiHitRect ballInHand{
        "action:bih", rules::kBreakFoulBallInHandActionId,
        {40.0f, 40.0f, 220.0f, 52.0f}, true, false};
    session.setRulesDecisionHitRects({ballInHand});
    const auto parentRevision = runtime.rulesAuthorityRevision();
    expect(clickRulesRect(session, ballInHand).ok &&
        runtime.rulesAuthorityRevision() == parentRevision &&
        session.snapshot().rulesDecision.armedActionId ==
            rules::kBreakFoulBallInHandActionId,
        "06E parent action click only arms and has zero authority mutation");
    const ui::RulesDecisionUiHitRect parentConfirm{
        "confirm:bih", rules::kBreakFoulBallInHandActionId,
        {40.0f, 104.0f, 220.0f, 52.0f}, true, true};
    session.setRulesDecisionHitRects({parentConfirm});
    expect(clickRulesRect(session, parentConfirm).ok &&
        runtime.rulesAuthorityRevision() == parentRevision + 1 &&
        runtime.ballInHandPlacementOffer().has_value() &&
        session.snapshot().rulesDecision.offer.flowId ==
            rules::kBehindHeadStringBallInHandFlowId,
        "explicit parent confirm commits once and exposes its exact BIH child");

    const auto childRevision = runtime.rulesAuthorityRevision();
    session.setRulesDecisionHitRects({});
    session.setRulesDecisionExclusionRects({{0.0, 0.0, 320.0, 180.0}});
    (void)session.handlePlatformInput(pointerEvent(
        PlatformInputKind::PointerMove, 100.0, 100.0));
    expect(!session.snapshot().rulesDecision.placementPreviewVisible &&
        runtime.rulesAuthorityRevision() == childRevision,
        "pointer over modal exclusion cannot create a placement preview");
    session.setRulesDecisionExclusionRects({});
    const auto invalidPointer = placementPointer(
        placementCamera, runtime, false);
    const auto legalPointer = placementPointer(placementCamera, runtime, true);
    expect(invalidPointer.has_value() && legalPointer.has_value(),
        "actual camera projection finds both runtime-invalid and legal table points");
    (void)session.handlePlatformInput(pointerEvent(
        PlatformInputKind::PointerMove, invalidPointer->first,
        invalidPointer->second));
    expect(session.snapshot().rulesDecision.placementPreviewVisible &&
        !session.snapshot().rulesDecision.placementPreviewLegal &&
        !session.snapshot().rulesDecision.placementPreviewErrorCode.empty() &&
        runtime.rulesAuthorityRevision() == childRevision,
        "invalid table pointer remains a visible local reason with zero authority mutation");

    const std::string requestAction = rules::kRequestNearestLegalRespotActionId;
    const ui::RulesDecisionUiHitRect requestRespot{
        "action:request-respot", requestAction,
        {340.0f, 40.0f, 260.0f, 52.0f}, true, false};
    session.setRulesDecisionHitRects({requestRespot});
    expect(clickRulesRect(session, requestRespot).ok &&
        runtime.rulesAuthorityRevision() == childRevision,
        "all-behind request arms without requiring a coordinate preview");
    const ui::RulesDecisionUiHitRect requestConfirm{
        "confirm:request-respot", requestAction,
        {340.0f, 104.0f, 260.0f, 52.0f}, true, true};
    session.setRulesDecisionHitRects({requestConfirm});
    expect(clickRulesRect(session, requestConfirm).ok &&
        runtime.rulesAuthorityRevision() == childRevision + 1 &&
        runtime.behindHeadStringRespotOffer().has_value() &&
        session.snapshot().rulesDecision.offer.flowId ==
            rules::kBehindHeadStringRespotFlowId &&
        session.snapshot().rulesDecision.visibleActionIds.size() == 2,
        "all-behind tie becomes a visible typed child instead of hidden authority state");

    const auto tieRevision = runtime.rulesAuthorityRevision();
    const std::string tiedAction =
        session.snapshot().rulesDecision.visibleActionIds.front();
    const ui::RulesDecisionUiHitRect tiedBall{
        "action:tied-ball", tiedAction,
        {40.0f, 40.0f, 220.0f, 52.0f}, true, false};
    session.setRulesDecisionHitRects({tiedBall});
    expect(clickRulesRect(session, tiedBall).ok &&
        runtime.rulesAuthorityRevision() == tieRevision,
        "tie ball selection click only arms");
    const ui::RulesDecisionUiHitRect tiedConfirm{
        "confirm:tied-ball", tiedAction,
        {40.0f, 104.0f, 220.0f, 52.0f}, true, true};
    session.setRulesDecisionHitRects({tiedConfirm});
    expect(clickRulesRect(session, tiedConfirm).ok &&
        runtime.rulesAuthorityRevision() == tieRevision + 1 &&
        !runtime.behindHeadStringRespotOffer().has_value() &&
        runtime.ballInHandPlacementOffer().has_value() &&
        session.snapshot().rulesDecision.offer.flowId ==
            rules::kBehindHeadStringBallInHandFlowId,
        "explicit tie confirm respots one selected ball and returns to replacement BIH");

    const auto placementRevision = runtime.rulesAuthorityRevision();
    const auto freshLegalPointer = placementPointer(
        placementCamera, runtime, true);
    expect(freshLegalPointer.has_value(),
        "replacement BIH has a legal point under the actual camera");
    session.setRulesDecisionHitRects({});
    (void)session.handlePlatformInput(pointerEvent(
        PlatformInputKind::PointerMove, freshLegalPointer->first,
        freshLegalPointer->second));
    const auto preview = session.snapshot().rulesDecision;
    expect(preview.placementPreviewLegal &&
        runtime.rulesAuthorityRevision() == placementRevision,
        "legal pointer preview remains local before explicit confirm");
    const ui::RulesDecisionUiHitRect placementAction{
        "action:placement", rules::kConfirmBehindHeadStringPlacementActionId,
        {40.0f, 40.0f, 250.0f, 52.0f}, true, false};
    session.setRulesDecisionHitRects({placementAction});
    expect(clickRulesRect(session, placementAction).ok &&
        runtime.rulesAuthorityRevision() == placementRevision,
        "legal placement action still requires a separate confirm");
    const ui::RulesDecisionUiHitRect placementConfirm{
        "confirm:placement", rules::kConfirmBehindHeadStringPlacementActionId,
        {40.0f, 104.0f, 250.0f, 52.0f}, true, true};
    session.setRulesDecisionHitRects({placementConfirm});
    expect(clickRulesRect(session, placementConfirm).ok &&
        runtime.rulesAuthorityRevision() == placementRevision + 1 &&
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kConfirmBehindHeadStringPlacementActionId,
        "fresh legal placement confirm commits exactly once through authority");
    cleanup(path);
}

void testProduct06FEightOnBreakTypedParentAndChild()
{
    {
        const auto path = settingsPath("product-06f-legal");
        GameRuntime runtime;
        ai::AiPlayerController ai;
        ModernProductSessionOptions options;
        options.settingsPath = path.string();
        options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
        ModernProductSession session(runtime, ai, options);
        expect(session.shell().startSinglePlayer(607).ok,
            "06F legal fixture starts Match");
        ai::PlayerConfiguration localTwoPlayer;
        expect(ai.configure(runtime, localTwoPlayer).ok,
            "06F legal fixture makes the frozen actor local");
        stageLegalEightOnBreak(runtime);
        session.syncRulesDecisionAuthority();
        const auto open = session.snapshot();
        expect(open.rulesDecisionEightOnBreakProductEnabled &&
            open.rulesDecisionProductActive &&
            open.rulesDecision.offer.flowId == rules::kEightOnBreakFlowId &&
            open.rulesDecision.visibleActionIds == std::vector<std::string>{
                rules::kEightOnBreakSpotContinueActionId,
                rules::kEightOnBreakRerackOriginalActionId},
            "typed 06F capability exposes only the exact legal parent offer");
        const auto revision = runtime.rulesAuthorityRevision();
        const ui::RulesDecisionUiHitRect action{
            "action:spot", rules::kEightOnBreakSpotContinueActionId,
            {40.0f, 40.0f, 240.0f, 52.0f}, true, false};
        session.setRulesDecisionHitRects({action});
        expect(clickRulesRect(session, action).ok &&
            runtime.rulesAuthorityRevision() == revision &&
            session.snapshot().rulesDecision.armedActionId ==
                rules::kEightOnBreakSpotContinueActionId,
            "06F legal action click only arms with zero authority mutation");
        const ui::RulesDecisionUiHitRect confirm{
            "confirm:spot", rules::kEightOnBreakSpotContinueActionId,
            {40.0f, 104.0f, 240.0f, 52.0f}, true, true};
        session.setRulesDecisionHitRects({confirm});
        expect(clickRulesRect(session, confirm).ok &&
            runtime.rulesAuthorityRevision() == revision + 1 &&
            !runtime.eightOnBreakDecisionOffer() &&
            runtime.rulesAuthorityActions().back().actionId ==
                rules::kEightOnBreakSpotContinueActionId &&
            runtime.matchState().phase == rules::MatchPhase::OpenTable &&
            !runtime.matchState().ballInHand,
            "explicit 06F legal confirm applies one exact spot/continue ACK");
        cleanup(path);
    }

    {
        const auto path = settingsPath("product-06f-foul-child");
        GameRuntime runtime;
        ai::AiPlayerController ai;
        ModernProductSessionOptions options;
        options.settingsPath = path.string();
        options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
        ModernProductSession session(runtime, ai, options);
        expect(session.shell().startSinglePlayer(608).ok,
            "06F foul fixture starts Match");
        ai::PlayerConfiguration localTwoPlayer;
        expect(ai.configure(runtime, localTwoPlayer).ok,
            "06F foul fixture makes the opposing actor local");
        stageFoulEightOnBreak(runtime);
        session.syncRulesDecisionAuthority();
        const auto parent = session.snapshot();
        expect(parent.rulesDecisionProductActive &&
            parent.rulesDecision.visibleActionIds.size() == 3 &&
            parent.rulesDecision.visibleActionIds.front() ==
                rules::kEightOnBreakSpotBallInHandActionId,
            "typed 06F foul parent exposes exact three-choice order");
        const auto parentRevision = runtime.rulesAuthorityRevision();
        const ui::RulesDecisionUiHitRect action{
            "action:foul-spot", rules::kEightOnBreakSpotBallInHandActionId,
            {40.0f, 40.0f, 260.0f, 52.0f}, true, false};
        session.setRulesDecisionHitRects({action});
        expect(clickRulesRect(session, action).ok &&
            runtime.rulesAuthorityRevision() == parentRevision,
            "06F foul action click only arms");
        const ui::RulesDecisionUiHitRect confirm{
            "confirm:foul-spot", rules::kEightOnBreakSpotBallInHandActionId,
            {40.0f, 104.0f, 260.0f, 52.0f}, true, true};
        session.setRulesDecisionHitRects({confirm});
        expect(clickRulesRect(session, confirm).ok &&
            runtime.rulesAuthorityRevision() == parentRevision + 1 &&
            runtime.ballInHandPlacementOffer().has_value() &&
            session.snapshot().rulesDecisionProductActive &&
            session.snapshot().rulesDecision.offer.flowId ==
                rules::kBehindHeadStringBallInHandFlowId,
            "06F foul confirm grants and exposes its own BehindHeadString child");

        const rules::BallInHandPlacementPosition legal{-20.0, -90.0};
        expect(session.previewProductAuthorizedBallInHandPlacement(legal).ok,
            "06F child accepts a legal validation-only preview");
        const auto childRevision = runtime.rulesAuthorityRevision();
        expect(session.confirmProductAuthorizedBallInHandPlacement(
                rules::kConfirmBehindHeadStringPlacementActionId, legal,
                "product-06f-placement").ok &&
            runtime.rulesAuthorityRevision() == childRevision + 1 &&
            runtime.matchState().ballInHand &&
            runtime.ballInHandPlacementOffer().has_value() &&
            !session.snapshot().rulesDecisionProductActive,
            "06F child confirm commits once and closes only its completed modal");
        cleanup(path);
    }
}

void testProduct06FPhysicalCollectorPreflight()
{
    const auto path = settingsPath("product-06f-physical-preflight");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionProductCapabilities.eightOnBreakChoice = true;
    ModernProductSession session(runtime, ai, options);
    expect(session.shell().startSinglePlayer(0x06fULL).ok,
        "06F physical preflight starts a product match");
    ai::PlayerConfiguration localTwoPlayer;
    expect(ai.configure(runtime, localTwoPlayer).ok,
        "06F physical preflight makes the opposing actor local");

    const float halfWidth = runtime.physicsProfile().tableBoundary.
        playfieldWidthCm * 0.5f;
    const float radius = runtime.physicsProfile().ball.radiusCm;
    auto cue = runtime.state().balls[0];
    cue.position = {-halfWidth + radius, kTableHeight + radius, 0.0f};
    cue.velocity = {};
    cue.angularVelocity = {};
    cue.speed = 0.0f;
    cue.motionState = BallMotionState::Stationary;
    cue.pocketed = false;
    cue.outOfPlay = false;
    cue.pocketInteraction = {};
    auto eight = runtime.state().balls[8];
    eight.position = {halfWidth - radius, kTableHeight + radius, 0.0f};
    eight.velocity = {};
    eight.angularVelocity = {};
    eight.speed = 0.0f;
    eight.motionState = BallMotionState::Stationary;
    eight.pocketed = false;
    eight.outOfPlay = false;
    eight.pocketInteraction = {};
    expect(runtime.setBall(0, cue).ok && runtime.setBall(8, eight).ok,
        "fixture places live cue/eight balls at opposite side-pocket mouths");

    CueImpactInput impact;
    impact.cueBallIndex = 0;
    impact.cueSpeedCmS = 400.0;
    impact.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
    impact.direction = {{-1.0, 0.0, 0.0}};
    impact.chalkState = "CHALKED";
    expect(runtime.applyCueImpact(impact).ok &&
        runtime.shotFactCollectorActive(),
        "real cue impulse starts the 06F collector");
    int ticks = 0;
    while (runtime.state().ballsMoving && ticks < 480) {
        expect(runtime.step(1).ok,
            "06F physical preflight advances public physics");
        session.consumeAuthoritySteps();
        ++ticks;
    }
    session.syncRulesDecisionAuthority();
    const auto offer = runtime.eightOnBreakDecisionOffer();
    const bool eightCaptured = runtime.hasLastShotFacts() &&
        std::find(runtime.lastShotFacts().pocketedBallsInOrder.begin(),
            runtime.lastShotFacts().pocketedBallsInOrder.end(), 8) !=
            runtime.lastShotFacts().pocketedBallsInOrder.end();
    expect(!runtime.state().ballsMoving && ticks < 480 &&
        runtime.rulesShotCompletionError().empty() && eightCaptured &&
        runtime.lastShotFacts().cueBallPocketed && offer &&
        offer->branch == rules::EightOnBreakBranch::FoulBreak &&
        session.snapshot().rulesDecisionProductActive,
        "real cue/eight capture telemetry produces the product 06F foul offer");
    cleanup(path);
}

void testProduct06GSignedNativeMiscueAndTypedRerack()
{
    const auto path = settingsPath("product-06g-native-miscue");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionProductCapabilities.noRackContactMiscueChoice = true;
    ModernProductSession session(runtime, ai, options);
    expect(session.shell().startSinglePlayer(607).ok,
        "06G native fixture starts a product Match");
    ai::PlayerConfiguration localPlayers;
    expect(ai.configure(runtime, localPlayers).ok,
        "06G component fixture makes both authority actors local");
    expect(runtime.replaceStateForScenario(runtime.state(),
            interactiveChinesePoolPhysicsProfile()).ok,
        "06G product fixture selects shipping physics");

    Product06GAuthenticator authenticator;
    runtime.configureUnintentionalBreakMiscueDeclarationAuthority(
        &authenticator);
    const std::string proposalPayload =
        "{\"actor_player_id\":0,\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
    const std::string finalPayload =
        "{\"actor_player_id\":0,\"proposal_decision_id\":\"product.unintentional_break_miscue.1\",\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
    auto proposal = product06GRequest(runtime,
        rules::RefereeAuthorityOperation::Propose,
        "product.06g.proposal", proposalPayload);
    expect(runtime.submitUnintentionalBreakMiscueDeclarationProposal(
               proposal).ok,
        "06G product fixture accepts a signed referee proposal");
    auto final = product06GRequest(runtime,
        rules::RefereeAuthorityOperation::FinalDeclare,
        "product.06g.final", finalPayload);
    expect(runtime.submitUnintentionalBreakMiscueDeclarationFinal(final).ok,
        "06G product fixture accepts the matching signed final declaration");

    CueImpactInput impact;
    impact.cueBallIndex = 0;
    impact.cueSpeedCmS = 100.0;
    impact.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
    impact.direction = {{0.0, 0.0, 1.0}};
    impact.tipOffsetRadius = {{0.81, 0.0}};
    impact.tipOffsetCm = {{
        0.81 * runtime.physicsProfile().ball.radiusCm, 0.0}};
    impact.chalkState = "CHALKED";
    expect(runtime.applyCueImpact(impact).ok &&
        runtime.cueContactResult().regime == CueContactRegime::Miscue &&
        !runtime.cueContactResult().applied,
        "06G product fixture executes a real modeled cue miscue");
    expect(runtime.step(1).ok &&
        runtime.lastShotFacts().breakEvidenceOrigin ==
            rules::BreakEvidenceOrigin::AuthoritativeShotFactCollector &&
        runtime.lastShotFacts().breakEvidenceComplete,
        "06G product fixture reaches the authoritative collector before product UI");

    session.syncRulesDecisionAuthority();
    const auto open = session.snapshot();
    expect(open.rulesDecisionNoRackContactMiscueProductEnabled &&
        open.rulesDecisionProductActive &&
        !open.rulesDecisionUiProductExposed &&
        open.rulesDecision.offer.branchId ==
            "no_rack_contact_miscue_6g" &&
        open.rulesDecision.visibleActionIds == std::vector<std::string>{
            rules::kBreakFoulRerackOpponentActionId,
            rules::kBreakFoulRerackOriginalActionId},
        "06G product capability exposes exactly two rerack actions after signed native evidence");
    const auto revision = runtime.rulesAuthorityRevision();
    const ui::RulesDecisionUiHitRect action{
        "action:06g-rerack-opponent",
        rules::kBreakFoulRerackOpponentActionId,
        {40.0f, 40.0f, 280.0f, 52.0f}, true, false};
    session.setRulesDecisionHitRects({action});
    expect(clickRulesRect(session, action).ok &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.breakFoulDecisionPending() &&
        session.snapshot().rulesDecision.armedActionId ==
            rules::kBreakFoulRerackOpponentActionId,
        "06G product action click only arms with zero authority mutation");
    const ui::RulesDecisionUiHitRect confirm{
        "confirm:06g-rerack-opponent",
        rules::kBreakFoulRerackOpponentActionId,
        {40.0f, 104.0f, 280.0f, 52.0f}, true, true};
    session.setRulesDecisionHitRects({confirm});
    expect(clickRulesRect(session, confirm).ok &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        !runtime.breakFoulDecisionPending() &&
        runtime.matchState().phase == rules::MatchPhase::Break &&
        runtime.matchState().currentPlayer == 1 &&
        !runtime.matchState().ballInHand &&
        runtime.rulesAuthorityActions().back().actionId ==
            rules::kBreakFoulRerackOpponentActionId,
        "06G explicit confirm verifies an exact one-revision rerack ACK");
    const std::string committedKey = "product.rules." +
        std::to_string(revision) + ".1";
    const auto retry = session.submitProductAuthorizedNoRackContactMiscueChoice(
        rules::kBreakFoulRerackOpponentActionId, committedKey);
    const auto conflict =
        session.submitProductAuthorizedNoRackContactMiscueChoice(
            rules::kBreakFoulRerackOriginalActionId,
            committedKey);
    expect(retry.ok && !conflict.ok && conflict.errorCode ==
            "rules_decision_idempotency_conflict" &&
        runtime.rulesAuthorityRevision() == revision + 1,
        "06G product rerack retry is idempotent and changed reuse has zero mutation");
    cleanup(path);

    const auto disabledPath = settingsPath("product-06g-disabled");
    GameRuntime disabledRuntime;
    ai::AiPlayerController disabledAi;
    ModernProductSessionOptions disabledOptions;
    disabledOptions.settingsPath = disabledPath.string();
    ModernProductSession disabledSession(
        disabledRuntime, disabledAi, disabledOptions);
    expect(disabledSession.shell().startSinglePlayer(608).ok,
        "06G disabled-capability fixture starts Match");
    const auto disabled =
        disabledSession.submitProductAuthorizedNoRackContactMiscueChoice(
            rules::kBreakFoulRerackOpponentActionId, "disabled.06g");
    expect(!disabled.ok && disabled.errorCode ==
            "rules_product_no_rack_miscue_capability_disabled",
        "06G sibling capability remains explicitly fail-closed by default");
    cleanup(disabledPath);
}

void testProductAuthorizedBehindHeadStringAllBehindRespot()
{
    const auto path = settingsPath("product-head-string-all-behind");
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    options.rulesDecisionLocalPlayerId = 1;
    ModernProductSession session(runtime, ai, options);
    expect(session.shell().startSinglePlayer(89).ok,
        "all-behind product fixture starts Match");
    ai::PlayerConfiguration localAllBehindPlayers;
    localAllBehindPlayers.enabled = false;
    expect(ai.configure(runtime, localAllBehindPlayers).ok,
        "all-behind component fixture makes both authority actors local");
    const double head = -runtime.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    for (int ball = 1; ball < kBallCount; ++ball) {
        runtime.mutableState().balls[ball].pocketed = ball == 8;
        runtime.mutableState().balls[ball].position.z =
            static_cast<float>(head - 30.0 - ball);
    }
    runtime.mutableState().balls[2].position.z = static_cast<float>(head - 1.0);
    stageBehindHeadStringPlacement(runtime);
    const auto offer = *runtime.ballInHandPlacementOffer();
    const auto foundation = session.submitProductAuthorizedBehindHeadStringRespot(
        rules::kFoundationRespotActionId, -1, "foundation-forbidden");
    const auto wrongActorPath = settingsPath("product-head-string-wrong-actor");
    expect(!foundation.ok && foundation.errorCode ==
            "rules_product_head_string_respot_action_not_authorized" &&
        !session.snapshot().rulesDecisionUiProductExposed,
        "foundation action and global UI remain outside narrow all-behind seam");
    const auto committed = session.submitProductAuthorizedBehindHeadStringRespot(
        rules::kRequestNearestLegalRespotActionId, -1, "all-behind-product", "{}");
    expect(committed.ok && runtime.ballInHandPlacementOffer().has_value() &&
        runtime.ballInHandPlacementOffer()->decisionId != offer.decisionId &&
        runtime.ballInHandPlacementOffer()->actorPlayerId == 1 &&
        !session.snapshot().rulesDecisionUiProductExposed,
        "narrow product request commits unique closest and ACK-syncs replacement child");
    const auto retry = session.submitProductAuthorizedBehindHeadStringRespot(
        rules::kRequestNearestLegalRespotActionId, -1, "all-behind-product", "{}");
    const auto conflict = session.submitProductAuthorizedBehindHeadStringRespot(
        rules::kRequestNearestLegalRespotActionId, 2, "all-behind-product",
        "{\"selected_ball_id\":2}");
    expect(retry.ok && !conflict.ok && conflict.errorCode ==
            "rules_decision_idempotency_conflict",
        "product all-behind exact retry is stable and payload/selection conflict fails closed");
    cleanup(path);
    cleanup(wrongActorPath);
}

void testTypedGroupChoiceAndFullTablePlacementProductChains()
{
    const auto siblingPath = settingsPath(
        "product-group-choice-sibling-capability-disabled");
    GameRuntime siblingRuntime;
    ai::AiPlayerController siblingAi;
    ModernProductSessionOptions siblingOptions;
    siblingOptions.settingsPath = siblingPath.string();
    siblingOptions.rulesDecisionProductCapabilities.fullTableBallInHand = true;
    ModernProductSession siblingSession(
        siblingRuntime, siblingAi, siblingOptions);
    expect(siblingSession.shell().startSinglePlayer(609).ok,
        "sibling-capability fixture starts Match");
    ai::PlayerConfiguration siblingLocalPlayers;
    siblingLocalPlayers.enabled = false;
    expect(siblingAi.configure(siblingRuntime, siblingLocalPlayers).ok,
        "sibling-capability fixture explicitly models local actors");
    stageGroupChoice(siblingRuntime);
    siblingSession.syncRulesDecisionAuthority();
    const std::uint64_t siblingRevision =
        siblingRuntime.rulesAuthorityRevision();
    const ui::RulesDecisionUiHitRect forgedGroupAction{
        "action:forged-group-solids", rules::kGroupChoiceSolidsActionId,
        {40.0f, 40.0f, 260.0f, 52.0f}, true, false};
    siblingSession.setRulesDecisionHitRects({forgedGroupAction});
    (void)clickRulesRect(siblingSession, forgedGroupAction);
    PlatformInputEvent forgedKeyboardConfirm;
    forgedKeyboardConfirm.kind = PlatformInputKind::KeyDown;
    forgedKeyboardConfirm.control = "Enter";
    (void)siblingSession.handlePlatformInput(forgedKeyboardConfirm);
    const auto siblingSnapshot = siblingSession.snapshot();
    expect(!siblingSnapshot.rulesDecisionGroupChoiceProductEnabled &&
        siblingSnapshot.rulesDecisionFullTableBallInHandProductEnabled &&
        siblingSnapshot.rulesDecisionProductActive &&
        siblingSnapshot.rulesDecision.visibleActionIds.empty() &&
        siblingSnapshot.rulesDecision.armedActionId.empty() &&
        siblingRuntime.rulesAuthorityRevision() == siblingRevision &&
        siblingRuntime.groupChoiceDecisionOffer().has_value(),
        "a sibling FullTable capability cannot expose or commit GroupChoice "
        "through forged pointer/key input: group_cap=" +
            std::to_string(siblingSnapshot.
                rulesDecisionGroupChoiceProductEnabled) +
            " full_cap=" + std::to_string(siblingSnapshot.
                rulesDecisionFullTableBallInHandProductEnabled) +
            " unsupported_active=" + std::to_string(
                siblingSnapshot.rulesDecisionProductActive) +
            " revision=" + std::to_string(
                siblingRuntime.rulesAuthorityRevision()) +
            " expected=" + std::to_string(siblingRevision) +
            " offer=" + std::to_string(siblingRuntime.
                groupChoiceDecisionOffer().has_value()));
    cleanup(siblingPath);

    const auto groupPath = settingsPath("product-group-choice-typed-ui");
    GameRuntime groupRuntime;
    ai::AiPlayerController groupAi;
    ModernProductSessionOptions groupOptions;
    groupOptions.settingsPath = groupPath.string();
    groupOptions.rulesDecisionProductCapabilities.groupChoice = true;
    ModernProductSession groupSession(groupRuntime, groupAi, groupOptions);
    expect(groupSession.shell().startSinglePlayer(610).ok,
        "typed group-choice fixture starts Match");
    ai::PlayerConfiguration localPlayers;
    localPlayers.enabled = false;
    expect(groupAi.configure(groupRuntime, localPlayers).ok,
        "typed group-choice fixture explicitly models local two-player actors");
    stageGroupChoice(groupRuntime);
    groupSession.syncRulesDecisionAuthority();
    const auto groupOffer = *groupRuntime.groupChoiceDecisionOffer();
    const auto groupOpen = groupSession.snapshot();
    expect(groupOpen.rulesDecisionGroupChoiceProductEnabled &&
        groupOpen.rulesDecisionProductActive &&
        !groupOpen.rulesDecisionUiImplementationEnabled &&
        groupOpen.rulesDecision.offer.flowId ==
            rules::kGroupChoiceAuthorityFlowId &&
        groupOpen.rulesDecision.visibleActionIds.size() == 2,
        "typed shipping capability exposes only the current local group offer");
    const std::uint64_t groupRevision =
        groupRuntime.rulesAuthorityRevision();
    const ui::RulesDecisionUiHitRect stripes{
        "action:group-stripes", rules::kGroupChoiceStripesActionId,
        {40.0f, 40.0f, 260.0f, 52.0f}, true, false};
    groupSession.setRulesDecisionHitRects({stripes});
    expect(clickRulesRect(groupSession, stripes).ok &&
        groupRuntime.rulesAuthorityRevision() == groupRevision &&
        groupSession.snapshot().rulesDecision.armedActionId ==
            rules::kGroupChoiceStripesActionId,
        "group action pointer click only arms with zero authority mutation");
    const ui::RulesDecisionUiHitRect groupConfirm{
        "confirm:group-stripes", rules::kGroupChoiceStripesActionId,
        {40.0f, 104.0f, 260.0f, 52.0f}, true, true};
    groupSession.setRulesDecisionHitRects({groupConfirm});
    expect(clickRulesRect(groupSession, groupConfirm).ok &&
        groupRuntime.rulesAuthorityRevision() == groupRevision + 1 &&
        !groupRuntime.groupChoiceDecisionOffer().has_value() &&
        groupRuntime.matchState().groups[groupOffer.actorPlayerId] ==
            rules::BallGroup::Stripes &&
        groupRuntime.rulesAuthorityActions().back().actionId ==
            rules::kGroupChoiceStripesActionId,
        "explicit group confirm commits one exact current-authority ACK");
    cleanup(groupPath);

    const auto placementPath = settingsPath("product-full-table-typed-ui");
    GameRuntime placementRuntime;
    ai::AiPlayerController placementAi;
    ModernProductSessionOptions placementOptions;
    placementOptions.settingsPath = placementPath.string();
    placementOptions.rulesDecisionProductCapabilities.
        fullTableBallInHand = true;
    ModernProductSession placementSession(
        placementRuntime, placementAi, placementOptions);
    expect(placementSession.shell().startSinglePlayer(611).ok,
        "typed full-table placement fixture starts Match");
    placementRuntime.mutableState().camera.panX = 13.0f;
    placementRuntime.mutableState().camera.panY = 0.0f;
    placementRuntime.mutableState().camera.panZ = -7.0f;
    stageFullTablePlacement(placementRuntime);
    const std::uint64_t placementFramingRevision =
        placementRuntime.rulesAuthorityRevision();
    const std::string placementFramingRulesHash =
        placementRuntime.rulesAuthorityStateHash();
    const std::string placementFramingGameHash =
        gameStateSha256(placementRuntime.state());
    placementSession.setShellViewport({1280, 720, 1280, 720, 96.0f});
    placementSession.syncRulesDecisionAuthority();
    for (int i = 0; i < 12; ++i) {
        expect(placementRuntime.step(1).ok,
            "FullTable presentation tick remains valid while balls are still");
        placementSession.syncRulesDecisionAuthority();
    }
    const camera::Frame placementFrame =
        placementSession.presentationCameraFrame();
    expect(placementFrame.state == camera::SemanticState::AimOverhead &&
        close(placementFrame.pose.target.x, 0.0) &&
        close(placementFrame.pose.target.z, 0.0) &&
        close(placementRuntime.state().camera.panX, 20.0) &&
        close(placementRuntime.state().camera.panY, 0.0) &&
        close(placementRuntime.state().camera.panZ, 0.0) &&
        placementRuntime.rulesAuthorityRevision() ==
            placementFramingRevision &&
        placementRuntime.rulesAuthorityStateHash() ==
            placementFramingRulesHash &&
        gameStateSha256(placementRuntime.state()) ==
            placementFramingGameHash,
        "FullTable child alone canonicalizes its persistent presentation pan "
        "beside the modal across camera ticks without mutating authority or "
        "canonical gameplay state");
    CameraPresentation placementCamera;
    placementCamera.eye = {placementFrame.pose.eye.x,
        placementFrame.pose.eye.y, placementFrame.pose.eye.z};
    placementCamera.target = {placementFrame.pose.target.x,
        placementFrame.pose.target.y, placementFrame.pose.target.z};
    placementCamera.fieldOfViewDegrees =
        placementFrame.pose.fieldOfViewDegrees;
    placementCamera.projectionPan = {
        placementRuntime.state().camera.panX,
        placementRuntime.state().camera.panY,
        placementRuntime.state().camera.panZ};
    placementSession.setRulesDecisionProjectionCamera(placementCamera);
    const auto placementOpen = placementSession.snapshot();
    expect(placementOpen.rulesDecisionFullTableBallInHandProductEnabled &&
        placementOpen.rulesDecisionProductActive &&
        !placementOpen.rulesDecisionUiImplementationEnabled &&
        placementOpen.rulesDecision.offer.flowId ==
            rules::kFullTableBallInHandFlowId &&
        placementOpen.rulesDecision.visibleActionIds.size() == 1,
        "typed shipping capability exposes one exact full-table placement action");
    const std::uint64_t placementRevision =
        placementRuntime.rulesAuthorityRevision();
    placementSession.setRulesDecisionExclusionRects(
        {{0.0, 0.0, 320.0, 180.0}});
    (void)placementSession.handlePlatformInput(pointerEvent(
        PlatformInputKind::PointerMove, 100.0, 100.0));
    expect(!placementSession.snapshot().rulesDecision.
            placementPreviewVisible &&
        placementRuntime.rulesAuthorityRevision() == placementRevision,
        "modal pixels cannot leak into a full-table placement preview");
    placementSession.setRulesDecisionExclusionRects({});
    const auto legalPointer = placementPointer(
        placementCamera, placementRuntime, true);
    expect(legalPointer.has_value(),
        "rendered camera and nonzero projection pan find a legal table point");
    (void)placementSession.handlePlatformInput(pointerEvent(
        PlatformInputKind::PointerMove, legalPointer->first,
        legalPointer->second));
    const auto preview = placementSession.snapshot();
    expect(preview.rulesDecision.placementPreviewVisible &&
        preview.rulesDecision.placementPreviewLegal &&
        placementRuntime.rulesAuthorityRevision() == placementRevision,
        "full-table pointer preview is local and authority mutation-free");
    const rules::BallInHandPlacementPosition previewPosition =
        preview.rulesDecision.placementPreview;
    const ui::RulesDecisionUiHitRect place{
        "action:full-table-place", rules::kConfirmFullTablePlacementActionId,
        {40.0f, 40.0f, 300.0f, 52.0f}, true, false};
    placementSession.setRulesDecisionHitRects({place});
    expect(clickRulesRect(placementSession, place).ok &&
        placementRuntime.rulesAuthorityRevision() == placementRevision &&
        placementSession.snapshot().rulesDecision.armedActionId ==
            rules::kConfirmFullTablePlacementActionId,
        "full-table action click arms without placing the cue ball");
    const ui::RulesDecisionUiHitRect placeConfirm{
        "confirm:full-table-place", rules::kConfirmFullTablePlacementActionId,
        {40.0f, 104.0f, 300.0f, 52.0f}, true, true};
    placementSession.setRulesDecisionHitRects({placeConfirm});
    expect(clickRulesRect(placementSession, placeConfirm).ok &&
        placementRuntime.rulesAuthorityRevision() == placementRevision + 1 &&
        close(placementRuntime.state().balls[0].position.x,
            previewPosition.worldX) &&
        close(placementRuntime.state().balls[0].position.z,
            previewPosition.worldZ) &&
        placementRuntime.rulesAuthorityActions().back().actionId ==
            rules::kConfirmFullTablePlacementActionId &&
        placementSession.snapshot().rulesDecision.state ==
            rules::ui::DecisionUiState::HiddenNoOffer &&
        close(placementRuntime.state().camera.panX, 13.0) &&
        close(placementRuntime.state().camera.panY, 0.0) &&
        close(placementRuntime.state().camera.panZ, -7.0),
        "explicit full-table confirm verifies one ACK, hides retained "
        "replacement entitlement, and restores the exact same-session user pan");
    cleanup(placementPath);

    const std::array<std::array<float, 3>, 3> initialPlacementPans{{
        {{0.0f, 0.0f, 0.0f}},
        {{13.0f, 0.0f, -7.0f}},
        {{-13.0f, 0.0f, 7.0f}}}};
    for (std::size_t variant = 0;
            variant < initialPlacementPans.size(); ++variant) {
        const std::string freshRackSuffix =
            "product-full-table-camera-new-rack-reset-" +
            std::to_string(variant);
        const auto freshRackPath = settingsPath(freshRackSuffix.c_str());
        GameRuntime freshRackRuntime;
        ai::AiPlayerController freshRackAi;
        ModernProductSessionOptions freshRackOptions;
        freshRackOptions.settingsPath = freshRackPath.string();
        freshRackOptions.rulesDecisionProductCapabilities.
            fullTableBallInHand = true;
        ModernProductSession freshRackSession(
            freshRackRuntime, freshRackAi, freshRackOptions);
        expect(freshRackSession.shell().startSinglePlayer(612 + variant).ok,
            "fresh-rack camera fixture starts first Match");
        freshRackRuntime.mutableState().camera.panX =
            initialPlacementPans[variant][0];
        freshRackRuntime.mutableState().camera.panY =
            initialPlacementPans[variant][1];
        freshRackRuntime.mutableState().camera.panZ =
            initialPlacementPans[variant][2];
        stageFullTablePlacement(freshRackRuntime);
        freshRackSession.syncRulesDecisionAuthority();
        const float framedDistance =
            freshRackSession.presentationCameraFrame().pose.distanceCm;
        expect(close(freshRackRuntime.state().camera.panX, 20.0) &&
            close(freshRackRuntime.state().camera.panY, 0.0) &&
            close(freshRackRuntime.state().camera.panZ, 0.0),
            "each supported incoming pan receives the same canonical "
            "FullTable composition");

        PlatformInputEvent zoomPlacementCamera;
        zoomPlacementCamera.kind = PlatformInputKind::Scroll;
        zoomPlacementCamera.scrollY = 3.0;
        const auto zoomedPlacement =
            freshRackSession.handlePlatformInput(zoomPlacementCamera);
        const float zoomedDistance =
            freshRackSession.presentationCameraFrame().pose.distanceCm;
        for (int tick = 0; tick < 12; ++tick) {
            expect(freshRackRuntime.step(1).ok,
                "variant FullTable presentation tick remains valid");
            freshRackSession.syncRulesDecisionAuthority();
        }
        expect(zoomedPlacement.ok && zoomedPlacement.handled &&
            zoomedDistance <= framedDistance - 29.0f &&
            close(freshRackSession.presentationCameraFrame().pose.distanceCm,
                zoomedDistance) &&
            close(freshRackRuntime.state().camera.panX, 20.0),
            "same-child user zoom remains live and canonical pan survives "
            "subsequent presentation ticks");

        const std::uint64_t firstGeneration =
            freshRackRuntime.eventJournalGeneration();
        expect(freshRackSession.shell().startSinglePlayer(620 + variant).ok &&
            freshRackRuntime.eventJournalGeneration() > firstGeneration,
            "fresh-rack camera fixture starts a distinct replacement Match");
        freshRackRuntime.mutableState().camera.panX = -9.0f;
        freshRackRuntime.mutableState().camera.panY = 2.0f;
        freshRackRuntime.mutableState().camera.panZ = 5.0f;
        const std::string replacementRulesHash =
            freshRackRuntime.rulesAuthorityStateHash();
        freshRackSession.syncRulesDecisionAuthority();
        expect(close(freshRackRuntime.state().camera.panX, -9.0) &&
            close(freshRackRuntime.state().camera.panY, 2.0) &&
            close(freshRackRuntime.state().camera.panZ, 5.0) &&
            freshRackRuntime.rulesAuthorityStateHash() == replacementRulesHash,
            "cross-session cleanup discards the prior saved view without "
            "writing into the replacement generation's explicit camera");

        stageFullTablePlacement(freshRackRuntime);
        freshRackSession.syncRulesDecisionAuthority();
        expect(close(freshRackSession.presentationCameraFrame().pose.target.x,
                0.0) &&
            close(freshRackRuntime.state().camera.panX, 20.0) &&
            close(freshRackRuntime.state().camera.panY, 0.0) &&
            close(freshRackRuntime.state().camera.panZ, 0.0) &&
            close(freshRackSession.presentationCameraFrame().pose.distanceCm,
                framedDistance),
            "new rack generation never restores the old user view and applies "
            "one fresh FullTable composition");
        cleanup(freshRackPath);
    }
}

}  // namespace

int main()
{
    try {
        testDefaultProductSessionCannotExposeOrSubmit();
        testInternalCapabilityUsesExactRuntimeTransaction();
        testProductAuthorizedEightOnBreakRespotParentSeam();
        testProductAuthorizedRespotRejectsWrongLocalActor();
        testProductAuthorizedEightOnBreakExactRerackActions();
        testProductAuthorizedEightOnBreakFoulSpotParentChildAtomicity();
        testProductAuthorizedEightOnBreakDriftResetAndSiblingFailures();
        testProductAuthorizedGroupChoiceParentSeam();
        testProductAuthorizedGroupChoiceRejectsActorAndStateDrift();
        testProductAuthorizedFullTablePlacementPreviewAndConfirm();
        testProductAuthorizedPlacementRejectsActorStateAndScopeDrift();
        testProductAuthorizedBreakFoulParentAndBehindHeadStringChild();
        testProductAuthorizedBreakFoulRerackBranches();
        testProductAuthorizedBreakFoulRejectsActorSiblingResetAndDrift();
        testProductIllegalBreakTypedCapabilityAndAuthorityAck();
        testProduct06EParentPlacementAndTieRespotPointerChain();
        testProduct06FEightOnBreakTypedParentAndChild();
        testProduct06FPhysicalCollectorPreflight();
        testProduct06GSignedNativeMiscueAndTypedRerack();
        testProductAuthorizedBehindHeadStringAllBehindRespot();
        testTypedGroupChoiceAndFullTablePlacementProductChains();
        std::cout << "Rules decision ModernProductSession integration passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
