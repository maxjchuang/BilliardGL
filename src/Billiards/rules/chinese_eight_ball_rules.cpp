#include "rules/chinese_eight_ball_rules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <tuple>

namespace billiardgl::rules {
namespace {

bool isSolid(int ball) noexcept { return ball >= 1 && ball <= 7; }
bool isStripe(int ball) noexcept { return ball >= 9 && ball <= 15; }
bool isObjectBall(int ball) noexcept { return ball >= 1 && ball <= 15; }

bool contains(const std::vector<int>& balls, int ball)
{
    return std::find(balls.begin(), balls.end(), ball) != balls.end();
}

template <typename Predicate>
bool anyFirstContact(const ShotFacts& facts, Predicate predicate)
{
    if (!facts.firstContactBalls.empty()) {
        return std::any_of(facts.firstContactBalls.begin(),
                           facts.firstContactBalls.end(), predicate);
    }
    return isObjectBall(facts.firstContactBall) &&
        predicate(facts.firstContactBall);
}

bool pocketedGroup(const ShotFacts& facts, BallGroup group)
{
    return std::any_of(facts.pocketedBallsInOrder.begin(),
                       facts.pocketedBallsInOrder.end(),
                       [group](int ball) {
                           return group == BallGroup::Solids ? isSolid(ball)
                                                            : isStripe(ball);
                       });
}

bool groupClearedBefore(const ShotFacts& facts, BallGroup group) noexcept
{
    return group == BallGroup::Solids ? facts.solidsRemainingBefore == 0
                                      : facts.stripesRemainingBefore == 0;
}

void addReason(AdjudicationResult& result, FoulReason reason)
{
    if (std::find(result.reasons.begin(), result.reasons.end(), reason) !=
        result.reasons.end()) return;
    result.reasons.push_back(reason);
    result.textKeys.emplace_back(foulReasonTextKey(reason));
    result.foul = true;
}

void addViolation(AdjudicationResult& result, FoulReason reason)
{
    if (std::find(result.reasons.begin(), result.reasons.end(), reason) !=
        result.reasons.end()) return;
    result.reasons.push_back(reason);
    result.textKeys.emplace_back(foulReasonTextKey(reason));
}

BallGroup groupForBall(int ball) noexcept
{
    if (isSolid(ball)) return BallGroup::Solids;
    if (isStripe(ball)) return BallGroup::Stripes;
    return BallGroup::Open;
}

bool legalFirstContactForState(const MatchState& state, int ball,
                               bool cleared) noexcept
{
    if (!isObjectBall(ball)) return false;
    const BallGroup group = state.groups[state.currentPlayer];
    if (group == BallGroup::Open) return ball != 8;
    return cleared ? ball == 8 : groupForBall(ball) == group;
}

BallGroup opposite(BallGroup group) noexcept
{
    if (group == BallGroup::Solids) return BallGroup::Stripes;
    if (group == BallGroup::Stripes) return BallGroup::Solids;
    return BallGroup::Open;
}

void finishIllegalEightBall(const MatchState& state,
                            AdjudicationResult& result)
{
    addReason(result, FoulReason::IllegalEightBall);
    result.gameOver = true;
    result.winner = 1 - state.currentPlayer;
    result.nextState.phase = MatchPhase::Complete;
    result.nextState.winner = result.winner;
    result.continueTurn = false;
    result.nextPlayer = result.winner;
    result.nextState.currentPlayer = result.nextPlayer;
}

auto breakContactKey(const BreakObjectCushionContact& contact) noexcept
{
    return std::make_tuple(
        contact.objectBallId, contact.physicsTick,
        static_cast<int>(contact.boundaryKind),
        static_cast<int>(contact.provenance), contact.solverEventId,
        contact.solverIslandId, contact.timeOfImpactSeconds,
        contact.normalImpulseNs, contact.solverResidualCmS,
        contact.solverProjectionCm, contact.velocityImpulseApplied,
        contact.positionCorrected);
}

bool validBreakContact(const BreakObjectCushionContact& contact) noexcept
{
    if (!isObjectBall(contact.objectBallId) || contact.physicsTick == 0 ||
        !std::isfinite(contact.normalImpulseNs) ||
        !std::isfinite(contact.solverResidualCmS) ||
        !std::isfinite(contact.solverProjectionCm) ||
        !std::isfinite(contact.timeOfImpactSeconds) ||
        contact.normalImpulseNs < 0.0 || contact.solverResidualCmS < 0.0 ||
        contact.solverProjectionCm < 0.0 ||
        contact.timeOfImpactSeconds < 0.0)
        return false;
    if (contact.boundaryKind != BreakCushionBoundaryKind::StraightCushion &&
        contact.boundaryKind != BreakCushionBoundaryKind::PocketJaw)
        return false;
    if (contact.provenance ==
            BreakCushionContactProvenance::ContinuousSolver)
        return contact.solverEventId >= 0 && contact.solverIslandId >= 0;
    if (contact.provenance ==
            BreakCushionContactProvenance::DiscretePhysics)
        return contact.solverEventId == -1 && contact.solverIslandId == -1;
    return false;
}

bool validCueBallPocketCapture(
    const CueBallPocketCaptureEvidence& evidence) noexcept
{
    return evidence.pocketId >= 0 && evidence.pocketId < 6 &&
        evidence.captureSequence > 0 && evidence.physicsTick > 0 &&
        evidence.physicsContactOrdinal >= 0 &&
        std::isfinite(evidence.timeOfImpactSeconds) &&
        evidence.timeOfImpactSeconds >= 0.0;
}

bool validHeadStringCrossingEvidence(const ShotFacts& facts) noexcept
{
    if (facts.headStringCrossingEvidenceOrigin ==
            HeadStringCrossingEvidenceOrigin::DirectOrSynthetic) {
        return !facts.behindHeadStringFirstContactRestrictionActive &&
            !facts.headStringCrossingEvidenceComplete &&
            !facts.cueBallCrossedHeadStringBeforeFirstContact &&
            facts.cueCenterPlaneCrossingEvents.empty() &&
            facts.firstCueObjectContactEvidence.empty();
    }
    if (facts.headStringCrossingEvidenceOrigin !=
            HeadStringCrossingEvidenceOrigin::
                AuthoritativeShotFactCollector ||
        !facts.behindHeadStringFirstContactRestrictionActive ||
        !std::isfinite(facts.headStringWorldZCm))
        return false;

    const CueCenterPlaneCrossingEvidence* previousCrossing = nullptr;
    for (std::size_t index = 0;
         index < facts.cueCenterPlaneCrossingEvents.size(); ++index) {
        const auto& evidence = facts.cueCenterPlaneCrossingEvents[index];
        if (evidence.observationId.empty() || evidence.physicsTick == 0 ||
            evidence.segmentOrdinal < 0 ||
            evidence.canonicalEventOrdinal != static_cast<int>(index) ||
            !std::isfinite(evidence.wholeTickTimeOfImpactSeconds) ||
            evidence.wholeTickTimeOfImpactSeconds < 0.0 ||
            !std::isfinite(evidence.signedDistanceBeforeCm) ||
            !std::isfinite(evidence.signedDistanceAfterCm) ||
            !(evidence.signedDistanceBeforeCm < 0.0) ||
            !(evidence.signedDistanceAfterCm > 0.0) ||
            !std::isfinite(evidence.axisVelocityAtCrossingCmS) ||
            !(evidence.axisVelocityAtCrossingCmS > 0.0))
            return false;
        if (previousCrossing &&
            !(std::tie(previousCrossing->physicsTick,
                       previousCrossing->wholeTickTimeOfImpactSeconds,
                       previousCrossing->segmentOrdinal,
                       previousCrossing->observationId) <
              std::tie(evidence.physicsTick,
                       evidence.wholeTickTimeOfImpactSeconds,
                       evidence.segmentOrdinal, evidence.observationId)))
            return false;
        previousCrossing = &evidence;
    }

    int previousBall = -1;
    const FirstCueObjectContactEvidence* firstContact = nullptr;
    std::vector<int> contactBalls;
    for (const auto& evidence : facts.firstCueObjectContactEvidence) {
        if (!isObjectBall(evidence.objectBallId) ||
            evidence.objectBallId <= previousBall ||
            evidence.physicsTick == 0 ||
            !std::isfinite(evidence.wholeTickTimeOfImpactSeconds) ||
            evidence.wholeTickTimeOfImpactSeconds < 0.0 ||
            evidence.physicsContactOrdinal < 0 ||
            !std::isfinite(evidence.objectBallCenterWorldXCm) ||
            !std::isfinite(evidence.objectBallCenterWorldZCm))
            return false;
        if (firstContact &&
            (evidence.physicsTick != firstContact->physicsTick ||
             evidence.wholeTickTimeOfImpactSeconds !=
                firstContact->wholeTickTimeOfImpactSeconds ||
             evidence.solverEventId != firstContact->solverEventId))
            return false;
        if (!firstContact) firstContact = &evidence;
        previousBall = evidence.objectBallId;
        contactBalls.push_back(evidence.objectBallId);
    }
    if (contactBalls != facts.firstContactBalls) return false;

    bool orderedBefore = false;
    if (facts.headStringCrossingEvidenceComplete &&
        !facts.cueCenterPlaneCrossingEvents.empty() && firstContact) {
        const auto& crossing = facts.cueCenterPlaneCrossingEvents.front();
        orderedBefore = std::tie(crossing.physicsTick,
                            crossing.wholeTickTimeOfImpactSeconds) <
            std::tie(firstContact->physicsTick,
                     firstContact->wholeTickTimeOfImpactSeconds);
    }
    if (facts.cueBallCrossedHeadStringBeforeFirstContact != orderedBefore)
        return false;
    if (facts.cueBallCrossedHeadStringBeforeFirstContact &&
        !facts.headStringCrossingEvidenceComplete)
        return false;
    return true;
}

int qualifyingBreakObjectCushionCount(const ShotFacts& facts)
{
    // Direct/synthetic scalar facts remain a legacy CPU-only fixture surface.
    // Product authority is separately forbidden from opening a WPA-II-06-C
    // offer from that origin. Authoritative play consumes only the canonical
    // selected-policy evidence and never asks the product layer to reinterpret
    // physics contacts.
    if (facts.breakEvidenceOrigin !=
            BreakEvidenceOrigin::AuthoritativeShotFactCollector ||
        !facts.breakEvidenceComplete)
        return facts.distinctObjectBallsToRail;

    std::set<int> qualifyingObjects;
    for (const BreakObjectCushionContact& contact :
         facts.breakObjectCushionContacts) {
        if (contact.boundaryKind ==
                BreakCushionBoundaryKind::StraightCushion &&
            contact.velocityImpulseApplied) {
            qualifyingObjects.insert(contact.objectBallId);
        }
    }
    return static_cast<int>(qualifyingObjects.size());
}

}  // namespace

bool isLegalFirstContactBall(const MatchState& state, int ball,
                             bool groupCleared) noexcept
{
    return legalFirstContactForState(state, ball, groupCleared);
}

ShotFactsValidationError validateShotFacts(const ShotFacts& facts) noexcept
{
    if (facts.shooter < 0 || facts.shooter > 1 ||
        (facts.firstContactBall != -1 &&
            !isObjectBall(facts.firstContactBall)) ||
        facts.distinctObjectBallsToRail < 0 ||
        facts.distinctObjectBallsToRail > 15 ||
        facts.solidsRemainingBefore < 0 || facts.solidsRemainingBefore > 7 ||
        facts.stripesRemainingBefore < 0 || facts.stripesRemainingBefore > 7 ||
        facts.calledPocket < -1 || facts.calledPocket >= 6 ||
        facts.eightBallPocket < -1 || facts.eightBallPocket >= 6) {
        return ShotFactsValidationError::OutOfRange;
    }

    int previousFirstContact = -1;
    for (int ball : facts.firstContactBalls) {
        if (!isObjectBall(ball))
            return ShotFactsValidationError::OutOfRange;
        if (ball == previousFirstContact)
            return ShotFactsValidationError::DuplicateBall;
        if (ball < previousFirstContact)
            return ShotFactsValidationError::NonCanonicalOrder;
        previousFirstContact = ball;
    }
    if (!facts.firstContactBalls.empty() && facts.firstContactBall >= 1 &&
        facts.firstContactBall != facts.firstContactBalls.front())
        return ShotFactsValidationError::ConflictingBallState;

    if (facts.breakEvidenceOrigin ==
            BreakEvidenceOrigin::DirectOrSynthetic) {
        if (facts.breakEvidenceComplete ||
            !facts.breakObjectCushionContacts.empty())
            return ShotFactsValidationError::InvalidBreakEvidence;
    } else if (facts.breakEvidenceOrigin ==
               BreakEvidenceOrigin::AuthoritativeShotFactCollector) {
        if (!facts.breakShot) {
            if (facts.breakEvidenceComplete ||
                !facts.breakObjectCushionContacts.empty())
                return ShotFactsValidationError::InvalidBreakEvidence;
        } else {
            if (!facts.breakEvidenceComplete)
                return ShotFactsValidationError::InvalidBreakEvidence;
            std::set<int> evidencedObjects;
            const BreakObjectCushionContact* previous = nullptr;
            for (std::size_t index = 0;
                 index < facts.breakObjectCushionContacts.size(); ++index) {
                const auto& contact = facts.breakObjectCushionContacts[index];
                if (!validBreakContact(contact) ||
                    contact.canonicalContactOrdinal !=
                        static_cast<int>(index))
                    return ShotFactsValidationError::InvalidBreakEvidence;
                if (previous && !(breakContactKey(*previous) <
                                  breakContactKey(contact)))
                    return ShotFactsValidationError::InvalidBreakEvidence;
                evidencedObjects.insert(contact.objectBallId);
                previous = &contact;
            }
            if (static_cast<int>(evidencedObjects.size()) !=
                facts.distinctObjectBallsToRail)
                return ShotFactsValidationError::InvalidBreakEvidence;
        }
    } else {
        return ShotFactsValidationError::InvalidBreakEvidence;
    }

    std::array<bool, 16> pocketed{};
    for (int ball : facts.pocketedBallsInOrder) {
        if (ball < 0 || ball > 15)
            return ShotFactsValidationError::OutOfRange;
        if (pocketed[static_cast<std::size_t>(ball)])
            return ShotFactsValidationError::DuplicateBall;
        pocketed[static_cast<std::size_t>(ball)] = true;
    }

    std::array<bool, 16> offTable{};
    for (int ball : facts.objectBallsOffTable) {
        if (!isObjectBall(ball))
            return ShotFactsValidationError::OutOfRange;
        if (offTable[static_cast<std::size_t>(ball)])
            return ShotFactsValidationError::DuplicateBall;
        offTable[static_cast<std::size_t>(ball)] = true;
        if (pocketed[static_cast<std::size_t>(ball)])
            return ShotFactsValidationError::ConflictingBallState;
    }
    if (facts.cueBallOffTable &&
        (facts.cueBallPocketed || pocketed[0]))
        return ShotFactsValidationError::ConflictingBallState;
    if (facts.eightBallPocket >= 0 && !pocketed[8])
        return ShotFactsValidationError::ConflictingBallState;

    if (facts.cueBallScratchEvidenceOrigin ==
            CueBallScratchEvidenceOrigin::DirectOrSynthetic) {
        if (facts.cueBallPocketed ||
            facts.cueBallScratchEvidenceComplete ||
            !facts.cueBallPocketCaptures.empty())
            return ShotFactsValidationError::
                InvalidCueBallScratchEvidence;
    } else if (facts.cueBallScratchEvidenceOrigin ==
               CueBallScratchEvidenceOrigin::
                   AuthoritativeShotFactCollector) {
        if (!facts.cueBallScratchEvidenceComplete ||
            facts.cueBallPocketCaptures.size() > 1 ||
            facts.cueBallPocketed !=
                (facts.cueBallPocketCaptures.size() == 1))
            return ShotFactsValidationError::
                InvalidCueBallScratchEvidence;
        for (std::size_t index = 0;
             index < facts.cueBallPocketCaptures.size(); ++index) {
            const auto& evidence = facts.cueBallPocketCaptures[index];
            if (!validCueBallPocketCapture(evidence) ||
                evidence.canonicalEventOrdinal !=
                    static_cast<int>(index))
                return ShotFactsValidationError::
                    InvalidCueBallScratchEvidence;
        }
    } else {
        return ShotFactsValidationError::InvalidCueBallScratchEvidence;
    }
    if (!validHeadStringCrossingEvidence(facts))
        return ShotFactsValidationError::InvalidHeadStringCrossingEvidence;
    if (facts.hasCanonicalDoubleHitClassification) {
        CanonicalDoubleHitClassification classification =
            facts.canonicalDoubleHitClassification;
        const std::string suppliedDigest = classification.digest;
        const auto validation =
            validateCanonicalDoubleHitClassification(classification);
        if (!validation.valid || !validation.complete ||
            classification.digest != suppliedDigest)
            return ShotFactsValidationError::InvalidDoubleHitClassification;
    } else if (facts.canonicalDoubleHitClassification.complete ||
               !facts.canonicalDoubleHitClassification.digest.empty()) {
        return ShotFactsValidationError::InvalidDoubleHitClassification;
    }
    if (facts.hasCanonicalPushClassification) {
        CanonicalPushClassification classification =
            facts.canonicalPushClassification;
        const std::string suppliedDigest = classification.digest;
        const auto validation =
            validateCanonicalPushClassification(classification);
        if (!validation.valid || !validation.complete ||
            classification.digest != suppliedDigest)
            return ShotFactsValidationError::InvalidPushClassification;
    } else if (facts.canonicalPushClassification.complete ||
               !facts.canonicalPushClassification.digest.empty()) {
        return ShotFactsValidationError::InvalidPushClassification;
    }
    if (facts.hasCanonicalFrozenCushionClassification) {
        CanonicalFrozenCushionClassification classification =
            facts.canonicalFrozenCushionClassification;
        const std::string suppliedDigest = classification.digest;
        const auto validation =
            validateCanonicalFrozenCushionClassification(classification);
        if (!validation.valid || !validation.complete ||
            classification.digest != suppliedDigest)
            return ShotFactsValidationError::InvalidFrozenCushionClassification;
    } else if (facts.canonicalFrozenCushionClassification.complete ||
               !facts.canonicalFrozenCushionClassification.digest.empty()) {
        return ShotFactsValidationError::InvalidFrozenCushionClassification;
    }
    return ShotFactsValidationError::None;
}

const char* shotFactsValidationErrorCode(
    ShotFactsValidationError error) noexcept
{
    switch (error) {
    case ShotFactsValidationError::None: return "none";
    case ShotFactsValidationError::OutOfRange:
        return "rules_facts_out_of_range";
    case ShotFactsValidationError::DuplicateBall:
        return "rules_facts_duplicate";
    case ShotFactsValidationError::NonCanonicalOrder:
        return "rules_facts_not_canonical";
    case ShotFactsValidationError::ConflictingBallState:
        return "rules_facts_conflict";
    case ShotFactsValidationError::InvalidBreakEvidence:
        return "rules_facts_break_evidence_invalid";
    case ShotFactsValidationError::InvalidCueBallScratchEvidence:
        return "rules_facts_cue_scratch_evidence_invalid";
    case ShotFactsValidationError::InvalidHeadStringCrossingEvidence:
        return "rules_facts_head_string_crossing_evidence_invalid";
    case ShotFactsValidationError::InvalidDoubleHitClassification:
        return "rules_facts_double_hit_classification_invalid";
    case ShotFactsValidationError::InvalidPushClassification:
        return "rules_facts_push_classification_invalid";
    case ShotFactsValidationError::InvalidFrozenCushionClassification:
        return "rules_facts_frozen_cushion_classification_invalid";
    }
    return "rules_facts_invalid";
}

ChineseEightBallRules::ChineseEightBallRules(ChineseEightBallRuleset ruleset)
    : ruleset_(std::move(ruleset))
{
    if (ruleset_.id.empty()) throw std::invalid_argument("ruleset id is required");
    if (ruleset_.schemaVersion != 1)
        throw std::invalid_argument("unsupported Chinese eight-ball ruleset schema");
    if (ruleset_.minimumBreakObjectBallsToRail < 0 ||
        ruleset_.minimumBreakObjectBallsToRail > 15)
        throw std::invalid_argument("invalid minimum break rail count");
}

AdjudicationResult ChineseEightBallRules::adjudicate(
    const MatchState& state, const ShotFacts& facts) const
{
    AdjudicationResult result;
    result.nextState = state;
    result.nextPlayer = state.currentPlayer;
    result.winner = state.winner;
    if (state.phase == MatchPhase::Complete || state.winner >= 0) return result;
    if (state.groupChoicePending) return result;
    if (validateShotFacts(facts) != ShotFactsValidationError::None)
        return result;
    if (ruleset_.requireCalledEightPocket &&
        contains(facts.pocketedBallsInOrder, 8) &&
        facts.eightBallPocket < 0)
        return result;
    if (facts.shooter != state.currentPlayer || facts.shooter < 0 ||
        facts.shooter > 1) return result;

    result.accepted = true;
    result.nextState.ballInHand = false;
    const bool isBreak = state.phase == MatchPhase::Break;
    // WPA Heyball treats every numbered ball, including the 8-ball, as an
    // object ball for the break and post-contact pocket-or-rail conditions.
    const bool pocketedAnyObject = std::any_of(
        facts.pocketedBallsInOrder.begin(), facts.pocketedBallsInOrder.end(),
        [](int ball) { return isObjectBall(ball); });
    const bool pocketedNonEightObject = std::any_of(
        facts.pocketedBallsInOrder.begin(), facts.pocketedBallsInOrder.end(),
        [](int ball) { return ball != 8 && isObjectBall(ball); });

    const bool hasObjectContact = anyFirstContact(
        facts, [](int) { return true; });
    if (!hasObjectContact)
        addReason(result, FoulReason::NoObjectBallContact);
    if (facts.behindHeadStringFirstContactRestrictionActive &&
        hasObjectContact) {
        const bool directContactAllowed = std::any_of(
            facts.firstCueObjectContactEvidence.begin(),
            facts.firstCueObjectContactEvidence.end(),
            [&](const FirstCueObjectContactEvidence& evidence) {
                return evidence.objectBallCenterWorldZCm >=
                    facts.headStringWorldZCm;
            });
        if (!directContactAllowed &&
            (!facts.headStringCrossingEvidenceComplete ||
             !facts.cueBallCrossedHeadStringBeforeFirstContact))
            addReason(result, FoulReason::WrongFirstContact);
    }
    if (facts.cueBallPocketed) addReason(result, FoulReason::CueBallScratch);
    if (facts.hasCanonicalDoubleHitClassification &&
        facts.canonicalDoubleHitClassification.result ==
            CanonicalDoubleHitResult::DoubleHitFoul)
        addReason(result, FoulReason::DoubleHit);
    if (facts.hasCanonicalPushClassification &&
        facts.canonicalPushClassification.result ==
            CanonicalPushResult::PushFoul)
        addReason(result, FoulReason::Push);
    if (facts.hasCanonicalFrozenCushionClassification &&
        facts.canonicalFrozenCushionClassification.result ==
            CanonicalFrozenCushionResult::FrozenCushionFoul)
        addReason(result, FoulReason::FrozenCushionObligation);
    if (facts.cueBallOffTable) addReason(result, FoulReason::CueBallOffTable);
    if (!facts.objectBallsOffTable.empty())
        addReason(result, FoulReason::ObjectBallOffTable);

    if (isBreak) {
        const bool legalByPocket =
            ruleset_.pocketedObjectBallMakesLegalBreak && pocketedAnyObject;
        const bool legalByRails = qualifyingBreakObjectCushionCount(facts) >=
                                  ruleset_.minimumBreakObjectBallsToRail;
        if (!legalByPocket && !legalByRails) {
            addViolation(result, FoulReason::IllegalBreak);
            result.rerackAllowed = true;
            result.decisionOptions = {"accept_table", "rerack_opponent_breaks",
                                      "rerack_original_breaker"};
        }
    } else if (hasObjectContact) {
        const BallGroup shooterGroup = state.groups[state.currentPlayer];
        if (shooterGroup == BallGroup::Open) {
            const bool hasLegalOpenTableContact = anyFirstContact(
                facts, [&](int ball) {
                    return legalFirstContactForState(state, ball, false);
                });
            if (!hasLegalOpenTableContact)
                addReason(result, FoulReason::WrongFirstContact);
        } else if (shooterGroup != BallGroup::Open) {
            const bool cleared = groupClearedBefore(facts, shooterGroup);
            const bool legalFirst = anyFirstContact(facts,
                [&](int ball) {
                    return legalFirstContactForState(state, ball, cleared);
                });
            if (!legalFirst) addReason(result, FoulReason::WrongFirstContact);
        }
        if (!pocketedAnyObject && !facts.anyRailAfterFirstContact)
            addReason(result, FoulReason::NoRailAfterContact);
    }

    const bool eightPocketed = contains(facts.pocketedBallsInOrder, 8);
    const bool eightOffTable = contains(facts.objectBallsOffTable, 8);
    if (eightOffTable) {
        // The unresolved general off-table policy remains unchanged for balls
        // 1-7 and 9-15. The 8-ball's explicit loss condition is independent.
        finishIllegalEightBall(state, result);
        return result;
    }
    if (eightPocketed) {
        if (isBreak) {
            result.eightBallMustBeSpotted = true;
            result.rerackAllowed = true;
            if (result.foul) {
                result.ballInHand = true;
                result.ballInHandBehindHeadString = true;
                result.nextState.ballInHand = true;
                result.nextPlayer = 1 - state.currentPlayer;
                result.decisionOptions = {"spot_eight_ball_in_hand_head_string",
                                          "rerack_opponent_breaks",
                                          "rerack_original_breaker"};
            } else {
                result.continueTurn = true;
                result.nextPlayer = state.currentPlayer;
                result.decisionOptions = {"spot_eight_continue", "rerack_original_breaker"};
            }
            result.nextState.phase = MatchPhase::OpenTable;
            result.nextState.currentPlayer = result.nextPlayer;
            return result;
        }
        const BallGroup shooterGroup = state.groups[state.currentPlayer];
        const bool calledPocketMatches = !ruleset_.requireCalledEightPocket ||
            (facts.calledPocket >= 0 && facts.calledPocket == facts.eightBallPocket);
        const bool legalEight = !result.foul && !isBreak &&
            shooterGroup != BallGroup::Open &&
            groupClearedBefore(facts, shooterGroup) &&
            anyFirstContact(facts, [](int ball) { return ball == 8; }) &&
            calledPocketMatches;
        if (legalEight) {
            result.gameOver = true;
            result.winner = state.currentPlayer;
            result.nextState.phase = MatchPhase::Complete;
            result.nextState.winner = result.winner;
            result.continueTurn = false;
            result.nextPlayer = state.currentPlayer;
            result.nextState.currentPlayer = result.nextPlayer;
            result.textKeys.emplace_back("rules.chinese8.win.legal_eight");
            return result;
        }
        // Remaining counts describe the table before this stroke. Pocketing
        // the last group ball and the 8-ball together is therefore still an
        // early 8-ball loss.
        finishIllegalEightBall(state, result);
        return result;
    }

    if (!isBreak && !result.foul &&
        state.groups[state.currentPlayer] == BallGroup::Open) {
        const bool contactedSolids = anyFirstContact(
            facts, [](int ball) { return isSolid(ball); });
        const bool contactedStripes = anyFirstContact(
            facts, [](int ball) { return isStripe(ball); });
        const bool pocketedSolids = pocketedGroup(facts, BallGroup::Solids);
        const bool pocketedStripes = pocketedGroup(facts, BallGroup::Stripes);
        if (contactedSolids && contactedStripes &&
            pocketedSolids && pocketedStripes) {
            result.groupChoiceRequired = true;
            result.nextState.groupChoicePending = true;
            result.decisionOptions = {
                "rules.group_choice.choose_solids",
                "rules.group_choice.choose_stripes"};
        } else {
            BallGroup assignment = BallGroup::Open;
            if (contactedSolids && pocketedSolids &&
                !(contactedStripes && pocketedStripes))
                assignment = BallGroup::Solids;
            else if (contactedStripes && pocketedStripes &&
                !(contactedSolids && pocketedSolids))
                assignment = BallGroup::Stripes;
            if (assignment != BallGroup::Open) {
                result.nextState.groups[state.currentPlayer] = assignment;
                result.nextState.groups[1 - state.currentPlayer] =
                    opposite(assignment);
                result.nextState.phase = MatchPhase::GroupsAssigned;
                result.textKeys.emplace_back(
                    assignment == BallGroup::Solids ?
                        "rules.chinese8.group.solids" :
                        "rules.chinese8.group.stripes");
            }
        }
    }

    if (result.nextState.phase == MatchPhase::Break)
        result.nextState.phase = MatchPhase::OpenTable;

    const BallGroup effectiveGroup = result.nextState.groups[state.currentPlayer];
    result.continueTurn = !result.foul &&
        (isBreak ? pocketedAnyObject
                 : (effectiveGroup == BallGroup::Open
                        ? pocketedNonEightObject
                        : pocketedGroup(facts, effectiveGroup)));
    result.ballInHand = result.foul;
    result.ballInHandBehindHeadString = result.foul && isBreak;
    result.nextState.ballInHand = result.ballInHand;
    result.nextPlayer = result.continueTurn ? state.currentPlayer
                                            : 1 - state.currentPlayer;
    result.nextState.currentPlayer = result.nextPlayer;
    return result;
}

const char* foulReasonCode(FoulReason reason) noexcept
{
    switch (reason) {
    case FoulReason::None: return "none";
    case FoulReason::IllegalBreak: return "illegal_break";
    case FoulReason::NoObjectBallContact: return "no_object_ball_contact";
    case FoulReason::WrongFirstContact: return "wrong_first_contact";
    case FoulReason::NoRailAfterContact: return "no_rail_after_contact";
    case FoulReason::CueBallScratch: return "cue_ball_scratch";
    case FoulReason::CueBallOffTable: return "cue_ball_off_table";
    case FoulReason::ObjectBallOffTable: return "object_ball_off_table";
    case FoulReason::BreakCueBallPastHeadString:
        return "break_cue_ball_past_head_string";
    case FoulReason::DoubleHit: return "double_hit";
    case FoulReason::Push: return "push";
    case FoulReason::FrozenCushionObligation:
        return "frozen_cushion_obligation";
    case FoulReason::IllegalEightBall: return "illegal_eight_ball";
    }
    return "unknown";
}

const char* foulReasonTextKey(FoulReason reason) noexcept
{
    switch (reason) {
    case FoulReason::None: return "rules.chinese8.none";
    case FoulReason::IllegalBreak: return "rules.chinese8.foul.illegal_break";
    case FoulReason::NoObjectBallContact: return "rules.chinese8.foul.no_object_contact";
    case FoulReason::WrongFirstContact: return "rules.chinese8.foul.wrong_first_contact";
    case FoulReason::NoRailAfterContact: return "rules.chinese8.foul.no_rail_after_contact";
    case FoulReason::CueBallScratch: return "rules.chinese8.foul.cue_ball_scratch";
    case FoulReason::CueBallOffTable: return "rules.chinese8.foul.cue_ball_off_table";
    case FoulReason::ObjectBallOffTable: return "rules.chinese8.foul.object_ball_off_table";
    case FoulReason::BreakCueBallPastHeadString:
        return "rules.chinese8.foul.break_head_string_more_than_half";
    case FoulReason::DoubleHit: return "rules.chinese8.foul.double_hit";
    case FoulReason::Push: return "rules.chinese8.foul.push";
    case FoulReason::FrozenCushionObligation:
        return "rules.chinese8.foul.frozen_cushion_obligation";
    case FoulReason::IllegalEightBall: return "rules.chinese8.loss.illegal_eight";
    }
    return "rules.chinese8.unknown";
}

}  // namespace billiardgl::rules
