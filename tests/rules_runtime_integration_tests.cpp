#include "automation_controller.h"
#include "deterministic_hash.h"
#include "render_state.h"
#include "replay/replay_recorder.h"
#include "rules/break_head_string_offense.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <tuple>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

class StalemateTestAuthenticator final :
    public billiardgl::rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const billiardgl::rules::RefereeAuthenticationInput& input)
        const override
    {
        return input.credentialId == "credential" &&
            input.keyVersion == "v1" &&
            input.authenticationProof == billiardgl::sha256Hex(
                "secret|" + input.authenticatedBytes);
    }
};

class StalemateTestGuard final :
    public billiardgl::rules::RefereeAuthorityCommitGuard {
public:
    bool allowCommit() const override { return allow; }
    bool allow = true;
};

void signStalemateRequest(
    billiardgl::rules::RefereeAuthorityRequest& request)
{
    request.authenticationProof = billiardgl::sha256Hex("secret|" +
        billiardgl::rules::canonicalRefereeAuthenticatedBytes(request));
}

billiardgl::rules::RefereeAuthorityRequest stalemateRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& kind, const std::string& subject,
    const std::string& nonce, const std::string& idempotency,
    const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId = kRefereeAuthorityProtocolId;
    request.schemaVersion = 1;
    request.operation = operation;
    request.declarationKind = kind;
    request.decisionId = "runtime.stalemate.1";
    request.rackIdentity = runtime.stalemateState().rackIdentity;
    request.expectedAuthorityRevision =
        runtime.stalemateProtocolState().authorityRevision;
    request.expectedAuthorityStateHash =
        runtime.stalemateProtocolState().authorityStateHash;
    request.authorityRole = subject == "player.0" ?
        RefereeAuthorityRole::Player0 : subject == "player.1" ?
        RefereeAuthorityRole::Player1 : RefereeAuthorityRole::Referee;
    request.authoritySubjectId = subject;
    request.credentialId = "credential";
    request.keyVersion = "v1";
    request.authenticationNonce = nonce;
    request.expiresAtAuthorityTick = 1000;
    request.idempotencyKey = idempotency;
    request.payloadCanonical = payload;
    signStalemateRequest(request);
    return request;
}

billiardgl::rules::RefereeAuthorityRequest groupConfusionRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& subject, const std::string& nonce,
    const std::string& key, const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId = kRefereeAuthorityProtocolId;
    request.schemaVersion = 1;
    request.operation = operation;
    request.declarationKind = kGroupConfusionDeclarationKind;
    request.decisionId = "runtime.confusion.1";
    request.rackIdentity =
        runtime.groupConfusionState().window.rackIdentity;
    request.expectedAuthorityRevision =
        runtime.groupConfusionProtocolState().authorityRevision;
    request.expectedAuthorityStateHash =
        runtime.groupConfusionProtocolState().authorityStateHash;
    request.authorityRole = subject == "player.0" ?
        RefereeAuthorityRole::Player0 : subject == "player.1" ?
        RefereeAuthorityRole::Player1 : RefereeAuthorityRole::Referee;
    request.authoritySubjectId = subject;
    request.credentialId = "credential";
    request.keyVersion = "v1";
    request.authenticationNonce = nonce;
    request.expiresAtAuthorityTick = 1000;
    request.idempotencyKey = key;
    request.payloadCanonical = payload;
    signStalemateRequest(request);
    return request;
}

billiardgl::rules::RefereeAuthorityRequest frozenDeclarationRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& nonce, const std::string& key,
    const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;request.schemaVersion=1;
    request.operation=operation;
    request.declarationKind=kFrozenCueObjectDeclarationKind;
    request.decisionId="runtime.frozen.1";
    request.rackIdentity=runtime.frozenBallDeclarationState().rackIdentity;
    request.expectedAuthorityRevision=
        runtime.frozenBallDeclarationProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=
        runtime.frozenBallDeclarationProtocolState().authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;
    request.authoritySubjectId="referee";request.credentialId="credential";
    request.keyVersion="v1";request.authenticationNonce=nonce;
    request.expiresAtAuthorityTick=1000;request.idempotencyKey=key;
    request.payloadCanonical=payload;signStalemateRequest(request);return request;
}

billiardgl::rules::RefereeAuthorityRequest frozenCushionRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& nonce, const std::string& key,
    const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;request.schemaVersion=1;
    request.operation=operation;
    request.declarationKind=kFrozenCushionDeclarationKind;
    request.decisionId="runtime.cushion.1";
    request.rackIdentity=runtime.frozenCushionDeclarationState().rackIdentity;
    request.expectedAuthorityRevision=
        runtime.frozenCushionDeclarationProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=
        runtime.frozenCushionDeclarationProtocolState().authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;
    request.authoritySubjectId="referee";request.credentialId="credential";
    request.keyVersion="v1";request.authenticationNonce=nonce;
    request.expiresAtAuthorityTick=1000;request.idempotencyKey=key;
    request.payloadCanonical=payload;signStalemateRequest(request);return request;
}

billiardgl::rules::RefereeAuthorityRequest closeCaseRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& nonce,const std::string& key,const std::string& payload)
{
    using namespace billiardgl::rules;RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;request.schemaVersion=1;
    request.operation=operation;request.declarationKind=kDoubleHitCloseCaseDeclarationKind;
    request.decisionId="runtime.close.1";
    request.rackIdentity=runtime.doubleHitCloseCaseState().rackIdentity;
    request.expectedAuthorityRevision=runtime.doubleHitCloseCaseProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=runtime.doubleHitCloseCaseProtocolState().authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;request.authoritySubjectId="referee";
    request.credentialId="credential";request.keyVersion="v1";
    request.authenticationNonce=nonce;request.expiresAtAuthorityTick=1000;
    request.idempotencyKey=key;request.payloadCanonical=payload;
    signStalemateRequest(request);return request;
}

billiardgl::rules::RefereeAuthorityRequest pushReviewRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& nonce,const std::string& key,const std::string& payload)
{
    using namespace billiardgl::rules;RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;request.schemaVersion=1;
    request.operation=operation;request.declarationKind=kPushReviewDeclarationKind;
    request.decisionId="runtime.push.1";
    request.rackIdentity=runtime.pushReviewState().rackIdentity;
    request.expectedAuthorityRevision=runtime.pushReviewProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=runtime.pushReviewProtocolState().authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;request.authoritySubjectId="referee";
    request.credentialId="credential";request.keyVersion="v1";
    request.authenticationNonce=nonce;request.expiresAtAuthorityTick=1000;
    request.idempotencyKey=key;request.payloadCanonical=payload;
    signStalemateRequest(request);return request;
}

billiardgl::rules::RefereeAuthorityRequest unintentionalBreakMiscueRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& key, const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId = kRefereeAuthorityProtocolId;
    request.schemaVersion = 1;
    request.operation = operation;
    request.declarationKind = kUnintentionalBreakMiscueDeclarationKind;
    request.decisionId = "runtime.unintentional_break_miscue.1";
    request.rackIdentity =
        runtime.unintentionalBreakMiscueDeclarationState().rackIdentity;
    request.expectedAuthorityRevision =
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityRevision;
    request.expectedAuthorityStateHash =
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityStateHash;
    request.authorityRole = RefereeAuthorityRole::Referee;
    request.authoritySubjectId = "referee.06g";
    request.credentialId = "credential";
    request.keyVersion = "v1";
    request.authenticationNonce = "nonce." + key;
    request.expiresAtAuthorityTick = 1000;
    request.idempotencyKey = key;
    request.payloadCanonical = payload;
    signStalemateRequest(request);
    return request;
}

billiardgl::rules::RefereeAuthorityRequest nonphysicalFoulRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& nonce,const std::string& key,const std::string& payload,
    const std::string& decision="runtime.nonphysical.1")
{
    using namespace billiardgl::rules;RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;request.schemaVersion=1;
    request.operation=operation;request.declarationKind=kNonphysicalGeneralFoulKind;
    request.decisionId=decision;
    request.rackIdentity=runtime.nonphysicalGeneralFoulState().rackIdentity;
    request.expectedAuthorityRevision=
        runtime.nonphysicalGeneralFoulProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=
        runtime.nonphysicalGeneralFoulProtocolState().authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;
    request.authoritySubjectId="referee";request.credentialId="credential";
    request.keyVersion="v1";request.authenticationNonce=nonce;
    request.expiresAtAuthorityTick=1000;request.idempotencyKey=key;
    request.payloadCanonical=payload;signStalemateRequest(request);return request;
}

billiardgl::rules::ShotFacts facts(int shooter, int firstContact)
{
    billiardgl::rules::ShotFacts value;
    value.shooter = shooter;
    value.firstContactBall = firstContact;
    value.anyRailAfterFirstContact = true;
    return value;
}

void attestBreakCushions(
    billiardgl::rules::ShotFacts& value,
    const std::vector<std::tuple<int,
        billiardgl::rules::BreakCushionBoundaryKind, bool>>& rows)
{
    using namespace billiardgl::rules;
    value.breakEvidenceOrigin =
        BreakEvidenceOrigin::AuthoritativeShotFactCollector;
    value.breakEvidenceComplete = true;
    value.breakObjectCushionContacts.clear();
    std::set<int> objects;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto& [ball, boundary, velocityImpulseApplied] = rows[index];
        BreakObjectCushionContact contact;
        contact.objectBallId = ball;
        contact.boundaryKind = boundary;
        contact.provenance =
            BreakCushionContactProvenance::ContinuousSolver;
        contact.velocityImpulseApplied = velocityImpulseApplied;
        contact.positionCorrected = !velocityImpulseApplied;
        contact.normalImpulseNs = velocityImpulseApplied ? 0.1 : 0.0;
        contact.solverResidualCmS = 0.001;
        contact.solverProjectionCm = velocityImpulseApplied ? 0.0 : 0.002;
        contact.timeOfImpactSeconds = 0.001 * static_cast<double>(index);
        contact.solverEventId = 100 + static_cast<int>(index);
        contact.solverIslandId = 200 + static_cast<int>(index);
        contact.physicsTick = 10;
        contact.canonicalContactOrdinal = static_cast<int>(index);
        value.breakObjectCushionContacts.push_back(contact);
        objects.insert(ball);
    }
    value.distinctObjectBallsToRail = static_cast<int>(objects.size());
}

void attestStraightBreakCushions(billiardgl::rules::ShotFacts& value,
                                 std::initializer_list<int> balls)
{
    std::vector<std::tuple<int,
        billiardgl::rules::BreakCushionBoundaryKind, bool>> rows;
    for (int ball : balls) {
        rows.emplace_back(ball,
            billiardgl::rules::BreakCushionBoundaryKind::StraightCushion,
            true);
    }
    attestBreakCushions(value, rows);
}

void attestCueScratch(billiardgl::rules::ShotFacts& value,
                      int pocketId = 2,
                      std::uint64_t physicsTick = 7,
                      std::uint64_t captureSequence = 11)
{
    value.cueBallPocketed = true;
    value.cueBallScratchEvidenceOrigin =
        billiardgl::rules::CueBallScratchEvidenceOrigin::
            AuthoritativeShotFactCollector;
    value.cueBallScratchEvidenceComplete = true;
    billiardgl::rules::CueBallPocketCaptureEvidence evidence;
    evidence.pocketId = pocketId;
    evidence.captureSequence = captureSequence;
    evidence.physicsTick = physicsTick;
    evidence.physicsContactOrdinal = 0;
    evidence.canonicalEventOrdinal = 0;
    evidence.timeOfImpactSeconds = 0.01;
    value.cueBallPocketCaptures = {evidence};
}

billiardgl::ControllerResult send(billiardgl::AutomationController& controller,
    int id, const std::string& command,
    const billiardgl::json::Value& params = billiardgl::json::Value::object())
{
    billiardgl::AutomationRequest request;
    request.id = id;
    request.version = 1;
    request.command = command;
    request.params = params;
    return controller.handle(request);
}

billiardgl::json::Value object(
    std::initializer_list<std::pair<const char*, billiardgl::json::Value>> fields)
{
    billiardgl::json::Value value = billiardgl::json::Value::object();
    for (const auto& field : fields) value[field.first] = field.second;
    return value;
}

billiardgl::json::Value balls(std::initializer_list<int> values)
{
    billiardgl::json::Value result = billiardgl::json::Value::array();
    for (int value : values)
        result.asArray().push_back(billiardgl::json::Value(value));
    return result;
}

bool hasReason(const billiardgl::rules::AdjudicationResult& result,
               billiardgl::rules::FoulReason reason)
{
    return std::find(result.reasons.begin(), result.reasons.end(), reason) !=
        result.reasons.end();
}

bool prepareAssignedTable(billiardgl::AutomationController& controller,
                          int idBase)
{
    const auto enabled = send(controller, idBase, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")}}));
    const auto breakShot = send(controller, idBase + 1,
        "adjudicate_rules_shot", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"shooter", billiardgl::json::Value(0)},
            {"break_shot", billiardgl::json::Value(true)},
            {"first_contact_ball", billiardgl::json::Value(1)},
            {"distinct_object_balls_to_rail", billiardgl::json::Value(4)}}));
    const auto assignment = send(controller, idBase + 2,
        "adjudicate_rules_shot", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"shooter", billiardgl::json::Value(1)},
            {"first_contact_ball", billiardgl::json::Value(1)},
            {"pocketed_balls_in_order", balls({1})}}));
    return enabled.response.at("ok").asBool() &&
        breakShot.response.at("ok").asBool() &&
        assignment.response.at("ok").asBool();
}

bool stageSimultaneousGroupChoice(billiardgl::GameRuntime& runtime)
{
    if (!runtime.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok)
        return false;
    auto breakShot = facts(0, 1);
    breakShot.breakShot = true;
    breakShot.distinctObjectBallsToRail = 4;
    if (!runtime.adjudicateRulesShot(breakShot).ok) return false;
    auto choice = facts(1, 1);
    choice.firstContactBalls = {1, 9};
    choice.pocketedBallsInOrder = {1, 9};
    return runtime.adjudicateRulesShot(choice).ok &&
        runtime.matchState().groupChoicePending &&
        runtime.groupChoiceDecisionOffer().has_value();
}

bool stageFullTableBallInHand(billiardgl::GameRuntime& runtime,
                              bool pocketCueBall = false)
{
    if (!runtime.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok)
        return false;
    auto breakShot = facts(0, 1);
    breakShot.breakShot = true;
    breakShot.distinctObjectBallsToRail = 4;
    if (!runtime.adjudicateRulesShot(breakShot).ok) return false;
    auto assignment = facts(1, 1);
    assignment.pocketedBallsInOrder = {1};
    if (!runtime.adjudicateRulesShot(assignment).ok) return false;
    if (pocketCueBall) runtime.mutableState().balls[0].pocketed = true;
    auto foul = facts(1, 9);
    foul.anyRailAfterFirstContact = false;
    if (pocketCueBall) attestCueScratch(foul);
    return runtime.adjudicateRulesShot(foul).ok &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementScope() ==
            billiardgl::rules::BallInHandPlacementScope::FullTable &&
        runtime.ballInHandPlacementOffer().has_value();
}

billiardgl::rules::BreakFoulDecisionRequest breakFoulRequest(
    const billiardgl::rules::BreakFoulDecisionOffer& offer,
    const std::string& actionId, const std::string& key)
{
    billiardgl::rules::BreakFoulDecisionRequest request;
    request.rulesetId = billiardgl::rules::kBreakFoulRulesetId;
    request.capabilityId = billiardgl::rules::kBreakFoulCapabilityId;
    request.flowId = billiardgl::rules::kBreakFoulFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision = offer.authorityStateRevision;
    request.idempotencyKey = key;
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId = actionId;
    request.payloadCanonical = "{}";
    return request;
}

struct AuthoritySnapshot {
    std::string gameState;
    std::string rulesHash;
    std::uint64_t revision = 0;
    std::uint64_t tick = 0;
    std::size_t events = 0;
    std::size_t semanticActions = 0;
    std::size_t authorityActions = 0;
};

AuthoritySnapshot authoritySnapshot(const billiardgl::GameRuntime& runtime)
{
    return AuthoritySnapshot{
        billiardgl::canonicalGameState(runtime.state()),
        runtime.rulesAuthorityStateHash(), runtime.rulesAuthorityRevision(),
        runtime.tick(), runtime.events().size(),
        runtime.appliedSemanticActions().size(),
        runtime.rulesAuthorityActions().size()};
}

bool sameAuthoritySnapshot(const AuthoritySnapshot& first,
                           const AuthoritySnapshot& second)
{
    return first.gameState == second.gameState &&
        first.rulesHash == second.rulesHash &&
        first.revision == second.revision && first.tick == second.tick &&
        first.events == second.events &&
        first.semanticActions == second.semanticActions &&
        first.authorityActions == second.authorityActions;
}

bool near(double first, double second, double tolerance = 0.0001)
{
    return std::fabs(first - second) <= tolerance;
}

bool isSolid(int ball)
{
    return ball >= 1 && ball <= 7;
}

void testOfficialRackContractUnit()
{
    const auto profile = billiardgl::defaultChinesePoolPhysicsProfile();
    const auto first = billiardgl::rules::buildOfficialRackContract(
        profile, 0x123456789abcdef0ULL);
    const auto repeated = billiardgl::rules::buildOfficialRackContract(
        profile, 0x123456789abcdef0ULL);
    const auto otherSeed = billiardgl::rules::buildOfficialRackContract(
        profile, 0x0fedcba987654321ULL);
    const double footZ =
        profile.tableBoundary.playfieldLengthCm * 0.25;
    const double diameter = 2.0 * profile.ball.radiusCm;

    expect(first.rulesetId == "wpa_heyball_2025_08_16_v1" &&
        first.sourceClause == "WPA-II-04" &&
        first.algorithmId ==
            billiardgl::rules::kOfficialRackAlgorithmId &&
        !first.numberedBallOrderIsOfficial &&
        near(first.footSpot.x, 0.0) && near(first.footSpot.z, footZ) &&
        near(first.cueBallBreakPosition.z, -footZ),
        "rack contract separates official WPA geometry from seeded unspecified number order");

    std::array<bool, 16> seen{};
    int observedSameGroupAdjacencies = 0;
    bool deterministic = true;
    bool numberedOrderDiffers = false;
    for (std::size_t index = 0; index < first.slots.size(); ++index) {
        const auto& slot = first.slots[index];
        const auto& repeatedSlot = repeated.slots[index];
        seen[slot.ball] = slot.ball >= 1 && slot.ball <= 15;
        deterministic = deterministic &&
            slot.ball == repeatedSlot.ball &&
            near(slot.position.x, repeatedSlot.position.x) &&
            near(slot.position.z, repeatedSlot.position.z);
        numberedOrderDiffers = numberedOrderDiffers ||
            slot.ball != otherSeed.slots[index].ball;
        for (std::size_t other = index + 1;
             other < first.slots.size(); ++other) {
            const double dx = slot.position.x -
                first.slots[other].position.x;
            const double dz = slot.position.z -
                first.slots[other].position.z;
            const double squared = dx * dx + dz * dz;
            expect(squared >= diameter * diameter - 0.001,
                "official rack geometry never overlaps at the real profile radius");
            if (squared <= diameter * diameter + 0.001 &&
                isSolid(slot.ball) == isSolid(first.slots[other].ball) &&
                slot.ball != 8 && first.slots[other].ball != 8)
                ++observedSameGroupAdjacencies;
        }
    }
    bool allObjectBallsPresent = true;
    for (int ball = 1; ball <= 15; ++ball)
        allObjectBallsPresent = allObjectBallsPresent && seen[ball];
    std::array<int, 15> candidateIndex;
    candidateIndex.fill(-1);
    int candidateCount = 0;
    for (int slot = 0; slot < 15; ++slot)
        if (slot != first.eightBallSlot)
            candidateIndex[slot] = candidateCount++;
    int independentlyComputedMinimum = 1000;
    for (unsigned mask = 0; mask < (1u << candidateCount); ++mask) {
        int solidCount = 0;
        for (unsigned bits = mask; bits != 0; bits &= bits - 1)
            ++solidCount;
        if (solidCount != 7) continue;
        const bool firstCornerSolid = mask &
            (1u << candidateIndex[first.baseCornerSlots[0]]);
        const bool secondCornerSolid = mask &
            (1u << candidateIndex[first.baseCornerSlots[1]]);
        if (firstCornerSolid == secondCornerSolid) continue;
        int sameGroupAdjacencies = 0;
        for (int slot = 0; slot < 15; ++slot) {
            if (slot == first.eightBallSlot) continue;
            const bool slotSolid = mask & (1u << candidateIndex[slot]);
            for (int other = slot + 1; other < 15; ++other) {
                if (other == first.eightBallSlot) continue;
                const bool otherSolid =
                    mask & (1u << candidateIndex[other]);
                if (slotSolid != otherSolid) continue;
                const double dx = first.slots[slot].position.x -
                    first.slots[other].position.x;
                const double dz = first.slots[slot].position.z -
                    first.slots[other].position.z;
                if (dx * dx + dz * dz <=
                    diameter * diameter + 0.001)
                    ++sameGroupAdjacencies;
            }
        }
        independentlyComputedMinimum = std::min(
            independentlyComputedMinimum, sameGroupAdjacencies);
    }
    const int firstCorner =
        first.slots[first.baseCornerSlots[0]].ball;
    const int secondCorner =
        first.slots[first.baseCornerSlots[1]].ball;
    expect(allObjectBallsPresent && deterministic && numberedOrderDiffers &&
        first.slots[first.apexSlot].slotIndex == first.apexSlot &&
        near(first.slots[first.apexSlot].position.z, footZ) &&
        first.slots[first.eightBallSlot].ball == 8 &&
        isSolid(firstCorner) != isSolid(secondCorner) &&
        observedSameGroupAdjacencies ==
            first.sameGroupAdjacencyCount &&
        first.sameGroupAdjacencyCount ==
            first.minimumPossibleSameGroupAdjacencyCount &&
        first.sameGroupAdjacencyCount == independentlyComputedMinimum,
        "rack contract is complete, deterministic, 8-centered, opposite-corner grouped, and maximally separated");
}

void pocketAllObjectBallsExcept(billiardgl::GameRuntime& runtime,
                                int activeBall)
{
    for (int ball = 1; ball < billiardgl::kBallCount; ++ball)
        runtime.mutableState().balls[ball].pocketed = ball != activeBall;
    runtime.mutableState().pocketedBallCount =
        billiardgl::kBallCount - 1 - (activeBall > 0 ? 1 : 0);
    runtime.mutableState().ballsMoving = false;
}

void testRackFoundationTransactions()
{
    billiardgl::GameRuntime rerack;
    expect(rerack.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok,
        "rack foundation enables only under the explicit Heyball ruleset");
    for (int ball = 0; ball < billiardgl::kBallCount; ++ball) {
        auto& state = rerack.mutableState().balls[ball];
        state.position = billiardgl::Point3{
            static_cast<float>(ball), state.position.y,
            static_cast<float>(-ball)};
        state.velocity = billiardgl::Point3{1.0f, 2.0f, 3.0f};
        state.angularVelocity = billiardgl::Point3{4.0f, 5.0f, 6.0f};
        state.speed = 0.0f;
        state.pocketed = true;
        state.pocketInteraction.phase =
            billiardgl::PocketInteractionPhase::Captured;
        state.pocketInteraction.pocketId = ball % 6;
        state.pocketInteraction.captureSequence = ball + 1;
    }
    rerack.mutableState().ballsMoving = false;
    rerack.mutableState().pocketedBallCount = billiardgl::kBallCount;
    billiardgl::rules::RerackFoundationParameters rerackParameters;
    rerackParameters.deterministicSeed = 77;
    rerackParameters.breakerPlayerId = 1;
    const AuthoritySnapshot beforeStage = authoritySnapshot(rerack);
    const auto rerackPlan = rerack.stageOfficialRerack(rerackParameters);
    const auto repeatedPlan = rerack.stageOfficialRerack(rerackParameters);
    expect(rerackPlan.ok && rerackPlan.placements.size() ==
            billiardgl::kBallCount &&
        rerackPlan.transactionId == repeatedPlan.transactionId &&
        rerackPlan.baseAuthorityStateHash == beforeStage.rulesHash &&
        sameAuthoritySnapshot(beforeStage, authoritySnapshot(rerack)),
        "rerack staging is read-only, complete, revision-bound, and deterministic");

    auto malformedPlan = rerackPlan;
    malformedPlan.placements.front().second.x += 0.25f;
    const AuthoritySnapshot beforeMalformed = authoritySnapshot(rerack);
    const auto malformed = rerack.commitRackFoundation(
        malformedPlan, "rerack-malformed");
    expect(!malformed.ok &&
        malformed.errorCode == "rules_foundation_plan_mismatch" &&
        sameAuthoritySnapshot(beforeMalformed, authoritySnapshot(rerack)),
        "rerack commit regenerates and rejects a tampered plan with zero mutation");

    const AuthoritySnapshot beforeCommit = authoritySnapshot(rerack);
    const auto committed = rerack.commitRackFoundation(
        rerackPlan, "rerack-commit");
    bool allRestored = true;
    for (const auto& ball : rerack.state().balls) {
        allRestored = allRestored && !ball.pocketed &&
            near(ball.velocity.x, 0.0) && near(ball.velocity.y, 0.0) &&
            near(ball.velocity.z, 0.0) && near(ball.speed, 0.0) &&
            ball.pocketInteraction.phase ==
                billiardgl::PocketInteractionPhase::Outside &&
            ball.pocketInteraction.pocketId == -1 &&
            ball.pocketInteraction.captureSequence == 0;
    }
    bool noOverlap = true;
    const double minimum = 2.0 * rerack.physicsProfile().ball.radiusCm;
    for (int first = 0; first < billiardgl::kBallCount; ++first) {
        for (int second = first + 1; second < billiardgl::kBallCount;
             ++second) {
            const double dx = rerack.state().balls[first].position.x -
                rerack.state().balls[second].position.x;
            const double dz = rerack.state().balls[first].position.z -
                rerack.state().balls[second].position.z;
            noOverlap = noOverlap &&
                dx * dx + dz * dz >= minimum * minimum - 0.001;
        }
    }
    expect(committed.ok && committed.ack.accepted && allRestored &&
        noOverlap && rerack.state().pocketedBallCount == 0 &&
        !rerack.state().ballsMoving &&
        rerack.matchState().phase ==
            billiardgl::rules::MatchPhase::Break &&
        rerack.matchState().currentPlayer == 1 &&
        rerack.state().players.currentPlayer == 1 &&
        committed.ack.previousStateRevision == beforeCommit.revision &&
        committed.ack.authoritativeStateRevision ==
            beforeCommit.revision + 1 &&
        committed.ack.authoritativeStateHash ==
            rerack.rulesAuthorityStateHash() &&
        rerack.events().size() == beforeCommit.events + 1 &&
        rerack.rulesAuthorityActions().size() ==
            beforeCommit.authorityActions + 1,
        "rerack atomically restores all 16 balls and explicit breaker with one revision/event/action");
    const AuthoritySnapshot afterCommit = authoritySnapshot(rerack);
    const auto retry = rerack.commitRackFoundation(
        rerackPlan, "rerack-commit");
    expect(retry.ok && retry.ack.authoritativeStateHash ==
            committed.ack.authoritativeStateHash &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(rerack)),
        "rerack exact retry returns the original ACK without duplicate effects");
    auto changedRetry = rerackPlan;
    changedRetry.placements.front().second.z += 0.25f;
    const auto conflict = rerack.commitRackFoundation(
        changedRetry, "rerack-commit");
    expect(!conflict.ok &&
        conflict.errorCode == "rules_decision_idempotency_conflict" &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(rerack)),
        "rerack idempotency key rejects a changed plan without mutation");

    const auto resetPlan = rerack.stageOfficialRerack(rerackParameters);
    rerack.reset();
    const AuthoritySnapshot afterReset = authoritySnapshot(rerack);
    const auto resetStale = rerack.commitRackFoundation(
        resetPlan, "rerack-after-reset");
    expect(!resetStale.ok &&
        resetStale.errorCode == "rules_decision_stale_revision" &&
        sameAuthoritySnapshot(afterReset, authoritySnapshot(rerack)),
        "full reset invalidates a staged rerack without mutation");

    billiardgl::GameRuntime changedState;
    changedState.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
    const auto changedStatePlan =
        changedState.stageOfficialRerack(rerackParameters);
    auto moved = changedState.state().balls[2];
    moved.position.x += 1.0f;
    expect(changedState.setBall(2, moved).ok,
        "fixture changes physical authority state after staging");
    const AuthoritySnapshot afterExternalMutation =
        authoritySnapshot(changedState);
    const auto changedStateCommit = changedState.commitRackFoundation(
        changedStatePlan, "rerack-state-changed");
    expect(!changedStateCommit.ok &&
        changedStateCommit.errorCode ==
            "rules_foundation_state_changed" &&
        sameAuthoritySnapshot(afterExternalMutation,
            authoritySnapshot(changedState)),
        "rerack commit revalidates the current physical state and rolls back on change");

    billiardgl::GameRuntime rulesetReset;
    rulesetReset.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1);
    const auto rulesetResetPlan =
        rulesetReset.stageOfficialRerack(rerackParameters);
    rulesetReset.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1);
    const AuthoritySnapshot afterRulesetReset =
        authoritySnapshot(rulesetReset);
    const auto rulesetStale = rulesetReset.commitRackFoundation(
        rulesetResetPlan, "rerack-after-ruleset-reset");
    expect(!rulesetStale.ok &&
        rulesetStale.errorCode == "rules_decision_stale_revision" &&
        sameAuthoritySnapshot(afterRulesetReset,
            authoritySnapshot(rulesetReset)),
        "ruleset reset invalidates a staged rerack without mutation");

    billiardgl::GameRuntime respot;
    respot.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
    pocketAllObjectBallsExcept(respot, -1);
    respot.mutableState().balls[8].physicalDomain =
        billiardgl::PhysicalBallDomain::PocketChannel;
    billiardgl::rules::RespotFoundationParameters spot;
    spot.ball = 8;
    spot.nominalPosition = billiardgl::Point3{0.0f,
        static_cast<float>(billiardgl::kTableHeight +
            respot.physicsProfile().ball.radiusCm),
        static_cast<float>(
            respot.physicsProfile().tableBoundary.playfieldLengthCm *
            0.25)};
    const AuthoritySnapshot beforeSpotStage = authoritySnapshot(respot);
    const auto spotPlan = respot.stageBallRespot(spot);
    expect(spotPlan.ok && spotPlan.candidatesExamined == 1 &&
        near(spotPlan.placements.front().second.z,
            spot.nominalPosition.z) &&
        sameAuthoritySnapshot(beforeSpotStage, authoritySnapshot(respot)),
        "8-ball respot selects a clear nominal foot spot without staging mutation");
    const AuthoritySnapshot beforeSpotCommit = authoritySnapshot(respot);
    const auto spotted = respot.commitRackFoundation(
        spotPlan, "eight-respot");
    expect(spotted.ok && !respot.state().balls[8].pocketed &&
        respot.state().balls[8].physicalDomain ==
            billiardgl::PhysicalBallDomain::ClothSupported &&
        near(respot.state().balls[8].position.z,
            spot.nominalPosition.z) &&
        respot.rulesAuthorityRevision() == beforeSpotCommit.revision + 1 &&
        respot.events().size() == beforeSpotCommit.events + 1 &&
        respot.rulesAuthorityActions().size() ==
            beforeSpotCommit.authorityActions + 1,
        "8-ball respot commits once at the nominal point and clears its pending physical effect");

    billiardgl::GameRuntime fallback;
    fallback.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
    pocketAllObjectBallsExcept(fallback, 1);
    fallback.mutableState().balls[1].position = spot.nominalPosition;
    const AuthoritySnapshot beforeFallback = authoritySnapshot(fallback);
    const auto fallbackPlan = fallback.stageBallRespot(spot);
    expect(fallbackPlan.ok && fallbackPlan.candidatesExamined > 1 &&
        fallbackPlan.placements.front().second.z >
            spot.nominalPosition.z &&
        sameAuthoritySnapshot(beforeFallback, authoritySnapshot(fallback)),
        "obstructed foot spot deterministically searches the longitudinal axis toward the top cushion");

    auto impossible = spot;
    impossible.search.maximumCandidates = 3;
    const AuthoritySnapshot beforeImpossible = authoritySnapshot(fallback);
    const auto impossiblePlan = fallback.stageBallRespot(impossible);
    const auto impossibleCommit = fallback.commitRackFoundation(
        impossiblePlan, "respot-no-solution");
    expect(!impossiblePlan.ok &&
        impossiblePlan.errorCode == "rules_respot_no_legal_position" &&
        !impossibleCommit.ok &&
        impossibleCommit.errorCode == "rules_respot_no_legal_position" &&
        sameAuthoritySnapshot(beforeImpossible,
            authoritySnapshot(fallback)),
        "finite respot search reports no solution and preserves authority state");

    const auto staleSpotPlan = fallback.stageBallRespot(spot);
    fallback.mutableState().balls[1].position.x += 10.0f;
    const AuthoritySnapshot afterSpotStateChange = authoritySnapshot(fallback);
    const auto staleSpot = fallback.commitRackFoundation(
        staleSpotPlan, "respot-state-changed");
    expect(!staleSpot.ok &&
        staleSpot.errorCode == "rules_foundation_state_changed" &&
        sameAuthoritySnapshot(afterSpotStateChange,
            authoritySnapshot(fallback)),
        "respot commit detects collision-state changes and performs zero partial mutation");
}

bool stageEightOnBreak(billiardgl::GameRuntime& runtime, bool foul,
                       bool clearObjectBalls)
{
    if (!runtime.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok)
        return false;
    if (clearObjectBalls)
        pocketAllObjectBallsExcept(runtime, -1);
    else {
        runtime.mutableState().balls[8].pocketed = true;
        runtime.mutableState().pocketedBallCount = 1;
    }
    if (foul) runtime.mutableState().balls[0].pocketed = true;
    billiardgl::rules::ShotFacts shot = facts(0, 1);
    shot.breakShot = true;
    shot.pocketedBallsInOrder = {8};
    if (foul) attestCueScratch(shot);
    return runtime.adjudicateRulesShot(shot).ok &&
        runtime.eightOnBreakDecisionPending() &&
        runtime.eightOnBreakDecisionOffer().has_value();
}

billiardgl::rules::EightOnBreakDecisionRequest eightOnBreakRequest(
    const billiardgl::rules::EightOnBreakDecisionOffer& offer,
    const std::string& action, const std::string& key)
{
    billiardgl::rules::EightOnBreakDecisionRequest request;
    request.rulesetId = billiardgl::rules::kEightOnBreakRulesetId;
    request.capabilityId =
        billiardgl::rules::kEightOnBreakCapabilityId;
    request.flowId = billiardgl::rules::kEightOnBreakFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision =
        offer.authorityStateRevision;
    request.idempotencyKey = key;
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId = action;
    request.payloadCanonical = "{}";
    return request;
}

void testEightOnBreakAuthorityTransaction()
{
    billiardgl::GameRuntime legal;
    expect(stageEightOnBreak(legal, false, false),
        "legal 8-on-break stages a dedicated authority decision");
    const auto legalOffer = *legal.eightOnBreakDecisionOffer();
    expect(legalOffer.branch ==
            billiardgl::rules::EightOnBreakBranch::LegalBreak &&
        legalOffer.actorPlayerId == 0 &&
        legalOffer.originalBreakerPlayerId == 0 &&
        legalOffer.frozenNextPlayer == 0 &&
        legalOffer.frozenContinueTurn &&
        legalOffer.authorityStateRevision ==
            legal.rulesAuthorityRevision() &&
        legalOffer.offeredActionIds == std::vector<std::string>({
            billiardgl::rules::kEightOnBreakSpotContinueActionId,
            billiardgl::rules::kEightOnBreakRerackOriginalActionId}),
        "legal branch offers only spot-and-continue or original-breaker rerack");
    expect(legal.lastAdjudication().decisionOptions.size() == 2 &&
        billiardgl::buildRulesPresentation(legal).decisionOptions.empty(),
        "runtime keeps the frozen adjudication while Product HUD remains hidden");

    const AuthoritySnapshot beforeBlocked = authoritySnapshot(legal);
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    const auto blockedShot = legal.dispatch(shoot);
    const auto blockedCue = legal.applyCueImpact({});
    auto anotherShot = facts(0, 1);
    anotherShot.breakShot = false;
    const auto blockedAdjudication = legal.adjudicateRulesShot(anotherShot);
    const auto blockedFoundation = legal.stageBallRespot({});
    expect(!blockedShot.ok &&
        std::string(blockedShot.errorCode) ==
            "rules_eight_on_break_decision_pending" &&
        !blockedCue.ok &&
        std::string(blockedCue.errorCode) ==
            "rules_eight_on_break_decision_pending" &&
        !blockedAdjudication.ok &&
        std::string(blockedAdjudication.errorCode) ==
            "rules_eight_on_break_decision_pending" &&
        !blockedFoundation.ok &&
        blockedFoundation.errorCode ==
            "rules_eight_on_break_decision_pending" &&
        sameAuthoritySnapshot(beforeBlocked, authoritySnapshot(legal)),
        "pending 8-on-break choice blocks shot, direct cue, adjudication, and foundation bypass without mutation");

    auto valid = eightOnBreakRequest(legalOffer,
        billiardgl::rules::kEightOnBreakSpotContinueActionId,
        "eight-legal-spot");
    auto expectRejected = [&](const auto& request, const char* code,
                              const char* message) {
        const AuthoritySnapshot before = authoritySnapshot(legal);
        const auto result = legal.submitEightOnBreakDecision(request);
        expect(!result.ok && result.errorCode == code &&
            sameAuthoritySnapshot(before, authoritySnapshot(legal)),
            message);
    };
    auto bad = valid;
    bad.actorPlayerId = 1;
    bad.idempotencyKey = "eight-wrong-actor";
    expectRejected(bad, "rules_decision_actor_mismatch",
        "8-on-break rejects the wrong actor without mutation");
    bad = valid;
    bad.actionId =
        billiardgl::rules::kEightOnBreakSpotBallInHandActionId;
    bad.idempotencyKey = "eight-unoffered-action";
    expectRejected(bad, "rules_decision_action_not_offered",
        "legal branch rejects the foul-only spot action");
    bad = valid;
    --bad.expectedAuthorityStateRevision;
    bad.idempotencyKey = "eight-stale-revision";
    expectRejected(bad, "rules_decision_stale_revision",
        "8-on-break rejects a stale revision without mutation");
    bad = valid;
    bad.capabilityId = "ball_respot_and_rerack_executor";
    bad.idempotencyKey = "eight-bad-context";
    expectRejected(bad, "rules_decision_context_mismatch",
        "8-on-break validates exact ruleset/capability/flow context");
    bad = valid;
    bad.payloadCanonical = "{\"unexpected\":true}";
    bad.idempotencyKey = "eight-bad-payload";
    expectRejected(bad, "rules_decision_invalid_payload",
        "8-on-break rejects nonempty payloads without mutation");
    bad = valid;
    bad.idempotencyKey.clear();
    expectRejected(bad, "rules_decision_idempotency_key_invalid",
        "8-on-break rejects an empty idempotency key without mutation");
    bad = valid;
    bad.decisionId += ".old";
    bad.idempotencyKey = "eight-stale-id";
    expectRejected(bad, "rules_decision_stale",
        "8-on-break rejects an unknown decision id without mutation");

    const AuthoritySnapshot beforeCommit = authoritySnapshot(legal);
    const auto committed = legal.submitEightOnBreakDecision(valid);
    const double footZ =
        legal.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    expect(committed.ok && committed.ack.accepted &&
        committed.ack.previousStateRevision == beforeCommit.revision &&
        committed.ack.authoritativeStateRevision ==
            beforeCommit.revision + 1 &&
        committed.ack.authoritativeStateHash ==
            legal.rulesAuthorityStateHash() &&
        committed.ack.effects.eightBallRespotted &&
        !committed.ack.effects.rackRebuilt &&
        committed.ack.effects.turnContinues &&
        committed.ack.effects.currentPlayer == 0 &&
        !committed.ack.effects.ballInHand &&
        committed.ack.effects.ballInHandScope ==
            billiardgl::rules::BallInHandPlacementScope::None &&
        !legal.eightOnBreakDecisionPending() &&
        !legal.state().balls[8].pocketed &&
        legal.state().balls[8].position.z > footZ &&
        legal.lastShotFacts().pocketedBallsInOrder ==
            std::vector<int>({8}) &&
        !legal.lastAdjudication().eightBallMustBeSpotted &&
        legal.lastAdjudication().decisionOptions.empty() &&
        legal.events().size() == beforeCommit.events + 1 &&
        legal.rulesAuthorityActions().size() ==
            beforeCommit.authorityActions + 1&&
        !legal.rulesReplayEvents().empty()&&
        legal.rulesReplayEvents().back().type==
            billiardgl::RuntimeRulesReplayEventType::EightOnBreakChoiceCommit,
        "legal spot atomically reuses collision-safe fallback and preserves frozen breaker turn with one revision/event/action");
    const AuthoritySnapshot afterCommit = authoritySnapshot(legal);
    const auto retry = legal.submitEightOnBreakDecision(valid);
    expect(retry.ok && retry.ack.authoritativeStateHash ==
            committed.ack.authoritativeStateHash &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(legal)),
        "8-on-break exact retry returns the original ACK exactly once");
    bad = valid;
    bad.actionId =
        billiardgl::rules::kEightOnBreakRerackOriginalActionId;
    const auto conflict = legal.submitEightOnBreakDecision(bad);
    expect(!conflict.ok &&
        conflict.errorCode == "rules_decision_idempotency_conflict" &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(legal)),
        "8-on-break reused key with changed action is an idempotency conflict");

    billiardgl::GameRuntime foulSpot;
    expect(stageEightOnBreak(foulSpot, true, true),
        "foul 8-on-break stages the opponent authority decision");
    const auto foulOffer = *foulSpot.eightOnBreakDecisionOffer();
    expect(foulOffer.branch ==
            billiardgl::rules::EightOnBreakBranch::FoulBreak &&
        foulOffer.actorPlayerId == 1 &&
        foulOffer.originalBreakerPlayerId == 0 &&
        foulOffer.frozenNextPlayer == 1 &&
        !foulOffer.frozenContinueTurn &&
        foulOffer.offeredActionIds == std::vector<std::string>({
            billiardgl::rules::kEightOnBreakSpotBallInHandActionId,
            billiardgl::rules::kEightOnBreakRerackOpponentActionId,
            billiardgl::rules::kEightOnBreakRerackOriginalActionId}),
        "foul branch offers exactly opponent spot/head-string and both authorized reracks");
    expect(!foulSpot.matchState().ballInHand &&
        !foulSpot.lastAdjudication().ballInHand &&
        !foulSpot.lastAdjudication().ballInHandBehindHeadString &&
        foulSpot.ballInHandPlacementScope() ==
            billiardgl::rules::BallInHandPlacementScope::None &&
        !foulSpot.ballInHandPlacementPending() &&
        !foulSpot.ballInHandPlacementOffer().has_value(),
        "foul 8-on-break parent stages with no BIH entitlement before the exact spot choice commits");
    const auto foulSpotRequest = eightOnBreakRequest(foulOffer,
        billiardgl::rules::kEightOnBreakSpotBallInHandActionId,
        "eight-foul-spot");
    auto expectFoulRejected = [&](const auto& request, const char* code,
                                  const char* message) {
        const AuthoritySnapshot before = authoritySnapshot(foulSpot);
        const auto rejected = foulSpot.submitEightOnBreakDecision(request);
        expect(!rejected.ok && rejected.errorCode == code &&
            sameAuthoritySnapshot(before, authoritySnapshot(foulSpot)) &&
            !foulSpot.matchState().ballInHand &&
            foulSpot.ballInHandPlacementScope() ==
                billiardgl::rules::BallInHandPlacementScope::None &&
            !foulSpot.ballInHandPlacementPending() &&
            !foulSpot.ballInHandPlacementOffer().has_value(), message);
    };
    auto badFoul = foulSpotRequest;
    badFoul.actorPlayerId = foulOffer.originalBreakerPlayerId;
    badFoul.idempotencyKey = "eight-foul-wrong-actor";
    expectFoulRejected(badFoul, "rules_decision_actor_mismatch",
        "wrong foul actor cannot grant an early BIH entitlement");
    badFoul = foulSpotRequest;
    badFoul.actionId =
        billiardgl::rules::kEightOnBreakSpotContinueActionId;
    badFoul.idempotencyKey = "eight-foul-wrong-action";
    expectFoulRejected(badFoul, "rules_decision_action_not_offered",
        "legal-only spot action cannot grant foul-branch BIH");
    badFoul = foulSpotRequest;
    --badFoul.expectedAuthorityStateRevision;
    badFoul.idempotencyKey = "eight-foul-stale-revision";
    expectFoulRejected(badFoul, "rules_decision_stale_revision",
        "stale foul revision cannot grant an early BIH entitlement");
    const AuthoritySnapshot beforeFoulSpot = authoritySnapshot(foulSpot);
    const auto foulSpotted =
        foulSpot.submitEightOnBreakDecision(foulSpotRequest);
    expect(foulSpotted.ok &&
        foulSpotted.ack.effects.eightBallRespotted &&
        foulSpotted.ack.effects.currentPlayer == 1 &&
        !foulSpotted.ack.effects.turnContinues &&
        foulSpotted.ack.effects.ballInHand &&
        foulSpotted.ack.effects.ballInHandScope ==
            billiardgl::rules::BallInHandPlacementScope::BehindHeadString &&
        foulSpot.matchState().ballInHand &&
        foulSpot.ballInHandPlacementScope() ==
            billiardgl::rules::BallInHandPlacementScope::BehindHeadString &&
        !foulSpot.rulesReplayEvents().empty()&&
        foulSpot.rulesReplayEvents().back().type==
            billiardgl::RuntimeRulesReplayEventType::
                EightOnBreakChoiceCommit&&
        foulSpot.ballInHandPlacementOffer() &&
        foulSpot.ballInHandPlacementOffer()->offeredActionIds[0] ==
            billiardgl::rules::kConfirmBehindHeadStringPlacementActionId &&
        foulSpot.lastAdjudication().ballInHand &&
        foulSpot.lastAdjudication().ballInHandBehindHeadString &&
        foulSpot.lastAdjudication().nextState.ballInHand &&
        !foulSpot.eightOnBreakDecisionPending() &&
        foulSpot.rulesAuthorityRevision() == beforeFoulSpot.revision + 1 &&
        foulSpotted.ack.authoritativeStateHash ==
            foulSpot.rulesAuthorityStateHash(),
        "foul spot atomically restores 8 and stages the opponent head-string placement offer");
    const auto committedPlacementOffer = foulSpot.ballInHandPlacementOffer();
    const AuthoritySnapshot afterFoulSpot = authoritySnapshot(foulSpot);
    const auto foulSpotRetry =
        foulSpot.submitEightOnBreakDecision(foulSpotRequest);
    expect(foulSpotRetry.ok && committedPlacementOffer &&
        foulSpot.ballInHandPlacementOffer() &&
        foulSpot.ballInHandPlacementOffer()->decisionId ==
            committedPlacementOffer->decisionId &&
        foulSpot.lastAdjudication().ballInHand &&
        foulSpot.lastAdjudication().ballInHandBehindHeadString &&
        sameAuthoritySnapshot(afterFoulSpot, authoritySnapshot(foulSpot)),
        "exact foul spot retry preserves the single granted BIH child without duplicating it");
    badFoul = foulSpotRequest;
    badFoul.actionId =
        billiardgl::rules::kEightOnBreakRerackOriginalActionId;
    const auto foulSpotConflict =
        foulSpot.submitEightOnBreakDecision(badFoul);
    expect(!foulSpotConflict.ok &&
        foulSpotConflict.errorCode == "rules_decision_idempotency_conflict" &&
        foulSpot.ballInHandPlacementOffer() && committedPlacementOffer &&
        foulSpot.ballInHandPlacementOffer()->decisionId ==
            committedPlacementOffer->decisionId &&
        foulSpot.lastAdjudication().ballInHand &&
        foulSpot.lastAdjudication().ballInHandBehindHeadString &&
        sameAuthoritySnapshot(afterFoulSpot, authoritySnapshot(foulSpot)),
        "conflicting retry neither revokes nor duplicates the already committed BIH child");
    const auto placementOffer = *foulSpot.ballInHandPlacementOffer();
    billiardgl::rules::BallInHandPlacementRequest placementRequest;
    placementRequest.rulesetId =
        billiardgl::rules::kBallInHandRulesetId;
    placementRequest.capabilityId =
        billiardgl::rules::kFreeBallPlacementCapabilityId;
    placementRequest.flowId =
        billiardgl::rules::kBehindHeadStringBallInHandFlowId;
    placementRequest.decisionId = placementOffer.decisionId;
    placementRequest.expectedAuthorityStateRevision =
        placementOffer.authorityStateRevision;
    placementRequest.idempotencyKey = "eight-foul-head-placement";
    placementRequest.actorPlayerId = placementOffer.actorPlayerId;
    placementRequest.actionId =
        billiardgl::rules::kConfirmBehindHeadStringPlacementActionId;
    placementRequest.position = {0.0, -90.0};
    const AuthoritySnapshot beforeHeadPlacement =
        authoritySnapshot(foulSpot);
    const auto placedAfterFoul =
        foulSpot.submitBallInHandPlacement(placementRequest);
    expect(placedAfterFoul.ok &&
        !foulSpot.state().balls[0].pocketed &&
        near(foulSpot.state().balls[0].position.z, -90.0) &&
        foulSpot.matchState().ballInHand &&
        foulSpot.ballInHandPlacementPending() &&
        foulSpot.rulesAuthorityRevision() ==
            beforeHeadPlacement.revision + 1 &&
        foulSpot.events().size() == beforeHeadPlacement.events + 1 &&
        foulSpot.rulesAuthorityActions().size() ==
            beforeHeadPlacement.authorityActions + 1,
        "Core F foul spot closes end-to-end through Core G head-string placement");

    auto rerackCase = [](bool foul, const char* action,
                         const char* key, int expectedBreaker,
                         const char* message) {
        billiardgl::GameRuntime runtime;
        expect(stageEightOnBreak(runtime, foul, false),
            "rerack fixture stages 8-on-break decision");
        const auto offer = *runtime.eightOnBreakDecisionOffer();
        const auto request = eightOnBreakRequest(offer, action, key);
        const AuthoritySnapshot before = authoritySnapshot(runtime);
        expect(!runtime.matchState().ballInHand &&
            runtime.ballInHandPlacementScope() ==
                billiardgl::rules::BallInHandPlacementScope::None &&
            !runtime.ballInHandPlacementPending() &&
            !runtime.ballInHandPlacementOffer().has_value(),
            "every rerack parent begins without an unchosen BIH entitlement");
        const auto result = runtime.submitEightOnBreakDecision(request);
        bool allRestored = true;
        for (const auto& ball : runtime.state().balls)
            allRestored = allRestored && !ball.pocketed;
        expect(result.ok && result.ack.effects.rackRebuilt &&
            !result.ack.effects.eightBallRespotted && allRestored &&
            result.ack.effects.breakerPlayerId == expectedBreaker &&
            result.ack.effects.currentPlayer == expectedBreaker &&
            runtime.matchState().phase ==
                billiardgl::rules::MatchPhase::Break &&
            !runtime.matchState().ballInHand &&
            runtime.ballInHandPlacementScope() ==
                billiardgl::rules::BallInHandPlacementScope::None &&
            !runtime.ballInHandPlacementPending() &&
            !runtime.ballInHandPlacementOffer().has_value() &&
            runtime.rulesAuthorityRevision() == before.revision + 1 &&
            runtime.events().size() == before.events + 1 &&
            runtime.rulesAuthorityActions().size() ==
                before.authorityActions + 1,
            message);
        const AuthoritySnapshot after = authoritySnapshot(runtime);
        const auto retry = runtime.submitEightOnBreakDecision(request);
        auto conflictRequest = request;
        conflictRequest.actionId = foul ?
            billiardgl::rules::kEightOnBreakRerackOriginalActionId :
            billiardgl::rules::kEightOnBreakSpotContinueActionId;
        const auto conflict =
            runtime.submitEightOnBreakDecision(conflictRequest);
        expect(retry.ok && !conflict.ok &&
            conflict.errorCode == "rules_decision_idempotency_conflict" &&
            sameAuthoritySnapshot(after, authoritySnapshot(runtime)) &&
            !runtime.matchState().ballInHand &&
            !runtime.ballInHandPlacementPending() &&
            !runtime.ballInHandPlacementOffer().has_value(),
            "rerack retry/conflict cannot create or retain BIH state");
    };
    rerackCase(false,
        billiardgl::rules::kEightOnBreakRerackOriginalActionId,
        "eight-rerack-original", 0,
        "legal branch reracks atomically with the original breaker explicit");
    rerackCase(true,
        billiardgl::rules::kEightOnBreakRerackOpponentActionId,
        "eight-rerack-opponent", 1,
        "foul branch reracks atomically with the opponent as breaker");

    billiardgl::GameRuntime noSolution;
    noSolution.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1);
    noSolution.mutableState().balls[1].position.x =
        std::numeric_limits<float>::quiet_NaN();
    noSolution.mutableState().balls[8].pocketed = true;
    auto noSolutionShot = facts(0, 1);
    noSolutionShot.breakShot = true;
    noSolutionShot.pocketedBallsInOrder = {8};
    expect(noSolution.adjudicateRulesShot(noSolutionShot).ok,
        "no-solution fixture stages after invalid physical blocker geometry");
    const auto noSolutionOffer = *noSolution.eightOnBreakDecisionOffer();
    const auto noSolutionRequest = eightOnBreakRequest(noSolutionOffer,
        billiardgl::rules::kEightOnBreakSpotContinueActionId,
        "eight-no-solution");
    const std::string beforeNoSolutionHash =
        noSolution.rulesAuthorityStateHash();
    const std::uint64_t beforeNoSolutionRevision =
        noSolution.rulesAuthorityRevision();
    const std::size_t beforeNoSolutionEvents = noSolution.events().size();
    const std::size_t beforeNoSolutionActions =
        noSolution.rulesAuthorityActions().size();
    const auto rejectedNoSolution =
        noSolution.submitEightOnBreakDecision(noSolutionRequest);
    expect(!rejectedNoSolution.ok &&
        rejectedNoSolution.errorCode ==
            "rules_respot_no_legal_position" &&
        noSolution.rulesAuthorityStateHash() == beforeNoSolutionHash &&
        noSolution.rulesAuthorityRevision() == beforeNoSolutionRevision &&
        noSolution.events().size() == beforeNoSolutionEvents &&
        noSolution.rulesAuthorityActions().size() ==
            beforeNoSolutionActions &&
        std::isnan(noSolution.state().balls[1].position.x),
        "8-on-break no-solution returns the Core E error with zero partial mutation");

    billiardgl::GameRuntime foulNoSolution;
    foulNoSolution.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1);
    foulNoSolution.mutableState().balls[0].pocketed = true;
    foulNoSolution.mutableState().balls[1].position.x =
        std::numeric_limits<float>::quiet_NaN();
    foulNoSolution.mutableState().balls[8].pocketed = true;
    auto foulNoSolutionShot = facts(0, 1);
    foulNoSolutionShot.breakShot = true;
    foulNoSolutionShot.pocketedBallsInOrder = {8};
    attestCueScratch(foulNoSolutionShot);
    expect(foulNoSolution.adjudicateRulesShot(foulNoSolutionShot).ok &&
        !foulNoSolution.matchState().ballInHand &&
        !foulNoSolution.ballInHandPlacementPending(),
        "foul no-solution fixture stages with entitlement deferred");
    const auto foulNoSolutionRequest = eightOnBreakRequest(
        *foulNoSolution.eightOnBreakDecisionOffer(),
        billiardgl::rules::kEightOnBreakSpotBallInHandActionId,
        "eight-foul-no-solution");
    const std::string beforeFoulNoSolutionHash =
        foulNoSolution.rulesAuthorityStateHash();
    const std::uint64_t beforeFoulNoSolutionRevision =
        foulNoSolution.rulesAuthorityRevision();
    const std::size_t beforeFoulNoSolutionEvents =
        foulNoSolution.events().size();
    const std::size_t beforeFoulNoSolutionActions =
        foulNoSolution.rulesAuthorityActions().size();
    const auto rejectedFoulNoSolution =
        foulNoSolution.submitEightOnBreakDecision(foulNoSolutionRequest);
    expect(!rejectedFoulNoSolution.ok &&
        rejectedFoulNoSolution.errorCode ==
            "rules_respot_no_legal_position" &&
        foulNoSolution.rulesAuthorityStateHash() ==
            beforeFoulNoSolutionHash &&
        foulNoSolution.rulesAuthorityRevision() ==
            beforeFoulNoSolutionRevision &&
        foulNoSolution.events().size() == beforeFoulNoSolutionEvents &&
        foulNoSolution.rulesAuthorityActions().size() ==
            beforeFoulNoSolutionActions &&
        !foulNoSolution.matchState().ballInHand &&
        foulNoSolution.ballInHandPlacementScope() ==
            billiardgl::rules::BallInHandPlacementScope::None &&
        !foulNoSolution.ballInHandPlacementPending() &&
        !foulNoSolution.ballInHandPlacementOffer().has_value(),
        "foul respot failure rolls back without early or residual BIH state");

    billiardgl::GameRuntime changed;
    expect(stageEightOnBreak(changed, false, false),
        "state-change fixture stages 8-on-break decision");
    const auto changedOffer = *changed.eightOnBreakDecisionOffer();
    const auto changedRequest = eightOnBreakRequest(changedOffer,
        billiardgl::rules::kEightOnBreakSpotContinueActionId,
        "eight-state-changed");
    changed.mutableState().balls[2].position.x += 1.0f;
    const AuthoritySnapshot afterPhysicalChange = authoritySnapshot(changed);
    const auto changedRejected =
        changed.submitEightOnBreakDecision(changedRequest);
    expect(!changedRejected.ok &&
        changedRejected.errorCode ==
            "rules_eight_on_break_state_changed" &&
        sameAuthoritySnapshot(afterPhysicalChange,
            authoritySnapshot(changed)),
        "8-on-break revalidates current collision state before any mutation");

    billiardgl::GameRuntime foulChanged;
    expect(stageEightOnBreak(foulChanged, true, false),
        "foul state-change fixture stages 8-on-break decision");
    const auto foulChangedRequest = eightOnBreakRequest(
        *foulChanged.eightOnBreakDecisionOffer(),
        billiardgl::rules::kEightOnBreakSpotBallInHandActionId,
        "eight-foul-state-changed");
    foulChanged.mutableState().balls[2].position.x += 1.0f;
    const AuthoritySnapshot afterFoulPhysicalChange =
        authoritySnapshot(foulChanged);
    const auto foulChangedRejected =
        foulChanged.submitEightOnBreakDecision(foulChangedRequest);
    expect(!foulChangedRejected.ok &&
        foulChangedRejected.errorCode ==
            "rules_eight_on_break_state_changed" &&
        sameAuthoritySnapshot(afterFoulPhysicalChange,
            authoritySnapshot(foulChanged)) &&
        !foulChanged.matchState().ballInHand &&
        !foulChanged.ballInHandPlacementPending() &&
        !foulChanged.ballInHandPlacementOffer().has_value(),
        "foul authority hash drift cannot grant or retain BIH");

    billiardgl::GameRuntime reset;
    expect(stageEightOnBreak(reset, false, false),
        "reset fixture stages 8-on-break decision");
    const auto resetRequest = eightOnBreakRequest(
        *reset.eightOnBreakDecisionOffer(),
        billiardgl::rules::kEightOnBreakSpotContinueActionId,
        "eight-reset-stale");
    reset.reset();
    const AuthoritySnapshot afterReset = authoritySnapshot(reset);
    const auto staleReset = reset.submitEightOnBreakDecision(resetRequest);
    expect(!staleReset.ok &&
        staleReset.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterReset, authoritySnapshot(reset)),
        "full reset makes the old 8-on-break decision stale");

    billiardgl::GameRuntime foulReset;
    expect(stageEightOnBreak(foulReset, true, false),
        "foul reset fixture stages 8-on-break decision");
    const auto foulResetRequest = eightOnBreakRequest(
        *foulReset.eightOnBreakDecisionOffer(),
        billiardgl::rules::kEightOnBreakSpotBallInHandActionId,
        "eight-foul-reset-stale");
    foulReset.reset();
    const AuthoritySnapshot afterFoulReset = authoritySnapshot(foulReset);
    const auto foulResetStale =
        foulReset.submitEightOnBreakDecision(foulResetRequest);
    expect(!foulResetStale.ok &&
        foulResetStale.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterFoulReset,
            authoritySnapshot(foulReset)) &&
        !foulReset.matchState().ballInHand &&
        !foulReset.ballInHandPlacementPending() &&
        !foulReset.ballInHandPlacementOffer().has_value(),
        "foul parent reset invalidates the request without residual BIH");

    billiardgl::GameRuntime rulesetReset;
    expect(stageEightOnBreak(rulesetReset, false, false),
        "ruleset-reset fixture stages 8-on-break decision");
    const auto rulesetRequest = eightOnBreakRequest(
        *rulesetReset.eightOnBreakDecisionOffer(),
        billiardgl::rules::kEightOnBreakSpotContinueActionId,
        "eight-ruleset-reset-stale");
    rulesetReset.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1);
    const AuthoritySnapshot afterRulesetReset =
        authoritySnapshot(rulesetReset);
    const auto staleRuleset =
        rulesetReset.submitEightOnBreakDecision(rulesetRequest);
    expect(!staleRuleset.ok &&
        staleRuleset.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterRulesetReset,
            authoritySnapshot(rulesetReset)),
        "ruleset reset makes the old 8-on-break decision stale");
}

void testLegacyDefaultEquivalence()
{
    billiardgl::GameRuntime implicitLegacy;
    billiardgl::GameRuntime explicitLegacy;
    expect(explicitLegacy.setRulesMode(billiardgl::RulesMode::Legacy).ok,
        "legacy rules mode can be selected explicitly");
    for (billiardgl::GameRuntime* runtime : {&implicitLegacy, &explicitLegacy}) {
        billiardgl::GameAction yaw;
        yaw.type = billiardgl::ActionType::SetAimYaw;
        yaw.first = -billiardgl::kPi / 2.0f;
        billiardgl::GameAction power;
        power.type = billiardgl::ActionType::SetShotPower;
        power.first = 60.0f;
        billiardgl::GameAction shoot;
        shoot.type = billiardgl::ActionType::Shoot;
        expect(runtime->dispatch(yaw).ok && runtime->dispatch(power).ok &&
            runtime->dispatch(shoot).ok && runtime->step(25).ok,
            "legacy equivalence shot executes");
    }
    expect(billiardgl::canonicalGameState(implicitLegacy.state()) ==
        billiardgl::canonicalGameState(explicitLegacy.state()) &&
        implicitLegacy.tick() == explicitLegacy.tick(),
        "default legacy path is semantically identical after rules integration");
}

void testWholeGameStateTransitions()
{
    billiardgl::GameRuntime runtime;
    expect(runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1).ok,
        "WPA Heyball rules enable while stopped");

    auto breakShot = facts(0, 1);
    breakShot.breakShot = true;
    breakShot.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(breakShot).ok &&
        runtime.matchState().phase == billiardgl::rules::MatchPhase::OpenTable &&
        runtime.matchState().currentPlayer == 1,
        "legal dry break opens table and transfers turn");

    auto assignment = facts(1, 1);
    assignment.pocketedBallsInOrder = {1};
    expect(runtime.adjudicateRulesShot(assignment).ok &&
        runtime.matchState().phase == billiardgl::rules::MatchPhase::GroupsAssigned &&
        runtime.matchState().groups[1] == billiardgl::rules::BallGroup::Solids &&
        runtime.matchState().currentPlayer == 1,
        "first legal object pocket assigns groups and retains turn");

    auto foul = facts(1, 9);
    foul.anyRailAfterFirstContact = false;
    expect(runtime.adjudicateRulesShot(foul).ok &&
        runtime.lastAdjudication().foul && runtime.matchState().ballInHand &&
        runtime.matchState().currentPlayer == 0,
        "wrong first contact/no rail produces explained foul and ball in hand");
    expect(runtime.lastAdjudication().reasons.size() == 2 &&
        runtime.lastAdjudication().textKeys.size() == 2,
        "all independent foul reasons have HUD text keys");
    const auto foulSnapshot = billiardgl::buildPresentationSnapshot(
        runtime.state(), runtime.state(), 1.0, runtime.shotControl(),
        runtime.shotControlCapabilities(), runtime.cameraFrame(),
        billiardgl::buildRulesPresentation(runtime));
    expect(foulSnapshot.rules.enabled && foulSnapshot.rules.foul &&
        foulSnapshot.rules.ballInHand &&
        foulSnapshot.rules.reasonCodes.size() == 2 &&
        foulSnapshot.rules.textKeys.size() == 2,
        "presentation snapshot exposes authoritative foul reasons and text keys");

    const auto placementOffer = *runtime.ballInHandPlacementOffer();
    billiardgl::rules::BallInHandPlacementRequest placement;
    placement.decisionId = placementOffer.decisionId;
    placement.expectedAuthorityStateRevision =
        placementOffer.authorityStateRevision;
    placement.idempotencyKey = "whole-game-placement";
    placement.actorPlayerId = placementOffer.actorPlayerId;
    placement.actionId =
        billiardgl::rules::kConfirmFullTablePlacementActionId;
    placement.position = {-20.0, -30.0};
    expect(runtime.submitBallInHandPlacement(placement).ok &&
        runtime.matchState().ballInHand,
        "whole-game fixture confirms awarded full-table ball in hand");

    expect(!runtime.adjudicateRulesShot(facts(0, 8)).ok,
        "semantic facts cannot bypass retained ball-in-hand before a positive launch");
}

void testBreakAndEightBallFailures()
{
    billiardgl::GameRuntime illegalBreak;
    illegalBreak.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
    auto weak = facts(0, 1);
    weak.breakShot = true;
    weak.distinctObjectBallsToRail = 3;
    attestStraightBreakCushions(weak, {1, 2, 3});
    expect(illegalBreak.adjudicateRulesShot(weak).ok &&
        !illegalBreak.lastAdjudication().foul &&
        illegalBreak.lastAdjudication().rerackAllowed &&
        illegalBreak.lastAdjudication().decisionOptions.size() == 3,
        "illegal break exposes accept/rerack decisions without generic foul");
    const auto breakPresentation = billiardgl::buildRulesPresentation(illegalBreak);
    expect(breakPresentation.reasonCodes.size() == 1 &&
        breakPresentation.decisionOptions.empty() &&
        illegalBreak.breakFoulDecisionPending(),
        "presentation keeps runtime-only illegal-break actions hidden");

    billiardgl::GameRuntime earlyEight;
    earlyEight.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
    auto opening = facts(0, 1);
    opening.breakShot = true;
    opening.distinctObjectBallsToRail = 4;
    earlyEight.adjudicateRulesShot(opening);
    auto assign = facts(1, 1);
    assign.pocketedBallsInOrder = {1};
    earlyEight.adjudicateRulesShot(assign);
    auto early = facts(1, 8);
    early.solidsRemainingBefore = 2;
    early.pocketedBallsInOrder = {8};
    expect(earlyEight.adjudicateRulesShot(early).ok &&
        earlyEight.matchState().winner == 0 &&
        earlyEight.lastAdjudication().foul,
        "early 8-ball loses the game with explicit foul");
}

void testSelectedBreakThresholdRuntimeAuthority()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;

    GameRuntime direct;
    direct.setRulesMode(RulesMode::ChineseEightBallV1);
    auto directWeak = facts(0, 1);
    directWeak.breakShot = true;
    directWeak.distinctObjectBallsToRail = 3;
    const AuthoritySnapshot beforeDirect = authoritySnapshot(direct);
    const auto directResult = direct.adjudicateRulesShot(directWeak);
    expect(!directResult.ok &&
        std::string(directResult.errorCode) ==
            "rules_illegal_break_requires_authoritative_evidence" &&
        !direct.breakFoulDecisionPending() &&
        sameAuthoritySnapshot(beforeDirect, authoritySnapshot(direct)),
        "direct synthetic threshold facts cannot open the product illegal-break authority");

    GameRuntime incomplete;
    incomplete.setRulesMode(RulesMode::ChineseEightBallV1);
    auto incompleteFacts = facts(0, 1);
    incompleteFacts.breakShot = true;
    attestStraightBreakCushions(incompleteFacts, {1, 2, 3, 4});
    incompleteFacts.breakEvidenceComplete = false;
    const AuthoritySnapshot beforeIncomplete = authoritySnapshot(incomplete);
    const auto incompleteResult =
        incomplete.adjudicateRulesShot(incompleteFacts);
    expect(!incompleteResult.ok &&
        std::string(incompleteResult.errorCode) ==
            "rules_facts_break_evidence_invalid" &&
        sameAuthoritySnapshot(beforeIncomplete,
            authoritySnapshot(incomplete)),
        "incomplete authoritative threshold provenance rejects before any runtime mutation");

    GameRuntime mixed;
    mixed.setRulesMode(RulesMode::ChineseEightBallV1);
    auto mixedFacts = facts(0, 1);
    mixedFacts.breakShot = true;
    attestBreakCushions(mixedFacts, {
        {1, BreakCushionBoundaryKind::StraightCushion, true},
        {2, BreakCushionBoundaryKind::StraightCushion, true},
        {3, BreakCushionBoundaryKind::StraightCushion, true},
        {4, BreakCushionBoundaryKind::PocketJaw, true},
        {5, BreakCushionBoundaryKind::StraightCushion, false}});
    expect(mixed.adjudicateRulesShot(mixedFacts).ok &&
        mixed.breakFoulDecisionOffer() &&
        mixed.breakFoulDecisionOffer()->branch ==
            BreakFoulBranch::IllegalBreak,
        "only three qualifying straight impulses stage the authoritative illegal-break offer despite five scalar objects");

    GameRuntime straight;
    straight.setRulesMode(RulesMode::ChineseEightBallV1);
    auto straightFacts = facts(0, 1);
    straightFacts.breakShot = true;
    attestStraightBreakCushions(straightFacts, {1, 2, 3, 4});
    expect(straight.adjudicateRulesShot(straightFacts).ok &&
        !straight.breakFoulDecisionPending() &&
        !hasReason(straight.lastAdjudication(), FoulReason::IllegalBreak),
        "four canonical distinct straight impulses are legal without product reclassification");

    GameRuntime pocket;
    pocket.setRulesMode(RulesMode::ChineseEightBallV1);
    auto pocketFacts = facts(0, 1);
    pocketFacts.breakShot = true;
    attestStraightBreakCushions(pocketFacts, {});
    pocketFacts.pocketedBallsInOrder = {9};
    expect(pocket.adjudicateRulesShot(pocketFacts).ok &&
        !pocket.breakFoulDecisionPending() &&
        !hasReason(pocket.lastAdjudication(), FoulReason::IllegalBreak),
        "canonical object pocket independently satisfies the break threshold");

    GameRuntime scratch;
    scratch.setRulesMode(RulesMode::ChineseEightBallV1);
    auto scratchFacts = facts(0, 1);
    scratchFacts.breakShot = true;
    attestStraightBreakCushions(scratchFacts, {});
    attestCueScratch(scratchFacts);
    expect(scratch.adjudicateRulesShot(scratchFacts).ok &&
        hasReason(scratch.lastAdjudication(), FoulReason::CueBallScratch) &&
        hasReason(scratch.lastAdjudication(), FoulReason::IllegalBreak) &&
        scratch.breakFoulDecisionOffer() &&
        scratch.breakFoulDecisionOffer()->branch ==
            BreakFoulBranch::BreakShotFoul,
        "cue scratch is a separate foul and never substitutes for the object-pocket legal branch");
}

void testBreakFoulOpponentChoiceAuthority()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;

    GameRuntime legal;
    legal.setRulesMode(RulesMode::ChineseEightBallV1);
    auto legalBreak = facts(0, 1);
    legalBreak.breakShot = true;
    legalBreak.distinctObjectBallsToRail = 4;
    expect(legal.adjudicateRulesShot(legalBreak).ok &&
        !legal.breakFoulDecisionPending(),
        "legal break does not create a break-foul authority offer");

    GameRuntime noRackContact;
    noRackContact.setRulesMode(RulesMode::ChineseEightBallV1);
    const AuthoritySnapshot beforeNoRack = authoritySnapshot(noRackContact);
    ShotFacts ambiguousMiscue;
    ambiguousMiscue.shooter = 0;
    ambiguousMiscue.breakShot = true;
    const auto noRackResult =
        noRackContact.adjudicateRulesShot(ambiguousMiscue);
    expect(!noRackResult.ok &&
        std::string(noRackResult.errorCode) ==
            "rules_break_no_rack_contact_classification_pending" &&
        !noRackContact.breakFoulDecisionPending() &&
        sameAuthoritySnapshot(beforeNoRack,
            authoritySnapshot(noRackContact)),
        "WPA II 6(g) stays fail-closed because ShotFacts cannot prove an unintentional miscue");

    GameRuntime illegal;
    illegal.setRulesMode(RulesMode::ChineseEightBallV1);
    auto weak = facts(0, 1);
    weak.breakShot = true;
    weak.distinctObjectBallsToRail = 3;
    attestStraightBreakCushions(weak, {1, 2, 3});
    expect(illegal.adjudicateRulesShot(weak).ok &&
        illegal.breakFoulDecisionOffer().has_value(),
        "WPA II 6(c) illegal break stages an opponent authority offer");
    const auto illegalOffer = *illegal.breakFoulDecisionOffer();
    expect(illegalOffer.branch == BreakFoulBranch::IllegalBreak &&
        illegalOffer.actorPlayerId == 1 &&
        illegalOffer.originalBreakerPlayerId == 0 &&
        illegalOffer.frozenNextPlayer == 1 &&
        illegalOffer.sourceClause == "WPA-II-06-C" &&
        illegalOffer.sourcePdfPage == 7 &&
        illegalOffer.offeredActionIds == std::vector<std::string>{
            kBreakFoulAcceptTableActionId,
            kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId} &&
        !illegal.matchState().ballInHand &&
        !illegal.ballInHandPlacementPending(),
        "illegal-break offer freezes actor, breaker, source, and exact no-BIH actions");

    GameAction shoot;
    shoot.type = ActionType::Shoot;
    const AuthoritySnapshot pendingSnapshot = authoritySnapshot(illegal);
    const auto blockedShot = illegal.dispatch(shoot);
    const auto blockedCue = illegal.applyCueImpact({});
    const auto blockedAdjudication = illegal.adjudicateRulesShot(weak);
    RerackFoundationParameters bypassParameters;
    bypassParameters.breakerPlayerId = 0;
    const auto blockedFoundation =
        illegal.stageOfficialRerack(bypassParameters);
    const auto hidden = buildRulesPresentation(illegal);
    expect(!blockedShot.ok &&
        std::string(blockedShot.errorCode) ==
            "rules_break_foul_decision_pending" &&
        !blockedCue.ok &&
        std::string(blockedCue.errorCode) ==
            "rules_break_foul_decision_pending" &&
        !blockedAdjudication.ok &&
        std::string(blockedAdjudication.errorCode) ==
            "rules_break_foul_decision_pending" &&
        !blockedFoundation.ok &&
        blockedFoundation.errorCode ==
            "rules_break_foul_decision_pending" &&
        hidden.decisionOptions.empty() &&
        sameAuthoritySnapshot(pendingSnapshot, authoritySnapshot(illegal)),
        "break-foul pending blocks shot, direct cue, adjudication, Core E bypass, and Product actions");

    auto valid = breakFoulRequest(illegalOffer,
        kBreakFoulAcceptTableActionId, "break-accept");
    auto bad = valid;
    bad.capabilityId = kEightOnBreakCapabilityId;
    expect(!illegal.submitBreakFoulDecision(bad).ok,
        "break-foul choice rejects wrong capability context");
    bad = valid;
    bad.actorPlayerId = 0;
    expect(illegal.submitBreakFoulDecision(bad).errorCode ==
        "rules_decision_actor_mismatch",
        "break-foul choice rejects the original breaker as actor");
    bad = valid;
    bad.actionId = kBreakFoulBallInHandActionId;
    expect(illegal.submitBreakFoulDecision(bad).errorCode ==
        "rules_decision_action_not_offered",
        "illegal-break branch rejects the foul-only BIH action");
    bad = valid;
    ++bad.expectedAuthorityStateRevision;
    expect(illegal.submitBreakFoulDecision(bad).errorCode ==
        "rules_decision_stale_revision",
        "break-foul choice rejects a stale revision");
    bad = valid;
    bad.payloadCanonical = "{\"tampered\":true}";
    expect(illegal.submitBreakFoulDecision(bad).errorCode ==
        "rules_decision_invalid_payload" &&
        sameAuthoritySnapshot(pendingSnapshot, authoritySnapshot(illegal)),
        "break-foul validation errors have zero authoritative mutation");

    const std::size_t eventsBefore = illegal.events().size();
    const std::size_t actionsBefore = illegal.rulesAuthorityActions().size();
    const std::uint64_t revisionBefore = illegal.rulesAuthorityRevision();
    const auto accepted = illegal.submitBreakFoulDecision(valid);
    expect(accepted.ok && accepted.ack.effects.tableAccepted &&
        !accepted.ack.effects.rackRebuilt &&
        accepted.ack.effects.currentPlayer == 1 &&
        !accepted.ack.effects.ballInHand &&
        !accepted.ack.effects.childDecisionPending &&
        !illegal.breakFoulDecisionPending() &&
        illegal.matchState().phase == MatchPhase::OpenTable &&
        illegal.rulesAuthorityRevision() == revisionBefore + 1 &&
        illegal.events().size() == eventsBefore + 1 &&
        illegal.rulesAuthorityActions().size() == actionsBefore + 1&&
        !illegal.rulesReplayEvents().empty()&&
        illegal.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::BreakFoulChoiceCommit,
        "accept-table commits one revision/event/action and preserves the opponent turn without BIH");
    const AuthoritySnapshot afterAccept = authoritySnapshot(illegal);
    const auto retry = illegal.submitBreakFoulDecision(valid);
    bad = valid;
    bad.actionId = kBreakFoulRerackOriginalActionId;
    const auto conflict = illegal.submitBreakFoulDecision(bad);
    expect(retry.ok &&
        retry.ack.authoritativeStateHash ==
            accepted.ack.authoritativeStateHash &&
        !conflict.ok &&
        conflict.errorCode == "rules_decision_idempotency_conflict" &&
        sameAuthoritySnapshot(afterAccept, authoritySnapshot(illegal)),
        "break-foul exact retry returns one ACK and changed retry conflicts without mutation");

    GameRuntime foul;
    foul.setRulesMode(RulesMode::ChineseEightBallV1);
    auto scratched = facts(0, 1);
    scratched.breakShot = true;
    scratched.distinctObjectBallsToRail = 4;
        attestCueScratch(scratched);
    expect(foul.adjudicateRulesShot(scratched).ok &&
        foul.breakFoulDecisionOffer().has_value() &&
        !foul.matchState().ballInHand,
        "WPA II 6(e) break foul freezes BIH until the parent choice commits");
    const auto foulOffer = *foul.breakFoulDecisionOffer();
    expect(foulOffer.branch == BreakFoulBranch::BreakShotFoul &&
        foulOffer.sourceClause == "WPA-II-06-E" &&
        foulOffer.sourcePdfPage == 7 &&
        foulOffer.offeredActionIds == std::vector<std::string>{
            kBreakFoulBallInHandActionId,
            kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId} &&
        std::find(foulOffer.offeredActionIds.begin(),
            foulOffer.offeredActionIds.end(),
            kBreakFoulAcceptTableActionId) ==
                foulOffer.offeredActionIds.end(),
        "break-shot-foul offer exposes exactly BIH and the two rerack choices");
    const auto bih = foul.submitBreakFoulDecision(breakFoulRequest(
        foulOffer, kBreakFoulBallInHandActionId, "break-bih"));
    expect(bih.ok && bih.ack.effects.ballInHand &&
        bih.ack.effects.ballInHandScope ==
            BallInHandPlacementScope::BehindHeadString &&
        bih.ack.effects.childDecisionPending &&
        !foul.breakFoulDecisionPending() &&
        !foul.rulesReplayEvents().empty()&&
        foul.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::BreakFoulChoiceCommit&&
        foul.ballInHandPlacementOffer() &&
        foul.ballInHandPlacementOffer()->actorPlayerId == 1 &&
        foul.ballInHandPlacementOffer()->sourceClause == "WPA-II-08",
        "BIH parent commit atomically creates the scoped Core G child offer");

    GameRuntime foulRerack;
    foulRerack.setRulesMode(RulesMode::ChineseEightBallV1);
    foulRerack.adjudicateRulesShot(scratched);
    const auto foulReracked = foulRerack.submitBreakFoulDecision(
        breakFoulRequest(*foulRerack.breakFoulDecisionOffer(),
            kBreakFoulRerackOriginalActionId, "foul-rerack-original"));
    expect(foulReracked.ok && foulReracked.ack.effects.rackRebuilt &&
        !foulReracked.ack.effects.ballInHand &&
        !foulReracked.ack.effects.childDecisionPending &&
        foulReracked.ack.effects.breakerPlayerId == 0 &&
        foulRerack.matchState().phase == MatchPhase::Break,
        "WPA II 6(e) rerack choice rebuilds atomically without creating a BIH child");

    GameRuntime staleTerminal;
    staleTerminal.setRulesMode(RulesMode::ChineseEightBallV1);
    expect(staleTerminal.adjudicateRulesShot(scratched).ok &&
        staleTerminal.breakFoulDecisionOffer().has_value(),
        "terminal guard fixture first stages an ordinary live break-foul offer");
    const auto staleTerminalRequest = breakFoulRequest(
        *staleTerminal.breakFoulDecisionOffer(),
        kBreakFoulRerackOriginalActionId, "terminal-stale-rerack");
    // Model a retained offer crossing a terminal boundary. The normal
    // adjudication path now clears/suppresses it, while the submit boundary is
    // independently responsible for preventing a stale rerack from reviving
    // a completed game.
    staleTerminal.mutableState().gameOver = true;
    const AuthoritySnapshot terminalBefore = authoritySnapshot(staleTerminal);
    const auto blockedTerminalRerack =
        staleTerminal.submitBreakFoulDecision(staleTerminalRequest);
    expect(!blockedTerminalRerack.ok &&
        blockedTerminalRerack.errorCode == "game_over" &&
        staleTerminal.breakFoulDecisionPending() &&
        sameAuthoritySnapshot(terminalBefore,
            authoritySnapshot(staleTerminal)),
        "a retained break-foul offer cannot rerack or mutate a terminal game");

    GameRuntime rerackChain;
    rerackChain.setRulesMode(RulesMode::ChineseEightBallV1);
    expect(rerackChain.adjudicateRulesShot(weak).ok,
        "opponent-break rerack fixture stages illegal-break offer");
    const auto opponentRerack = rerackChain.submitBreakFoulDecision(
        breakFoulRequest(*rerackChain.breakFoulDecisionOffer(),
            kBreakFoulRerackOpponentActionId, "break-rerack-opponent"));
    expect(opponentRerack.ok && opponentRerack.ack.effects.rackRebuilt &&
        opponentRerack.ack.effects.breakerPlayerId == 1 &&
        rerackChain.matchState().phase == MatchPhase::Break &&
        rerackChain.matchState().currentPlayer == 1,
        "Core E rerack makes the opposing chooser the next breaker");
    auto secondWeak = facts(1, 1);
    secondWeak.breakShot = true;
    secondWeak.distinctObjectBallsToRail = 3;
    attestStraightBreakCushions(secondWeak, {1, 2, 3});
    expect(rerackChain.adjudicateRulesShot(secondWeak).ok &&
        rerackChain.breakFoulDecisionOffer()->actorPlayerId == 0 &&
        rerackChain.breakFoulDecisionOffer()->originalBreakerPlayerId == 1,
        "reracked game freezes the new original breaker and opposite actor");
    const auto originalRerack = rerackChain.submitBreakFoulDecision(
        breakFoulRequest(*rerackChain.breakFoulDecisionOffer(),
            kBreakFoulRerackOriginalActionId, "break-rerack-original"));
    expect(originalRerack.ok &&
        originalRerack.ack.effects.breakerPlayerId == 1 &&
        rerackChain.matchState().currentPlayer == 1,
        "original-breaker action reracks with the frozen original breaker");

    GameRuntime drift;
    drift.setRulesMode(RulesMode::ChineseEightBallV1);
    drift.adjudicateRulesShot(weak);
    const auto driftRequest = breakFoulRequest(
        *drift.breakFoulDecisionOffer(), kBreakFoulAcceptTableActionId,
        "break-drift");
    drift.mutableState().balls[2].position.x += 0.5f;
    const AuthoritySnapshot afterDrift = authoritySnapshot(drift);
    const auto drifted = drift.submitBreakFoulDecision(driftRequest);
    expect(!drifted.ok &&
        drifted.errorCode == "rules_break_foul_state_changed" &&
        sameAuthoritySnapshot(afterDrift, authoritySnapshot(drift)),
        "break-foul commit revalidates physical authority state with zero mutation on drift");

    GameRuntime reset;
    reset.setRulesMode(RulesMode::ChineseEightBallV1);
    reset.adjudicateRulesShot(weak);
    const auto staleRequest = breakFoulRequest(
        *reset.breakFoulDecisionOffer(), kBreakFoulAcceptTableActionId,
        "break-reset");
    reset.reset();
    const AuthoritySnapshot afterReset = authoritySnapshot(reset);
    const auto stale = reset.submitBreakFoulDecision(staleRequest);
    expect(!stale.ok && stale.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterReset, authoritySnapshot(reset)),
        "reset invalidates a break-foul offer and keeps stale submit non-mutating");

    GameRuntime rulesetReset;
    rulesetReset.setRulesMode(RulesMode::ChineseEightBallV1);
    rulesetReset.adjudicateRulesShot(weak);
    const auto rulesetStaleRequest = breakFoulRequest(
        *rulesetReset.breakFoulDecisionOffer(),
        kBreakFoulAcceptTableActionId, "break-ruleset-reset");
    rulesetReset.setRulesMode(RulesMode::Legacy);
    rulesetReset.setRulesMode(RulesMode::ChineseEightBallV1);
    const AuthoritySnapshot afterRulesetReset =
        authoritySnapshot(rulesetReset);
    const auto rulesetStale =
        rulesetReset.submitBreakFoulDecision(rulesetStaleRequest);
    expect(!rulesetStale.ok &&
        rulesetStale.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterRulesetReset,
            authoritySnapshot(rulesetReset)),
        "ruleset reset invalidates a break-foul offer without mutation on stale submit");
}

void testUnintentionalBreakMiscueNativeAuthority()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;

    const std::string proposalPayload =
        "{\"actor_player_id\":0,\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
    const std::string finalPayload =
        "{\"actor_player_id\":0,\"proposal_decision_id\":\"runtime.unintentional_break_miscue.1\",\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
    const auto miscueInput = [](const GameRuntime& runtime) {
        CueImpactInput input;
        input.cueBallIndex = 0;
        input.cueSpeedCmS = 100.0;
        input.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
        input.direction = {{0.0, 0.0, 1.0}};
        input.tipOffsetRadius = {{0.81, 0.0}};
        input.tipOffsetCm = {{
            0.81 * runtime.physicsProfile().ball.radiusCm, 0.0}};
        input.chalkState = "CHALKED";
        return input;
    };
    const auto prepare = [](GameRuntime& runtime) {
        return runtime.replaceStateForScenario(runtime.state(),
                   interactiveChinesePoolPhysicsProfile()).ok &&
            runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok;
    };

    StalemateTestAuthenticator authenticator;
    GameRuntime runtime;
    expect(prepare(runtime),
        "06G native fixture selects shipping rules and physics");
    runtime.configureUnintentionalBreakMiscueDeclarationAuthority(
        &authenticator);
    auto proposal = unintentionalBreakMiscueRequest(runtime,
        RefereeAuthorityOperation::Propose, "06g.proposal",
        proposalPayload);
    expect(runtime.submitUnintentionalBreakMiscueDeclarationProposal(
               proposal).ok,
        "06G signed referee proposal opens the shot-bound declaration");
    auto final = unintentionalBreakMiscueRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare, "06g.final",
        finalPayload);
    expect(runtime.submitUnintentionalBreakMiscueDeclarationFinal(final).ok &&
        runtime.unintentionalBreakMiscueDeclarationState().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Active,
        "06G signed referee final activates the exact break-shot declaration");

    const std::uint64_t preShotRevision = runtime.rulesAuthorityRevision();
    expect(runtime.applyCueImpact(miscueInput(runtime)).ok &&
        !runtime.cueContactResult().applied &&
        runtime.cueContactResult().regime == CueContactRegime::Miscue &&
        runtime.cueContactResult().error ==
            "miscue_offset_exceeds_reliable_radius" &&
        runtime.unintentionalBreakMiscueDeclarationState().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Consumed,
        "real cue solver miscue consumes the signed 06G declaration without fabricating impulse");
    expect(runtime.step(1).ok &&
        runtime.rulesShotCompletionError().empty() &&
        runtime.hasLastShotFacts() &&
        runtime.lastShotFacts().breakEvidenceOrigin ==
            BreakEvidenceOrigin::AuthoritativeShotFactCollector &&
        runtime.lastShotFacts().breakEvidenceComplete &&
        runtime.lastShotFacts().firstContactBall < 1 &&
        runtime.lastShotFacts().firstContactBalls.empty(),
        "stationary native miscue completes after one successful collector tick with complete no-contact evidence");
    expect(runtime.breakFoulDecisionOffer().has_value() &&
        runtime.breakFoulDecisionOffer()->branch ==
            BreakFoulBranch::NoRackContactMiscue &&
        runtime.breakFoulDecisionOffer()->sourceClause == "WPA-II-06-G" &&
        runtime.breakFoulDecisionOffer()->sourcePdfPage == 8 &&
        runtime.breakFoulDecisionOffer()->offeredActionIds ==
            std::vector<std::string>{
                kBreakFoulRerackOpponentActionId,
                kBreakFoulRerackOriginalActionId} &&
        !runtime.matchState().ballInHand &&
        !runtime.ballInHandPlacementPending() &&
        runtime.rulesAuthorityRevision() == preShotRevision + 1,
        "06G native completion publishes exactly the two rerack choices and no BIH");
    const auto offer = *runtime.breakFoulDecisionOffer();
    const auto reracked = runtime.submitBreakFoulDecision(
        breakFoulRequest(offer, kBreakFoulRerackOpponentActionId,
            "06g.rerack.opponent"));
    expect(reracked.ok && reracked.ack.effects.rackRebuilt &&
        !reracked.ack.effects.ballInHand &&
        reracked.ack.effects.currentPlayer == offer.actorPlayerId &&
        runtime.matchState().phase == MatchPhase::Break,
        "06G selected rerack commits through the ordinary typed break authority");

    GameRuntime unsignedRuntime;
    expect(prepare(unsignedRuntime),
        "unsigned 06G negative fixture selects shipping rules and physics");
    const auto unsignedRevision = unsignedRuntime.rulesAuthorityRevision();
    const auto unsignedImpact = unsignedRuntime.applyCueImpact(
        miscueInput(unsignedRuntime));
    const auto unsignedStep = unsignedRuntime.step(1);
    if (!unsignedImpact.ok || unsignedStep.ok ||
        std::string(unsignedStep.errorCode) !=
            "rules_break_no_rack_contact_classification_pending" ||
        unsignedRuntime.rulesShotCompletionError() !=
            "rules_break_no_rack_contact_classification_pending" ||
        unsignedRuntime.breakFoulDecisionPending() ||
        unsignedRuntime.rulesAuthorityRevision() != unsignedRevision)
        std::cerr << "06G unsigned diagnostic impact=" << unsignedImpact.ok
                  << ':' << unsignedImpact.errorCode
                  << " step=" << unsignedStep.ok << ':'
                  << unsignedStep.errorCode << " completion="
                  << unsignedRuntime.rulesShotCompletionError()
                  << " pending="
                  << unsignedRuntime.breakFoulDecisionPending()
                  << " revision="
                  << unsignedRuntime.rulesAuthorityRevision() << '/'
                  << unsignedRevision << '\n';
    expect(unsignedImpact.ok && !unsignedStep.ok &&
        std::string(unsignedStep.errorCode) ==
            "rules_break_no_rack_contact_classification_pending" &&
        unsignedRuntime.rulesShotCompletionError() ==
            "rules_break_no_rack_contact_classification_pending" &&
        !unsignedRuntime.breakFoulDecisionPending() &&
        unsignedRuntime.rulesAuthorityRevision() == unsignedRevision,
        "native mechanical miscue without signed intent declaration stays fail-closed with no rules mutation");

    GameRuntime proposalOnly;
    expect(prepare(proposalOnly),
        "proposal-only 06G negative fixture selects shipping rules and physics");
    proposalOnly.configureUnintentionalBreakMiscueDeclarationAuthority(
        &authenticator);
    proposal = unintentionalBreakMiscueRequest(proposalOnly,
        RefereeAuthorityOperation::Propose, "06g.proposal.only",
        proposalPayload);
    const auto proposalAccepted =
        proposalOnly.submitUnintentionalBreakMiscueDeclarationProposal(
            proposal);
    const auto proposalImpact =
        proposalOnly.applyCueImpact(miscueInput(proposalOnly));
    const auto proposalStep = proposalOnly.step(1);
    if (!proposalAccepted.ok || !proposalImpact.ok || proposalStep.ok ||
        std::string(proposalStep.errorCode) !=
            "rules_break_no_rack_contact_classification_pending" ||
        proposalOnly.rulesShotCompletionError() !=
            "rules_break_no_rack_contact_classification_pending" ||
        proposalOnly.breakFoulDecisionPending())
        std::cerr << "06G proposal diagnostic proposal="
                  << proposalAccepted.ok << ':' << proposalAccepted.errorCode
                  << " impact=" << proposalImpact.ok << ':'
                  << proposalImpact.errorCode << " step=" << proposalStep.ok
                  << ':' << proposalStep.errorCode << " completion="
                  << proposalOnly.rulesShotCompletionError() << " pending="
                  << proposalOnly.breakFoulDecisionPending() << '\n';
    expect(proposalAccepted.ok && proposalImpact.ok && !proposalStep.ok &&
        std::string(proposalStep.errorCode) ==
            "rules_break_no_rack_contact_classification_pending" &&
        proposalOnly.rulesShotCompletionError() ==
            "rules_break_no_rack_contact_classification_pending" &&
        !proposalOnly.breakFoulDecisionPending(),
        "proposal without signed final cannot classify a native miscue as 06G");

    GameRuntime imported;
    expect(prepare(imported),
        "imported-facts 06G negative fixture selects shipping rules and physics");
    imported.configureUnintentionalBreakMiscueDeclarationAuthority(
        &authenticator);
    proposal = unintentionalBreakMiscueRequest(imported,
        RefereeAuthorityOperation::Propose, "06g.imported.proposal",
        proposalPayload);
    expect(imported.submitUnintentionalBreakMiscueDeclarationProposal(
               proposal).ok,
        "imported-facts 06G fixture opens its declaration");
    final = unintentionalBreakMiscueRequest(imported,
        RefereeAuthorityOperation::FinalDeclare, "06g.imported.final",
        finalPayload);
    expect(imported.submitUnintentionalBreakMiscueDeclarationFinal(final).ok,
        "imported-facts 06G fixture commits its declaration");
    ShotFacts importedFacts;
    importedFacts.shooter = 0;
    importedFacts.breakShot = true;
    const auto importedRevision = imported.rulesAuthorityRevision();
    const auto importedResult = imported.adjudicateRulesShot(importedFacts);
    expect(!importedResult.ok &&
        std::string(importedResult.errorCode) ==
            "rules_break_no_rack_contact_classification_pending" &&
        !imported.breakFoulDecisionPending() &&
        imported.rulesAuthorityRevision() == importedRevision,
        "signed declaration cannot turn imported facts without private native stroke evidence into 06G");

    GameRuntime normalStroke;
    expect(prepare(normalStroke),
        "normal-stroke 06G negative fixture selects shipping rules and physics");
    normalStroke.configureUnintentionalBreakMiscueDeclarationAuthority(
        &authenticator);
    proposal = unintentionalBreakMiscueRequest(normalStroke,
        RefereeAuthorityOperation::Propose, "06g.normal.proposal",
        proposalPayload);
    expect(normalStroke.submitUnintentionalBreakMiscueDeclarationProposal(
               proposal).ok,
        "normal-stroke 06G fixture opens its declaration");
    final = unintentionalBreakMiscueRequest(normalStroke,
        RefereeAuthorityOperation::FinalDeclare, "06g.normal.final",
        finalPayload);
    expect(normalStroke.submitUnintentionalBreakMiscueDeclarationFinal(final).ok,
        "normal-stroke 06G fixture commits its declaration");
    CueImpactInput center = miscueInput(normalStroke);
    center.tipOffsetRadius = {{0.0, 0.0}};
    center.tipOffsetCm = {{0.0, 0.0}};
    expect(normalStroke.applyCueImpact(center).ok &&
        normalStroke.cueContactResult().applied &&
        normalStroke.unintentionalBreakMiscueDeclarationState().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Invalidated,
        "a normal positive stroke invalidates 06G permission so it cannot leak to a later shot");
}

void testRulesCoreAOutcomes()
{
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto breakShot = facts(0, 1);
        breakShot.breakShot = true;
        breakShot.pocketedBallsInOrder = {8};
        expect(runtime.adjudicateRulesShot(breakShot).ok &&
            !hasReason(runtime.lastAdjudication(),
                billiardgl::rules::FoulReason::IllegalBreak),
            "runtime counts a break-pocketed 8 as an object ball");
    }
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto breakShot = facts(0, 1);
        breakShot.breakShot = true;
        breakShot.distinctObjectBallsToRail = 4;
        runtime.adjudicateRulesShot(breakShot);
        auto assignment = facts(1, 1);
        assignment.pocketedBallsInOrder = {1};
        runtime.adjudicateRulesShot(assignment);
        auto legalEight = facts(1, 8);
        legalEight.solidsRemainingBefore = 0;
        legalEight.anyRailAfterFirstContact = false;
        legalEight.pocketedBallsInOrder = {8};
        expect(runtime.adjudicateRulesShot(legalEight).ok &&
            runtime.matchState().winner == 1 &&
            !hasReason(runtime.lastAdjudication(),
                billiardgl::rules::FoulReason::NoRailAfterContact),
            "runtime legal 8 needs no extra rail after the pocket");
    }
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto breakShot = facts(0, 1);
        breakShot.breakShot = true;
        breakShot.distinctObjectBallsToRail = 4;
        runtime.adjudicateRulesShot(breakShot);
        auto eightOffTable = facts(1, 1);
        eightOffTable.objectBallsOffTable = {8};
        expect(runtime.adjudicateRulesShot(eightOffTable).ok &&
            runtime.matchState().winner == 0 &&
            runtime.lastAdjudication().gameOver &&
            hasReason(runtime.lastAdjudication(),
                billiardgl::rules::FoulReason::IllegalEightBall),
            "runtime treats 8 off table as immediate loss");
    }
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto breakShot = facts(0, 1);
        breakShot.breakShot = true;
        breakShot.distinctObjectBallsToRail = 4;
        runtime.adjudicateRulesShot(breakShot);
        auto assignment = facts(1, 1);
        assignment.pocketedBallsInOrder = {1};
        runtime.adjudicateRulesShot(assignment);
        auto combined = facts(1, 7);
        combined.solidsRemainingBefore = 1;
        combined.anyRailAfterFirstContact = false;
        combined.pocketedBallsInOrder = {7, 8};
        expect(runtime.adjudicateRulesShot(combined).ok &&
            runtime.matchState().winner == 0 &&
            hasReason(runtime.lastAdjudication(),
                billiardgl::rules::FoulReason::IllegalEightBall),
            "runtime uses pre-shot remaining count for group-ball plus 8");
    }
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto invalid = facts(0, 1);
        invalid.breakShot = true;
        invalid.pocketedBallsInOrder = {1, 1};
        const auto duplicatePocket = runtime.adjudicateRulesShot(invalid);
        expect(!duplicatePocket.ok &&
            std::string(duplicatePocket.errorCode) == "rules_facts_duplicate",
            "runtime rejects duplicate pocket facts with a stable code");
        invalid.pocketedBallsInOrder = {8};
        invalid.objectBallsOffTable = {8};
        const auto conflict = runtime.adjudicateRulesShot(invalid);
        expect(!conflict.ok &&
            std::string(conflict.errorCode) == "rules_facts_conflict" &&
            runtime.matchState().phase == billiardgl::rules::MatchPhase::Break,
            "runtime rejects 8 pocket/off-table conflict without mutation");
        invalid.pocketedBallsInOrder = {16};
        invalid.objectBallsOffTable.clear();
        const auto outOfRange = runtime.adjudicateRulesShot(invalid);
        expect(!outOfRange.ok &&
            std::string(outOfRange.errorCode) == "rules_facts_out_of_range",
            "runtime rejects out-of-range facts with a stable code");
        invalid.pocketedBallsInOrder.clear();
        attestCueScratch(invalid);
        invalid.cueBallOffTable = true;
        const auto cueConflict = runtime.adjudicateRulesShot(invalid);
        expect(!cueConflict.ok &&
            std::string(cueConflict.errorCode) == "rules_facts_conflict",
            "runtime rejects mutually exclusive cue-ball terminal facts");
        invalid.cueBallPocketed = false;
        invalid.cueBallOffTable = false;
        invalid.eightBallPocket = 2;
        const auto orphanPocket = runtime.adjudicateRulesShot(invalid);
        expect(!orphanPocket.ok &&
            std::string(orphanPocket.errorCode) == "rules_facts_conflict",
            "runtime rejects an orphan 8-ball pocket id");
    }
}

void testRulesCoreBSimultaneousContacts()
{
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto breakShot = facts(0, 1);
        breakShot.breakShot = true;
        breakShot.distinctObjectBallsToRail = 4;
        runtime.adjudicateRulesShot(breakShot);
        auto choice = facts(1, 1);
        choice.firstContactBalls = {1, 9};
        choice.pocketedBallsInOrder = {1, 9};
        expect(runtime.adjudicateRulesShot(choice).ok &&
            runtime.lastAdjudication().groupChoiceRequired &&
            runtime.lastAdjudication().decisionOptions ==
                std::vector<std::string>({
                    "rules.group_choice.choose_solids",
                    "rules.group_choice.choose_stripes"}) &&
            runtime.matchState().phase ==
                billiardgl::rules::MatchPhase::OpenTable &&
            runtime.matchState().groups[0] ==
                billiardgl::rules::BallGroup::Open &&
            runtime.matchState().groups[1] ==
                billiardgl::rules::BallGroup::Open,
            "runtime stages but does not execute simultaneous group choice");
        expect(runtime.matchState().groupChoicePending,
            "group choice is stored as an authoritative pending gate");
        expect(billiardgl::buildRulesPresentation(runtime)
                   .decisionOptions.empty(),
            "group-choice staging is not exposed through product presentation");
        const std::string stateBeforeBlockedActions =
            billiardgl::canonicalGameState(runtime.state());
        const std::uint64_t tickBeforeBlockedActions = runtime.tick();
        const std::vector<billiardgl::RuntimeEvent> eventsBeforeBlock =
            runtime.eventsSince(0);
        const std::size_t eventsBeforeBlockedActions =
            eventsBeforeBlock.size();
        const std::size_t actionsBeforeBlockedActions =
            runtime.appliedSemanticActions().size();
        const bool pendingEvent = std::any_of(
            eventsBeforeBlock.begin(), eventsBeforeBlock.end(),
            [](const billiardgl::RuntimeEvent& event) {
                return event.name == "rules_group_choice_pending";
            });
        auto nextFacts = facts(1, 1);
        nextFacts.pocketedBallsInOrder = {1};
        const auto blockedAdjudication = runtime.adjudicateRulesShot(nextFacts);
        billiardgl::GameAction shoot;
        shoot.type = billiardgl::ActionType::Shoot;
        const auto blockedShoot = runtime.dispatch(shoot);
        const auto blockedCueImpact = runtime.applyCueImpact({});
        expect(pendingEvent && !blockedAdjudication.ok &&
            std::string(blockedAdjudication.errorCode) ==
                "rules_group_choice_pending" &&
            !blockedShoot.ok &&
            std::string(blockedShoot.errorCode) ==
                "rules_group_choice_pending" &&
            !blockedCueImpact.ok &&
            std::string(blockedCueImpact.errorCode) ==
                "rules_group_choice_pending",
            "pending group choice rejects semantic, Shoot, and direct cue impact");
        expect(runtime.matchState().groupChoicePending &&
            runtime.matchState().phase ==
                billiardgl::rules::MatchPhase::OpenTable &&
            billiardgl::canonicalGameState(runtime.state()) ==
                stateBeforeBlockedActions &&
            runtime.tick() == tickBeforeBlockedActions &&
            runtime.eventsSince(0).size() == eventsBeforeBlockedActions &&
            runtime.appliedSemanticActions().size() ==
                actionsBeforeBlockedActions,
            "blocked group-choice actions mutate no authority hash, tick, events, or actions");
        expect(runtime.setRulesMode(
                   billiardgl::RulesMode::ChineseEightBallV1).ok &&
            !runtime.matchState().groupChoicePending,
            "ruleset reset is an explicit way to clear the pending gate");
    }
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto breakShot = facts(0, 1);
        breakShot.breakShot = true;
        breakShot.distinctObjectBallsToRail = 4;
        runtime.adjudicateRulesShot(breakShot);
        auto assignment = facts(1, 1);
        assignment.pocketedBallsInOrder = {1};
        runtime.adjudicateRulesShot(assignment);
        auto simultaneous = facts(1, 1);
        simultaneous.firstContactBalls = {1, 9};
        expect(runtime.adjudicateRulesShot(simultaneous).ok &&
            !hasReason(runtime.lastAdjudication(),
                billiardgl::rules::FoulReason::WrongFirstContact),
            "runtime applies legal-contact presumption to assigned groups");
    }
    {
        billiardgl::GameRuntime runtime;
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        auto invalid = facts(0, 1);
        invalid.breakShot = true;
        invalid.firstContactBalls = {1, 1};
        const auto duplicate = runtime.adjudicateRulesShot(invalid);
        expect(!duplicate.ok &&
            std::string(duplicate.errorCode) == "rules_facts_duplicate",
            "runtime rejects duplicate simultaneous first contacts");
        invalid.firstContactBall = 9;
        invalid.firstContactBalls = {9, 1};
        const auto unsorted = runtime.adjudicateRulesShot(invalid);
        expect(!unsorted.ok &&
            std::string(unsorted.errorCode) == "rules_facts_not_canonical",
            "runtime rejects non-canonical simultaneous first contacts");
        invalid.firstContactBall = 1;
        invalid.firstContactBalls = {0};
        const auto outOfRange = runtime.adjudicateRulesShot(invalid);
        expect(!outOfRange.ok &&
            std::string(outOfRange.errorCode) == "rules_facts_out_of_range",
            "runtime rejects non-object entries in the first-contact set");
        invalid.firstContactBall = 9;
        invalid.firstContactBalls = {1, 9};
        const auto conflict = runtime.adjudicateRulesShot(invalid);
        expect(!conflict.ok &&
            std::string(conflict.errorCode) == "rules_facts_conflict" &&
            runtime.matchState().phase ==
                billiardgl::rules::MatchPhase::Break &&
            !runtime.hasLastAdjudication(),
            "runtime rejects scalar/set conflict without state mutation");
    }
}

void testGroupChoiceAuthorityTransaction()
{
    {
        billiardgl::GameRuntime runtime;
        billiardgl::rules::GroupChoiceDecisionRequest request;
        request.rulesetId = billiardgl::rules::kGroupChoiceRulesetId;
        request.capabilityId =
            billiardgl::rules::kGroupChoiceAuthorityCapabilityId;
        request.flowId = billiardgl::rules::kGroupChoiceAuthorityFlowId;
        request.decisionId = "rules.group_choice.missing";
        request.expectedAuthorityStateRevision =
            runtime.rulesAuthorityRevision();
        request.idempotencyKey = "no-pending";
        request.actorPlayerId = 0;
        request.actionId = billiardgl::rules::kGroupChoiceSolidsActionId;
        const AuthoritySnapshot before = authoritySnapshot(runtime);
        const auto result = runtime.submitGroupChoiceDecision(request);
        expect(!result.ok &&
            result.errorCode == "rules_group_choice_not_pending" &&
            sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
            "group choice rejects no-pending requests without mutation");
    }

    billiardgl::GameRuntime runtime;
    expect(stageSimultaneousGroupChoice(runtime),
        "runtime stages a simultaneous group-choice authority offer");
    const auto offer = *runtime.groupChoiceDecisionOffer();
    const auto repeatedOffer = *runtime.groupChoiceDecisionOffer();
    expect(!offer.decisionId.empty() &&
        offer.decisionId == repeatedOffer.decisionId &&
        offer.authorityStateRevision == runtime.rulesAuthorityRevision() &&
        offer.actorPlayerId == runtime.matchState().currentPlayer &&
        offer.offeredActionIds[0] ==
            billiardgl::rules::kGroupChoiceSolidsActionId &&
        offer.offeredActionIds[1] ==
            billiardgl::rules::kGroupChoiceStripesActionId &&
        runtime.rulesAuthorityStateHash().size() == 64,
        "pending offer is stable, revisioned, actor-bound, and exact-action only");

    billiardgl::rules::GroupChoiceDecisionRequest valid;
    valid.rulesetId = billiardgl::rules::kGroupChoiceRulesetId;
    valid.capabilityId =
        billiardgl::rules::kGroupChoiceAuthorityCapabilityId;
    valid.flowId = billiardgl::rules::kGroupChoiceAuthorityFlowId;
    valid.decisionId = offer.decisionId;
    valid.expectedAuthorityStateRevision = offer.authorityStateRevision;
    valid.idempotencyKey = "choice-runtime-001";
    valid.actorPlayerId = offer.actorPlayerId;
    valid.actionId = billiardgl::rules::kGroupChoiceSolidsActionId;

    auto expectRejectedWithoutMutation = [&](const auto& request,
                                             const char* code,
                                             const char* message) {
        const AuthoritySnapshot before = authoritySnapshot(runtime);
        const auto result = runtime.submitGroupChoiceDecision(request);
        expect(!result.ok && result.errorCode == code &&
            sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
            message);
    };
    auto bad = valid;
    bad.rulesetId = "wrong_ruleset";
    bad.idempotencyKey = "wrong-ruleset";
    expectRejectedWithoutMutation(bad, "rules_decision_context_mismatch",
        "wrong group-choice ruleset fails closed");
    bad = valid;
    bad.capabilityId = "wrong_capability";
    bad.idempotencyKey = "wrong-capability";
    expectRejectedWithoutMutation(bad, "rules_decision_context_mismatch",
        "wrong group-choice capability fails closed");
    bad = valid;
    bad.flowId = "wrong_flow";
    bad.idempotencyKey = "wrong-flow";
    expectRejectedWithoutMutation(bad, "rules_decision_context_mismatch",
        "wrong group-choice flow fails closed");
    bad = valid;
    bad.actionId.clear();
    bad.idempotencyKey = "missing-action";
    expectRejectedWithoutMutation(bad, "rules_decision_action_not_offered",
        "missing group-choice action fails closed");
    bad = valid;
    bad.actionId = "rules.group_choice.choose_eight";
    bad.idempotencyKey = "unknown-action";
    expectRejectedWithoutMutation(bad, "rules_decision_action_not_offered",
        "unknown group-choice action fails closed");
    bad = valid;
    bad.actorPlayerId = 1 - offer.actorPlayerId;
    bad.idempotencyKey = "wrong-actor";
    expectRejectedWithoutMutation(bad, "rules_decision_actor_mismatch",
        "wrong group-choice actor fails closed");
    bad = valid;
    bad.idempotencyKey.clear();
    expectRejectedWithoutMutation(bad,
        "rules_decision_idempotency_key_invalid",
        "empty group-choice idempotency key fails closed");
    bad = valid;
    bad.idempotencyKey.assign(129, 'k');
    expectRejectedWithoutMutation(bad,
        "rules_decision_idempotency_key_invalid",
        "long group-choice idempotency key fails closed");
    bad = valid;
    bad.decisionId += ".stale";
    bad.idempotencyKey = "stale-decision";
    expectRejectedWithoutMutation(bad, "rules_decision_stale",
        "stale group-choice decision fails closed");
    bad = valid;
    --bad.expectedAuthorityStateRevision;
    bad.idempotencyKey = "stale-revision";
    expectRejectedWithoutMutation(bad, "rules_decision_stale_revision",
        "stale group-choice revision fails closed");
    bad = valid;
    bad.payloadCanonical = "{\"unexpected\":true}";
    bad.idempotencyKey = "bad-payload";
    expectRejectedWithoutMutation(bad, "rules_decision_invalid_payload",
        "nonempty group-choice payload fails closed");

    bad = valid;
    bad.idempotencyKey = "state-hash-drift";
    runtime.mutableState().balls[2].position.x += 0.25f;
    expectRejectedWithoutMutation(bad, "rules_group_choice_state_changed",
        "group-choice frozen authority hash rejects table-state drift");
    runtime.mutableState().balls[2].position.x -= 0.25f;

    const AuthoritySnapshot beforeCommit = authoritySnapshot(runtime);
    const auto committed = runtime.submitGroupChoiceDecision(valid);
    expect(committed.ok && committed.ack.accepted &&
        committed.ack.decisionId == valid.decisionId &&
        committed.ack.actionId == valid.actionId &&
        committed.ack.idempotencyKey == valid.idempotencyKey &&
        committed.ack.previousStateRevision == beforeCommit.revision &&
        committed.ack.authoritativeStateRevision ==
            beforeCommit.revision + 1 &&
        committed.ack.authoritativeStateHash ==
            runtime.rulesAuthorityStateHash()&&
        !runtime.rulesReplayEvents().empty()&&
        runtime.rulesReplayEvents().back().type==
            billiardgl::RuntimeRulesReplayEventType::GroupChoiceCommit,
        "accepted group choice returns an authority-complete ACK");
    expect(runtime.matchState().groups[offer.actorPlayerId] ==
            billiardgl::rules::BallGroup::Solids &&
        runtime.matchState().groups[1 - offer.actorPlayerId] ==
            billiardgl::rules::BallGroup::Stripes &&
        runtime.matchState().phase ==
            billiardgl::rules::MatchPhase::GroupsAssigned &&
        !runtime.matchState().groupChoicePending &&
        runtime.matchState().currentPlayer == offer.nextPlayer &&
        committed.ack.effects.turnContinues == offer.continueTurn &&
        committed.ack.effects.nextPlayer == offer.nextPlayer &&
        !runtime.lastAdjudication().groupChoiceRequired &&
        runtime.lastAdjudication().decisionOptions.empty() &&
        runtime.lastShotFacts().firstContactBalls == std::vector<int>({1, 9}) &&
        runtime.lastShotFacts().pocketedBallsInOrder ==
            std::vector<int>({1, 9}),
        "commit assigns complementary groups without reinterpreting shot facts or turn outcome");
    const auto committedEvents = std::count_if(runtime.events().begin(),
        runtime.events().end(), [](const billiardgl::RuntimeEvent& event) {
            return event.name == "rules_group_choice_committed";
        });
    expect(committedEvents == 1 &&
        runtime.rulesAuthorityActions().size() == 1 &&
        runtime.rulesAuthorityActions().back().payloadCanonical == "{}" &&
        runtime.events().size() == beforeCommit.events + 1,
        "group-choice commit emits exactly one event and authority action");

    const AuthoritySnapshot afterCommit = authoritySnapshot(runtime);
    const auto retry = runtime.submitGroupChoiceDecision(valid);
    expect(retry.ok &&
        retry.ack.authoritativeStateHash ==
            committed.ack.authoritativeStateHash &&
        retry.ack.authoritativeStateRevision ==
            committed.ack.authoritativeStateRevision &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(runtime)),
        "same idempotency key and request returns the original ACK exactly once");
    bad = valid;
    bad.actionId = billiardgl::rules::kGroupChoiceStripesActionId;
    expectRejectedWithoutMutation(bad, "rules_decision_idempotency_conflict",
        "same idempotency key with a different action is rejected");
    bad = valid;
    bad.payloadCanonical = "{\"different\":1}";
    expectRejectedWithoutMutation(bad, "rules_decision_idempotency_conflict",
        "same idempotency key with a different payload is rejected");

    runtime.reset();
    const AuthoritySnapshot afterReset = authoritySnapshot(runtime);
    const auto staleAfterReset = runtime.submitGroupChoiceDecision(valid);
    expect(!staleAfterReset.ok &&
        staleAfterReset.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterReset, authoritySnapshot(runtime)),
        "full reset invalidates old group-choice requests without mutation");

    billiardgl::GameRuntime rulesetReset;
    expect(stageSimultaneousGroupChoice(rulesetReset),
        "runtime stages a second offer for ruleset-reset coverage");
    const auto resetOffer = *rulesetReset.groupChoiceDecisionOffer();
    auto resetRequest = valid;
    resetRequest.decisionId = resetOffer.decisionId;
    resetRequest.expectedAuthorityStateRevision =
        resetOffer.authorityStateRevision;
    resetRequest.actorPlayerId = resetOffer.actorPlayerId;
    resetRequest.idempotencyKey = "ruleset-reset-old-request";
    expect(rulesetReset.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok &&
        !rulesetReset.matchState().groupChoicePending,
        "ruleset reset clears pending group-choice authority");
    const AuthoritySnapshot afterRulesetReset =
        authoritySnapshot(rulesetReset);
    const auto staleAfterRulesetReset =
        rulesetReset.submitGroupChoiceDecision(resetRequest);
    expect(!staleAfterRulesetReset.ok &&
        staleAfterRulesetReset.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterRulesetReset,
            authoritySnapshot(rulesetReset)),
        "ruleset reset makes the old decision stale without mutation");
}

void testFullTableBallInHandAuthorityTransaction()
{
    {
        billiardgl::GameRuntime noPending;
        const AuthoritySnapshot before = authoritySnapshot(noPending);
        const auto preview = noPending.validateBallInHandPlacement({0.0, 0.0});
        expect(!preview.ok &&
            preview.errorCode == "rules_placement_not_pending" &&
            sameAuthoritySnapshot(before, authoritySnapshot(noPending)),
            "ball-in-hand preview rejects no-pending state without mutation");
    }

    billiardgl::GameRuntime runtime;
    expect(stageFullTableBallInHand(runtime, true),
        "runtime stages ordinary full-table ball-in-hand placement");
    const auto offer = *runtime.ballInHandPlacementOffer();
    const auto repeatedOffer = *runtime.ballInHandPlacementOffer();
    expect(offer.decisionId == repeatedOffer.decisionId &&
        !offer.decisionId.empty() &&
        offer.authorityStateRevision == runtime.rulesAuthorityRevision() &&
        offer.actorPlayerId == runtime.matchState().currentPlayer &&
        offer.scope ==
            billiardgl::rules::BallInHandPlacementScope::FullTable &&
        offer.offeredActionIds.size() == 1 &&
        offer.offeredActionIds[0] ==
            billiardgl::rules::kConfirmFullTablePlacementActionId &&
        billiardgl::buildRulesPresentation(runtime).decisionOptions.empty(),
        "full-table placement offer is stable, actor-bound, and exact-action only");

    const AuthoritySnapshot beforeBlocked = authoritySnapshot(runtime);
    auto nextFacts = facts(runtime.matchState().currentPlayer, 1);
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    const auto blockedFacts = runtime.adjudicateRulesShot(nextFacts);
    const auto blockedCue = runtime.applyCueImpact({});
    expect(!blockedFacts.ok &&
        std::string(blockedFacts.errorCode) ==
            "rules_ball_in_hand_placement_pending" &&
        !blockedCue.ok &&
        sameAuthoritySnapshot(beforeBlocked, authoritySnapshot(runtime)),
        "full-table BIH blocks semantic facts while rejected cue launch preserves authority");

    auto expectPreview = [&](const billiardgl::rules::
                                 BallInHandPlacementPosition& position,
                             bool expectedOk, const char* code,
                             const char* message) {
        const AuthoritySnapshot before = authoritySnapshot(runtime);
        const auto result = runtime.validateBallInHandPlacement(position);
        expect(result.ok == expectedOk &&
            (expectedOk || result.errorCode == code) &&
            sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
            message);
    };
    expectPreview({std::numeric_limits<double>::quiet_NaN(), 0.0}, false,
        "rules_placement_nonfinite",
        "preview rejects NaN without authority mutation");
    expectPreview({0.0, std::numeric_limits<double>::infinity()}, false,
        "rules_placement_nonfinite",
        "preview rejects infinity without authority mutation");
    const double xLimit =
        runtime.physicsProfile().tableBoundary.playfieldWidthCm * 0.5 -
        runtime.physicsProfile().ball.radiusCm;
    expectPreview({xLimit + 0.001, -30.0}, false,
        "rules_placement_out_of_bounds",
        "preview rejects a center outside the production playing surface");
    expectPreview({xLimit, 0.0}, false,
        "rules_placement_forbidden_geometry",
        "preview rejects the production side-pocket mouth");
    expectPreview({runtime.state().balls[1].position.x,
                      runtime.state().balls[1].position.z},
        false, "rules_placement_overlap",
        "preview rejects overlap with an active object ball");
    expectPreview({xLimit, -30.0}, true, "",
        "preview accepts an exact legal center boundary away from pockets");
    expectPreview({-20.0, -30.0}, true, "",
        "preview accepts a clear full-table placement");

    billiardgl::rules::BallInHandPlacementRequest valid;
    valid.decisionId = offer.decisionId;
    valid.expectedAuthorityStateRevision = offer.authorityStateRevision;
    valid.idempotencyKey = "placement-runtime-001";
    valid.actorPlayerId = offer.actorPlayerId;
    valid.actionId =
        billiardgl::rules::kConfirmFullTablePlacementActionId;
    valid.position = {-20.0, -30.0};
    auto expectRejectedWithoutMutation = [&](const auto& request,
                                             const char* code,
                                             const char* message) {
        const AuthoritySnapshot before = authoritySnapshot(runtime);
        const auto result = runtime.submitBallInHandPlacement(request);
        expect(!result.ok && result.errorCode == code &&
            sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
            message);
    };
    auto bad = valid;
    bad.actionId.clear();
    bad.idempotencyKey = "placement-missing-action";
    expectRejectedWithoutMutation(bad, "rules_decision_action_not_offered",
        "placement submit rejects a missing action without mutation");
    bad = valid;
    bad.actionId = "rules.ball_in_hand.confirm_behind_head_string";
    bad.idempotencyKey = "placement-unknown-action";
    expectRejectedWithoutMutation(bad, "rules_decision_action_not_offered",
        "placement submit rejects an unoffered action without mutation");
    bad = valid;
    bad.actorPlayerId = 1 - offer.actorPlayerId;
    bad.idempotencyKey = "placement-wrong-actor";
    expectRejectedWithoutMutation(bad, "rules_decision_actor_mismatch",
        "placement submit rejects the wrong actor without mutation");
    bad = valid;
    --bad.expectedAuthorityStateRevision;
    bad.idempotencyKey = "placement-stale-revision";
    expectRejectedWithoutMutation(bad, "rules_decision_stale_revision",
        "placement submit rejects stale revision without mutation");
    bad = valid;
    bad.decisionId += ".old";
    bad.idempotencyKey = "placement-stale-decision";
    expectRejectedWithoutMutation(bad, "rules_decision_stale",
        "placement submit rejects stale decision without mutation");
    bad = valid;
    bad.idempotencyKey.clear();
    expectRejectedWithoutMutation(bad,
        "rules_decision_idempotency_key_invalid",
        "placement submit rejects an empty idempotency key");
    bad = valid;
    bad.idempotencyKey.assign(129, 'p');
    expectRejectedWithoutMutation(bad,
        "rules_decision_idempotency_key_invalid",
        "placement submit rejects a long idempotency key");
    bad = valid;
    bad.position = {runtime.state().balls[1].position.x,
        runtime.state().balls[1].position.z};
    bad.idempotencyKey = "placement-overlap";
    expectRejectedWithoutMutation(bad, "rules_placement_overlap",
        "placement submit revalidates overlap instead of trusting preview");

    const AuthoritySnapshot beforeCommit = authoritySnapshot(runtime);
    const auto committed = runtime.submitBallInHandPlacement(valid);
    expect(committed.ok && committed.ack.accepted &&
        committed.ack.decisionId == valid.decisionId &&
        committed.ack.actionId == valid.actionId &&
        committed.ack.idempotencyKey == valid.idempotencyKey &&
        committed.ack.previousStateRevision == beforeCommit.revision &&
        committed.ack.authoritativeStateRevision ==
            beforeCommit.revision + 1 &&
        committed.ack.authoritativeStateHash ==
            runtime.rulesAuthorityStateHash()&&
        !runtime.rulesReplayEvents().empty()&&
        runtime.rulesReplayEvents().back().type==
            billiardgl::RuntimeRulesReplayEventType::
                BallInHandPlacementCommit,
        "full-table placement returns a revisioned authoritative ACK");
    expect(runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementPending() &&
        runtime.ballInHandPlacementOffer() &&
        runtime.ballInHandPlacementOffer()->decisionId != offer.decisionId &&
        !runtime.state().balls[0].pocketed &&
        std::fabs(runtime.state().balls[0].position.x + 20.0f) < 0.00001f &&
        std::fabs(runtime.state().balls[0].position.z + 30.0f) < 0.00001f &&
        committed.ack.effects.currentPlayer == offer.actorPlayerId &&
        committed.ack.effects.nextPlayer == offer.actorPlayerId &&
        committed.ack.effects.ballInHand &&
        committed.ack.effects.ballInHandScope ==
            billiardgl::rules::BallInHandPlacementScope::FullTable &&
        committed.ack.effects.placementPending &&
        committed.ack.replacementPlacementDecisionId ==
            runtime.ballInHandPlacementOffer()->decisionId,
        "commit atomically moves cue ball and replaces the retained entitlement offer");
    expect(std::count_if(runtime.events().begin(), runtime.events().end(),
               [](const billiardgl::RuntimeEvent& event) {
                   return event.name ==
                       "rules_ball_in_hand_placement_committed";
               }) == 1 &&
        runtime.events().size() == beforeCommit.events + 1 &&
        runtime.rulesAuthorityActions().size() ==
            beforeCommit.authorityActions + 1 &&
        !runtime.rulesAuthorityActions().back().payloadCanonical.empty(),
        "placement commit emits exactly one event and one auditable authority action");

    const AuthoritySnapshot afterCommit = authoritySnapshot(runtime);
    const auto retry = runtime.submitBallInHandPlacement(valid);
    expect(retry.ok &&
        retry.ack.authoritativeStateHash ==
            committed.ack.authoritativeStateHash &&
        retry.ack.authoritativeStateRevision ==
            committed.ack.authoritativeStateRevision &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(runtime)),
        "placement exact retry returns original ACK without duplicate effects");
    bad = valid;
    bad.position.worldX += 1.0;
    expectRejectedWithoutMutation(bad, "rules_decision_idempotency_conflict",
        "placement reused key with changed coordinates is rejected");

    const auto replacement = *runtime.ballInHandPlacementOffer();
    auto second = valid;
    second.decisionId = replacement.decisionId;
    second.expectedAuthorityStateRevision = replacement.authorityStateRevision;
    second.idempotencyKey = "placement-runtime-002";
    second.position = {-10.0, -35.0};
    const auto secondCommit = runtime.submitBallInHandPlacement(second);
    expect(secondCommit.ok && runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementOffer() &&
        runtime.ballInHandPlacementOffer()->decisionId != replacement.decisionId &&
        std::fabs(runtime.state().balls[0].position.x + 10.0f) < 0.00001f,
        "a second coordinate commit remains reachable and replaces the offer again");

    const AuthoritySnapshot beforeRejectedLaunch = authoritySnapshot(runtime);
    billiardgl::CueImpactInput zeroImpact;
    zeroImpact.cueBallIndex = 0;
    zeroImpact.cueSpeedCmS = 0.0;
    zeroImpact.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
    zeroImpact.direction = {{1.0, 0.0, 0.0}};
    zeroImpact.chalkState = "CHALKED";
    expect(!runtime.applyCueImpact(zeroImpact).ok &&
        runtime.matchState().ballInHand &&
        sameAuthoritySnapshot(beforeRejectedLaunch, authoritySnapshot(runtime)),
        "zero impulse launch does not consume or mutate ball-in-hand authority");
    auto miscue = zeroImpact;
    miscue.cueSpeedCmS = 100.0;
    miscue.tipOffsetRadius = {{0.81, 0.0}};
    miscue.tipOffsetCm = {{0.81 * runtime.physicsProfile().ball.radiusCm, 0.0}};
    expect(runtime.applyCueImpact(miscue).ok &&
        runtime.matchState().ballInHand &&
        sameAuthoritySnapshot(beforeRejectedLaunch, authoritySnapshot(runtime)),
        "miscue with applied=false does not consume or mutate ball-in-hand authority");
    auto positive = zeroImpact;
    positive.cueSpeedCmS = 100.0;
    const auto oldDecision = runtime.ballInHandPlacementOffer()->decisionId;
    expect(runtime.applyCueImpact(positive).ok &&
        runtime.cueContactResult().applied &&
        runtime.cueContactResult().normalImpulseNs > 0.0 &&
        !runtime.matchState().ballInHand &&
        !runtime.ballInHandPlacementPending() &&
        !runtime.ballInHandPlacementOffer() &&
        runtime.rulesAuthorityRevision() == beforeRejectedLaunch.revision + 1 &&
        runtime.rulesAuthorityActions().back().decisionId == oldDecision &&
        runtime.rulesAuthorityActions().back().actionId ==
            billiardgl::rules::kConsumeBallInHandPositiveImpulseActionId,
        "strictly positive authoritative cue-ball impulse consumes entitlement exactly once");

    billiardgl::GameRuntime laterFailure;
    auto failingState = laterFailure.state();
    failingState.balls[1].position = {0.0f,
        failingState.balls[1].position.y, 20.0f};
    failingState.balls[2].position = failingState.balls[1].position;
    auto failingProfile = laterFailure.physicsProfile();
    failingProfile.id = "wpa_ii_09_later_step_failure";
    failingProfile.solver.maximumPenetrationCm = 0.01f;
    expect(laterFailure.replaceStateForScenario(
            failingState, failingProfile).ok &&
        stageFullTableBallInHand(laterFailure),
        "later-step failure fixture stages BIH over a valid but solver-failing layout");
    const auto failureOffer = *laterFailure.ballInHandPlacementOffer();
    auto failurePlacement = valid;
    failurePlacement.decisionId = failureOffer.decisionId;
    failurePlacement.expectedAuthorityStateRevision =
        failureOffer.authorityStateRevision;
    failurePlacement.actorPlayerId = failureOffer.actorPlayerId;
    failurePlacement.idempotencyKey = "placement-before-later-failure";
    failurePlacement.position = {-40.0, -30.0};
    expect(laterFailure.submitBallInHandPlacement(failurePlacement).ok,
        "later-step failure fixture commits its final coordinate");
    auto failureImpact = positive;
    expect(laterFailure.applyCueImpact(failureImpact).ok &&
        !laterFailure.matchState().ballInHand,
        "successful positive launch consumes before physics stepping");
    const auto consumedRevision = laterFailure.rulesAuthorityRevision();
    expect(!laterFailure.step(1).ok &&
        !laterFailure.matchState().ballInHand &&
        !laterFailure.ballInHandPlacementOffer() &&
        laterFailure.rulesAuthorityRevision() == consumedRevision,
        "later physics failure rolls back the tick but never restores consumed BIH");

    runtime.reset();
    const AuthoritySnapshot afterReset = authoritySnapshot(runtime);
    const auto staleAfterReset = runtime.submitBallInHandPlacement(valid);
    expect(!staleAfterReset.ok &&
        staleAfterReset.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterReset, authoritySnapshot(runtime)),
        "full reset makes an old placement request stale");

    billiardgl::GameRuntime rulesetReset;
    expect(stageFullTableBallInHand(rulesetReset),
        "runtime stages placement for ruleset-reset coverage");
    const auto resetOffer = *rulesetReset.ballInHandPlacementOffer();
    auto resetRequest = valid;
    resetRequest.decisionId = resetOffer.decisionId;
    resetRequest.expectedAuthorityStateRevision =
        resetOffer.authorityStateRevision;
    resetRequest.actorPlayerId = resetOffer.actorPlayerId;
    resetRequest.idempotencyKey = "placement-ruleset-reset";
    expect(rulesetReset.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok &&
        !rulesetReset.ballInHandPlacementPending(),
        "ruleset reset clears placement pending state");
    const AuthoritySnapshot afterRulesetReset =
        authoritySnapshot(rulesetReset);
    const auto staleAfterRuleset =
        rulesetReset.submitBallInHandPlacement(resetRequest);
    expect(!staleAfterRuleset.ok &&
        staleAfterRuleset.errorCode == "rules_decision_stale" &&
        sameAuthoritySnapshot(afterRulesetReset,
            authoritySnapshot(rulesetReset)),
        "ruleset reset makes old placement request stale without mutation");

    billiardgl::GameRuntime behindHeadString;
    expect(behindHeadString.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok,
        "behind-head-string fixture enables rules");
    auto illegalBreak = facts(0, 1);
    illegalBreak.breakShot = true;
    illegalBreak.anyRailAfterFirstContact = false;
    attestCueScratch(illegalBreak);
    const bool breakFoulStaged =
        behindHeadString.adjudicateRulesShot(illegalBreak).ok &&
        behindHeadString.breakFoulDecisionOffer().has_value();
    const auto breakFoulApplied = breakFoulStaged ?
        behindHeadString.submitBreakFoulDecision(breakFoulRequest(
            *behindHeadString.breakFoulDecisionOffer(),
            billiardgl::rules::kBreakFoulBallInHandActionId,
            "legacy-behind-parent")) :
        billiardgl::rules::BreakFoulDecisionResult{};
    expect(breakFoulApplied.ok &&
        behindHeadString.matchState().ballInHand &&
        behindHeadString.ballInHandPlacementScope() ==
            billiardgl::rules::BallInHandPlacementScope::BehindHeadString &&
        behindHeadString.ballInHandPlacementOffer() &&
        behindHeadString.ballInHandPlacementOffer()->scope ==
            billiardgl::rules::BallInHandPlacementScope::BehindHeadString,
        "behind-head-string entitlement stages its own exact authority offer");
    const AuthoritySnapshot behindBefore =
        authoritySnapshot(behindHeadString);
    const auto wrongScopePreview =
        behindHeadString.validateBallInHandPlacement({0.0, -30.0});
    auto wrongScopeRequest = valid;
    const auto behindOffer =
        *behindHeadString.ballInHandPlacementOffer();
    wrongScopeRequest.rulesetId =
        billiardgl::rules::kBallInHandRulesetId;
    wrongScopeRequest.capabilityId =
        billiardgl::rules::kFreeBallPlacementCapabilityId;
    wrongScopeRequest.flowId =
        billiardgl::rules::kBehindHeadStringBallInHandFlowId;
    wrongScopeRequest.decisionId = behindOffer.decisionId;
    wrongScopeRequest.expectedAuthorityStateRevision =
        behindHeadString.rulesAuthorityRevision();
    wrongScopeRequest.actorPlayerId = behindOffer.actorPlayerId;
    wrongScopeRequest.actionId =
        billiardgl::rules::kConfirmBehindHeadStringPlacementActionId;
    wrongScopeRequest.idempotencyKey = "behind-wrong-scope";
    wrongScopeRequest.position = {0.0, -30.0};
    const auto wrongScopeSubmit =
        behindHeadString.submitBallInHandPlacement(wrongScopeRequest);
    expect(!wrongScopePreview.ok &&
        wrongScopePreview.errorCode ==
            "rules_placement_outside_head_string_area" &&
        !wrongScopeSubmit.ok &&
        wrongScopeSubmit.errorCode ==
            "rules_placement_outside_head_string_area" &&
        sameAuthoritySnapshot(behindBefore,
            authoritySnapshot(behindHeadString)),
        "behind-head-string wrong-half placement remains fail-closed and non-mutating");
}

bool stageBehindHeadStringBallInHand(billiardgl::GameRuntime& runtime,
                                     bool addOverlapBlocker = false,
                                     bool invalidBlocker = false)
{
    if (!runtime.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok)
        return false;
    runtime.mutableState().balls[0].pocketed = true;
    if (addOverlapBlocker) {
        runtime.mutableState().balls[1].position.x = -20.0f;
        runtime.mutableState().balls[1].position.z = -90.0f;
    }
    if (invalidBlocker)
        runtime.mutableState().balls[1].position.x =
            std::numeric_limits<float>::quiet_NaN();
    auto foul = facts(0, 1);
    foul.breakShot = true;
    attestCueScratch(foul);
    if (!runtime.adjudicateRulesShot(foul).ok ||
        !runtime.breakFoulDecisionOffer())
        return false;
    const auto parent = runtime.submitBreakFoulDecision(breakFoulRequest(
        *runtime.breakFoulDecisionOffer(),
        billiardgl::rules::kBreakFoulBallInHandActionId,
        "stage-behind-parent"));
    return parent.ok &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementScope() ==
            billiardgl::rules::BallInHandPlacementScope::BehindHeadString &&
        runtime.ballInHandPlacementOffer().has_value();
}

billiardgl::rules::BallInHandPlacementRequest behindHeadStringRequest(
    const billiardgl::rules::BallInHandPlacementOffer& offer,
    double worldX, double worldZ, const std::string& key)
{
    billiardgl::rules::BallInHandPlacementRequest request;
    request.rulesetId = billiardgl::rules::kBallInHandRulesetId;
    request.capabilityId =
        billiardgl::rules::kFreeBallPlacementCapabilityId;
    request.flowId =
        billiardgl::rules::kBehindHeadStringBallInHandFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision =
        offer.authorityStateRevision;
    request.idempotencyKey = key;
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId =
        billiardgl::rules::kConfirmBehindHeadStringPlacementActionId;
    request.position = {worldX, worldZ};
    return request;
}

void testBehindHeadStringBallInHandAuthorityTransaction()
{
    billiardgl::GameRuntime runtime;
    expect(stageBehindHeadStringBallInHand(runtime, true),
        "behind-head-string fixture stages a scoped placement offer");
    const auto offer = *runtime.ballInHandPlacementOffer();
    const double length =
        runtime.physicsProfile().tableBoundary.playfieldLengthCm;
    const double width =
        runtime.physicsProfile().tableBoundary.playfieldWidthCm;
    const double radius = runtime.physicsProfile().ball.radiusCm;
    const double headStringZ = -length * 0.25;
    const double bottomCushionZ = -length * 0.5;
    const double xLimit = width * 0.5 - radius;
    const double zLimit = length * 0.5 - radius;
    expect(offer.scope ==
            billiardgl::rules::BallInHandPlacementScope::BehindHeadString &&
        offer.actorPlayerId == 1 &&
        offer.offeredActionIds[0] ==
            billiardgl::rules::kConfirmBehindHeadStringPlacementActionId &&
        offer.sourceClause == "WPA-II-08" &&
        offer.sourcePdfPage == 8 &&
        offer.headStringDefinitionSourceClause == "WPA-II-03-E" &&
        offer.headStringDefinitionPdfPage == 6 &&
        near(offer.headStringWorldZCm, headStringZ) &&
        near(offer.headStringWorldZCm - bottomCushionZ, 63.5) &&
        near(length, 254.0),
        "offer proves production -Z head side and WPA pages 6/8 head-string plane at -63.5 cm");
    expect(billiardgl::buildRulesPresentation(runtime)
            .decisionOptions.empty(),
        "behind-head-string placement stays hidden from Product HUD decisions");

    auto expectPreview = [&](const billiardgl::rules::
                                 BallInHandPlacementPosition& position,
                             bool expectedOk, const char* code,
                             const char* message) {
        const AuthoritySnapshot before = authoritySnapshot(runtime);
        const auto preview =
            runtime.validateBallInHandPlacement(position);
        expect(preview.ok == expectedOk &&
            (expectedOk || preview.errorCode == code) &&
            sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
            message);
    };
    expectPreview({std::numeric_limits<double>::quiet_NaN(), -90.0},
        false, "rules_placement_nonfinite",
        "head-string preview rejects NaN without mutation");
    expectPreview({0.0, -30.0}, false,
        "rules_placement_outside_head_string_area",
        "head-string preview rejects the wrong half of the table");
    expectPreview({0.0, headStringZ}, false,
        "rules_placement_outside_head_string_area",
        "head-string line itself is not behind the line under WPA II section 8");
    expectPreview({xLimit + 0.001, -90.0}, false,
        "rules_placement_out_of_bounds",
        "head-string preview applies real-radius production boundaries");
    expectPreview({xLimit, -zLimit}, false,
        "rules_placement_forbidden_geometry",
        "head-string preview rejects a production corner-pocket mouth");
    expectPreview({-20.0, -90.0}, false,
        "rules_placement_overlap",
        "head-string preview rejects overlap with an active object ball");
    expectPreview({0.0, -zLimit}, true, "",
        "head-string preview accepts the exact center boundary away from pockets");
    expectPreview({20.0, -90.0}, true, "",
        "head-string preview accepts a clear point strictly behind the line");

    const AuthoritySnapshot beforeBlocked = authoritySnapshot(runtime);
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    auto nextShot = facts(1, 1);
    nextShot.breakShot = false;
    const auto blockedCue = runtime.applyCueImpact({});
    const auto blockedAdjudication = runtime.adjudicateRulesShot(nextShot);
    const auto blockedRerack = runtime.stageOfficialRerack({});
    expect(!blockedCue.ok &&
        !blockedAdjudication.ok && !blockedRerack.ok &&
        std::string(blockedAdjudication.errorCode) ==
            "rules_ball_in_hand_behind_head_string_pending" &&
        blockedRerack.errorCode ==
            "rules_ball_in_hand_behind_head_string_pending" &&
        sameAuthoritySnapshot(beforeBlocked, authoritySnapshot(runtime)),
        "head-string BIH preserves authority on rejected cue and blocks semantic/foundation bypass");

    auto valid = behindHeadStringRequest(
        offer, 20.0, -90.0, "behind-commit");
    auto expectRejected = [&](const auto& request, const char* code,
                              const char* message) {
        const AuthoritySnapshot before = authoritySnapshot(runtime);
        const auto result = runtime.submitBallInHandPlacement(request);
        expect(!result.ok && result.errorCode == code &&
            sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
            message);
    };
    auto bad = valid;
    bad.actorPlayerId = 0;
    bad.idempotencyKey = "behind-wrong-actor";
    expectRejected(bad, "rules_decision_actor_mismatch",
        "head-string submit rejects the wrong actor");
    bad = valid;
    bad.actionId =
        billiardgl::rules::kConfirmFullTablePlacementActionId;
    bad.idempotencyKey = "behind-wrong-action";
    expectRejected(bad, "rules_decision_action_not_offered",
        "head-string submit rejects the full-table action");
    bad = valid;
    --bad.expectedAuthorityStateRevision;
    bad.idempotencyKey = "behind-stale-revision";
    expectRejected(bad, "rules_decision_stale_revision",
        "head-string submit rejects a stale revision");
    bad = valid;
    bad.flowId = billiardgl::rules::kFullTableBallInHandFlowId;
    bad.idempotencyKey = "behind-wrong-context";
    expectRejected(bad, "rules_decision_context_mismatch",
        "head-string submit rejects a mismatched flow context");
    bad = valid;
    bad.idempotencyKey.clear();
    expectRejected(bad, "rules_decision_idempotency_key_invalid",
        "head-string submit rejects an empty idempotency key");

    const AuthoritySnapshot beforeCommit = authoritySnapshot(runtime);
    const auto committed = runtime.submitBallInHandPlacement(valid);
    expect(committed.ok && committed.ack.accepted &&
        committed.ack.actionId == valid.actionId &&
        committed.ack.previousStateRevision == beforeCommit.revision &&
        committed.ack.authoritativeStateRevision ==
            beforeCommit.revision + 1 &&
        committed.ack.authoritativeStateHash ==
            runtime.rulesAuthorityStateHash() &&
        !runtime.state().balls[0].pocketed &&
        near(runtime.state().balls[0].position.x, 20.0) &&
        near(runtime.state().balls[0].position.z, -90.0) &&
        runtime.matchState().ballInHand &&
        runtime.ballInHandPlacementPending() &&
        runtime.ballInHandPlacementOffer() &&
        runtime.ballInHandPlacementOffer()->decisionId != offer.decisionId &&
        !runtime.behindHeadStringFirstContactRestrictionPending() &&
        committed.ack.replacementPlacementDecisionId ==
            runtime.ballInHandPlacementOffer()->decisionId &&
        runtime.events().size() == beforeCommit.events + 1 &&
        runtime.rulesAuthorityActions().size() ==
            beforeCommit.authorityActions + 1,
        "head-string coordinate commit retains entitlement and replaces its offer with one revision/event/action");
    const AuthoritySnapshot afterCommit = authoritySnapshot(runtime);
    const auto retry = runtime.submitBallInHandPlacement(valid);
    expect(retry.ok && retry.ack.authoritativeStateHash ==
            committed.ack.authoritativeStateHash &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(runtime)),
        "head-string exact retry returns the original ACK without duplicate effects");
    const AuthoritySnapshot beforeDirectBypass = authoritySnapshot(runtime);
    const auto directBypass = runtime.adjudicateRulesShot(nextShot);
    expect(!directBypass.ok &&
        std::string(directBypass.errorCode) ==
            "rules_ball_in_hand_behind_head_string_pending" &&
        !runtime.behindHeadStringFirstContactRestrictionPending() &&
        sameAuthoritySnapshot(beforeDirectBypass,
            authoritySnapshot(runtime)),
        "retained head-string entitlement rejects direct facts before positive launch");
    billiardgl::CueImpactInput positive;
    positive.cueBallIndex = 0;
    positive.cueSpeedCmS = 100.0;
    positive.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
    positive.direction = {{1.0, 0.0, 0.0}};
    positive.chalkState = "CHALKED";
    expect(runtime.applyCueImpact(positive).ok &&
        !runtime.matchState().ballInHand &&
        !runtime.ballInHandPlacementOffer() &&
        runtime.behindHeadStringFirstContactRestrictionPending() &&
        runtime.shotFactCollectorActive(),
        "positive launch consumes BIH and arms exact head-string context before collector begin");
    const AuthoritySnapshot afterPositiveLaunch = authoritySnapshot(runtime);
    bad = valid;
    bad.position.worldX += 1.0;
    const auto conflict = runtime.submitBallInHandPlacement(bad);
    expect(!conflict.ok &&
        conflict.errorCode == "rules_decision_idempotency_conflict" &&
        sameAuthoritySnapshot(afterPositiveLaunch, authoritySnapshot(runtime)),
        "head-string reused key with changed position is a conflict");
    runtime.reset();
    expect(!runtime.behindHeadStringFirstContactRestrictionPending(),
        "full reset clears the frozen head-string first-contact context");

    billiardgl::GameRuntime changed;
    expect(stageBehindHeadStringBallInHand(changed),
        "head-string drift fixture stages an offer");
    const auto changedRequest = behindHeadStringRequest(
        *changed.ballInHandPlacementOffer(), 20.0, -90.0,
        "behind-state-changed");
    changed.mutableState().balls[2].position.x += 1.0f;
    const AuthoritySnapshot afterDrift = authoritySnapshot(changed);
    const auto driftRejected =
        changed.submitBallInHandPlacement(changedRequest);
    expect(!driftRejected.ok &&
        driftRejected.errorCode == "rules_placement_state_changed" &&
        sameAuthoritySnapshot(afterDrift, authoritySnapshot(changed)),
        "head-string commit rejects authority-state drift before placement mutation");

    billiardgl::GameRuntime noSolution;
    expect(stageBehindHeadStringBallInHand(
            noSolution, false, true),
        "head-string invalid-geometry fixture stages an offer");
    const auto noSolutionRequest = behindHeadStringRequest(
        *noSolution.ballInHandPlacementOffer(), 20.0, -90.0,
        "behind-no-solution");
    const std::string noSolutionHash =
        noSolution.rulesAuthorityStateHash();
    const std::uint64_t noSolutionRevision =
        noSolution.rulesAuthorityRevision();
    const std::size_t noSolutionEvents = noSolution.events().size();
    const auto noSolutionPreview =
        noSolution.validateBallInHandPlacement({20.0, -90.0});
    const auto noSolutionCommit =
        noSolution.submitBallInHandPlacement(noSolutionRequest);
    expect(!noSolutionPreview.ok && !noSolutionCommit.ok &&
        noSolutionPreview.errorCode ==
            "rules_placement_authority_geometry_invalid" &&
        noSolutionCommit.errorCode ==
            "rules_placement_authority_geometry_invalid" &&
        noSolution.rulesAuthorityStateHash() == noSolutionHash &&
        noSolution.rulesAuthorityRevision() == noSolutionRevision &&
        noSolution.events().size() == noSolutionEvents,
        "head-string invalid/no-solution geometry fails without partial mutation");

    billiardgl::GameRuntime reset;
    expect(stageBehindHeadStringBallInHand(reset),
        "head-string reset fixture stages an offer");
    const auto resetRequest = behindHeadStringRequest(
        *reset.ballInHandPlacementOffer(), 20.0, -90.0,
        "behind-reset-stale");
    reset.reset();
    const AuthoritySnapshot afterReset = authoritySnapshot(reset);
    const auto staleReset = reset.submitBallInHandPlacement(resetRequest);
    expect(!staleReset.ok && staleReset.errorCode ==
            "rules_decision_stale" &&
        sameAuthoritySnapshot(afterReset, authoritySnapshot(reset)),
        "full reset makes the old head-string placement stale");

    billiardgl::GameRuntime rulesetReset;
    expect(stageBehindHeadStringBallInHand(rulesetReset),
        "head-string ruleset-reset fixture stages an offer");
    const auto rulesetRequest = behindHeadStringRequest(
        *rulesetReset.ballInHandPlacementOffer(), 20.0, -90.0,
        "behind-ruleset-stale");
    rulesetReset.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1);
    const AuthoritySnapshot afterRulesetReset =
        authoritySnapshot(rulesetReset);
    const auto staleRuleset =
        rulesetReset.submitBallInHandPlacement(rulesetRequest);
    expect(!staleRuleset.ok && staleRuleset.errorCode ==
            "rules_decision_stale" &&
        sameAuthoritySnapshot(afterRulesetReset,
            authoritySnapshot(rulesetReset)),
        "ruleset reset makes the old head-string placement stale");
}

void testBehindHeadStringAllBehindRespotAuthority()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    GameRuntime runtime;
    const double head = -runtime.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    for (int ball = 1; ball < kBallCount; ++ball) {
        runtime.mutableState().balls[ball].pocketed = ball == 8;
        runtime.mutableState().balls[ball].position.z =
            static_cast<float>(head - 30.0 - ball);
    }
    runtime.mutableState().balls[2].position.z = static_cast<float>(head - 2.0);
    expect(stageBehindHeadStringBallInHand(runtime),
        "all-behind fixture stages current Core-G child");
    const auto child = *runtime.ballInHandPlacementOffer();
    BehindHeadStringRespotRequest request;
    request.decisionId = child.decisionId;
    request.expectedAuthorityStateRevision = runtime.rulesAuthorityRevision();
    request.expectedAuthorityStateHash = runtime.rulesAuthorityStateHash();
    request.idempotencyKey = "all-behind-unique";
    request.actorPlayerId = child.actorPlayerId;
    request.actionId = kRequestNearestLegalRespotActionId;
    const std::uint64_t revision = runtime.rulesAuthorityRevision();
    const auto result = runtime.submitBehindHeadStringRespot(request);
    expect(result.ok && result.ack.respotCommitted &&
        result.ack.selectedBallId == 2 &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        runtime.ballInHandPlacementOffer().has_value() &&
        runtime.ballInHandPlacementOffer()->actorPlayerId == child.actorPlayerId &&
        runtime.ballInHandPlacementOffer()->scope ==
            BallInHandPlacementScope::BehindHeadString&&
        !runtime.rulesReplayEvents().empty()&&
        runtime.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::BehindHeadStringRespotCommit&&
        runtime.rulesReplayEvents().back().selectedBallId==2,
        "open-table all-behind unique closest atomically respots and replaces Core-G child");
    const std::string hash = runtime.rulesAuthorityStateHash();
    const auto retry = runtime.submitBehindHeadStringRespot(request);
    expect(retry.ok && retry.ack.authoritativeStateHash ==
            result.ack.authoritativeStateHash &&
        runtime.rulesAuthorityStateHash() == hash,
        "exact all-behind retry returns original ACK with zero delta");

    GameRuntime onLine;
    const double onLineHead = -onLine.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    for (int ball = 1; ball < kBallCount; ++ball) {
        onLine.mutableState().balls[ball].pocketed = ball == 8;
        onLine.mutableState().balls[ball].position.z =
            static_cast<float>(onLineHead - 20.0 - ball);
    }
    onLine.mutableState().balls[3].position.z =
        static_cast<float>(onLineHead);
    expect(stageBehindHeadStringBallInHand(onLine), "on-line fixture stages child");
    const auto onLineChild = *onLine.ballInHandPlacementOffer();
    request.decisionId = onLineChild.decisionId;
    request.expectedAuthorityStateRevision = onLine.rulesAuthorityRevision();
    request.expectedAuthorityStateHash = onLine.rulesAuthorityStateHash();
    request.idempotencyKey = "all-behind-on-line";
    request.actorPlayerId = onLineChild.actorPlayerId;
    const auto rejected = onLine.submitBehindHeadStringRespot(request);
    expect(!rejected.ok && rejected.errorCode ==
            "rules_head_string_not_all_legal_balls_behind" &&
        onLine.ballInHandPlacementOffer()->decisionId == onLineChild.decisionId,
        "legal ball exactly on line suppresses request with zero mutation");

    GameRuntime tie;
    const double tieHead = -tie.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    for (int ball = 1; ball < kBallCount; ++ball) {
        tie.mutableState().balls[ball].pocketed = ball == 8;
        tie.mutableState().balls[ball].position.z =
            static_cast<float>(tieHead - 40.0 - ball);
    }
    tie.mutableState().balls[4].position.z = static_cast<float>(tieHead - 1.0);
    tie.mutableState().balls[6].position.z = static_cast<float>(tieHead - 1.0);
    expect(stageBehindHeadStringBallInHand(tie), "tie fixture stages child");
    const auto tieChild = *tie.ballInHandPlacementOffer();
    request.decisionId = tieChild.decisionId;
    request.expectedAuthorityStateRevision = tie.rulesAuthorityRevision();
    request.expectedAuthorityStateHash = tie.rulesAuthorityStateHash();
    request.idempotencyKey = "all-behind-tie-stage";
    request.actorPlayerId = tieChild.actorPlayerId;
    const auto staged = tie.submitBehindHeadStringRespot(request);
    expect(staged.ok && staged.ack.tieChoiceStaged &&
        staged.ack.tiedCandidateBallIds == std::vector<int>({4, 6}) &&
        tie.behindHeadStringRespotOffer().has_value()&&
        !tie.rulesReplayEvents().empty()&&
        tie.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::BehindHeadStringRespotCommit&&
        tie.rulesReplayEvents().back().selectedBallId==-1,
        "exact tie stages ascending player-choice parent without moving balls");
    const auto tieOffer = *tie.behindHeadStringRespotOffer();
    BehindHeadStringRespotRequest choose;
    choose.decisionId = tieOffer.decisionId;
    choose.expectedAuthorityStateRevision = tie.rulesAuthorityRevision();
    choose.expectedAuthorityStateHash = tie.rulesAuthorityStateHash();
    choose.idempotencyKey = "all-behind-tie-choose";
    choose.actorPlayerId = tieOffer.actorPlayerId;
    choose.actionId = kChooseEquidistantRespotBallActionId;
    choose.selectedBallId = 6;
    choose.payloadCanonical = "{\"selected_ball_id\":6}";
    const auto selected = tie.submitBehindHeadStringRespot(choose);
    expect(selected.ok && selected.ack.respotCommitted &&
        selected.ack.selectedBallId == 6 &&
        !tie.behindHeadStringRespotOffer().has_value() &&
        tie.ballInHandPlacementOffer().has_value()&&
        tie.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::BehindHeadStringRespotCommit&&
        tie.rulesReplayEvents().back().selectedBallId==6,
        "entitled player tie selection atomically respots exact offered ball");
    replay::RulesReceipt selectedReceipt;
    selectedReceipt.tick=tie.rulesReplayEvents().back().tick;
    selectedReceipt.sequence=tie.rulesReplayEvents().back().sequence;
    selectedReceipt.event=tie.rulesReplayEvents().back();
    const std::string selectedDigest=
        replay::rulesReceiptSha256(selectedReceipt);
    selectedReceipt.event.selectedBallId=4;
    expect(replay::rulesReceiptSha256(selectedReceipt)!=selectedDigest,
        "respot receipt digest binds the exact selected object ball");

    GameRuntime wrongActor;
    const double wrongHead = -wrongActor.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    for (int ball = 1; ball < kBallCount; ++ball) {
        wrongActor.mutableState().balls[ball].pocketed = ball == 8;
        wrongActor.mutableState().balls[ball].position.z =
            static_cast<float>(wrongHead - 20.0 - ball);
    }
    expect(stageBehindHeadStringBallInHand(wrongActor),
        "negative authority fixture stages child");
    const auto negativeOffer = *wrongActor.ballInHandPlacementOffer();
    BehindHeadStringRespotRequest negative;
    negative.decisionId = negativeOffer.decisionId;
    negative.expectedAuthorityStateRevision = wrongActor.rulesAuthorityRevision();
    negative.expectedAuthorityStateHash = wrongActor.rulesAuthorityStateHash();
    negative.idempotencyKey = "all-behind-negative";
    negative.actorPlayerId = 1 - negativeOffer.actorPlayerId;
    negative.actionId = kRequestNearestLegalRespotActionId;
    const AuthoritySnapshot beforeWrong = authoritySnapshot(wrongActor);
    const auto actorRejected = wrongActor.submitBehindHeadStringRespot(negative);
    negative.actorPlayerId = negativeOffer.actorPlayerId;
    negative.actionId = kFoundationRespotActionId;
    const auto foundationRejected = wrongActor.submitBehindHeadStringRespot(negative);
    expect(!actorRejected.ok && actorRejected.errorCode ==
            "rules_decision_actor_mismatch" &&
        !foundationRejected.ok && foundationRejected.errorCode ==
            "rules_decision_action_not_offered" &&
        sameAuthoritySnapshot(beforeWrong, authoritySnapshot(wrongActor)),
        "wrong actor and direct foundation action fail with zero delta");
    negative.actionId = kRequestNearestLegalRespotActionId;
    negative.expectedAuthorityStateHash = "0" +
        negative.expectedAuthorityStateHash.substr(1);
    const AuthoritySnapshot beforeHash = authoritySnapshot(wrongActor);
    const auto hashRejected = wrongActor.submitBehindHeadStringRespot(negative);
    expect(!hashRejected.ok && hashRejected.errorCode ==
            "rules_head_string_respot_state_changed" &&
        sameAuthoritySnapshot(beforeHash, authoritySnapshot(wrongActor)),
        "wrong frozen hash fails before mutation");

    GameRuntime nonfinite;
    nonfinite.mutableState().balls[1].position.z =
        std::numeric_limits<float>::quiet_NaN();
    expect(stageBehindHeadStringBallInHand(nonfinite),
        "nonfinite fixture stages child with frozen malformed geometry");
    auto nonfiniteOffer = *nonfinite.ballInHandPlacementOffer();
    BehindHeadStringRespotRequest malformed;
    malformed.decisionId = nonfiniteOffer.decisionId;
    malformed.expectedAuthorityStateRevision = nonfinite.rulesAuthorityRevision();
    malformed.expectedAuthorityStateHash = nonfinite.rulesAuthorityStateHash();
    malformed.idempotencyKey = "all-behind-nonfinite";
    malformed.actorPlayerId = nonfiniteOffer.actorPlayerId;
    malformed.actionId = kRequestNearestLegalRespotActionId;
    const std::uint64_t nonfiniteRevision = nonfinite.rulesAuthorityRevision();
    const std::size_t nonfiniteEvents = nonfinite.events().size();
    const std::size_t nonfiniteActions =
        nonfinite.rulesAuthorityActions().size();
    const auto malformedResult = nonfinite.submitBehindHeadStringRespot(malformed);
    expect(!malformedResult.ok && malformedResult.errorCode ==
            "rules_head_string_respot_geometry_invalid" &&
        nonfinite.rulesAuthorityRevision() == nonfiniteRevision &&
        nonfinite.events().size() == nonfiniteEvents &&
        nonfinite.rulesAuthorityActions().size() == nonfiniteActions &&
        std::isnan(nonfinite.state().balls[1].position.z),
        "nonfinite legal-object geometry rolls back completely");

    GameRuntime reset;
    const double resetHead = -reset.physicsProfile().tableBoundary.playfieldLengthCm * 0.25;
    for (int ball = 1; ball < kBallCount; ++ball) {
        reset.mutableState().balls[ball].pocketed = ball == 8;
        reset.mutableState().balls[ball].position.z =
            static_cast<float>(resetHead - 30.0 - ball);
    }
    expect(stageBehindHeadStringBallInHand(reset), "reset fixture stages child");
    auto resetOffer = *reset.ballInHandPlacementOffer();
    BehindHeadStringRespotRequest stale;
    stale.decisionId = resetOffer.decisionId;
    stale.expectedAuthorityStateRevision = reset.rulesAuthorityRevision();
    stale.expectedAuthorityStateHash = reset.rulesAuthorityStateHash();
    stale.idempotencyKey = "all-behind-reset";
    stale.actorPlayerId = resetOffer.actorPlayerId;
    stale.actionId = kRequestNearestLegalRespotActionId;
    reset.reset();
    const AuthoritySnapshot afterReset = authoritySnapshot(reset);
    const auto resetRejected = reset.submitBehindHeadStringRespot(stale);
    expect(!resetRejected.ok &&
        sameAuthoritySnapshot(afterReset, authoritySnapshot(reset)) &&
        !reset.behindHeadStringRespotOffer().has_value(),
        "reset invalidates the parent and prevents stale resurrection");
}

void testSelectedBreakHeadStringReleaseAuthority()
{
    using billiardgl::rules::BreakHeadStringOffenseAuthority;
    using billiardgl::rules::BreakHeadStringOffenseOutcome;
    using billiardgl::rules::BreakHeadStringReleaseSample;

    auto sample = [](std::uint64_t identity, int actor, double z) {
        BreakHeadStringReleaseSample value;
        value.shotIdentity = identity;
        value.actorPlayerId = actor;
        value.frozenCueBallCenterWorldXCm = 12.0;
        value.frozenCueBallCenterWorldZCm = z;
        value.headStringWorldZCm = -63.5;
        return value;
    };

    BreakHeadStringOffenseAuthority authority;
    const auto headSide = authority.commit(sample(1, 0, -63.500001));
    const auto exactHalf = authority.commit(sample(2, 0, -63.5));
    expect(headSide.ok && exactHalf.ok &&
        headSide.receipt.outcome == BreakHeadStringOffenseOutcome::Compliant &&
        exactHalf.receipt.outcome == BreakHeadStringOffenseOutcome::Compliant &&
        authority.offenseCount(0) == 0,
        "quantized head-side and exact-half release centers are not offenses");

    const auto player0Reminder = authority.commit(sample(3, 0, -63.499999));
    const auto exactRetry = authority.commit(sample(3, 0, -63.499999));
    expect(player0Reminder.ok && exactRetry.ok &&
        player0Reminder.receipt.outcome == BreakHeadStringOffenseOutcome::Reminder &&
        player0Reminder.receipt.quantizedSignedCrossingCm == 0.000001 &&
        exactRetry.receipt == player0Reminder.receipt &&
        authority.offenseCount(0) == 1,
        "first strict foot-side offense reminds exactly once and exact retry is idempotent");
    const auto beforeConflict = authority.stateDigest();
    const auto conflict = authority.commit(sample(3, 0, -63.4));
    expect(!conflict.ok && conflict.errorCode ==
            "rules_break_head_string_identity_conflict" &&
        authority.stateDigest() == beforeConflict,
        "same shot identity with changed frozen position fails without mutation");

    const auto player1Reminder = authority.commit(sample(4, 1, -63.0));
    const auto player0Foul = authority.commit(sample(5, 0, -63.0));
    expect(player1Reminder.ok && player0Foul.ok &&
        player1Reminder.receipt.outcome == BreakHeadStringOffenseOutcome::Reminder &&
        player0Foul.receipt.outcome == BreakHeadStringOffenseOutcome::Foul &&
        authority.offenseCount(0) == 2 && authority.offenseCount(1) == 1,
        "history is per player and only a later offense by that player is a foul");
    const auto beforeInvalid = authority.stateDigest();
    const auto invalidActor = authority.commit(sample(6, 2, -63.0));
    auto nonfinite = sample(7, 0, -63.0);
    nonfinite.frozenCueBallCenterWorldZCm =
        std::numeric_limits<double>::quiet_NaN();
    const auto invalidPosition = authority.commit(nonfinite);
    expect(!invalidActor.ok && !invalidPosition.ok &&
        authority.stateDigest() == beforeInvalid,
        "invalid actor and nonfinite release samples fail closed atomically");
    authority.resetMatch();
    expect(authority.offenseCount(0) == 0 && authority.offenseCount(1) == 0 &&
        !authority.lastReceipt().has_value(),
        "match reset clears both players' history and receipts");

    auto finishPhysicalBreak = [](billiardgl::GameRuntime& runtime) {
        for (int step = 0; step < 4000 && runtime.state().ballsMoving; ++step) {
            const auto result = runtime.step(1);
            if (!result.ok) {
                std::cerr << "head-string physical break step " << step << ": "
                    << result.errorCode << ' '
                    << runtime.latestPhysicsStep().telemetry.failureDetail << '\n';
                return false;
            }
        }
        return !runtime.state().ballsMoving &&
            !runtime.shotFactCollectorActive() &&
            runtime.hasLastAdjudication();
    };
    auto launchPhysicalBreak = [](billiardgl::GameRuntime& runtime,
                                  double cueZ) {
        auto cue = runtime.state().balls[0];
        cue.position.z = static_cast<float>(cueZ);
        cue.velocity = {};
        cue.angularVelocity = {};
        cue.speed = 0.0f;
        cue.pocketed = false;
        const auto placement = runtime.setBall(0, cue);
        if (!placement.ok) {
            std::cerr << "head-string cue placement: " << placement.errorCode << '\n';
            return false;
        }
        billiardgl::GameAction yaw;
        yaw.type = billiardgl::ActionType::SetAimYaw;
        yaw.first = -billiardgl::kPi / 2.0f;
        billiardgl::GameAction power;
        power.type = billiardgl::ActionType::SetShotPower;
        power.first = 120.0f;
        billiardgl::GameAction shoot;
        shoot.type = billiardgl::ActionType::Shoot;
        for (const auto& action : {yaw, power, shoot}) {
            const auto result = runtime.dispatch(action);
            if (!result.ok) {
                std::cerr << "head-string break action: " << result.errorCode << '\n';
                return false;
            }
        }
        return true;
    };

    billiardgl::GameRuntime rejected;
    expect(rejected.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok,
        "rejected-release fixture enables selected rules");
    auto previewCue = rejected.state().balls[0];
    previewCue.position.z = -63.0f;
    expect(rejected.setBall(0, previewCue).ok &&
        rejected.setBall(0, previewCue).ok &&
        rejected.breakHeadStringOffenseCount(0) == 0 &&
        !rejected.breakHeadStringReleasePending(),
        "repeated position preview/state edits do not count before an accepted release");
    billiardgl::CueImpactInput invalidImpact;
    invalidImpact.cueBallIndex = 0;
    invalidImpact.cueSpeedCmS = 120.0;
    invalidImpact.cueMassKg =
        rejected.physicsProfile().cue.effectiveMassKg;
    invalidImpact.direction = {{0.0, 0.0, 0.0}};
    invalidImpact.chalkState = "CHALKED";
    expect(!rejected.applyCueImpact(invalidImpact).ok &&
        rejected.breakHeadStringOffenseCount(0) == 0 &&
        !rejected.breakHeadStringReleasePending(),
        "cue launch rejected by contact validation never stages or counts");
    rejected.mutableState().ballsMoving = true;
    billiardgl::GameAction rejectedShoot;
    rejectedShoot.type = billiardgl::ActionType::Shoot;
    const auto rejectedHash = rejected.rulesAuthorityStateHash();
    expect(!rejected.dispatch(rejectedShoot).ok &&
        rejected.breakHeadStringOffenseCount(0) == 0 &&
        !rejected.breakHeadStringReleasePending() &&
        !rejected.lastBreakHeadStringOffense().has_value() &&
        rejected.rulesAuthorityStateHash() == rejectedHash,
        "rejected launch does not sample or mutate head-string history");

    billiardgl::GameState failingState;
    billiardgl::initializeBalls(failingState);
    failingState.balls[0].position.z = -63.0f;
    failingState.balls[1].position = failingState.balls[0].position;
    billiardgl::PhysicsProfile failingProfile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    failingProfile.id = "break_head_string_failed_step";
    failingProfile.solver.maximumPenetrationCm = 0.01f;
    billiardgl::GameRuntime failedPhysics;
    expect(failedPhysics.replaceStateForScenario(
            failingState, failingProfile).ok &&
        failedPhysics.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok &&
        launchPhysicalBreak(failedPhysics, -63.0) &&
        failedPhysics.breakHeadStringReleasePending(),
        "accepted release stages one frozen sample before physics");
    const auto failedRevision = failedPhysics.rulesAuthorityRevision();
    const auto failedHistoryHash = failedPhysics.rulesAuthorityStateHash();
    const auto failedStep = failedPhysics.step(1);
    expect(!failedStep.ok &&
        failedPhysics.breakHeadStringOffenseCount(0) == 0 &&
        !failedPhysics.breakHeadStringReleasePending() &&
        !failedPhysics.lastBreakHeadStringOffense().has_value() &&
        failedPhysics.rulesAuthorityRevision() == failedRevision &&
        failedPhysics.rulesAuthorityStateHash() != failedHistoryHash,
        "failed physics abandons the staged sample without committing an offense");

    billiardgl::GameRuntime runtime;
    expect(runtime.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok,
        "successful-release fixture enables selected rules");
    const auto firstRevision = runtime.rulesAuthorityRevision();
    expect(
        launchPhysicalBreak(runtime, -63.0) &&
        runtime.breakHeadStringReleasePending() &&
        runtime.breakHeadStringOffenseCount(0) == 0 &&
        finishPhysicalBreak(runtime),
        "successful physical break completes the staged release transaction");
    const auto firstRecord = runtime.lastBreakHeadStringOffense();
    expect(firstRecord &&
        firstRecord->outcome == BreakHeadStringOffenseOutcome::Reminder &&
        firstRecord->actorPlayerId == 0 &&
        firstRecord->frozenCueBallCenterWorldZCm == -63.0 &&
        firstRecord->textKey ==
            "rules.chinese8.reminder.break_head_string_more_than_half" &&
        runtime.breakHeadStringOffenseCount(0) == 1 &&
        runtime.rulesAuthorityRevision() == firstRevision + 1 &&
        !runtime.lastAdjudication().foul &&
        std::find(runtime.lastAdjudication().textKeys.begin(),
                  runtime.lastAdjudication().textKeys.end(),
                  firstRecord->textKey) != runtime.lastAdjudication().textKeys.end(),
        "first completed offense commits a visible reminder without a foul");

    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const auto readback = send(controller, 606, "get_rules_state");
    const auto& headString = readback.response.at("result")
        .at("break_head_string_offense");
    expect(headString.at("visible").asBool() &&
        headString.at("outcome").asString() == "reminder" &&
        headString.at("actor_player_id").asInt() == 0 &&
        headString.at("player_offense_count").asInt() == 1 &&
        headString.at("text_key").asString() ==
            "rules.chinese8.reminder.break_head_string_more_than_half" &&
        headString.at("sample_event").asString() ==
            "accepted_break_shot_release" &&
        headString.at("quantization_grid_world_units").asNumber() == 0.000001,
        "automation exposes the committed visible reminder and frozen sample contract");

    billiardgl::rules::RerackFoundationParameters rerackParameters;
    rerackParameters.deterministicSeed = 0x060d0001ULL;
    rerackParameters.breakerPlayerId = 1;
    const auto player1Plan = runtime.stageOfficialRerack(rerackParameters);
    expect(player1Plan.ok && runtime.commitRackFoundation(
            player1Plan, "head-string-rerack-player1").ok &&
        runtime.breakHeadStringOffenseCount(0) == 1 &&
        std::all_of(runtime.state().balls.begin(), runtime.state().balls.end(),
            [](const auto& ball) {
                return ball.physicalDomain == billiardgl::PhysicalBallDomain::ClothSupported;
            }) &&
        launchPhysicalBreak(runtime, -63.0) && finishPhysicalBreak(runtime) &&
        runtime.lastBreakHeadStringOffense()->outcome ==
            BreakHeadStringOffenseOutcome::Reminder &&
        runtime.breakHeadStringOffenseCount(0) == 1 &&
        runtime.breakHeadStringOffenseCount(1) == 1,
        "rerack preserves match history while the other player receives an independent first reminder");

    rerackParameters.deterministicSeed = 0x060d0002ULL;
    rerackParameters.breakerPlayerId = 0;
    const auto player0Plan = runtime.stageOfficialRerack(rerackParameters);
    expect(player0Plan.ok && runtime.commitRackFoundation(
            player0Plan, "head-string-rerack-player0").ok &&
        launchPhysicalBreak(runtime, -63.0) && finishPhysicalBreak(runtime) &&
        runtime.lastBreakHeadStringOffense()->outcome ==
            BreakHeadStringOffenseOutcome::Foul &&
        runtime.breakHeadStringOffenseCount(0) == 2 &&
        runtime.lastAdjudication().foul &&
        hasReason(runtime.lastAdjudication(),
            billiardgl::rules::FoulReason::BreakCueBallPastHeadString) &&
        runtime.breakFoulDecisionPending(),
        "same player's later completed offense becomes one break foul and stages the existing parent choice");

    runtime.reset();
    expect(runtime.breakHeadStringOffenseCount(0) == 0 &&
        runtime.breakHeadStringOffenseCount(1) == 0 &&
        !runtime.lastBreakHeadStringOffense().has_value(),
        "full match reset clears the match-scope head-string history");
}

void testAuthoritativeCollectorLifecycle()
{
    billiardgl::GameRuntime runtime;
    runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
    billiardgl::GameAction yaw;
    yaw.type = billiardgl::ActionType::SetAimYaw;
    yaw.first = -billiardgl::kPi / 2.0f;
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 120.0f;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(yaw).ok && runtime.dispatch(power).ok &&
        runtime.dispatch(shoot).ok && runtime.shotFactCollectorActive(),
        "successful physical shot starts the authoritative fact collector");
    expect(!runtime.setRulesMode(billiardgl::RulesMode::Legacy).ok,
        "ruleset cannot change while balls move");
    for (int tick = 0; tick < 4000 && runtime.state().ballsMoving; ++tick)
        expect(runtime.step(1).ok, "physical rules shot step succeeds");
    expect(!runtime.state().ballsMoving && runtime.hasLastShotFacts() &&
        runtime.hasLastAdjudication() && !runtime.shotFactCollectorActive(),
        "shot end finalizes collected telemetry exactly once");
    expect(runtime.lastShotFacts().breakEvidenceOrigin ==
            billiardgl::rules::BreakEvidenceOrigin::
                AuthoritativeShotFactCollector &&
        runtime.lastShotFacts().breakEvidenceComplete &&
        billiardgl::rules::validateShotFacts(runtime.lastShotFacts()) ==
            billiardgl::rules::ShotFactsValidationError::None,
        "physical break finalization preserves complete authoritative cushion evidence");
    for (std::size_t index = 0;
         index < runtime.lastShotFacts().breakObjectCushionContacts.size();
         ++index) {
        expect(runtime.lastShotFacts().breakObjectCushionContacts[index]
                   .canonicalContactOrdinal == static_cast<int>(index),
            "runtime break evidence retains canonical contiguous ordinals");
    }
}

void testAuthoritativeCueScratchPocketLifecycle()
{
    using billiardgl::rules::CueBallScratchEvidenceOrigin;
    using billiardgl::rules::FoulReason;
    using billiardgl::rules::ShotFactsValidationError;
    billiardgl::GameRuntime runtime;
    expect(runtime.setRulesMode(
               billiardgl::RulesMode::ChineseEightBallV1).ok,
           "cue scratch fixture enables authoritative rules");
    auto breakShot = facts(0, 1);
    breakShot.breakShot = true;
    breakShot.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(breakShot).ok,
           "cue scratch fixture advances beyond the break");

    for (int index = 1; index < billiardgl::kBallCount; ++index)
        runtime.mutableState().balls[index].pocketed = true;
    auto cue = runtime.state().balls[0];
    cue.position = billiardgl::Point3{
        -billiardgl::kTableInWidth / 2.0f +
            runtime.physicsProfile().ball.radiusCm,
        billiardgl::kTableHeight + runtime.physicsProfile().ball.radiusCm,
        0.0f};
    cue.pocketed = false;
    cue.velocity = {};
    cue.speed = 0.0f;
    cue.pocketInteraction = {};
    expect(runtime.setBall(0, cue).ok,
           "cue scratch fixture positions a live cue ball at the side mouth");

    billiardgl::CueImpactInput impact;
    impact.cueBallIndex = 0;
    impact.cueSpeedCmS = 400.0;
    impact.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
    impact.direction = {{-1.0, 0.0, 0.0}};
    impact.tipOffsetCm = {{0.0, 0.0}};
    impact.tipOffsetRadius = {{0.0, 0.0}};
    impact.chalkState = "CHALKED";
    expect(runtime.applyCueImpact(impact).ok &&
               runtime.shotFactCollectorActive(),
           "real cue impact starts the scratch collector lifecycle");
    for (int step = 0; step < 20 && runtime.state().ballsMoving; ++step)
        expect(runtime.step(1).ok,
               "real cue scratch physics tick succeeds");

    const auto& collected = runtime.lastShotFacts();
    expect(runtime.hasLastShotFacts() && runtime.hasLastAdjudication() &&
               !runtime.shotFactCollectorActive() &&
               collected.cueBallPocketed &&
               collected.cueBallScratchEvidenceOrigin ==
                   CueBallScratchEvidenceOrigin::
                       AuthoritativeShotFactCollector &&
               collected.cueBallScratchEvidenceComplete &&
               collected.cueBallPocketCaptures.size() == 1 &&
               collected.cueBallPocketCaptures[0].physicsTick > 0 &&
               collected.cueBallPocketCaptures[0].captureSequence > 0 &&
               collected.cueBallPocketCaptures[0].pocketId >= 0 &&
               collected.cueBallPocketCaptures[0].pocketId < 6 &&
               collected.cueBallPocketCaptures[0].canonicalEventOrdinal == 0 &&
               billiardgl::rules::validateShotFacts(collected) ==
                   ShotFactsValidationError::None,
           "real succeeded pocket transition becomes one complete canonical cue scratch fact");
    expect(!runtime.state().balls[0].pocketed &&
               hasReason(runtime.lastAdjudication(),
                   FoulReason::CueBallScratch) &&
               runtime.matchState().ballInHand &&
               runtime.ballInHandPlacementOffer().has_value(),
           "authoritative scratch adjudicates BIH without faking cue BallState.pocketed");

    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const auto readback = send(controller, 259,
        "get_rules_state").response.at("result").at("last_shot_facts");
    const auto& captures = readback.at(
        "cue_ball_pocket_captures").asArray();
    expect(readback.at("cue_ball_scratch_evidence_origin").asString() ==
               "authoritative_shot_fact_collector" &&
               readback.at("cue_ball_scratch_evidence_complete").asBool() &&
               captures.size() == 1 &&
               captures[0].at("canonical_event_ordinal").asInt() == 0 &&
               captures[0].at("capture_sequence").asNumber() > 0.0 &&
               captures[0].at("physics_tick").asNumber() > 0.0,
           "automation readback exposes canonical scratch origin, completeness, tick and event identity");
}

void testAutomationRulesRoundTrip()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const std::vector<std::string> capabilities = controller.capabilities();
    expect(std::find(capabilities.begin(), capabilities.end(),
        "chinese_eight_ball_rules_v1") != capabilities.end(),
        "rules capability is advertised");
    expect(std::find(capabilities.begin(), capabilities.end(),
        "chinese_eight_ball_first_contact_set_v1") != capabilities.end(),
        "versioned simultaneous first-contact capability is advertised");
    expect(std::find(capabilities.begin(), capabilities.end(),
            "ball_respot_and_rerack_executor") == capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            billiardgl::rules::kFoundationRerackActionId) ==
            capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            billiardgl::rules::kFoundationRespotActionId) ==
            capabilities.end(),
        "rack foundation remains internal and advertises no shipping capability or action");
    const auto hiddenStage = send(controller, 9001,
        "stage_official_rerack");
    const auto hiddenCommit = send(controller, 9002,
        "commit_rack_foundation");
    expect(!hiddenStage.response.at("ok").asBool() &&
        hiddenStage.response.at("error").at("code").asString() ==
            "unknown_command" &&
        !hiddenCommit.response.at("ok").asBool() &&
        hiddenCommit.response.at("error").at("code").asString() ==
            "unknown_command",
        "rack foundation exposes no automation staging or commit command");
    const auto enabled = send(controller, 1, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value("wpa_heyball_2025_08_16_v1")}}));
    expect(enabled.response.at("ok").asBool() &&
        enabled.response.at("result").at("phase").asString() == "break",
        "automation enables and round-trips versioned rules state");
    const auto adjudicated = send(controller, 2, "adjudicate_rules_shot", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"shooter", billiardgl::json::Value(0)},
        {"break_shot", billiardgl::json::Value(true)},
        {"first_contact_ball", billiardgl::json::Value(1)},
        {"distinct_object_balls_to_rail", billiardgl::json::Value(4)},
        {"any_rail_after_first_contact", billiardgl::json::Value(true)}}));
    expect(adjudicated.response.at("ok").asBool() &&
        adjudicated.response.at("result").at("phase").asString() == "open_table" &&
        adjudicated.response.at("result").at("last_adjudication")
            .at("accepted").asBool(),
        "automation submits semantic shot facts and returns adjudication");
    const auto state = send(controller, 3, "get_rules_state");
    expect(state.response.at("result").at("current_player").asInt() == 1 &&
        state.response.at("result").has("last_shot_facts"),
        "rules state query round-trips match and shot facts");
}

void testAutomationRulesCoreBRoundTrip()
{
    {
        billiardgl::GameRuntime runtime;
        billiardgl::AutomationController controller(
            runtime, billiardgl::AutomationMode::Headless);
        const auto enabled = send(controller, 200, "set_ruleset", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"ruleset_id", billiardgl::json::Value(
                "wpa_heyball_2025_08_16_v1")}}));
        const auto breakShot = send(controller, 201,
            "adjudicate_rules_shot", object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(0)},
                {"break_shot", billiardgl::json::Value(true)},
                {"first_contact_ball", billiardgl::json::Value(1)},
                {"distinct_object_balls_to_rail", billiardgl::json::Value(4)}}));
        const auto choice = send(controller, 202,
            "adjudicate_rules_shot", object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(1)},
                {"first_contact_balls", balls({1, 9})},
                {"pocketed_balls_in_order", balls({1, 9})}}));
        const auto& result = choice.response.at("result");
        const auto& adjudication = result.at("last_adjudication");
        const auto& options = adjudication.at("decision_options").asArray();
        const auto& lastFacts = result.at("last_shot_facts");
        const auto& contacts = lastFacts.at("first_contact_balls").asArray();
        expect(enabled.response.at("ok").asBool() &&
            breakShot.response.at("ok").asBool() &&
            choice.response.at("ok").asBool() &&
            adjudication.at("group_choice_required").asBool() &&
            result.at("phase").asString() == "open_table" &&
            result.at("groups").asArray()[0].asString() == "open" &&
            result.at("groups").asArray()[1].asString() == "open" &&
            options.size() == 2 &&
            options[0].asString() == "rules.group_choice.choose_solids" &&
            options[1].asString() == "rules.group_choice.choose_stripes" &&
            contacts.size() == 2 && contacts[0].asInt() == 1 &&
            contacts[1].asInt() == 9 &&
            lastFacts.at("first_contact_ball").asInt() == 1,
            "automation round-trips group-choice staging without authority action");
        const auto blockedFacts = send(controller, 203,
            "adjudicate_rules_shot", object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(1)},
                {"first_contact_ball", billiardgl::json::Value(1)}}));
        const auto blockedShoot = send(controller, 204, "shoot");
        const auto pendingState = send(controller, 205, "get_rules_state");
        expect(!blockedFacts.response.at("ok").asBool() &&
            blockedFacts.response.at("error").at("code").asString() ==
                "rules_group_choice_pending" &&
            !blockedShoot.response.at("ok").asBool() &&
            blockedShoot.response.at("error").at("code").asString() ==
                "rules_group_choice_pending" &&
            pendingState.response.at("result").at("group_choice_pending")
                .asBool() &&
            pendingState.response.at("result").at("phase").asString() ==
                "open_table",
            "automation cannot bypass the authoritative pending group-choice gate");
        expect(send(controller, 206, "reset_game").response.at("ok").asBool(),
            "automation reset remains available while group choice is pending");
        const auto resetState = send(controller, 207, "get_rules_state");
        expect(!resetState.response.at("result").at("group_choice_pending")
                    .asBool() &&
            resetState.response.at("result").at("phase").asString() == "break",
            "full reset clears the authoritative pending group-choice gate");
    }
    {
        billiardgl::GameRuntime runtime;
        billiardgl::AutomationController controller(
            runtime, billiardgl::AutomationMode::Headless);
        const auto enabled = send(controller, 210, "set_ruleset", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"ruleset_id", billiardgl::json::Value(
                "wpa_heyball_2025_08_16_v1")}}));
        expect(enabled.response.at("ok").asBool(),
            "automation enables rules for first-contact validation");
        auto invalid = [&](int id, int scalar,
                           billiardgl::json::Value contacts,
                           const char* expectedCode) {
            const auto response = send(controller, id,
                "adjudicate_rules_shot", object({
                    {"schema_version", billiardgl::json::Value(1)},
                    {"shooter", billiardgl::json::Value(0)},
                    {"break_shot", billiardgl::json::Value(true)},
                    {"first_contact_ball", billiardgl::json::Value(scalar)},
                    {"first_contact_balls", contacts}}));
            expect(!response.response.at("ok").asBool() &&
                response.response.at("error").at("code").asString() ==
                    expectedCode,
                "automation rejects invalid simultaneous first-contact facts");
        };
        invalid(211, 1, balls({1, 1}), "rules_facts_duplicate");
        invalid(212, 9, balls({9, 1}), "rules_facts_not_canonical");
        invalid(213, 1, balls({0}), "rules_facts_out_of_range");
        invalid(214, 9, balls({1, 9}), "rules_facts_conflict");
        const auto state = send(controller, 215, "get_rules_state");
        expect(state.response.at("result").at("phase").asString() == "break" &&
            !state.response.at("result").at("has_last_adjudication").asBool(),
            "automation bad first-contact facts do not mutate runtime state");
    }
}

void testAutomationGroupChoiceAuthorityRoundTrip()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const auto capabilities = send(controller, 300, "get_capabilities");
    const auto& commands = capabilities.response.at("result")
        .at("commands").asArray();
    const bool hasCapability = std::any_of(commands.begin(), commands.end(),
        [](const billiardgl::json::Value& value) {
            return value.asString() ==
                "simultaneous_contact_and_group_choice";
        });
    const bool hasCommand = std::any_of(commands.begin(), commands.end(),
        [](const billiardgl::json::Value& value) {
            return value.asString() ==
                "rules.submit_simultaneous_group_choice";
        });
    expect(hasCapability && hasCommand,
        "automation advertises the versioned group-choice authority surface");

    expect(send(controller, 301, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")}})).response.at("ok").asBool(),
        "automation enables rules for group-choice authority coverage");
    expect(send(controller, 302, "adjudicate_rules_shot", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"shooter", billiardgl::json::Value(0)},
        {"break_shot", billiardgl::json::Value(true)},
        {"first_contact_ball", billiardgl::json::Value(1)},
        {"distinct_object_balls_to_rail", billiardgl::json::Value(4)}}))
            .response.at("ok").asBool(),
        "automation accepts the setup break for group-choice authority");
    expect(send(controller, 303, "adjudicate_rules_shot", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"shooter", billiardgl::json::Value(1)},
        {"first_contact_balls", balls({1, 9})},
        {"pocketed_balls_in_order", balls({1, 9})}}))
            .response.at("ok").asBool(),
        "automation stages a simultaneous group-choice offer");

    const auto offeredState = send(controller, 304, "get_rules_state");
    const auto& offered = offeredState.response.at("result");
    const auto& pending = offered.at("pending_decision");
    const auto& offeredActions = pending.at("offered_action_ids").asArray();
    expect(offered.at("group_choice_pending").asBool() &&
        offered.at("authoritative_state_hash").asString().size() == 64 &&
        pending.at("decision_id").isString() &&
        pending.at("actor_player_id").asInt() == 1 &&
        pending.at("flow_id").asString() == "simultaneous_group_choice" &&
        offeredActions.size() == 2 &&
        offeredActions[0].asString() ==
            billiardgl::rules::kGroupChoiceSolidsActionId &&
        offeredActions[1].asString() ==
            billiardgl::rules::kGroupChoiceStripesActionId &&
        !pending.has("label") && !pending.has("title") &&
        !pending.has("text_key"),
        "automation exposes a minimal stable offer without UI strings");

    billiardgl::json::Value request = object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")},
        {"capability_id", billiardgl::json::Value(
            "simultaneous_contact_and_group_choice")},
        {"flow_id", billiardgl::json::Value(
            "simultaneous_group_choice")},
        {"decision_id", billiardgl::json::Value(
            pending.at("decision_id").asString())},
        {"action_id", billiardgl::json::Value(
            billiardgl::rules::kGroupChoiceStripesActionId)},
        {"authority_state_revision", billiardgl::json::Value(
            pending.at("authority_state_revision").asNumber())},
        {"idempotency_key", billiardgl::json::Value(
            "automation-choice-001")},
        {"actor_player_id", billiardgl::json::Value(1)},
        {"payload", billiardgl::json::Value::object()}});

    auto expectAutomationFailureWithoutMutation =
        [&](int id, const billiardgl::json::Value& params,
            const char* code, const char* message) {
            const AuthoritySnapshot before = authoritySnapshot(runtime);
            const auto response = send(controller, id,
                "rules.submit_simultaneous_group_choice", params);
            expect(!response.response.at("ok").asBool() &&
                response.response.at("error").at("code").asString() == code &&
                sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
                message);
        };
    auto bad = request;
    bad.asObject().erase("action_id");
    expectAutomationFailureWithoutMutation(305, bad, "invalid_argument",
        "automation missing action fails closed");
    bad = request;
    bad["action_id"] = billiardgl::json::Value(
        "rules.group_choice.unknown");
    bad["idempotency_key"] = billiardgl::json::Value("unknown-action");
    expectAutomationFailureWithoutMutation(306, bad,
        "rules_decision_action_not_offered",
        "automation unknown action fails closed");
    bad = request;
    bad["actor_player_id"] = billiardgl::json::Value(0);
    bad["idempotency_key"] = billiardgl::json::Value("wrong-actor");
    expectAutomationFailureWithoutMutation(307, bad,
        "rules_decision_actor_mismatch",
        "automation wrong actor fails closed");
    bad = request;
    bad["authority_state_revision"] = billiardgl::json::Value(
        pending.at("authority_state_revision").asNumber() - 1.0);
    bad["idempotency_key"] = billiardgl::json::Value("stale-revision");
    expectAutomationFailureWithoutMutation(308, bad,
        "rules_decision_stale_revision",
        "automation stale revision fails closed");
    bad = request;
    bad["decision_id"] = billiardgl::json::Value("rules.group_choice.old");
    bad["idempotency_key"] = billiardgl::json::Value("stale-decision");
    expectAutomationFailureWithoutMutation(309, bad,
        "rules_decision_stale",
        "automation stale decision fails closed");

    const AuthoritySnapshot beforeCommit = authoritySnapshot(runtime);
    const auto committed = send(controller, 310,
        "rules.submit_simultaneous_group_choice", request);
    const auto& ack = committed.response.at("result");
    const auto& effects = ack.at("effects");
    expect(committed.response.at("ok").asBool() &&
        ack.at("accepted").asBool() &&
        ack.at("decision_id").asString() ==
            pending.at("decision_id").asString() &&
        ack.at("action_id").asString() ==
            billiardgl::rules::kGroupChoiceStripesActionId &&
        ack.at("idempotency_key").asString() == "automation-choice-001" &&
        ack.at("previous_state_revision").asNumber() ==
            static_cast<double>(beforeCommit.revision) &&
        ack.at("authoritative_state_revision").asNumber() ==
            static_cast<double>(beforeCommit.revision + 1) &&
        effects.at("groups").asArray()[0].asString() == "solids" &&
        effects.at("groups").asArray()[1].asString() == "stripes" &&
        effects.at("phase").asString() == "groups_assigned" &&
        effects.at("next_player").asInt() == 1 &&
        effects.at("turn_continues").asBool() &&
        effects.at("ball_in_hand_scope").asString() == "none" &&
        !effects.at("group_choice_pending").asBool(),
        "automation ACK echoes identity, revisions, hash, and complete effects");
    const auto committedState = send(controller, 311, "get_rules_state");
    expect(committedState.response.at("result")
            .at("authoritative_state_hash").asString() ==
            ack.at("authoritative_state_hash").asString() &&
        committedState.response.at("result").at("phase").asString() ==
            "groups_assigned" &&
        !committedState.response.at("result").has("pending_decision"),
        "automation readback matches the committed authority ACK");

    const AuthoritySnapshot afterCommit = authoritySnapshot(runtime);
    const auto retry = send(controller, 312,
        "rules.submit_simultaneous_group_choice", request);
    expect(retry.response.at("ok").asBool() &&
        billiardgl::json::stringify(retry.response.at("result")) ==
            billiardgl::json::stringify(committed.response.at("result")) &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(runtime)),
        "automation exact retry returns the same ACK without duplicate effects");
    bad = request;
    bad["action_id"] = billiardgl::json::Value(
        billiardgl::rules::kGroupChoiceSolidsActionId);
    expectAutomationFailureWithoutMutation(313, bad,
        "rules_decision_idempotency_conflict",
        "automation reused key with another action is rejected");
    bad = request;
    billiardgl::json::Value changedPayload = billiardgl::json::Value::object();
    changedPayload["unexpected"] = billiardgl::json::Value(true);
    bad["payload"] = changedPayload;
    expectAutomationFailureWithoutMutation(314, bad,
        "rules_decision_idempotency_conflict",
        "automation reused key with another payload is rejected");

    expect(send(controller, 315, "reset_game").response.at("ok").asBool(),
        "automation reset clears committed group-choice receipts");
    expectAutomationFailureWithoutMutation(316, request,
        "rules_decision_stale",
        "automation old request is stale after reset");
    bad = request;
    bad["decision_id"] = billiardgl::json::Value("rules.group_choice.none");
    bad["idempotency_key"] = billiardgl::json::Value("no-pending-new");
    expectAutomationFailureWithoutMutation(317, bad,
        "rules_group_choice_not_pending",
        "automation no-pending request fails closed");
}

void testAutomationFullTableBallInHandAuthorityRoundTrip()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const auto capabilities = send(controller, 400, "get_capabilities");
    const auto& commands = capabilities.response.at("result")
        .at("commands").asArray();
    expect(std::any_of(commands.begin(), commands.end(),
               [](const billiardgl::json::Value& value) {
                   return value.asString() == "free_ball_placement";
               }) &&
        std::any_of(commands.begin(), commands.end(),
            [](const billiardgl::json::Value& value) {
                return value.asString() ==
                    "rules.confirm_ball_in_hand_placement";
            }),
        "automation advertises full-table placement capability and command");
    expect(send(controller, 401, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")}})).response.at("ok").asBool(),
        "automation enables rules for placement authority coverage");
    expect(send(controller, 402, "adjudicate_rules_shot", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"shooter", billiardgl::json::Value(0)},
        {"break_shot", billiardgl::json::Value(true)},
        {"first_contact_ball", billiardgl::json::Value(1)},
        {"distinct_object_balls_to_rail", billiardgl::json::Value(4)}}))
            .response.at("ok").asBool(),
        "automation accepts setup break for placement authority");
    expect(send(controller, 403, "adjudicate_rules_shot", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"shooter", billiardgl::json::Value(1)},
        {"first_contact_ball", billiardgl::json::Value(1)},
        {"pocketed_balls_in_order", balls({1})}}))
            .response.at("ok").asBool(),
        "automation assigns groups before placement foul");
    expect(send(controller, 404, "adjudicate_rules_shot", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"shooter", billiardgl::json::Value(1)},
        {"first_contact_ball", billiardgl::json::Value(9)}}))
            .response.at("ok").asBool(),
        "automation stages ordinary ball-in-hand placement");

    const auto pendingState = send(controller, 405, "get_rules_state");
    const auto& pendingResult = pendingState.response.at("result");
    const auto& pending = pendingResult.at("pending_decision");
    const auto& offeredActions = pending.at("offered_action_ids").asArray();
    expect(pendingResult.at("ball_in_hand").asBool() &&
        pendingResult.at("ball_in_hand_placement_pending").asBool() &&
        pendingResult.at("ball_in_hand_scope").asString() == "full_table" &&
        pending.at("capability_id").asString() == "free_ball_placement" &&
        pending.at("flow_id").asString() == "ball_in_hand_full_table" &&
        pending.at("actor_player_id").asInt() == 0 &&
        offeredActions.size() == 1 &&
        offeredActions[0].asString() ==
            billiardgl::rules::kConfirmFullTablePlacementActionId &&
        !pending.has("label") && !pending.has("title") &&
        !pending.has("text_key"),
        "automation readback exposes a minimal full-table placement offer");

    billiardgl::json::Value payload = object({
        {"world_x_cm", billiardgl::json::Value(-20.0)},
        {"world_z_cm", billiardgl::json::Value(-30.0)}});
    billiardgl::json::Value request = object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")},
        {"capability_id", billiardgl::json::Value("free_ball_placement")},
        {"flow_id", billiardgl::json::Value("ball_in_hand_full_table")},
        {"decision_id", billiardgl::json::Value(
            pending.at("decision_id").asString())},
        {"action_id", billiardgl::json::Value(
            billiardgl::rules::kConfirmFullTablePlacementActionId)},
        {"authority_state_revision", billiardgl::json::Value(
            pending.at("authority_state_revision").asNumber())},
        {"idempotency_key", billiardgl::json::Value(
            "automation-placement-001")},
        {"actor_player_id", billiardgl::json::Value(0)},
        {"payload", payload}});
    auto expectFailureWithoutMutation =
        [&](int id, const billiardgl::json::Value& params,
            const char* code, const char* message) {
            const AuthoritySnapshot before = authoritySnapshot(runtime);
            const auto response = send(controller, id,
                "rules.confirm_ball_in_hand_placement", params);
            expect(!response.response.at("ok").asBool() &&
                response.response.at("error").at("code").asString() == code &&
                sameAuthoritySnapshot(before, authoritySnapshot(runtime)),
                message);
        };
    auto bad = request;
    bad["payload"].asObject().erase("world_z_cm");
    expectFailureWithoutMutation(406, bad, "invalid_argument",
        "automation rejects incomplete placement payload without mutation");
    bad = request;
    bad["idempotency_key"] = billiardgl::json::Value("placement-inf");
    bad["payload"]["world_x_cm"] = billiardgl::json::Value(
        std::numeric_limits<double>::infinity());
    expectFailureWithoutMutation(407, bad, "invalid_argument",
        "automation rejects nonfinite placement without mutation");
    const double xLimit =
        runtime.physicsProfile().tableBoundary.playfieldWidthCm * 0.5 -
        runtime.physicsProfile().ball.radiusCm;
    bad = request;
    bad["idempotency_key"] = billiardgl::json::Value("placement-outside");
    bad["payload"]["world_x_cm"] =
        billiardgl::json::Value(xLimit + 0.001);
    expectFailureWithoutMutation(408, bad, "rules_placement_out_of_bounds",
        "automation authority rejects out-of-bounds placement");
    bad = request;
    bad["idempotency_key"] = billiardgl::json::Value("placement-pocket");
    bad["payload"]["world_x_cm"] = billiardgl::json::Value(xLimit);
    bad["payload"]["world_z_cm"] = billiardgl::json::Value(0.0);
    expectFailureWithoutMutation(409, bad,
        "rules_placement_forbidden_geometry",
        "automation authority rejects production pocket geometry");
    bad = request;
    bad["idempotency_key"] = billiardgl::json::Value("placement-overlap");
    bad["payload"]["world_x_cm"] = billiardgl::json::Value(
        runtime.state().balls[1].position.x);
    bad["payload"]["world_z_cm"] = billiardgl::json::Value(
        runtime.state().balls[1].position.z);
    expectFailureWithoutMutation(410, bad, "rules_placement_overlap",
        "automation authority rejects active-ball overlap");
    bad = request;
    bad["actor_player_id"] = billiardgl::json::Value(1);
    bad["idempotency_key"] = billiardgl::json::Value("placement-wrong-actor");
    expectFailureWithoutMutation(411, bad, "rules_decision_actor_mismatch",
        "automation authority rejects wrong placement actor");

    const AuthoritySnapshot beforeCommit = authoritySnapshot(runtime);
    const auto committed = send(controller, 412,
        "rules.confirm_ball_in_hand_placement", request);
    const auto& ack = committed.response.at("result");
    const auto& effects = ack.at("effects");
    expect(committed.response.at("ok").asBool() &&
        ack.at("accepted").asBool() &&
        ack.at("decision_id").asString() ==
            pending.at("decision_id").asString() &&
        ack.at("action_id").asString() ==
            billiardgl::rules::kConfirmFullTablePlacementActionId &&
        ack.at("idempotency_key").asString() ==
            "automation-placement-001" &&
        ack.at("previous_state_revision").asNumber() ==
            static_cast<double>(beforeCommit.revision) &&
        ack.at("authoritative_state_revision").asNumber() ==
            static_cast<double>(beforeCommit.revision + 1) &&
        ack.at("chosen_position").at("world_x_cm").asNumber() == -20.0 &&
        ack.at("chosen_position").at("world_z_cm").asNumber() == -30.0 &&
        effects.at("ball_in_hand").asBool() &&
        effects.at("ball_in_hand_scope").asString() == "full_table" &&
        effects.at("ball_in_hand_placement_pending").asBool() &&
        effects.at("current_player").asInt() == 0 &&
        effects.at("next_player").asInt() == 0 &&
        effects.at("groups").asArray().size() == 2 &&
        effects.at("phase").asString() == "groups_assigned" &&
        effects.at("winner").asInt() == -1 &&
        effects.at("ball_positions").at("cue_ball")
            .at("world_z_cm").asNumber() == -30.0,
        "automation placement ACK contains revisions, position, and complete effects");
    const auto readback = send(controller, 413, "get_rules_state");
    expect(readback.response.at("result").at("authoritative_state_hash")
            .asString() == ack.at("authoritative_state_hash").asString() &&
        readback.response.at("result").at("ball_in_hand").asBool() &&
        readback.response.at("result").at("ball_in_hand_placement_pending").asBool(),
        "automation placement readback matches committed authority hash");

    const AuthoritySnapshot afterCommit = authoritySnapshot(runtime);
    const auto retry = send(controller, 414,
        "rules.confirm_ball_in_hand_placement", request);
    expect(retry.response.at("ok").asBool() &&
        billiardgl::json::stringify(retry.response.at("result")) ==
            billiardgl::json::stringify(committed.response.at("result")) &&
        sameAuthoritySnapshot(afterCommit, authoritySnapshot(runtime)),
        "automation placement exact retry returns original ACK exactly once");
    bad = request;
    bad["payload"]["world_x_cm"] = billiardgl::json::Value(-19.0);
    expectFailureWithoutMutation(415, bad,
        "rules_decision_idempotency_conflict",
        "automation placement reused key with different payload conflicts");
    expect(send(controller, 416, "reset_game").response.at("ok").asBool(),
        "automation reset clears placement authority");
    expectFailureWithoutMutation(417, request, "rules_decision_stale",
        "automation old placement request is stale after reset");

    billiardgl::GameRuntime behind;
    billiardgl::AutomationController behindController(
        behind, billiardgl::AutomationMode::Headless);
    send(behindController, 420, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")}}));
    const auto behindFoul = send(behindController, 421,
        "adjudicate_rules_shot", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"shooter", billiardgl::json::Value(0)},
            {"break_shot", billiardgl::json::Value(true)},
            {"first_contact_ball", billiardgl::json::Value(1)},
            {"cue_ball_off_table", billiardgl::json::Value(true)}}));
    const auto behindParentState = send(
        behindController, 422, "get_rules_state");
    const auto& behindParent = behindParentState.response.at("result")
        .at("pending_decision");
    const auto parentCommitted = send(behindController, 423,
        "rules.submit_break_foul_choice", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"ruleset_id", billiardgl::json::Value(
                billiardgl::rules::kBreakFoulRulesetId)},
            {"capability_id", billiardgl::json::Value(
                billiardgl::rules::kBreakFoulCapabilityId)},
            {"flow_id", billiardgl::json::Value(
                billiardgl::rules::kBreakFoulFlowId)},
            {"decision_id", billiardgl::json::Value(
                behindParent.at("decision_id").asString())},
            {"authority_state_revision", billiardgl::json::Value(
                behindParent.at("authority_state_revision").asNumber())},
            {"idempotency_key", billiardgl::json::Value(
                "automation-behind-parent")},
            {"actor_player_id", billiardgl::json::Value(
                behindParent.at("actor_player_id").asInt())},
            {"action_id", billiardgl::json::Value(
                billiardgl::rules::kBreakFoulBallInHandActionId)},
            {"payload", billiardgl::json::Value::object()}}));
    const auto behindState = send(behindController, 424, "get_rules_state");
    expect(behindFoul.response.at("ok").asBool() &&
        parentCommitted.response.at("ok").asBool() &&
        behindState.response.at("result").at("ball_in_hand_scope")
            .asString() == "behind_head_string" &&
        behindState.response.at("result").at("ball_in_hand_placement_pending")
            .asBool() &&
        behindState.response.at("result").has("pending_decision") &&
        behindState.response.at("result").at("pending_decision")
            .at("flow_id").asString() ==
            billiardgl::rules::kBehindHeadStringBallInHandFlowId &&
        behindState.response.at("result").at("pending_decision")
            .at("offered_action_ids").asArray()[0].asString() ==
            billiardgl::rules::kConfirmBehindHeadStringPlacementActionId,
        "automation exposes a runtime-only behind-head-string exact-action offer");
    auto behindRequest = request;
    behindRequest["decision_id"] = billiardgl::json::Value(
        "rules.ball_in_hand.behind.pending");
    behindRequest["authority_state_revision"] = billiardgl::json::Value(
        static_cast<double>(behind.rulesAuthorityRevision()));
    behindRequest["actor_player_id"] = billiardgl::json::Value(
        behind.matchState().currentPlayer);
    behindRequest["idempotency_key"] = billiardgl::json::Value(
        "behind-full-table-forbidden");
    const AuthoritySnapshot behindBefore = authoritySnapshot(behind);
    const auto behindRejected = send(behindController, 425,
        "rules.confirm_ball_in_hand_placement", behindRequest);
    expect(!behindRejected.response.at("ok").asBool() &&
        behindRejected.response.at("error").at("code").asString() ==
            "rules_decision_context_mismatch" &&
        sameAuthoritySnapshot(behindBefore, authoritySnapshot(behind)),
        "automation behind-head-string flow rejects full-table context fail-closed");
    const auto& behindPending = behindState.response.at("result")
        .at("pending_decision");
    expect(behindPending.at("source_clause").asString() == "WPA-II-08" &&
        behindPending.at("source_pdf_page").asInt() == 8 &&
        behindPending.at("head_string_definition_source_clause").asString() ==
            "WPA-II-03-E" &&
        behindPending.at("head_string_definition_pdf_page").asInt() == 6 &&
        near(behindPending.at("head_string_world_z_cm").asNumber(),
            -63.5),
        "automation readback exposes the authoritative head-string plane and source clause");
    const auto hiddenPresentation = send(
        behindController, 426, "get_presentation_state");
    expect(hiddenPresentation.response.at("result").at("rules")
            .at("decision_options").asArray().empty(),
        "automation Product HUD readback keeps head-string actions hidden");
    const auto behindCommitted = send(behindController, 427,
        "rules.confirm_ball_in_hand_placement", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"ruleset_id", billiardgl::json::Value(
                billiardgl::rules::kBallInHandRulesetId)},
            {"capability_id", billiardgl::json::Value(
                billiardgl::rules::kFreeBallPlacementCapabilityId)},
            {"flow_id", billiardgl::json::Value(
                billiardgl::rules::kBehindHeadStringBallInHandFlowId)},
            {"decision_id", billiardgl::json::Value(
                behindPending.at("decision_id").asString())},
            {"action_id", billiardgl::json::Value(
                billiardgl::rules::kConfirmBehindHeadStringPlacementActionId)},
            {"authority_state_revision", billiardgl::json::Value(
                behindPending.at("authority_state_revision").asNumber())},
            {"idempotency_key", billiardgl::json::Value(
                "automation-behind-commit")},
            {"actor_player_id", billiardgl::json::Value(
                behindPending.at("actor_player_id").asInt())},
            {"payload", object({
                {"world_x_cm", billiardgl::json::Value(0.0)},
                {"world_z_cm", billiardgl::json::Value(-90.0)}})}}));
    const auto behindReadback = send(
        behindController, 428, "get_rules_state");
    expect(behindCommitted.response.at("ok").asBool() &&
        behindCommitted.response.at("result").at("action_id").asString() ==
            billiardgl::rules::kConfirmBehindHeadStringPlacementActionId &&
        behindCommitted.response.at("result").at("chosen_position")
            .at("world_z_cm").asNumber() == -90.0 &&
        behindReadback.response.at("result").at("ball_in_hand").asBool() &&
        behindReadback.response.at("result")
            .at("ball_in_hand_placement_pending").asBool(),
        "automation commits and reads back a legal runtime-only head-string placement");
}

void testAutomationRulesCoreARoundTrip()
{
    {
        billiardgl::GameRuntime runtime;
        billiardgl::AutomationController controller(
            runtime, billiardgl::AutomationMode::Headless);
        expect(prepareAssignedTable(controller, 100),
            "automation prepares an assigned table for legal-eight coverage");
        const auto response = send(controller, 103, "adjudicate_rules_shot",
            object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(1)},
                {"first_contact_ball", billiardgl::json::Value(8)},
                {"solids_remaining_before", billiardgl::json::Value(0)},
                {"pocketed_balls_in_order", balls({8})}}));
        const auto& result = response.response.at("result");
        expect(response.response.at("ok").asBool() &&
            result.at("winner").asInt() == 1 &&
            result.at("last_adjudication").at("reasons").asArray().empty() &&
            result.at("last_shot_facts").at("pocketed_balls_in_order")
                .asArray().size() == 1,
            "automation round-trips legal 8 without a no-rail reason");
    }
    {
        billiardgl::GameRuntime runtime;
        billiardgl::AutomationController controller(
            runtime, billiardgl::AutomationMode::Headless);
        expect(prepareAssignedTable(controller, 110),
            "automation prepares an assigned table for off-table coverage");
        const auto response = send(controller, 113, "adjudicate_rules_shot",
            object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(1)},
                {"first_contact_ball", billiardgl::json::Value(1)},
                {"any_rail_after_first_contact", billiardgl::json::Value(true)},
                {"object_balls_off_table", balls({8})}}));
        const auto& result = response.response.at("result");
        const auto& reasons = result.at("last_adjudication")
            .at("reasons").asArray();
        expect(response.response.at("ok").asBool() &&
            result.at("winner").asInt() == 0 && reasons.size() == 2 &&
            result.at("last_shot_facts").at("object_balls_off_table")
                .asArray().size() == 1,
            "automation round-trips 8 off-table loss and source fact");
    }
    {
        billiardgl::GameRuntime runtime;
        billiardgl::AutomationController controller(
            runtime, billiardgl::AutomationMode::Headless);
        expect(prepareAssignedTable(controller, 120),
            "automation prepares an assigned table for same-shot coverage");
        const auto response = send(controller, 123, "adjudicate_rules_shot",
            object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(1)},
                {"first_contact_ball", billiardgl::json::Value(7)},
                {"solids_remaining_before", billiardgl::json::Value(1)},
                {"pocketed_balls_in_order", balls({7, 8})}}));
        expect(response.response.at("ok").asBool() &&
            response.response.at("result").at("winner").asInt() == 0 &&
            response.response.at("result").at("last_adjudication")
                .at("reasons").asArray().size() == 1,
            "automation round-trips group-ball plus early-8 loss");
    }
    {
        billiardgl::GameRuntime runtime;
        billiardgl::AutomationController controller(
            runtime, billiardgl::AutomationMode::Headless);
        expect(prepareAssignedTable(controller, 130),
            "automation prepares an assigned table for non-8 policy coverage");
        const auto response = send(controller, 133, "adjudicate_rules_shot",
            object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(1)},
                {"first_contact_ball", billiardgl::json::Value(1)},
                {"any_rail_after_first_contact", billiardgl::json::Value(true)},
                {"object_balls_off_table", balls({3})}}));
        const auto& result = response.response.at("result");
        expect(response.response.at("ok").asBool() &&
            !result.at("last_adjudication").at("game_over").asBool() &&
            result.at("last_adjudication").at("reasons").asArray().size() == 1,
            "automation preserves the current non-8 off-table foul behavior");
    }
    {
        billiardgl::GameRuntime runtime;
        billiardgl::AutomationController controller(
            runtime, billiardgl::AutomationMode::Headless);
        const auto enabled = send(controller, 140, "set_ruleset", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"ruleset_id", billiardgl::json::Value(
                "wpa_heyball_2025_08_16_v1")}}));
        expect(enabled.response.at("ok").asBool(),
            "automation enables rules for invalid-fact checks");
        auto invalidRequest = [&](int id,
                                  billiardgl::json::Value pockets,
                                  billiardgl::json::Value offTable,
                                  const char* expectedCode) {
            const auto response = send(controller, id,
                "adjudicate_rules_shot", object({
                    {"schema_version", billiardgl::json::Value(1)},
                    {"shooter", billiardgl::json::Value(0)},
                    {"break_shot", billiardgl::json::Value(true)},
                    {"first_contact_ball", billiardgl::json::Value(1)},
                    {"pocketed_balls_in_order", pockets},
                    {"object_balls_off_table", offTable}}));
            expect(!response.response.at("ok").asBool() &&
                response.response.at("error").at("code").asString() ==
                    expectedCode,
                "automation rejects invalid rule facts with the expected code");
        };
        invalidRequest(141, balls({1, 1}), balls({}),
            "rules_facts_duplicate");
        invalidRequest(142, balls({}), balls({2, 2}),
            "rules_facts_duplicate");
        invalidRequest(143, balls({3}), balls({3}),
            "rules_facts_conflict");
        invalidRequest(144, balls({8}), balls({8}),
            "rules_facts_conflict");
        invalidRequest(145, balls({16}), balls({}),
            "rules_facts_out_of_range");
        const auto cueConflict = send(controller, 146,
            "adjudicate_rules_shot", object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(0)},
                {"break_shot", billiardgl::json::Value(true)},
                {"first_contact_ball", billiardgl::json::Value(1)},
                {"cue_ball_pocketed", billiardgl::json::Value(true)},
                {"cue_ball_off_table", billiardgl::json::Value(true)}}));
        expect(!cueConflict.response.at("ok").asBool() &&
            cueConflict.response.at("error").at("code").asString() ==
                "rules_facts_conflict",
            "automation rejects contradictory cue-ball terminal facts");
        const auto untrustedScratch = send(controller, 149,
            "adjudicate_rules_shot", object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(0)},
                {"break_shot", billiardgl::json::Value(true)},
                {"first_contact_ball", billiardgl::json::Value(1)},
                {"cue_ball_pocketed", billiardgl::json::Value(true)}}));
        expect(!untrustedScratch.response.at("ok").asBool() &&
            untrustedScratch.response.at("error").at("code").asString() ==
                "rules_facts_cue_scratch_evidence_invalid",
            "automation cannot inject an unattested cue scratch fact");
        const auto orphanPocket = send(controller, 147,
            "adjudicate_rules_shot", object({
                {"schema_version", billiardgl::json::Value(1)},
                {"shooter", billiardgl::json::Value(0)},
                {"break_shot", billiardgl::json::Value(true)},
                {"first_contact_ball", billiardgl::json::Value(1)},
                {"eight_ball_pocket", billiardgl::json::Value(2)}}));
        expect(!orphanPocket.response.at("ok").asBool() &&
            orphanPocket.response.at("error").at("code").asString() ==
                "rules_facts_conflict",
            "automation rejects an orphan 8-ball pocket id");
        const auto state = send(controller, 148, "get_rules_state");
        expect(state.response.at("result").at("phase").asString() == "break" &&
            !state.response.at("result").at("has_last_adjudication").asBool(),
            "automation invalid facts fail closed without state mutation");
    }
}

void testAutomationBreakFoulAuthorityRoundTrip()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const auto capabilitiesResponse = send(
        controller, 560, "get_capabilities");
    std::set<std::string> capabilities;
    for (const auto& item : capabilitiesResponse.response.at("result")
            .at("commands").asArray())
        capabilities.insert(item.asString());
    expect(capabilities.count("break_foul_opponent_choices") != 0 &&
        capabilities.count("rules.submit_break_foul_choice") != 0,
        "automation advertises Core H capability and submit command");
    expect(send(controller, 561, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")}})).response.at("ok").asBool(),
        "automation Core H fixture enables rules");
    const auto injected = send(controller, 562, "adjudicate_rules_shot",
        object({
            {"schema_version", billiardgl::json::Value(1)},
            {"shooter", billiardgl::json::Value(0)},
            {"break_shot", billiardgl::json::Value(true)},
            {"first_contact_ball", billiardgl::json::Value(1)},
            {"distinct_object_balls_to_rail",
                billiardgl::json::Value(3)}}));
    expect(!injected.response.at("ok").asBool() &&
        injected.response.at("error").at("code").asString() ==
            "rules_illegal_break_requires_authoritative_evidence" &&
        !runtime.breakFoulDecisionPending(),
        "automation direct scalar facts cannot stage a product illegal-break authority offer");
    auto canonicalWeak = facts(0, 1);
    canonicalWeak.breakShot = true;
    attestStraightBreakCushions(canonicalWeak, {1, 2, 3});
    expect(runtime.adjudicateRulesShot(canonicalWeak).ok,
        "authoritative canonical facts stage the illegal-break authority before automation readback");
    const auto staged = send(controller, 563, "get_rules_state");
    const auto presentation = send(
        controller, 564, "get_presentation_state");
    const auto& state = staged.response.at("result");
    const auto& pending = state.at("pending_decision");
    const auto& actions = pending.at("offered_action_ids").asArray();
    expect(state.at("break_foul_decision_pending").asBool() &&
        pending.at("capability_id").asString() ==
            billiardgl::rules::kBreakFoulCapabilityId &&
        pending.at("flow_id").asString() ==
            billiardgl::rules::kBreakFoulFlowId &&
        pending.at("branch").asString() == "illegal_break" &&
        pending.at("source_clause").asString() == "WPA-II-06-C" &&
        pending.at("source_pdf_page").asInt() == 7 &&
        pending.at("actor_player_id").asInt() == 1 &&
        pending.at("original_breaker_player_id").asInt() == 0 &&
        actions.size() == 3 &&
        actions[0].asString() ==
            billiardgl::rules::kBreakFoulAcceptTableActionId &&
        presentation.response.at("result").at("rules")
            .at("decision_options").asArray().empty(),
        "automation readback exposes exact authority metadata while Product presentation stays hidden");

    auto request = object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            billiardgl::rules::kBreakFoulRulesetId)},
        {"capability_id", billiardgl::json::Value(
            billiardgl::rules::kBreakFoulCapabilityId)},
        {"flow_id", billiardgl::json::Value(
            billiardgl::rules::kBreakFoulFlowId)},
        {"decision_id", billiardgl::json::Value(
            pending.at("decision_id").asString())},
        {"authority_state_revision", billiardgl::json::Value(
            pending.at("authority_state_revision").asNumber())},
        {"idempotency_key", billiardgl::json::Value(
            "automation-break-accept")},
        {"actor_player_id", billiardgl::json::Value(1)},
        {"action_id", billiardgl::json::Value(
            billiardgl::rules::kBreakFoulAcceptTableActionId)},
        {"payload", billiardgl::json::Value::object()}});
    auto wrong = request;
    wrong["flow_id"] = billiardgl::json::Value(
        billiardgl::rules::kEightOnBreakFlowId);
    wrong["idempotency_key"] = billiardgl::json::Value(
        "automation-break-wrong");
    const auto rejected = send(controller, 564,
        "rules.submit_break_foul_choice", wrong);
    expect(!rejected.response.at("ok").asBool() &&
        rejected.response.at("error").at("code").asString() ==
            "rules_decision_context_mismatch",
        "automation Core H submit rejects mismatched context");
    const auto committed = send(controller, 565,
        "rules.submit_break_foul_choice", request);
    const auto readback = send(controller, 566, "get_rules_state");
    expect(committed.response.at("ok").asBool() &&
        committed.response.at("result").at("effects")
            .at("table_accepted").asBool() &&
        !committed.response.at("result").at("effects")
            .at("ball_in_hand").asBool() &&
        !readback.response.at("result")
            .at("break_foul_decision_pending").asBool() &&
        readback.response.at("result").at("current_player").asInt() == 1,
        "automation Core H ACK and readback reflect the committed accept-table result");

    billiardgl::GameRuntime ambiguousRuntime;
    billiardgl::AutomationController ambiguous(
        ambiguousRuntime, billiardgl::AutomationMode::Headless);
    send(ambiguous, 567, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            "wpa_heyball_2025_08_16_v1")}}));
    const auto noContact = send(ambiguous, 568,
        "adjudicate_rules_shot", object({
            {"schema_version", billiardgl::json::Value(1)},
            {"shooter", billiardgl::json::Value(0)},
            {"break_shot", billiardgl::json::Value(true)}}));
    expect(!noContact.response.at("ok").asBool() &&
        noContact.response.at("error").at("code").asString() ==
            "rules_break_no_rack_contact_classification_pending",
        "automation keeps WPA II 6(g) unintentional-miscue classification fail-closed");
}

void testAutomationEightOnBreakAuthorityRoundTrip()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const auto capabilities = controller.capabilities();
    expect(std::find(capabilities.begin(), capabilities.end(),
            billiardgl::rules::kEightOnBreakCapabilityId) !=
            capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            "rules.submit_eight_on_break_choice") != capabilities.end(),
        "automation advertises the runtime-only 8-on-break authority capability and submit command");
    const auto enabled = send(controller, 600, "set_ruleset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            billiardgl::rules::kEightOnBreakRulesetId)}}));
    const auto staged = send(controller, 601, "adjudicate_rules_shot",
        object({
            {"schema_version", billiardgl::json::Value(1)},
            {"shooter", billiardgl::json::Value(0)},
            {"break_shot", billiardgl::json::Value(true)},
            {"first_contact_ball", billiardgl::json::Value(1)},
            {"pocketed_balls_in_order", balls({8})}}));
    const auto& stagedRules = staged.response.at("result");
    const auto& pending = stagedRules.at("pending_decision");
    const auto& offered = pending.at("offered_action_ids").asArray();
    expect(enabled.response.at("ok").asBool() &&
        staged.response.at("ok").asBool() &&
        stagedRules.at("eight_on_break_decision_pending").asBool() &&
        pending.at("capability_id").asString() ==
            billiardgl::rules::kEightOnBreakCapabilityId &&
        pending.at("flow_id").asString() ==
            billiardgl::rules::kEightOnBreakFlowId &&
        pending.at("branch").asString() == "legal_break" &&
        pending.at("actor_player_id").asInt() == 0 &&
        pending.at("original_breaker_player_id").asInt() == 0 &&
        offered.size() == 2 &&
        offered[0].asString() ==
            billiardgl::rules::kEightOnBreakSpotContinueActionId &&
        offered[1].asString() ==
            billiardgl::rules::kEightOnBreakRerackOriginalActionId,
        "automation readback exposes the stable legal-branch exact-action offer");
    const auto presentation = send(
        controller, 602, "get_presentation_state");
    expect(presentation.response.at("result").at("rules")
            .at("decision_options").asArray().empty(),
        "automation presentation readback keeps the pending Product HUD actions hidden");

    auto submitParams = object({
        {"schema_version", billiardgl::json::Value(1)},
        {"ruleset_id", billiardgl::json::Value(
            billiardgl::rules::kEightOnBreakRulesetId)},
        {"capability_id", billiardgl::json::Value(
            billiardgl::rules::kEightOnBreakCapabilityId)},
        {"flow_id", billiardgl::json::Value(
            billiardgl::rules::kEightOnBreakFlowId)},
        {"decision_id", billiardgl::json::Value(
            pending.at("decision_id").asString())},
        {"authority_state_revision", billiardgl::json::Value(
            pending.at("authority_state_revision").asNumber())},
        {"idempotency_key", billiardgl::json::Value(
            "automation-eight-spot")},
        {"actor_player_id", billiardgl::json::Value(0)},
        {"action_id", billiardgl::json::Value(
            billiardgl::rules::kEightOnBreakSpotContinueActionId)},
        {"payload", billiardgl::json::Value::object()}});
    auto wrongContext = submitParams;
    wrongContext["flow_id"] = billiardgl::json::Value(
        "break_foul_opponent_choice");
    wrongContext["idempotency_key"] = billiardgl::json::Value(
        "automation-eight-wrong-context");
    const auto rejected = send(controller, 603,
        "rules.submit_eight_on_break_choice", wrongContext);
    expect(!rejected.response.at("ok").asBool() &&
        rejected.response.at("error").at("code").asString() ==
            "rules_decision_context_mismatch",
        "automation submit validates exact ruleset/capability/flow context");
    const auto committed = send(controller, 604,
        "rules.submit_eight_on_break_choice", submitParams);
    const auto& ack = committed.response.at("result");
    const auto& effects = ack.at("effects");
    const auto readback = send(controller, 605, "get_rules_state");
    expect(committed.response.at("ok").asBool() &&
        ack.at("accepted").asBool() &&
        ack.at("action_id").asString() ==
            billiardgl::rules::kEightOnBreakSpotContinueActionId &&
        effects.at("eight_ball_respotted").asBool() &&
        !effects.at("rack_rebuilt").asBool() &&
        effects.at("turn_continues").asBool() &&
        effects.at("current_player").asInt() == 0 &&
        !effects.at("decision_pending").asBool() &&
        !readback.response.at("result")
            .at("eight_on_break_decision_pending").asBool() &&
        !readback.response.at("result").at("last_adjudication")
            .at("eight_ball_must_be_spotted").asBool(),
        "automation submit returns a complete authority ACK and committed readback without Product UI promotion");
}

void testRefereeAuthorityFoundationRuntimeIsolation()
{
    billiardgl::GameRuntime runtime;
    expect(runtime.refereeAuthorityState().protocolId ==
            billiardgl::rules::kRefereeAuthorityProtocolId &&
        runtime.refereeAuthorityState().authorityRevision == 0 &&
        !runtime.refereeAuthorityState().liveProposal.has_value() &&
        !runtime.refereeAuthorityState().latestFinalDeclaration.has_value(),
        "runtime exposes an empty generic referee-authority foundation");
    const auto rulesBefore = runtime.matchState();
    const auto gameBefore = runtime.state();
    const std::string authorityHashBefore =
        runtime.refereeAuthorityState().authorityStateHash;
    runtime.reset();
    expect(runtime.refereeAuthorityState().authorityRevision == 0 &&
        runtime.refereeAuthorityState().journal.empty() &&
        runtime.refereeAuthorityState().authorityStateHash !=
            authorityHashBefore &&
        runtime.matchState().currentPlayer == rulesBefore.currentPlayer &&
        runtime.state().players.currentPlayer ==
            gameBefore.players.currentPlayer,
        "runtime reset invalidates generic authority without synthesizing rule or physics facts");
}

void testStalemateRuntimeCoreERerackTransaction()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;
    StalemateTestGuard guard;
    GameRuntime runtime;
    runtime.configureStalemateAuthority(&auth, &guard);
    const std::string proposalPayload =
        "{\"directive_kind\":\"DEFAULT_ADVISORY_THREE_EACH\","
        "\"per_player_shot_allowance\":3,\"stalemate_attested\":true}";
    auto proposal = stalemateRequest(runtime,
        RefereeAuthorityOperation::Propose, kStalemateDirectiveKind,
        "referee", "runtime.proposal", "runtime.proposal.key",
        proposalPayload);
    expect(runtime.submitStalemateProposal(proposal).ok,
        "runtime accepts authenticated stalemate proposal");
    auto refusal = stalemateRequest(runtime,
        RefereeAuthorityOperation::Propose, kStalemateRefusalKind,
        "player.1", "runtime.refusal", "runtime.refusal.key",
        "{\"proposal_decision_id\":\"runtime.stalemate.1\","
        "\"refusing_player_id\":1}");
    expect(runtime.submitStalemateRefusal(refusal).ok &&
        runtime.stalemateState().phase ==
            StalematePhase::ContinuationActive,
        "detached signed player refusal starts runtime counter window");

    const auto stateBefore = runtime.state();
    const auto orderBefore = runtime.stalemateState().originalShootingOrder;
    const std::string finalPayload =
        std::string("{\"counter_snapshot_sha256\":\"") +
        runtime.stalemateState().counterSnapshotSha256 +
        "\",\"directive_kind\":\"DEFAULT_ADVISORY_THREE_EACH\","
        "\"per_player_completed_shots\":[0,0],"
        "\"per_player_shot_allowance\":3,"
        "\"proposal_decision_id\":\"runtime.stalemate.1\","
        "\"stalemate_still_present_attested\":true}";
    auto final = stalemateRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare, kStalemateDirectiveKind,
        "referee", "runtime.final", "runtime.final.key", finalPayload);
    guard.allow = false;
    const auto rejected = runtime.submitStalemateFinal(final);
    expect(!rejected.ok && !runtime.stalemateState().finalAuthorized &&
        !runtime.stalemateState().rerackCommitted &&
        runtime.state().balls[8].position.x ==
            stateBefore.balls[8].position.x &&
        runtime.state().balls[8].position.z ==
            stateBefore.balls[8].position.z,
        "CORE_E_PARENT_COMMIT_FAILURE_FULL_ROLLBACK");
    guard.allow = true;
    const auto committed = runtime.submitStalemateFinal(final);
    expect(committed.ok && runtime.stalemateState().rerackCommitted &&
        runtime.state().players.currentPlayer == orderBefore[0] &&
        runtime.state().players.nextPlayer == orderBefore[1] &&
        runtime.matchState().currentPlayer == orderBefore[0],
        "REFEREE_FINAL_ATOMICALLY_RERACKS_FROZEN_ORIGINAL_ORDER");
    const std::size_t eventCount = runtime.events().size();
    expect(runtime.submitStalemateFinal(final).ok &&
        runtime.events().size() == eventCount,
        "RUNTIME_FINAL_EXACT_RETRY_HAS_ZERO_RERACK_EVENT_DELTA");
    runtime.reset();
    expect(!runtime.submitStalemateFinal(final).ok &&
        runtime.stalemateState().phase == StalematePhase::Inactive,
        "RESET_INVALIDATES_OLD_STALEMATE_FINAL");
}

void testGroupConfusionRuntimeCoreERerackTransaction()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;
    StalemateTestGuard guard;
    GameRuntime runtime;
    runtime.configureGroupConfusionAuthority(&auth, &guard);
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "group confusion runtime enables rules");
    auto breakShot = facts(0, 1);
    breakShot.breakShot = true;
    breakShot.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(breakShot).ok,
        "group confusion setup accepts legal break");
    auto assignment = facts(1, 1);
    assignment.pocketedBallsInOrder = {1};
    expect(runtime.adjudicateRulesShot(assignment).ok &&
        runtime.groupConfusionState().phase ==
            GroupConfusionPhase::TimelyWindowOpen,
        "closed-table completed shot opens section 19 window");

    const auto& open = runtime.groupConfusionState();
    const std::string proposalPayload =
        std::string("{\"assignment_snapshot_sha256\":\"") +
        open.window.assignmentSnapshotSha256 +
        "\",\"claimed_confusion_kind\":\"ASSIGNMENT_OR_SEQUENCE_CONFUSION\"," +
        "\"completed_shot_identity\":\"" +
        open.window.completedShotIdentity +
        "\",\"realization_attested\":true,\"window_generation\":" +
        std::to_string(open.windowGeneration) + "}";
    auto proposal = groupConfusionRequest(runtime,
        RefereeAuthorityOperation::Propose, "player.1",
        "runtime.confusion.proposal", "runtime.confusion.proposal.key",
        proposalPayload);
    expect(runtime.submitGroupConfusionProposal(proposal).ok &&
        runtime.groupConfusionState().phase ==
            GroupConfusionPhase::RealizationProposed,
        "signed player realization stops rack");
    expect(!runtime.applyCueImpact({}).ok,
        "stopped rack rejects additional cue launch");

    const auto before = runtime.state();
    const auto order =
        runtime.groupConfusionState().window.originalShootingOrder;
    const std::string finalPayload =
        std::string("{\"assignment_snapshot_sha256\":\"") +
        runtime.groupConfusionState().window.assignmentSnapshotSha256 +
        "\",\"claimed_confusion_kind\":\"ASSIGNMENT_OR_SEQUENCE_CONFUSION\"," +
        "\"completed_shot_identity\":\"" +
        runtime.groupConfusionState().window.completedShotIdentity +
        "\",\"proposal_decision_id\":\"runtime.confusion.1\"," +
        "\"rack_order_snapshot_sha256\":\"" +
        runtime.groupConfusionState().rackOrderSnapshotSha256 +
        "\",\"realization_still_attested\":true,\"window_generation\":" +
        std::to_string(runtime.groupConfusionState().windowGeneration) +
        "}";
    auto final = groupConfusionRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare, "player.1",
        "runtime.confusion.final", "runtime.confusion.final.key",
        finalPayload);
    guard.allow = false;
    expect(!runtime.submitGroupConfusionFinal(final).ok &&
        !runtime.groupConfusionState().finalAuthorized &&
        runtime.state().balls[8].position.x == before.balls[8].position.x &&
        runtime.state().balls[8].position.z == before.balls[8].position.z,
        "group confusion final guard failure is atomic");
    guard.allow = true;
    expect(runtime.submitGroupConfusionFinal(final).ok &&
        runtime.groupConfusionState().rerackCommitted &&
        runtime.state().players.currentPlayer == order[0] &&
        runtime.state().players.nextPlayer == order[1] &&
        !runtime.matchState().ballInHand,
        "section 19 final atomically reracks original breaker and order");
    const std::size_t events = runtime.events().size();
    expect(runtime.submitGroupConfusionFinal(final).ok &&
        runtime.events().size() == events,
        "group confusion final retry has zero event delta");
    runtime.reset();
    expect(!runtime.submitGroupConfusionFinal(final).ok &&
        runtime.groupConfusionState().phase ==
            GroupConfusionPhase::Inactive,
        "reset invalidates section 19 request");
}

void testFrozenDeclarationRuntimeLifecycle()
{
    using namespace billiardgl;using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;GameRuntime runtime;
    runtime.configureFrozenBallDeclarationAuthority(&auth);
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "frozen declaration enables rules");
    const std::string proposalPayload=
        "{\"cue_ball_id\":0,\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"relation\":\"CUE_BALL_FROZEN_TO_LEGAL_OBJECT_BALL\"}";
    auto proposal=frozenDeclarationRequest(runtime,
        RefereeAuthorityOperation::Propose,"frozen.p","frozen.p.key",
        proposalPayload);
    expect(runtime.submitFrozenBallDeclarationProposal(proposal).ok&&
        !runtime.frozenBallDeclarationState().committed,
        "referee proposal does not itself declare frozen pair");
    const std::string finalPayload=
        "{\"cue_ball_id\":0,\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"proposal_decision_id\":\"runtime.frozen.1\","
        "\"relation\":\"CUE_BALL_FROZEN_TO_LEGAL_OBJECT_BALL\"}";
    auto final=frozenDeclarationRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"frozen.f","frozen.f.key",
        finalPayload);
    expect(runtime.submitFrozenBallDeclarationFinal(final).ok&&
        runtime.frozenBallDeclarationState().phase==
            FrozenBallDeclarationPhase::Active&&
        runtime.rulesReplayEvents().size()==1&&
        runtime.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::FrozenBallDeclarationCommit,
        "same referee final commits exact pre-shot declaration");
    expect(runtime.submitFrozenBallDeclarationFinal(final).ok&&
        runtime.rulesReplayEvents().size()==1,
        "exact frozen-ball final retry emits no duplicate receipt");
    CueImpactInput zero;zero.cueSpeedCmS=0.0;
    expect(!runtime.applyCueImpact(zero).ok&&
        runtime.frozenBallDeclarationState().phase==
            FrozenBallDeclarationPhase::Active,
        "zero rejected launch does not consume declaration");
    CueImpactInput positive;
    positive.cueBallIndex=0;
    positive.cueSpeedCmS=120.0;
    positive.cueMassKg=runtime.physicsProfile().cue.effectiveMassKg;
    positive.direction={{1.0,0.0,0.0}};
    positive.chalkState="CHALKED";
    expect(runtime.applyCueImpact(positive).ok&&
        runtime.frozenBallDeclarationState().phase==
            FrozenBallDeclarationPhase::Consumed&&
        !runtime.frozenBallDeclarationState().consumedShotIdentity.empty(),
        "strict positive accepted impulse consumes declaration exactly once");
    runtime.reset();
    expect(!runtime.submitFrozenBallDeclarationFinal(final).ok&&
        runtime.frozenBallDeclarationState().phase==
            FrozenBallDeclarationPhase::Inactive,
        "reset invalidates stale frozen declaration request");
}

void testFrozenCushionDeclarationRuntimeLifecycle()
{
    using namespace billiardgl;using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;GameRuntime runtime;
    runtime.configureFrozenCushionDeclarationAuthority(&auth);
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "frozen cushion declaration enables rules");
    const std::string proposalPayload=
        "{\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\","
        "\"straight_cushion_id\":\"HEAD\"}";
    auto proposal=frozenCushionRequest(runtime,
        RefereeAuthorityOperation::Propose,"cushion.p","cushion.p.key",
        proposalPayload);
    expect(runtime.submitFrozenCushionDeclarationProposal(proposal).ok&&
        !runtime.frozenCushionDeclarationState().committed,
        "cushion proposal does not itself declare frozen relation");
    const std::string finalPayload=
        "{\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"proposal_decision_id\":\"runtime.cushion.1\","
        "\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\","
        "\"straight_cushion_id\":\"HEAD\"}";
    auto final=frozenCushionRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"cushion.f","cushion.f.key",
        finalPayload);
    expect(runtime.submitFrozenCushionDeclarationFinal(final).ok&&
        runtime.frozenCushionDeclarationState().phase==
            FrozenCushionDeclarationPhase::Active&&
        runtime.rulesReplayEvents().size()==1&&
        runtime.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::FrozenCushionDeclarationCommit,
        "cushion final commits exact pre-shot declaration");
    expect(runtime.submitFrozenCushionDeclarationFinal(final).ok&&
        runtime.rulesReplayEvents().size()==1,
        "exact frozen-cushion final retry emits no duplicate receipt");
    CueImpactInput zero;zero.cueSpeedCmS=0.0;
    expect(!runtime.applyCueImpact(zero).ok&&
        runtime.frozenCushionDeclarationState().phase==
            FrozenCushionDeclarationPhase::Active,
        "zero launch does not consume cushion declaration");
    CueImpactInput positive;positive.cueBallIndex=0;
    positive.cueSpeedCmS=120.0;
    positive.cueMassKg=runtime.physicsProfile().cue.effectiveMassKg;
    positive.direction={{1.0,0.0,0.0}};positive.chalkState="CHALKED";
    expect(runtime.applyCueImpact(positive).ok&&
        runtime.frozenCushionDeclarationState().phase==
            FrozenCushionDeclarationPhase::Consumed&&
        !runtime.frozenCushionDeclarationState().consumedShotIdentity.empty(),
        "positive accepted impulse consumes cushion declaration once");
    runtime.reset();
    expect(!runtime.submitFrozenCushionDeclarationFinal(final).ok&&
        runtime.frozenCushionDeclarationState().phase==
            FrozenCushionDeclarationPhase::Inactive,
        "reset invalidates stale cushion declaration");
}

void testDoubleHitCloseCaseRuntimeLifecycle()
{
    using namespace billiardgl;using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;GameRuntime runtime;
    runtime.configureDoubleHitCloseCaseAuthority(&auth);
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,"close case rules");
    const std::string proposalPayload="{\"close_case_attested\":true,\"cue_ball_id\":0,\"object_ball_id\":1,\"relation\":\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";
    auto proposal=closeCaseRequest(runtime,RefereeAuthorityOperation::Propose,
        "close.p","close.p.key",proposalPayload);
    expect(runtime.submitDoubleHitCloseCaseProposal(proposal).ok&&
        !runtime.doubleHitCloseCaseState().reviewNeeded,"proposal is not attestation");
    const std::string finalPayload="{\"close_case_attested\":true,\"cue_ball_id\":0,\"object_ball_id\":1,\"proposal_decision_id\":\"runtime.close.1\",\"relation\":\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";
    auto final=closeCaseRequest(runtime,RefereeAuthorityOperation::FinalDeclare,
        "close.f","close.f.key",finalPayload);
    expect(runtime.submitDoubleHitCloseCaseFinal(final).ok&&
        runtime.doubleHitCloseCaseState().reviewNeeded&&
        runtime.rulesReplayEvents().size()==1&&
        runtime.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::DoubleHitCloseCaseDeclarationCommit,
        "final marks review only and emits one typed receipt");
    expect(runtime.submitDoubleHitCloseCaseFinal(final).ok&&
        runtime.rulesReplayEvents().size()==1,
        "exact close-case final retry emits no duplicate receipt");
    CueImpactInput zero;expect(!runtime.applyCueImpact(zero).ok&&
        runtime.doubleHitCloseCaseState().phase==DoubleHitCloseCasePhase::Active,
        "zero does not consume close case");
    CueImpactInput positive;positive.cueBallIndex=0;positive.cueSpeedCmS=120;
    positive.cueMassKg=runtime.physicsProfile().cue.effectiveMassKg;
    positive.direction={{1,0,0}};positive.chalkState="CHALKED";
    expect(runtime.applyCueImpact(positive).ok&&
        runtime.doubleHitCloseCaseState().phase==DoubleHitCloseCasePhase::Consumed&&
        runtime.doubleHitCloseCaseState().consumedShotIdentity==
            runtime.activeCanonicalCueTipStrokeIdentity(),
        "positive consumes close case with the exact canonical shot identity");
    runtime.reset();expect(!runtime.submitDoubleHitCloseCaseFinal(final).ok&&
        runtime.doubleHitCloseCaseState().phase==DoubleHitCloseCasePhase::Inactive,
        "reset invalidates close case");
}

void testPushReviewRuntimeLifecycle()
{
    using namespace billiardgl;using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;GameRuntime runtime;
    runtime.configurePushReviewAuthority(&auth);
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,"push review rules");
    const std::string proposalPayload="{\"cue_ball_id\":0,\"push_review_required\":true,\"relation\":\"POTENTIAL_OBVIOUS_PUSH_REQUIRES_COMPLETE_MEASUREMENTS_AND_REFEREE_REVIEW\"}";
    auto proposal=pushReviewRequest(runtime,RefereeAuthorityOperation::Propose,
        "push.p","push.p.key",proposalPayload);
    expect(runtime.submitPushReviewProposal(proposal).ok&&
        !runtime.pushReviewState().reviewNeeded,"proposal is not push attestation");
    const std::string finalPayload="{\"cue_ball_id\":0,\"proposal_decision_id\":\"runtime.push.1\",\"push_review_required\":true,\"relation\":\"POTENTIAL_OBVIOUS_PUSH_REQUIRES_COMPLETE_MEASUREMENTS_AND_REFEREE_REVIEW\"}";
    auto final=pushReviewRequest(runtime,RefereeAuthorityOperation::FinalDeclare,
        "push.f","push.f.key",finalPayload);
    expect(runtime.submitPushReviewFinal(final).ok&&runtime.pushReviewState().reviewNeeded,
        "final marks push review needed only");
    expect(runtime.rulesReplayEvents().size()==1&&
        runtime.rulesReplayEvents().back().type==
            RuntimeRulesReplayEventType::PushReviewDeclarationCommit&&
        runtime.submitPushReviewFinal(final).ok&&
        runtime.rulesReplayEvents().size()==1,
        "push review final emits one receipt and exact retry is idempotent");
    CueImpactInput zero;expect(!runtime.applyCueImpact(zero).ok&&
        runtime.pushReviewState().phase==PushReviewPhase::Active,
        "zero launch does not consume push review");
    CueImpactInput positive;positive.cueBallIndex=0;positive.cueSpeedCmS=120;
    positive.cueMassKg=runtime.physicsProfile().cue.effectiveMassKg;
    positive.direction={{1,0,0}};positive.chalkState="CHALKED";
    expect(runtime.applyCueImpact(positive).ok&&
        runtime.pushReviewState().phase==PushReviewPhase::Consumed&&
        runtime.pushReviewState().consumedShotIdentity==
            runtime.activeCanonicalCueTipStrokeIdentity(),
        "positive accepted impulse consumes push review with the exact canonical shot identity");
    runtime.reset();expect(!runtime.submitPushReviewFinal(final).ok&&
        runtime.pushReviewState().phase==PushReviewPhase::Inactive,
        "reset invalidates stale push review");
}

void testDeclarationReplayLifecycleBoundaries()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    const std::string executable="declaration-lifecycle-v3";
    const std::string content=sha256Hex("declaration-lifecycle-content");
    const std::string proposalPayload=
        "{\"cue_ball_id\":0,\"push_review_required\":true,"
        "\"relation\":\"POTENTIAL_OBVIOUS_PUSH_REQUIRES_COMPLETE_MEASUREMENTS_AND_REFEREE_REVIEW\"}";
    const std::string finalPayload=
        "{\"cue_ball_id\":0,\"proposal_decision_id\":\"runtime.push.1\","
        "\"push_review_required\":true,\"relation\":"
        "\"POTENTIAL_OBVIOUS_PUSH_REQUIRES_COMPLETE_MEASUREMENTS_AND_REFEREE_REVIEW\"}";
    StalemateTestAuthenticator auth;

    GameRuntime pending;
    pending.configurePushReviewAuthority(&auth);
    expect(pending.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "pending-declaration recorder fixture enables rules");
    replay::Recorder pendingRecorder;
    expect(pendingRecorder.begin(pending,executable,content,81).ok,
        "recorder starts before an explicit declaration proposal");
    auto proposal=pushReviewRequest(pending,
        RefereeAuthorityOperation::Propose,"lifecycle.pending.p",
        "lifecycle.pending.p.key",proposalPayload);
    expect(pending.submitPushReviewProposal(proposal).ok,
        "signed proposal opens the declaration transaction");
    const auto pendingCapture=pendingRecorder.captureApplied(pending);
    expect(!pendingCapture.ok&&pendingCapture.errorCode==
            "replay_rules_declaration_proposal_pending"&&
        !pendingRecorder.active()&&
        pendingRecorder.finish(pending,0).errorCode==
            "replay_rules_declaration_proposal_pending",
        "observed unrepresented proposal permanently fails the recording closed");
    replay::Recorder beginsPending;
    expect(beginsPending.begin(pending,executable,content,82).errorCode==
            "replay_rules_declaration_proposal_pending",
        "recording cannot begin from a pending declaration transaction");

    auto cancel=pushReviewRequest(pending,
        RefereeAuthorityOperation::CancelProposal,"lifecycle.cancel",
        "lifecycle.cancel.key",
        "{\"proposal_decision_id\":\"runtime.push.1\"}");
    expect(pending.submitPushReviewCancel(cancel).ok&&
        pending.pushReviewState().phase==PushReviewPhase::Cancelled,
        "same referee can explicitly cancel the proposal");
    replay::Recorder beginsCancelled;
    expect(beginsCancelled.begin(pending,executable,content,83).errorCode==
            "replay_rules_declaration_cancel_unsupported",
        "recording cannot silently start after an unrepresented cancel");

    GameRuntime cancelledDuringRecording;
    cancelledDuringRecording.configurePushReviewAuthority(&auth);
    expect(cancelledDuringRecording.setRulesMode(
            RulesMode::ChineseEightBallV1).ok,
        "cancel-during-recording fixture enables rules");
    replay::Recorder cancelRecorder;
    expect(cancelRecorder.begin(cancelledDuringRecording,executable,content,83).ok,
        "recorder starts before a proposal-cancel pair");
    proposal=pushReviewRequest(cancelledDuringRecording,
        RefereeAuthorityOperation::Propose,"lifecycle.cancelled.p",
        "lifecycle.cancelled.p.key",proposalPayload);
    expect(cancelledDuringRecording.submitPushReviewProposal(proposal).ok,
        "cancel-during-recording fixture accepts proposal");
    cancel=pushReviewRequest(cancelledDuringRecording,
        RefereeAuthorityOperation::CancelProposal,"lifecycle.cancelled.c",
        "lifecycle.cancelled.c.key",
        "{\"proposal_decision_id\":\"runtime.push.1\"}");
    expect(cancelledDuringRecording.submitPushReviewCancel(cancel).ok,
        "cancel-during-recording fixture accepts cancellation");
    const auto cancelledCapture=
        cancelRecorder.captureApplied(cancelledDuringRecording);
    expect(!cancelledCapture.ok&&cancelledCapture.errorCode==
            "replay_rules_declaration_cancel_unsupported"&&
        !cancelRecorder.active()&&
        cancelRecorder.finish(cancelledDuringRecording,0).errorCode==
            "replay_rules_declaration_cancel_unsupported",
        "unrepresented accepted cancel permanently fails recording closed");

    GameRuntime committed;
    committed.configurePushReviewAuthority(&auth);
    expect(committed.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "committed-declaration recorder fixture enables rules");
    replay::Recorder committedRecorder;
    expect(committedRecorder.begin(committed,executable,content,84).ok,
        "recorder starts before an atomic proposal-final pair");
    proposal=pushReviewRequest(committed,
        RefereeAuthorityOperation::Propose,"lifecycle.commit.p",
        "lifecycle.commit.p.key",proposalPayload);
    expect(committed.submitPushReviewProposal(proposal).ok,
        "atomic declaration fixture accepts proposal");
    auto final=pushReviewRequest(committed,
        RefereeAuthorityOperation::FinalDeclare,"lifecycle.commit.f",
        "lifecycle.commit.f.key",finalPayload);
    const auto revisionBeforeRejected=
        committed.pushReviewProtocolState().authorityRevision;
    const auto stateBeforeRejected=
        committed.pushReviewProtocolState().authorityStateHash;
    const auto eventsBeforeRejected=committed.rulesReplayEvents().size();
    auto badFinal=final;
    badFinal.authenticationProof="invalid-proof";
    expect(!committed.submitPushReviewFinal(badFinal).ok&&
        committed.pushReviewProtocolState().authorityRevision==
            revisionBeforeRejected&&
        committed.pushReviewProtocolState().authorityStateHash==
            stateBeforeRejected&&
        committed.rulesReplayEvents().size()==eventsBeforeRejected,
        "rejected signed final has no authority or replay side effect");
    committed.mutableState().ballsMoving=true;
    expect(!committed.submitPushReviewFinal(final).ok&&
        committed.pushReviewProtocolState().authorityRevision==
            revisionBeforeRejected&&
        committed.rulesReplayEvents().size()==eventsBeforeRejected,
        "final declaration is rejected atomically while a shot is moving");
    committed.mutableState().ballsMoving=false;
    expect(committed.submitPushReviewFinal(final).ok&&
        committedRecorder.captureApplied(committed).ok&&
        committedRecorder.document().rulesReceipts.size()==1,
        "proposal-final completed between captures is represented by its committed receipt");

    GameRuntime nextShot;
    nextShot.configurePushReviewAuthority(&auth);
    expect(nextShot.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "next-shot boundary fixture enables rules");
    auto prior=facts(0,1);
    prior.breakShot=true;
    prior.distinctObjectBallsToRail=4;
    expect(nextShot.adjudicateRulesShot(prior).ok,
        "a completed prior shot establishes the stopped next-shot boundary");
    proposal=pushReviewRequest(nextShot,
        RefereeAuthorityOperation::Propose,"lifecycle.next.p",
        "lifecycle.next.p.key",proposalPayload);
    expect(nextShot.submitPushReviewProposal(proposal).ok,
        "declaration proposal after shot completion targets the following shot");
    final=pushReviewRequest(nextShot,
        RefereeAuthorityOperation::FinalDeclare,"lifecycle.next.f",
        "lifecycle.next.f.key",finalPayload);
    expect(nextShot.submitPushReviewFinal(final).ok,
        "declaration final after shot completion remains pre-shot authority");
    CueImpactInput zero;
    expect(!nextShot.applyCueImpact(zero).ok&&
        nextShot.pushReviewState().phase==PushReviewPhase::Active,
        "zero-impulse rejection does not consume the next-shot declaration");
    CueImpactInput positive;
    positive.cueBallIndex=0;
    positive.cueSpeedCmS=120.0;
    positive.cueMassKg=nextShot.physicsProfile().cue.effectiveMassKg;
    positive.direction={{1.0,0.0,0.0}};
    positive.chalkState="CHALKED";
    expect(nextShot.applyCueImpact(positive).ok&&
        nextShot.pushReviewState().phase==PushReviewPhase::Consumed&&
        nextShot.pushReviewState().consumedShotIdentity==
            nextShot.activeCanonicalCueTipStrokeIdentity(),
        "positive launch binds the post-completion declaration to the next shot");
}

void testCommittedDeclarationsReplayThroughRealShot()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;
    GameRuntime runtime;
    expect(runtime.replaceStateForScenario(runtime.state(),
            interactiveChinesePoolPhysicsProfile()).ok&&
        runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "declaration replay fixture selects shipping rules and physics");
    runtime.configurePushReviewAuthority(&auth);
    runtime.configureDoubleHitCloseCaseAuthority(&auth);
    runtime.configureFrozenBallDeclarationAuthority(&auth);
    runtime.configureFrozenCushionDeclarationAuthority(&auth);
    replay::Recorder recorder;
    const std::string executable="declaration-replay-v3";
    const std::string content=sha256Hex("declaration-replay-content");
    expect(recorder.begin(runtime,executable,content,73).ok,
        "declaration replay fixture starts before signed declarations");

    const std::string pushProposalPayload=
        "{\"cue_ball_id\":0,\"push_review_required\":true,"
        "\"relation\":\"POTENTIAL_OBVIOUS_PUSH_REQUIRES_COMPLETE_MEASUREMENTS_AND_REFEREE_REVIEW\"}";
    auto pushProposal=pushReviewRequest(runtime,
        RefereeAuthorityOperation::Propose,"replay.push.p",
        "replay.push.p.key",pushProposalPayload);
    const std::string pushFinalPayload=
        "{\"cue_ball_id\":0,\"proposal_decision_id\":\"runtime.push.1\","
        "\"push_review_required\":true,\"relation\":"
        "\"POTENTIAL_OBVIOUS_PUSH_REQUIRES_COMPLETE_MEASUREMENTS_AND_REFEREE_REVIEW\"}";
    const auto pushProposed=runtime.submitPushReviewProposal(pushProposal);
    auto pushFinal=pushReviewRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"replay.push.f",
        "replay.push.f.key",pushFinalPayload);
    const auto pushCommitted=runtime.submitPushReviewFinal(pushFinal);
    if(!pushProposed.ok||!pushCommitted.ok)
        std::cerr<<"declaration replay push errors "<<pushProposed.errorCode
                 <<' '<<pushCommitted.errorCode<<'\n';
    expect(pushProposed.ok&&pushCommitted.ok,
        "fixture commits signed push review");

    const std::string closeProposalPayload=
        "{\"close_case_attested\":true,\"cue_ball_id\":0,"
        "\"object_ball_id\":1,\"relation\":"
        "\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";
    auto closeProposal=closeCaseRequest(runtime,
        RefereeAuthorityOperation::Propose,"replay.close.p",
        "replay.close.p.key",closeProposalPayload);
    const std::string closeFinalPayload=
        "{\"close_case_attested\":true,\"cue_ball_id\":0,"
        "\"object_ball_id\":1,\"proposal_decision_id\":"
        "\"runtime.close.1\",\"relation\":"
        "\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";
    const auto closeProposed=runtime.submitDoubleHitCloseCaseProposal(closeProposal);
    auto closeFinal=closeCaseRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"replay.close.f",
        "replay.close.f.key",closeFinalPayload);
    const auto closeCommitted=runtime.submitDoubleHitCloseCaseFinal(closeFinal);
    if(!closeProposed.ok||!closeCommitted.ok)
        std::cerr<<"declaration replay close errors "<<closeProposed.errorCode
                 <<' '<<closeCommitted.errorCode<<'\n';
    expect(closeProposed.ok&&closeCommitted.ok,
        "fixture commits signed double-hit close case");

    const std::string frozenProposalPayload=
        "{\"cue_ball_id\":0,\"frozen_attested\":true,"
        "\"object_ball_id\":1,\"relation\":"
        "\"CUE_BALL_FROZEN_TO_LEGAL_OBJECT_BALL\"}";
    auto frozenProposal=frozenDeclarationRequest(runtime,
        RefereeAuthorityOperation::Propose,"replay.frozen.p",
        "replay.frozen.p.key",frozenProposalPayload);
    const std::string frozenFinalPayload=
        "{\"cue_ball_id\":0,\"frozen_attested\":true,"
        "\"object_ball_id\":1,\"proposal_decision_id\":"
        "\"runtime.frozen.1\",\"relation\":"
        "\"CUE_BALL_FROZEN_TO_LEGAL_OBJECT_BALL\"}";
    const auto frozenProposed=runtime.submitFrozenBallDeclarationProposal(frozenProposal);
    auto frozenFinal=frozenDeclarationRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"replay.frozen.f",
        "replay.frozen.f.key",frozenFinalPayload);
    const auto frozenCommitted=runtime.submitFrozenBallDeclarationFinal(frozenFinal);
    if(!frozenProposed.ok||!frozenCommitted.ok)
        std::cerr<<"declaration replay frozen errors "<<frozenProposed.errorCode
                 <<' '<<frozenCommitted.errorCode<<'\n';
    expect(frozenProposed.ok&&frozenCommitted.ok,
        "fixture commits signed frozen cue-object declaration");

    const std::string cushionProposalPayload=
        "{\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\","
        "\"straight_cushion_id\":\"HEAD\"}";
    auto cushionProposal=frozenCushionRequest(runtime,
        RefereeAuthorityOperation::Propose,"replay.cushion.p",
        "replay.cushion.p.key",cushionProposalPayload);
    const std::string cushionFinalPayload=
        "{\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"proposal_decision_id\":\"runtime.cushion.1\","
        "\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\","
        "\"straight_cushion_id\":\"HEAD\"}";
    const auto cushionProposed=runtime.submitFrozenCushionDeclarationProposal(cushionProposal);
    auto cushionFinal=frozenCushionRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"replay.cushion.f",
        "replay.cushion.f.key",cushionFinalPayload);
    const auto cushionCommitted=runtime.submitFrozenCushionDeclarationFinal(cushionFinal);
    if(!cushionProposed.ok||!cushionCommitted.ok)
        std::cerr<<"declaration replay cushion errors "<<cushionProposed.errorCode
                 <<' '<<cushionCommitted.errorCode<<'\n';
    expect(cushionProposed.ok&&cushionCommitted.ok&&
        recorder.captureApplied(runtime).ok&&
        recorder.document().rulesReceipts.size()==4,
        "fixture records four ordered secret-free declaration receipts");

    GameAction yaw;yaw.type=ActionType::SetAimYaw;yaw.first=kPi/2.0f;
    GameAction power;power.type=ActionType::SetShotPower;power.first=65.0f;
    GameAction shoot;shoot.type=ActionType::Shoot;
    expect(runtime.dispatch(yaw).ok&&runtime.dispatch(power).ok&&
        runtime.dispatch(shoot).ok&&recorder.captureApplied(runtime).ok,
        "real shipping shot consumes all four declarations");
    for(int tick=0;tick<5000&&runtime.state().ballsMoving;++tick)
        expect(runtime.step(1).ok,
            "declared shipping shot advances through real physics");
    expect(!runtime.state().ballsMoving&&runtime.hasLastShotFacts()&&
        runtime.hasLastAdjudication()&&
        runtime.pushReviewState().phase==PushReviewPhase::Consumed&&
        runtime.doubleHitCloseCaseState().phase==
            DoubleHitCloseCasePhase::Consumed&&
        runtime.frozenBallDeclarationState().phase==
            FrozenBallDeclarationPhase::Consumed&&
        runtime.frozenCushionDeclarationState().phase==
            FrozenCushionDeclarationPhase::Consumed,
        "real shot binds declarations to canonical facts and adjudication");
    expect(runtime.step(1).ok&&recorder.finish(runtime,1).ok,
        "declared shot saves with final rules-effect digest");
    const auto recordedDocument=recorder.document();
    const std::string declarationWire=json::stringify(
        replay::serialize(recordedDocument));
    expect(recordedDocument.rulesReceiptSchema==
            replay::kReplaySingleRackDeclarationReceiptSchema&&
        declarationWire.find("credential")==std::string::npos&&
        declarationWire.find("authentication")==std::string::npos&&
        declarationWire.find("nonce")==std::string::npos&&
        declarationWire.find("proof")==std::string::npos&&
        declarationWire.find("secret")==std::string::npos,
        "declaration receipt schema contains no live credential material");
    const auto path=std::filesystem::temp_directory_path()/
        "billiardgl-declaration-replay-v3.json";
    std::error_code ignored;
    std::filesystem::remove(path,ignored);
    replay::Document document;
    expect(recorder.saveAtomic(path.string()).ok,
        "declaration replay saves atomically");
    std::ifstream input(path,std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const auto decoded=json::parse(bytes);
    expect(decoded.ok&&replay::parse(decoded.value,document).ok,
        "declaration replay reloads from its strict v3 receipt wire");
    std::filesystem::remove(path,ignored);

    replay::Player player;
    const auto loaded=player.load(document,runtime.physicsProfile(),executable,
        content);
    const auto sought=loaded.ok?player.seekShot(0):loaded;
    if(!loaded.ok||!sought.ok)
        std::cerr<<"declaration replay load/seek errors "<<loaded.errorCode
                 <<' '<<sought.errorCode<<'\n';
    expect(loaded.ok&&sought.ok&&
        player.runtime().pushReviewState().phase==PushReviewPhase::Active&&
        player.runtime().doubleHitCloseCaseState().phase==
            DoubleHitCloseCasePhase::Active&&
        player.runtime().frozenBallDeclarationState().phase==
            FrozenBallDeclarationPhase::Active&&
        player.runtime().frozenCushionDeclarationState().phase==
            FrozenCushionDeclarationPhase::Active,
        "seek restores all declarations before the bound shot");
    bool replayAdvanced=sought.ok;
    for(int tick=0;replayAdvanced&&!player.finished()&&tick<6000;++tick){
        const auto step=player.stepTick();
        if(!step.ok){
            std::cerr<<"declaration replay step error "<<step.errorCode
                     <<" at "<<player.runtime().tick()<<'\n';
            replayAdvanced=false;
        }
    }
    expect(replayAdvanced&&player.finished(),
        "player replays declared real shot and its adjudication");
    if(player.finished()&&!player.finalHashMatches())
        std::cerr<<"declaration final hashes state="
                 <<player.finalStateSha256()<<'/'
                 <<document.finalSummary.stateSha256<<" authority="
                 <<replay::replayAuthorityStateSha256(player.runtime().state())
                 <<'/'<<document.finalSummary.authorityStateSha256<<" effect="
                 <<player.runtime().rulesReplayEffectStateHash() << '/'
                 <<document.finalSummary.rulesEffectStateSha256
                 <<" push="<<static_cast<int>(player.runtime().pushReviewState().phase)
                 <<'/'<<static_cast<int>(runtime.pushReviewState().phase)
                 <<":"<<player.runtime().pushReviewState().consumedShotIdentity
                 <<'/'<<runtime.pushReviewState().consumedShotIdentity
                 <<" double="<<static_cast<int>(player.runtime().doubleHitCloseCaseState().phase)
                 <<'/'<<static_cast<int>(runtime.doubleHitCloseCaseState().phase)
                 <<":"<<player.runtime().doubleHitCloseCaseState().consumedShotIdentity
                 <<'/'<<runtime.doubleHitCloseCaseState().consumedShotIdentity
                 <<" frozen="<<static_cast<int>(player.runtime().frozenBallDeclarationState().phase)
                 <<'/'<<static_cast<int>(runtime.frozenBallDeclarationState().phase)
                 <<":"<<player.runtime().frozenBallDeclarationState().consumedShotIdentity
                 <<'/'<<runtime.frozenBallDeclarationState().consumedShotIdentity
                 <<" cushion="<<static_cast<int>(player.runtime().frozenCushionDeclarationState().phase)
                 <<'/'<<static_cast<int>(runtime.frozenCushionDeclarationState().phase)
                 <<":"<<player.runtime().frozenCushionDeclarationState().consumedShotIdentity
                 <<'/'<<runtime.frozenCushionDeclarationState().consumedShotIdentity
                 <<'\n';
    expect(player.finalHashMatches()&&player.runtime().hasLastShotFacts()&&
        player.runtime().hasLastAdjudication(),
        "player verifies declared shot facts, adjudication, and final digest");

    auto missing=document;
    missing.rulesReceipts.erase(missing.rulesReceipts.begin()+1);
    replay::Player missingPlayer;
    expect(missingPlayer.load(missing,runtime.physicsProfile(),executable,
            content).ok,
        "missing declaration remains structurally parseable for runtime-chain test");
    expect(!missingPlayer.stepTick().ok,
        "missing declaration breaks the ordered effect chain");
    auto tampered=document;
    tampered.rulesReceipts[1].event.declarationObjectBallId=2;
    tampered.rulesReceipts[1].receiptSha256=
        replay::rulesReceiptSha256(tampered.rulesReceipts[1]);
    replay::Player tamperedPlayer;
    expect(tamperedPlayer.load(tampered,runtime.physicsProfile(),executable,
            content).ok&&!tamperedPlayer.stepTick().ok,
        "re-signed declaration fact mutation fails committed effect verification");
    auto duplicate=document;
    duplicate.rulesReceipts.insert(duplicate.rulesReceipts.begin()+1,
        duplicate.rulesReceipts.front());
    expect(!replay::validate(duplicate,runtime.physicsProfile(),executable,
            content).ok,
        "duplicate declaration receipt is rejected");
    auto reordered=document;
    std::swap(reordered.rulesReceipts[0],reordered.rulesReceipts[1]);
    expect(!replay::validate(reordered,runtime.physicsProfile(),executable,
            content).ok,
        "wrong declaration order is rejected");
    auto late=document;
    for(auto& receipt:late.rulesReceipts){
        receipt.afterSemanticActionOrdinal=late.actions.size();
        receipt.receiptSha256=replay::rulesReceiptSha256(receipt);
    }
    replay::Player latePlayer;
    expect(latePlayer.load(late,runtime.physicsProfile(),executable,
            content).ok&&!latePlayer.stepTick().ok,
        "same-tick declarations moved after Shoot fail their parent-effect chain");
}

void testNonphysicalGeneralFoulRuntimeLifecycle()
{
    using namespace billiardgl;using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;GameRuntime runtime;
    runtime.configureNonphysicalGeneralFoulAuthority(&auth);
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "nonphysical foul rules enabled");
    const std::string shot="runtime.nonphysical_general_foul.shot."+
        std::to_string(runtime.rulesAuthorityRevision());
    const auto kind=NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot;
    auto proposal=nonphysicalFoulRequest(runtime,RefereeAuthorityOperation::Propose,
        "np.p","np.p.key",
        canonicalNonphysicalGeneralFoulProposal(kind,0,shot));
    expect(runtime.submitNonphysicalGeneralFoulProposal(proposal).ok&&
        !runtime.matchState().ballInHand&&
        !runtime.nonphysicalGeneralFoulState().committed,
        "proposal creates neither foul nor entitlement");
    expect(!runtime.nonphysicalFoulExplanation(),
        "proposal does not publish a committed explanation");
    auto final=nonphysicalFoulRequest(runtime,RefereeAuthorityOperation::FinalDeclare,
        "np.f","np.f.key",
        canonicalNonphysicalGeneralFoulFinal(kind,0,"runtime.nonphysical.1",shot));
    expect(runtime.submitNonphysicalGeneralFoulFinal(final).ok&&
        runtime.nonphysicalGeneralFoulState().committed&&
        runtime.hasLastAdjudication()&&runtime.lastAdjudication().foul&&
        runtime.matchState().currentPlayer==1&&runtime.matchState().ballInHand&&
        runtime.ballInHandPlacementScope()==BallInHandPlacementScope::FullTable&&
        runtime.ballInHandPlacementOffer()&&
        runtime.ballInHandPlacementOffer()->actorPlayerId==1,
        "signed final atomically grants opponent Anywhere BIH");
    expect(runtime.nonphysicalFoulExplanation() &&
        runtime.nonphysicalFoulExplanation()->kind == kind &&
        runtime.lastAdjudication().reasons.empty(),
        "validated kind is available without physical foul pollution");
    const auto revision=runtime.rulesAuthorityRevision();
    const auto decision=runtime.ballInHandPlacementOffer()->decisionId;
    expect(runtime.submitNonphysicalGeneralFoulFinal(final).ok&&
        runtime.rulesAuthorityRevision()==revision&&
        runtime.ballInHandPlacementOffer()->decisionId==decision,
        "exact retry has zero entitlement delta");
    runtime.reset();expect(!runtime.submitNonphysicalGeneralFoulFinal(final).ok&&
        runtime.nonphysicalGeneralFoulState().phase==
            NonphysicalGeneralFoulPhase::Inactive,
        "reset invalidates prior authority request");
    expect(!runtime.nonphysicalFoulExplanation(),
        "reset clears derived explanation");
}

void testMobileDeviceRepeatRackTerminalRuntimeLifecycle()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    StalemateTestAuthenticator auth;
    StalemateTestGuard guard;
    GameRuntime runtime;
    runtime.configureNonphysicalGeneralFoulAuthority(&auth,&guard);
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "mobile repeat runtime enables Chinese rules");

    const std::string firstShot="runtime.nonphysical_general_foul.shot."+
        std::to_string(runtime.rulesAuthorityRevision());
    auto firstProposal=nonphysicalFoulRequest(runtime,
        RefereeAuthorityOperation::Propose,"mobile.first.p",
        "mobile.first.p.key",canonicalNonphysicalGeneralFoulProposal(
            NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
            0,firstShot),"runtime.mobile.first");
    expect(runtime.submitNonphysicalGeneralFoulProposal(firstProposal).ok,
        "first signed mobile disturbance opens a proposal");
    auto firstFinal=nonphysicalFoulRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"mobile.first.f",
        "mobile.first.f.key",canonicalNonphysicalGeneralFoulFinal(
            NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
            0,"runtime.mobile.first",firstShot),"runtime.mobile.first");
    const std::string beforeFirstHash=runtime.rulesAuthorityStateHash();
    const auto first=runtime.submitNonphysicalGeneralFoulFinal(firstFinal);
    expect(first.ok&&first.newlyCommitted&&!first.idempotentReplay&&
        first.outcome==NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand&&
        runtime.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::GeneralFoulCommitted&&
        runtime.matchState().currentPlayer==1&&
        runtime.matchState().ballInHand&&runtime.ballInHandPlacementPending()&&
        !runtime.rulesRackTerminalState().active&&
        runtime.rulesAuthorityStateHash()!=beforeFirstHash,
        "first mobile violation commits history and grants ordinary BIH only");

    const std::string repeatShot="runtime.nonphysical_general_foul.shot."+
        std::to_string(runtime.rulesAuthorityRevision());
    auto repeatProposal=nonphysicalFoulRequest(runtime,
        RefereeAuthorityOperation::Propose,"mobile.repeat.p",
        "mobile.repeat.p.key",canonicalNonphysicalGeneralFoulProposal(
            NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0,repeatShot),"runtime.mobile.repeat");
    expect(runtime.submitNonphysicalGeneralFoulProposal(repeatProposal).ok,
        "same-player signed repeat may supersede the unexercised first-foul BIH");
    auto repeatFinal=nonphysicalFoulRequest(runtime,
        RefereeAuthorityOperation::FinalDeclare,"mobile.repeat.f",
        "mobile.repeat.f.key",canonicalNonphysicalGeneralFoulFinal(
            NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0,"runtime.mobile.repeat",repeatShot),"runtime.mobile.repeat");
    const std::string firstPlacementDecision=
        runtime.ballInHandPlacementOffer()->decisionId;
    const std::uint64_t beforeDeniedRepeatRevision=
        runtime.rulesAuthorityRevision();
    const std::string beforeDeniedRepeatHash=runtime.rulesAuthorityStateHash();
    const std::size_t beforeDeniedRepeatEvents=runtime.events().size();
    guard.allow=false;
    const auto deniedRepeat=
        runtime.submitNonphysicalGeneralFoulFinal(repeatFinal);
    expect(!deniedRepeat.ok&&runtime.matchState().ballInHand&&
        runtime.ballInHandPlacementPending()&&
        runtime.ballInHandPlacementOffer()->decisionId==firstPlacementDecision&&
        runtime.rulesAuthorityRevision()==beforeDeniedRepeatRevision&&
        runtime.rulesAuthorityStateHash()==beforeDeniedRepeatHash&&
        runtime.events().size()==beforeDeniedRepeatEvents&&
        runtime.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::GeneralFoulCommitted&&
        !runtime.rulesRackTerminalState().active,
        "repeat commit denial preserves the first BIH and every rack authority ledger");
    expect(runtime.nonphysicalFoulExplanation() &&
        runtime.nonphysicalFoulExplanation()->kind ==
            NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
        "rejected repeat cannot replace a committed explanation");
    guard.allow=true;
    const auto repeat=runtime.submitNonphysicalGeneralFoulFinal(repeatFinal);
    const auto terminal=runtime.rulesRackTerminalState();
    const std::uint64_t terminalRevision=runtime.rulesAuthorityRevision();
    const std::string terminalHash=runtime.rulesAuthorityStateHash();
    expect(repeat.ok&&repeat.newlyCommitted&&
        repeat.outcome==NonphysicalGeneralFoulOutcome::LossOfRack&&
        terminal.active&&
        terminal.reason==RulesRackTerminalReason::MobileDeviceRepeatViolation&&
        terminal.losingPlayer==0&&terminal.winningPlayer==1&&
        terminal.authorityDecisionId=="runtime.mobile.repeat"&&
        terminal.committedAuthorityRevision==terminalRevision&&
        runtime.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::RackLossCommitted&&
        !runtime.matchState().ballInHand&&!runtime.ballInHandPlacementPending()&&
        runtime.matchState().phase!=MatchPhase::Complete&&
        runtime.matchState().winner==-1&&!runtime.state().gameOver,
        "repeat commits a rack-only terminal without inventing match loss");

    CueImpactInput positive;
    positive.cueBallIndex=0;positive.cueSpeedCmS=120.0;
    positive.cueMassKg=runtime.physicsProfile().cue.effectiveMassKg;
    positive.direction={{1.0,0.0,0.0}};positive.chalkState="CHALKED";
    GameAction shoot;shoot.type=ActionType::Shoot;
    const auto directBlocked=runtime.applyCueImpact(positive);
    const auto semanticBlocked=runtime.dispatch(shoot);
    expect(!directBlocked.ok&&
        std::string(directBlocked.errorCode)=="rules_rack_complete"&&
        !semanticBlocked.ok&&
        std::string(semanticBlocked.errorCode)=="rules_rack_complete",
        "rack terminal blocks direct and semantic cue launches");

    const auto repeatRetry=runtime.submitNonphysicalGeneralFoulFinal(repeatFinal);
    const auto oldFirstRetry=runtime.submitNonphysicalGeneralFoulFinal(firstFinal);
    expect(repeatRetry.ok&&repeatRetry.idempotentReplay&&
        oldFirstRetry.ok&&oldFirstRetry.idempotentReplay&&
        runtime.rulesAuthorityRevision()==terminalRevision&&
        runtime.rulesAuthorityStateHash()==terminalHash&&
        runtime.rulesRackTerminalState().authorityDecisionId==
            "runtime.mobile.repeat",
        "current and older exact finals replay after later transactions with zero effects");
    expect(!runtime.nonphysicalFoulExplanation(),
        "old exact first-foul retry cannot resurrect stale explanation");

    auto postTerminal=repeatProposal;
    postTerminal.decisionId="runtime.mobile.after";
    postTerminal.idempotencyKey="mobile.after.key";
    postTerminal.authenticationNonce="mobile.after";
    signStalemateRequest(postTerminal);
    expect(!runtime.submitNonphysicalGeneralFoulProposal(postTerminal).ok,
        "new authority work is rejected after rack terminal");

    StalemateTestGuard replacementGuard;
    replacementGuard.allow=false;
    const std::string terminalRack=
        runtime.nonphysicalGeneralFoulState().rackIdentity;
    runtime.configureNonphysicalGeneralFoulAuthority(&auth,&replacementGuard);
    expect(runtime.rulesRackTerminalState().active&&
        runtime.nonphysicalGeneralFoulState().rackIdentity==terminalRack&&
        runtime.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::RackLossCommitted&&
        runtime.rulesAuthorityRevision()==terminalRevision&&
        runtime.rulesAuthorityStateHash()==terminalHash,
        "same-runtime authority reconfiguration cannot erase rack punishment");
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok&&
        !runtime.rulesRackTerminalState().active&&
        runtime.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::None&&
        runtime.nonphysicalGeneralFoulState().rackIdentity!=terminalRack&&
        !runtime.submitNonphysicalGeneralFoulFinal(repeatFinal).ok,
        "ruleset reset is the existing explicit new-rules-session boundary");

    runtime.reset();
    expect(!runtime.rulesRackTerminalState().active&&
        runtime.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::None&&
        !runtime.submitNonphysicalGeneralFoulFinal(repeatFinal).ok,
        "runtime reset starts a fresh rack and invalidates stale terminal receipts");

    GameRuntime stale;
    stale.configureNonphysicalGeneralFoulAuthority(&auth);
    stale.setRulesMode(RulesMode::ChineseEightBallV1);
    const std::string staleShot="runtime.nonphysical_general_foul.shot."+
        std::to_string(stale.rulesAuthorityRevision());
    auto staleProposal=nonphysicalFoulRequest(stale,
        RefereeAuthorityOperation::Propose,"mobile.stale.p",
        "mobile.stale.p.key",canonicalNonphysicalGeneralFoulProposal(
            NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
            0,staleShot),"runtime.mobile.stale");
    expect(stale.submitNonphysicalGeneralFoulProposal(staleProposal).ok,
        "stale fixture opens proposal");
    auto staleFinal=nonphysicalFoulRequest(stale,
        RefereeAuthorityOperation::FinalDeclare,"mobile.stale.f",
        "mobile.stale.f.key",canonicalNonphysicalGeneralFoulFinal(
            NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
            0,"runtime.mobile.stale",staleShot),"runtime.mobile.stale");
    stale.mutableState().balls[2].position.x+=1.0f;
    expect(!stale.submitNonphysicalGeneralFoulFinal(staleFinal).ok&&
        stale.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::None&&
        !stale.rulesRackTerminalState().active,
        "parent hash drift rejects final without history or terminal mutation");

    GameRuntime denied;
    guard.allow=true;
    denied.configureNonphysicalGeneralFoulAuthority(&auth,&guard);
    denied.setRulesMode(RulesMode::ChineseEightBallV1);
    const std::string deniedShot="runtime.nonphysical_general_foul.shot."+
        std::to_string(denied.rulesAuthorityRevision());
    auto deniedProposal=nonphysicalFoulRequest(denied,
        RefereeAuthorityOperation::Propose,"mobile.denied.p",
        "mobile.denied.p.key",canonicalNonphysicalGeneralFoulProposal(
            NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
            0,deniedShot),"runtime.mobile.denied");
    expect(denied.submitNonphysicalGeneralFoulProposal(deniedProposal).ok,
        "commit-guard fixture opens proposal");
    auto deniedFinal=nonphysicalFoulRequest(denied,
        RefereeAuthorityOperation::FinalDeclare,"mobile.denied.f",
        "mobile.denied.f.key",canonicalNonphysicalGeneralFoulFinal(
            NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
            0,"runtime.mobile.denied",deniedShot),"runtime.mobile.denied");
    const std::string deniedHash=denied.rulesAuthorityStateHash();
    const std::uint64_t deniedRevision=denied.rulesAuthorityRevision();
    guard.allow=false;
    expect(!denied.submitNonphysicalGeneralFoulFinal(deniedFinal).ok&&
        denied.rulesAuthorityRevision()==deniedRevision&&
        denied.rulesAuthorityStateHash()==deniedHash&&
        denied.nonphysicalGeneralFoulState().mobileDeviceViolations[0]==
            MobileDeviceViolationState::None&&
        !denied.rulesRackTerminalState().active,
        "commit guard denial is atomic across runtime history and terminal state");
    guard.allow=true;
}

void testCanonicalCueTipStrokeObservationRuntimeAtomicity()
{
    using namespace billiardgl;
    GameRuntime runtime;
    CueImpactInput rejected;
    expect(!runtime.applyCueImpact(rejected).ok&&
        !runtime.canonicalCueTipStrokeObservationActive()&&
        !runtime.hasCanonicalCueTipStrokeObservation(),
        "rejected zero launch creates no stroke observation");
    CueImpactInput positive;
    positive.cueBallIndex=0;positive.cueSpeedCmS=120.0;
    positive.cueMassKg=runtime.physicsProfile().cue.effectiveMassKg;
    positive.direction={{1.0,0.0,0.0}};positive.chalkState="CHALKED";
    expect(runtime.applyCueImpact(positive).ok&&
        runtime.canonicalCueTipStrokeObservationActive()&&
        runtime.d5BObservationActive()&&
        !runtime.hasCanonicalCueTipStrokeObservation(),
        "accepted launch stages complete-source intervals without publishing");
    const std::string identity=runtime.activeCanonicalCueTipStrokeIdentity();
    expect(!runtime.commitAuthoritativeCueMotionObservationEnd(
            identity,runtime.tick()+1).ok&&
        runtime.canonicalCueTipStrokeObservationActive()&&
        !runtime.hasCanonicalCueTipStrokeObservation(),
        "non-authoritative future end tick fails with zero observation delta");
    expect(runtime.commitAuthoritativeCueMotionObservationEnd(
            identity,runtime.tick()).ok&&
        runtime.d5BObservationActive()&&
        !runtime.hasCanonicalCueTipStrokeObservation()&&
        runtime.canonicalCueTipStrokeObservation().fullStrokeComplete,
        "exact authoritative motion-end stages complete observation until physics success");
    const std::string digest=runtime.canonicalCueTipStrokeObservation().digest;
    expect(runtime.commitAuthoritativeCueMotionObservationEnd(
            identity,runtime.tick()).ok&&
        runtime.canonicalCueTipStrokeObservation().digest==digest,
        "runtime exact retry returns immutable observation");
    expect(runtime.step(1).ok&&runtime.hasCanonicalCueTipStrokeObservation(),
        "successful physics tick atomically publishes complete observation");
    runtime.reset();
    expect(!runtime.canonicalCueTipStrokeObservationActive()&&
        !runtime.d5BObservationActive()&&
        !runtime.hasCanonicalCueObjectOrderObservation()&&
        !runtime.hasCanonicalCueTipStrokeObservation()&&
        !runtime.commitAuthoritativeCueMotionObservationEnd(
            identity,runtime.tick()).ok,
        "reset invalidates active and committed stroke identities");

    GameRuntime failureRuntime;
    GameState failingState=failureRuntime.state();
    failingState.balls[1].pocketed=false;
    failingState.balls[2].pocketed=false;
    failingState.balls[1].position={10.0f,0.0f,0.0f};
    failingState.balls[2].position=failingState.balls[1].position;
    auto failingProfile=failureRuntime.physicsProfile();
    failingProfile.id="d5_a_failed_physics_rollback";
    failingProfile.solver.maximumPenetrationCm=0.01f;
    expect(failureRuntime.replaceStateForScenario(
            failingState,failingProfile).ok&&
        failureRuntime.applyCueImpact(positive).ok,
        "failed-step fixture accepts launch before solver failure");
    const std::string failingIdentity=
        failureRuntime.activeCanonicalCueTipStrokeIdentity();
    expect(failureRuntime.commitAuthoritativeCueMotionObservationEnd(
            failingIdentity,failureRuntime.tick()).ok&&
        !failureRuntime.step(1).ok&&
        !failureRuntime.d5BObservationActive()&&
        !failureRuntime.hasCanonicalCueObjectOrderObservation()&&
        !failureRuntime.canonicalCueTipStrokeObservationActive()&&
        !failureRuntime.hasCanonicalCueTipStrokeObservation(),
        "failed physics step rolls back completed but unpublished observation");
}

void testDoubleHitClassificationAdjudicationFailureAtomicity()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    GameRuntime runtime;
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "D5-C atomic fixture enables rules");
    ShotFacts facts;
    facts.breakShot = true;
    facts.firstContactBall = 1;
    facts.distinctObjectBallsToRail = 4;
    facts.hasCanonicalDoubleHitClassification = true;
    facts.canonicalDoubleHitClassification.shotIdentity = "shot.d5c.bad";
    facts.canonicalDoubleHitClassification.result =
        CanonicalDoubleHitResult::DoubleHitFoul;
    facts.canonicalDoubleHitClassification.canonicalTipIntervalCount = 2;
    facts.canonicalDoubleHitClassification.cueTipObservationDigest =
        "tip.sha";
    facts.canonicalDoubleHitClassification.cueObjectObservationDigest =
        "order.sha";
    validateCanonicalDoubleHitClassification(
        facts.canonicalDoubleHitClassification);
    facts.canonicalDoubleHitClassification.digest = "tampered";
    const auto revision = runtime.rulesAuthorityRevision();
    const auto state = runtime.matchState();
    expect(!runtime.adjudicateRulesShot(facts).ok &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.matchState().phase == state.phase &&
        runtime.matchState().currentPlayer == state.currentPlayer &&
        !runtime.hasLastShotFacts() && !runtime.hasLastAdjudication(),
        "failed classifier adjudication restores preclassification runtime state");
    runtime.reset();
    expect(!runtime.hasLastShotFacts() && !runtime.hasLastAdjudication(),
        "reset leaves no unpublished classifier result");
}

}  // namespace

int main()
{
    {
        billiardgl::GameRuntime runtime;
        expect(runtime.replaceStateForScenario(runtime.state(),
                billiardgl::interactiveChinesePoolPhysicsProfile()).ok,
            "weak-break fixture selects the shipping physical profile");
        expect(runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1).ok,
            "weak-break error fixture enables rules");
        billiardgl::CueImpactInput impact;
        impact.cueBallIndex = 0;
        impact.cueSpeedCmS = 10.0;
        impact.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
        impact.direction = {{0.0, 0.0, 1.0}};
        impact.chalkState = "CHALKED";
        expect(runtime.applyCueImpact(impact).ok, "weak break starts a real shot");
        for (int tick = 0; tick < 2000 && runtime.state().ballsMoving; ++tick) {
            const auto result = runtime.step(1);
            expect(result.ok, "actual non-miscue weak break completes adjudication");
        }
        expect(!runtime.state().ballsMoving && runtime.rulesShotCompletionError().empty() &&
                runtime.hasLastAdjudication() && runtime.lastAdjudication().foul,
            "real cue contact distinguishes an underpowered break from an unknown miscue");
        runtime.reset();
        expect(runtime.rulesShotCompletionError().empty(),
            "explicit reset clears the retained completion failure");
    }
    {
        using namespace billiardgl;
        using namespace billiardgl::rules;
        GameRuntime runtime;
        expect(runtime.replaceStateForScenario(runtime.state(),
                interactiveChinesePoolPhysicsProfile()).ok &&
            runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
            "native illegal-break offer fixture selects shipping rules and physics");
        replay::Recorder recorder;
        const std::string replayExecutable="native-illegal-break-replay-v2";
        const std::string replayContent=sha256Hex(
            "native-illegal-break-replay-content");
        expect(recorder.begin(runtime,replayExecutable,replayContent,65).ok,
            "native illegal-break fixture begins a single-rack recorder");
        auto zeroBall = runtime.state().balls[2];
        zeroBall.velocity.x = -0.0f;
        zeroBall.angularVelocity.z = -0.0f;
        zeroBall.speed = -0.0f;
        expect(runtime.setBall(2, zeroBall).ok,
            "authority signed-zero fixture installs negative IEEE zeros");
        const std::string negativeZeroHash = runtime.rulesAuthorityStateHash();
        zeroBall.velocity.x = 0.0f;
        zeroBall.angularVelocity.z = 0.0f;
        zeroBall.speed = 0.0f;
        expect(runtime.setBall(2, zeroBall).ok &&
                runtime.rulesAuthorityStateHash() == negativeZeroHash,
            "rules authority canonicalizes signed zero without removing physical fields");
        GameAction yaw;
        yaw.type = ActionType::SetAimYaw;
        yaw.first = kPi / 2.0f;
        GameAction power;
        power.type = ActionType::SetShotPower;
        power.first = 65.0f;
        GameAction shoot;
        shoot.type = ActionType::Shoot;
        expect(runtime.dispatch(yaw).ok && runtime.dispatch(power).ok &&
                runtime.dispatch(shoot).ok,
            "native illegal-break offer fixture applies the product 65-power semantic shot");
        for (int tick = 0; tick < 5000 && runtime.state().ballsMoving; ++tick)
            expect(runtime.step(1).ok,
                "native illegal-break offer fixture settles through public physics");
        const auto offer = runtime.breakFoulDecisionOffer();
        const std::string settledHash = runtime.rulesAuthorityStateHash();
        expect(!runtime.state().ballsMoving && offer &&
                offer->branch == BreakFoulBranch::IllegalBreak &&
                runtime.step(1).ok &&
                runtime.rulesAuthorityStateHash() == settledHash,
            "native illegal-break offer remains physically and authoritatively stable while idle");
        if (offer) {
            BreakFoulDecisionRequest request;
            request.rulesetId = kBreakFoulRulesetId;
            request.capabilityId = kBreakFoulCapabilityId;
            request.flowId = kBreakFoulFlowId;
            request.decisionId = offer->decisionId;
            request.expectedAuthorityStateRevision =
                offer->authorityStateRevision;
            request.idempotencyKey = "native-illegal-break-accept";
            request.actorPlayerId = offer->actorPlayerId;
            request.actionId = kBreakFoulAcceptTableActionId;
            request.payloadCanonical = "{}";
            expect(runtime.submitBreakFoulDecision(request).ok,
                "native illegal-break offer seals its final transaction state and accepts once");
            expect(recorder.captureApplied(runtime).ok&&runtime.step(1).ok&&
                recorder.finish(runtime,1).ok&&
                recorder.document().rulesReceipts.size()==1&&
                recorder.document().rulesReceipts.front().event.type==
                    RuntimeRulesReplayEventType::BreakFoulChoiceCommit,
                "native 06C accept-table commit records one typed choice receipt");
            replay::Player player;
            expect(player.load(recorder.document(),runtime.physicsProfile(),
                    replayExecutable,replayContent).ok,
                "native 06C receipt loads through the comprehensive player");
            while(!player.finished())
                expect(player.stepTick().ok,
                    "native 06C replay reproduces the physical break and choice");
            expect(player.finalHashMatches()&&
                !player.runtime().breakFoulDecisionPending(),
                "native 06C replay verifies the final choice/effect digest");
            auto tampered=recorder.document();
            tampered.rulesReceipts.front().event.actionId=
                kBreakFoulBallInHandActionId;
            tampered.rulesReceipts.front().receiptSha256=
                replay::rulesReceiptSha256(
                    tampered.rulesReceipts.front());
            replay::Player tamperedPlayer;
            expect(replay::validate(tampered,runtime.physicsProfile(),
                    replayExecutable,replayContent).ok&&
                tamperedPlayer.load(tampered,runtime.physicsProfile(),
                    replayExecutable,replayContent).ok,
                "self-consistent 06C semantic tamper reaches runtime verification");
            bool tamperRejected=false;
            while(!tamperedPlayer.finished()){
                const auto step=tamperedPlayer.stepTick();
                if(!step.ok){tamperRejected=true;break;}
            }
            expect(tamperRejected,
                "06C receipt cannot replay an action absent from the frozen offer");
        }
    }
    testOfficialRackContractUnit();
    testRackFoundationTransactions();
    testEightOnBreakAuthorityTransaction();
    testLegacyDefaultEquivalence();
    testWholeGameStateTransitions();
    testBreakAndEightBallFailures();
    testSelectedBreakThresholdRuntimeAuthority();
    testBreakFoulOpponentChoiceAuthority();
    testUnintentionalBreakMiscueNativeAuthority();
    testRulesCoreAOutcomes();
    testRulesCoreBSimultaneousContacts();
    testGroupChoiceAuthorityTransaction();
    testFullTableBallInHandAuthorityTransaction();
    testBehindHeadStringBallInHandAuthorityTransaction();
    testBehindHeadStringAllBehindRespotAuthority();
    testSelectedBreakHeadStringReleaseAuthority();
    testAuthoritativeCollectorLifecycle();
    testAuthoritativeCueScratchPocketLifecycle();
    testAutomationRulesRoundTrip();
    testAutomationRulesCoreARoundTrip();
    testAutomationRulesCoreBRoundTrip();
    testAutomationGroupChoiceAuthorityRoundTrip();
    testAutomationFullTableBallInHandAuthorityRoundTrip();
    testAutomationBreakFoulAuthorityRoundTrip();
    testAutomationEightOnBreakAuthorityRoundTrip();
    testRefereeAuthorityFoundationRuntimeIsolation();
    testStalemateRuntimeCoreERerackTransaction();
    testGroupConfusionRuntimeCoreERerackTransaction();
    testFrozenDeclarationRuntimeLifecycle();
    testFrozenCushionDeclarationRuntimeLifecycle();
    testDoubleHitCloseCaseRuntimeLifecycle();
    testPushReviewRuntimeLifecycle();
    testDeclarationReplayLifecycleBoundaries();
    testCommittedDeclarationsReplayThroughRealShot();
    testNonphysicalGeneralFoulRuntimeLifecycle();
    testMobileDeviceRepeatRackTerminalRuntimeLifecycle();
    testCanonicalCueTipStrokeObservationRuntimeAtomicity();
    testDoubleHitClassificationAdjudicationFailureAtomicity();
    if (failures != 0) {
        std::cerr << failures << " rules runtime integration checks failed\n";
        return 1;
    }
    std::cout << "Rules runtime integration checks passed\n";
    return 0;
}
