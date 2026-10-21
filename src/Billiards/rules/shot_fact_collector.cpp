#include "rules/shot_fact_collector.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace billiardgl::rules {
namespace {

auto evidenceKey(const BreakObjectCushionContact& contact) noexcept
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

auto cueCaptureKey(const CueBallPocketCaptureEvidence& evidence) noexcept
{
    return std::make_tuple(evidence.captureSequence, evidence.physicsTick,
        evidence.pocketId, evidence.timeOfImpactSeconds);
}

auto crossingKey(const CueCenterPlaneCrossingEvidence& evidence) noexcept
{
    return std::tie(evidence.physicsTick,
        evidence.wholeTickTimeOfImpactSeconds, evidence.segmentOrdinal,
        evidence.observationId);
}

std::string tickDigest(const PhysicsStepTelemetry& telemetry)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10)
           << static_cast<int>(telemetry.stepStatus) << '|'
           << telemetry.wholeTickDurationSeconds;
    for (const PhysicsContactRecord& contact : telemetry.contacts) {
        stream << "|c:" << static_cast<int>(contact.kind) << ':'
               << contact.firstBall << ':' << contact.secondBall << ':'
               << contact.solverEventId << ':' << contact.solverIslandId << ':'
               << contact.wholeTickTimeOfImpactSeconds << ':'
               << contact.firstPositionAtContactCm.x << ':'
               << contact.firstPositionAtContactCm.z << ':'
               << contact.secondPositionAtContactCm.x << ':'
               << contact.secondPositionAtContactCm.z;
    }
    for (const PhysicsPlaneCrossingRecord& crossing :
         telemetry.planeCrossings) {
        stream << "|x:" << crossing.observationId << ':'
               << crossing.ballIndex << ':' << static_cast<int>(crossing.axis)
               << ':' << static_cast<int>(crossing.direction) << ':'
               << crossing.wholeTickTimeOfImpactSeconds << ':'
               << crossing.segmentOrdinal << ':'
               << crossing.signedDistanceBeforeCm << ':'
               << crossing.signedDistanceAfterCm << ':'
               << crossing.axisVelocityAtCrossingCmS << ':'
               << (crossing.complete ? 1 : 0);
    }
    for (const PhysicsPlaneObservationAttestation& attestation :
         telemetry.planeAttestations) {
        stream << "|a:" << attestation.observationId << ':'
               << attestation.ballIndex << ':'
               << attestation.wholeTickDurationSeconds << ':'
               << attestation.coveredSegmentCount << ':'
               << attestation.crossingRecordCount << ':'
               << (attestation.complete ? 1 : 0);
    }
    return stream.str();
}

bool validCrossingRecord(const PhysicsPlaneCrossingRecord& crossing,
    const BehindHeadStringFirstContactContext& context,
    const PhysicsStepTelemetry& telemetry) noexcept
{
    return crossing.observationId == context.observationId &&
        crossing.ballIndex == 0 && crossing.axis == PhysicsPlaneAxis::Z &&
        crossing.direction ==
            PhysicsPlaneCrossingDirection::NegativeToPositive &&
        crossing.origin != nullptr &&
        std::string(crossing.origin) ==
            "AUTHORITATIVE_CONTINUOUS_SURFACE_INTEGRATOR" &&
        crossing.complete && crossing.segmentOrdinal >= 0 &&
        std::isfinite(crossing.wholeTickTimeOfImpactSeconds) &&
        crossing.wholeTickTimeOfImpactSeconds >= 0.0 &&
        crossing.wholeTickTimeOfImpactSeconds <=
            telemetry.wholeTickDurationSeconds &&
        std::isfinite(crossing.signedDistanceBeforeCm) &&
        std::isfinite(crossing.signedDistanceAfterCm) &&
        crossing.signedDistanceBeforeCm < 0.0 &&
        crossing.signedDistanceAfterCm > 0.0 &&
        std::isfinite(crossing.axisVelocityAtCrossingCmS) &&
        crossing.axisVelocityAtCrossingCmS > 0.0;
}

bool validCueCaptureContact(const PhysicsContactRecord& contact) noexcept
{
    return contact.kind == PhysicsContactKind::Pocket &&
        contact.firstBall == 0 && contact.secondBall == -1 &&
        contact.pocketBoundaryEvent == PocketBoundaryEventKind::Capture &&
        contact.pocketPhaseBefore != PocketInteractionPhase::Captured &&
        contact.pocketPhaseAfter == PocketInteractionPhase::Captured &&
        contact.pocketId >= 0 && contact.pocketId < 6 &&
        contact.pocketCaptureSequence > 0 &&
        std::isfinite(contact.timeOfImpactSeconds) &&
        contact.timeOfImpactSeconds >= 0.0;
}

bool isJaw(PocketBoundaryEventKind kind) noexcept
{
    return kind == PocketBoundaryEventKind::LeftJaw ||
        kind == PocketBoundaryEventKind::RightJaw;
}

bool matchedDiscreteJaw(const PhysicsContactRecord& rail,
                        const PhysicsStepTelemetry& telemetry) noexcept
{
    const int ball = rail.firstBall >= 0 ? rail.firstBall : rail.secondBall;
    return std::any_of(telemetry.contacts.begin(), telemetry.contacts.end(),
        [&](const PhysicsContactRecord& contact) {
            const int candidateBall = contact.firstBall >= 0
                ? contact.firstBall : contact.secondBall;
            return contact.kind == PhysicsContactKind::Pocket &&
                candidateBall == ball &&
                isJaw(contact.pocketBoundaryEvent) &&
                contact.timeOfImpactSeconds == rail.timeOfImpactSeconds;
        });
}

bool makeBreakEvidence(const PhysicsContactRecord& contact,
                       const PhysicsStepTelemetry& telemetry,
                       std::uint64_t physicsTick,
                       BreakObjectCushionContact& evidence) noexcept
{
    const int ball = contact.firstBall >= 0
        ? contact.firstBall : contact.secondBall;
    if (ball < 1 || ball >= kBallCount ||
        !std::isfinite(contact.normalImpulseNs) ||
        !std::isfinite(contact.solverResidualCmS) ||
        !std::isfinite(contact.solverProjectionCm) ||
        !std::isfinite(contact.timeOfImpactSeconds) ||
        contact.normalImpulseNs < 0.0 || contact.solverResidualCmS < 0.0 ||
        contact.solverProjectionCm < 0.0 ||
        contact.timeOfImpactSeconds < 0.0)
        return false;

    evidence.objectBallId = ball;
    evidence.physicsTick = physicsTick;
    evidence.velocityImpulseApplied = contact.velocityImpulseApplied;
    evidence.positionCorrected = contact.positionCorrected;
    evidence.normalImpulseNs = contact.normalImpulseNs;
    evidence.solverResidualCmS = contact.solverResidualCmS;
    evidence.solverProjectionCm = contact.solverProjectionCm;
    evidence.timeOfImpactSeconds = contact.timeOfImpactSeconds;
    evidence.solverEventId = contact.solverEventId;
    evidence.solverIslandId = contact.solverIslandId;

    if (isJaw(contact.pocketBoundaryEvent) ||
        (contact.pocketBoundaryEvent == PocketBoundaryEventKind::None &&
         matchedDiscreteJaw(contact, telemetry))) {
        evidence.boundaryKind = BreakCushionBoundaryKind::PocketJaw;
    } else if (contact.pocketBoundaryEvent ==
                   PocketBoundaryEventKind::None ||
               contact.pocketBoundaryEvent ==
                   PocketBoundaryEventKind::StraightRail) {
        evidence.boundaryKind = BreakCushionBoundaryKind::StraightCushion;
    } else {
        return false;
    }

    if (contact.solverEventId >= 0) {
        if (contact.solverIslandId < 0) return false;
        evidence.provenance =
            BreakCushionContactProvenance::ContinuousSolver;
    } else {
        if (contact.solverEventId != -1 || contact.solverIslandId != -1)
            return false;
        evidence.provenance =
            BreakCushionContactProvenance::DiscretePhysics;
    }
    return true;
}

}  // namespace

void ShotFactCollector::begin(const GameState& state, int shooter,
    bool breakShot,
    const BehindHeadStringFirstContactContext* headStringContext)
{
    if (shooter < 0 || shooter > 1)
        throw std::invalid_argument("shot fact collector shooter must be 0 or 1");
    active_ = true;
    firstContactObserved_ = false;
    facts_ = {};
    facts_.shooter = shooter;
    facts_.breakShot = breakShot;
    facts_.breakEvidenceOrigin =
        BreakEvidenceOrigin::AuthoritativeShotFactCollector;
    facts_.cueBallScratchEvidenceOrigin =
        CueBallScratchEvidenceOrigin::AuthoritativeShotFactCollector;
    objectBallsToRail_.clear();
    breakObjectCushionContacts_.clear();
    cueBallPocketCaptures_.clear();
    lastSucceededPhysicsTick_ = 0;
    breakEvidenceComplete_ = breakShot;
    cueBallScratchEvidenceComplete_ = true;
    headStringContext_ = {};
    crossingEvents_.clear();
    firstContactEvidence_.clear();
    headStringEvidenceComplete_ = false;
    consumedTickDigests_.clear();
    if (headStringContext && headStringContext->active) {
        if (headStringContext->actorPlayerId != shooter ||
            headStringContext->placementDecisionId.empty() ||
            headStringContext->observationId.empty() ||
            !std::isfinite(headStringContext->headStringWorldZCm))
            throw std::invalid_argument(
                "invalid behind-head-string collector context");
        headStringContext_ = *headStringContext;
        facts_.behindHeadStringFirstContactRestrictionActive = true;
        facts_.headStringWorldZCm =
            headStringContext_.headStringWorldZCm;
        facts_.headStringCrossingEvidenceOrigin =
            HeadStringCrossingEvidenceOrigin::
                AuthoritativeShotFactCollector;
        headStringEvidenceComplete_ = true;
    }
    pocketRecorded_.fill(false);
    facts_.solidsRemainingBefore = 0;
    facts_.stripesRemainingBefore = 0;
    for (int ball = 1; ball < kBallCount; ++ball) {
        pocketRecorded_[static_cast<std::size_t>(ball)] = state.balls[ball].pocketed;
        if (state.balls[ball].pocketed || state.balls[ball].outOfPlay || ball == 8) continue;
        if (ball <= 7) ++facts_.solidsRemainingBefore;
        else ++facts_.stripesRemainingBefore;
    }
    pocketRecorded_[0] = state.balls[0].pocketed;
}

void ShotFactCollector::consume(const GameState& before, const GameState& after,
                                const PhysicsStepTelemetry& telemetry,
                                std::uint64_t physicsTick)
{
    if (!active_) throw std::logic_error("shot fact collector consume before begin");
    if (telemetry.stepStatus != PhysicsStepStatus::Succeeded) return;
    if (physicsTick == 0)
        throw std::logic_error(
            "shot fact collector requires increasing succeeded physics ticks");
    const std::string digest = tickDigest(telemetry);
    const auto consumed = consumedTickDigests_.find(physicsTick);
    if (consumed != consumedTickDigests_.end()) {
        if (consumed->second == digest) return;
        throw std::logic_error(
            "shot fact collector conflicting succeeded physics tick");
    }
    if (physicsTick < lastSucceededPhysicsTick_)
        throw std::logic_error(
            "shot fact collector requires increasing succeeded physics ticks");
    consumedTickDigests_[physicsTick] = digest;
    lastSucceededPhysicsTick_ = physicsTick;

    std::vector<CueBallPocketCaptureEvidence> tickCaptures;
    bool malformedCueCapture = false;
    for (std::size_t index = 0; index < telemetry.contacts.size(); ++index) {
        const PhysicsContactRecord& contact = telemetry.contacts[index];
        if (contact.kind != PhysicsContactKind::Pocket ||
            contact.firstBall != 0 ||
            contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture)
            continue;
        if (!validCueCaptureContact(contact)) {
            malformedCueCapture = true;
            continue;
        }
        CueBallPocketCaptureEvidence evidence;
        evidence.pocketId = contact.pocketId;
        evidence.captureSequence = contact.pocketCaptureSequence;
        evidence.physicsTick = physicsTick;
        evidence.physicsContactOrdinal = static_cast<int>(index);
        evidence.timeOfImpactSeconds = contact.timeOfImpactSeconds;
        const auto key = cueCaptureKey(evidence);
        const bool duplicate = std::any_of(tickCaptures.begin(),
            tickCaptures.end(), [&](const auto& candidate) {
                return cueCaptureKey(candidate) == key;
            });
        if (!duplicate) tickCaptures.push_back(evidence);
    }
    if (after.events.cueBallPocketed != !tickCaptures.empty())
        malformedCueCapture = true;
    for (const CueBallPocketCaptureEvidence& evidence : tickCaptures) {
        const auto key = cueCaptureKey(evidence);
        const bool duplicate = std::any_of(cueBallPocketCaptures_.begin(),
            cueBallPocketCaptures_.end(), [&](const auto& candidate) {
                return cueCaptureKey(candidate) == key;
            });
        if (!duplicate) cueBallPocketCaptures_.push_back(evidence);
    }
    if (cueBallPocketCaptures_.size() > 1) malformedCueCapture = true;
    if (malformedCueCapture) {
        cueBallScratchEvidenceComplete_ = false;
        cueBallPocketCaptures_.clear();
    }

    const bool collectFirstContactTick = !firstContactObserved_;
    if (headStringContext_.active && collectFirstContactTick) {
        const PhysicsPlaneObservationAttestation* matching = nullptr;
        for (const auto& attestation : telemetry.planeAttestations) {
            if (attestation.observationId !=
                    headStringContext_.observationId ||
                attestation.ballIndex != 0)
                continue;
            if (matching != nullptr) headStringEvidenceComplete_ = false;
            matching = &attestation;
        }
        if (matching == nullptr || !matching->complete ||
            !std::isfinite(matching->wholeTickDurationSeconds) ||
            matching->wholeTickDurationSeconds !=
                telemetry.wholeTickDurationSeconds)
            headStringEvidenceComplete_ = false;
        int matchingCrossings = 0;
        for (const PhysicsPlaneCrossingRecord& crossing :
             telemetry.planeCrossings) {
            if (crossing.observationId !=
                    headStringContext_.observationId ||
                crossing.ballIndex != 0)
                continue;
            ++matchingCrossings;
            if (!validCrossingRecord(
                    crossing, headStringContext_, telemetry)) {
                headStringEvidenceComplete_ = false;
                continue;
            }
            CueCenterPlaneCrossingEvidence evidence;
            evidence.observationId = crossing.observationId;
            evidence.physicsTick = physicsTick;
            evidence.wholeTickTimeOfImpactSeconds =
                crossing.wholeTickTimeOfImpactSeconds;
            evidence.segmentOrdinal = crossing.segmentOrdinal;
            evidence.signedDistanceBeforeCm =
                crossing.signedDistanceBeforeCm;
            evidence.signedDistanceAfterCm =
                crossing.signedDistanceAfterCm;
            evidence.axisVelocityAtCrossingCmS =
                crossing.axisVelocityAtCrossingCmS;
            crossingEvents_.push_back(evidence);
        }
        if (matching && matching->crossingRecordCount != matchingCrossings)
            headStringEvidenceComplete_ = false;
    }

    std::vector<FirstCueObjectContactEvidence> tickFirstContacts;
    double earliestContactTime = std::numeric_limits<double>::infinity();
    int earliestSolverEvent = std::numeric_limits<int>::max();
    for (std::size_t index = 0;
         collectFirstContactTick && index < telemetry.contacts.size();
         ++index) {
        const PhysicsContactRecord& contact = telemetry.contacts[index];
        if (contact.kind != PhysicsContactKind::BallBall) continue;
        const int object = contact.firstBall == 0 ? contact.secondBall
            : contact.secondBall == 0 ? contact.firstBall : -1;
        if (object < 1 || object >= kBallCount) continue;
        if (!std::isfinite(contact.wholeTickTimeOfImpactSeconds) ||
            contact.wholeTickTimeOfImpactSeconds < 0.0 ||
            contact.wholeTickTimeOfImpactSeconds >
                telemetry.wholeTickDurationSeconds) {
            headStringEvidenceComplete_ = false;
            continue;
        }
        const Point3 objectPosition = contact.firstBall == object
            ? contact.firstPositionAtContactCm
            : contact.secondPositionAtContactCm;
        if (!std::isfinite(objectPosition.x) ||
            !std::isfinite(objectPosition.z)) {
            headStringEvidenceComplete_ = false;
            continue;
        }
        const double time = contact.wholeTickTimeOfImpactSeconds;
        if (time < earliestContactTime ||
            (time == earliestContactTime &&
             contact.solverEventId < earliestSolverEvent)) {
            earliestContactTime = time;
            earliestSolverEvent = contact.solverEventId;
            tickFirstContacts.clear();
        }
        if (time != earliestContactTime ||
            contact.solverEventId != earliestSolverEvent)
            continue;
        FirstCueObjectContactEvidence evidence;
        evidence.objectBallId = object;
        evidence.physicsTick = physicsTick;
        evidence.wholeTickTimeOfImpactSeconds = time;
        evidence.solverEventId = contact.solverEventId;
        evidence.physicsContactOrdinal = static_cast<int>(index);
        evidence.objectBallCenterWorldXCm = objectPosition.x;
        evidence.objectBallCenterWorldZCm = objectPosition.z;
        tickFirstContacts.push_back(evidence);
    }
    std::sort(tickFirstContacts.begin(), tickFirstContacts.end(),
        [](const auto& left, const auto& right) {
            return left.objectBallId < right.objectBallId;
        });
    tickFirstContacts.erase(std::unique(tickFirstContacts.begin(),
        tickFirstContacts.end(), [](const auto& left, const auto& right) {
            return left.objectBallId == right.objectBallId;
        }), tickFirstContacts.end());
    std::set<int> firstTickObjectBalls;
    for (const auto& evidence : tickFirstContacts) {
        firstTickObjectBalls.insert(evidence.objectBallId);
        firstContactEvidence_.push_back(evidence);
    }
    const bool firstContactInThisTick = !firstTickObjectBalls.empty();
    for (const PhysicsContactRecord& contact : telemetry.contacts) {
        if (contact.kind == PhysicsContactKind::Rail) {
            const int ball = contact.firstBall >= 0 ? contact.firstBall : contact.secondBall;
            if (ball >= 1 && ball < kBallCount) objectBallsToRail_.insert(ball);
            if (facts_.breakShot && ball >= 1 && ball < kBallCount) {
                BreakObjectCushionContact evidence;
                if (makeBreakEvidence(contact, telemetry, physicsTick,
                                      evidence)) {
                    breakObjectCushionContacts_.push_back(evidence);
                } else {
                    breakEvidenceComplete_ = false;
                }
            }
            if (firstContactObserved_ || firstContactInThisTick)
                facts_.anyRailAfterFirstContact = true;
        }
    }
    if (!firstTickObjectBalls.empty()) {
        facts_.firstContactBalls.assign(firstTickObjectBalls.begin(),
                                        firstTickObjectBalls.end());
        facts_.firstContactBall = facts_.firstContactBalls.front();
        firstContactObserved_ = true;
    }

    std::vector<std::pair<unsigned long long, int>> newlyPocketed;
    for (int ball = 0; ball < kBallCount; ++ball) {
        if (!before.balls[ball].pocketed && after.balls[ball].pocketed &&
            !pocketRecorded_[static_cast<std::size_t>(ball)]) {
            newlyPocketed.emplace_back(
                after.balls[ball].pocketInteraction.captureSequence, ball);
            pocketRecorded_[static_cast<std::size_t>(ball)] = true;
        }
    }
    std::stable_sort(newlyPocketed.begin(), newlyPocketed.end(),
                     [](const auto& left, const auto& right) {
                         return left.first < right.first;
                     });
    for (const auto& pocket : newlyPocketed) {
        facts_.pocketedBallsInOrder.push_back(pocket.second);
        if (pocket.second == 0) facts_.cueBallPocketed = true;
        if (pocket.second == 8)
            facts_.eightBallPocket = after.balls[8].pocketInteraction.pocketId;
    }
    facts_.distinctObjectBallsToRail =
        static_cast<int>(objectBallsToRail_.size());
}

bool ShotFactCollector::bindCanonicalDoubleHitClassification(
    const CanonicalDoubleHitClassification& classification)
{
    if (!active_) return false;
    CanonicalDoubleHitClassification verified = classification;
    const std::string suppliedDigest = classification.digest;
    const auto validation = validateCanonicalDoubleHitClassification(verified);
    if (!validation.valid || !validation.complete ||
        verified.digest != suppliedDigest)
        return false;
    if (facts_.hasCanonicalDoubleHitClassification)
        return facts_.canonicalDoubleHitClassification.digest ==
            suppliedDigest;
    facts_.hasCanonicalDoubleHitClassification = true;
    facts_.canonicalDoubleHitClassification = classification;
    return true;
}

bool ShotFactCollector::bindCanonicalPushClassification(
    const CanonicalPushClassification& input)
{
    if (!active_) return false;
    CanonicalPushClassification classification = input;
    const std::string suppliedDigest = classification.digest;
    const auto validation = validateCanonicalPushClassification(classification);
    if (!validation.valid || !validation.complete ||
        suppliedDigest != classification.digest) return false;
    if (facts_.hasCanonicalPushClassification)
        return facts_.canonicalPushClassification.digest == suppliedDigest;
    facts_.hasCanonicalPushClassification = true;
    facts_.canonicalPushClassification = std::move(classification);
    return true;
}

bool ShotFactCollector::bindCanonicalFrozenCushionClassification(
    const CanonicalFrozenCushionClassification& input)
{
    if (!active_) return false;
    CanonicalFrozenCushionClassification classification = input;
    const std::string suppliedDigest = classification.digest;
    const auto validation =
        validateCanonicalFrozenCushionClassification(classification);
    if (!validation.valid || !validation.complete ||
        suppliedDigest != classification.digest) return false;
    if (facts_.hasCanonicalFrozenCushionClassification)
        return facts_.canonicalFrozenCushionClassification.digest ==
            suppliedDigest;
    facts_.hasCanonicalFrozenCushionClassification = true;
    facts_.canonicalFrozenCushionClassification = std::move(classification);
    return true;
}

ShotFacts ShotFactCollector::finish()
{
    if (!active_) throw std::logic_error("shot fact collector finish before begin");
    active_ = false;
    facts_.cueBallScratchEvidenceComplete =
        cueBallScratchEvidenceComplete_ && lastSucceededPhysicsTick_ > 0;
    if (facts_.cueBallScratchEvidenceComplete) {
        for (std::size_t index = 0;
             index < cueBallPocketCaptures_.size(); ++index) {
            cueBallPocketCaptures_[index].canonicalEventOrdinal =
                static_cast<int>(index);
        }
        facts_.cueBallPocketCaptures = cueBallPocketCaptures_;
        facts_.cueBallPocketed = !facts_.cueBallPocketCaptures.empty();
    } else {
        facts_.cueBallPocketCaptures.clear();
        facts_.cueBallPocketed = false;
    }
    if (facts_.breakShot) {
        std::sort(breakObjectCushionContacts_.begin(),
                  breakObjectCushionContacts_.end(),
                  [](const auto& left, const auto& right) {
                      return evidenceKey(left) < evidenceKey(right);
                  });
        breakObjectCushionContacts_.erase(
            std::unique(breakObjectCushionContacts_.begin(),
                        breakObjectCushionContacts_.end(),
                        [](const auto& left, const auto& right) {
                            return evidenceKey(left) == evidenceKey(right);
                        }),
            breakObjectCushionContacts_.end());
        for (std::size_t index = 0;
             index < breakObjectCushionContacts_.size(); ++index) {
            breakObjectCushionContacts_[index].canonicalContactOrdinal =
                static_cast<int>(index);
        }
        facts_.breakObjectCushionContacts = breakObjectCushionContacts_;
        facts_.breakEvidenceComplete = breakEvidenceComplete_ &&
            lastSucceededPhysicsTick_ > 0;
    }
    if (headStringContext_.active) {
        std::sort(crossingEvents_.begin(), crossingEvents_.end(),
            [](const auto& left, const auto& right) {
                return crossingKey(left) < crossingKey(right);
            });
        std::vector<CueCenterPlaneCrossingEvidence> canonical;
        for (const auto& evidence : crossingEvents_) {
            if (!canonical.empty() &&
                crossingKey(canonical.back()) == crossingKey(evidence)) {
                if (canonical.back().signedDistanceBeforeCm !=
                        evidence.signedDistanceBeforeCm ||
                    canonical.back().signedDistanceAfterCm !=
                        evidence.signedDistanceAfterCm ||
                    canonical.back().axisVelocityAtCrossingCmS !=
                        evidence.axisVelocityAtCrossingCmS)
                    headStringEvidenceComplete_ = false;
                continue;
            }
            canonical.push_back(evidence);
        }
        for (std::size_t index = 0; index < canonical.size(); ++index)
            canonical[index].canonicalEventOrdinal =
                static_cast<int>(index);
        facts_.cueCenterPlaneCrossingEvents = canonical;
        facts_.firstCueObjectContactEvidence = firstContactEvidence_;
        facts_.headStringCrossingEvidenceComplete =
            headStringEvidenceComplete_ && lastSucceededPhysicsTick_ > 0;
        if (facts_.headStringCrossingEvidenceComplete &&
            !canonical.empty() && !firstContactEvidence_.empty()) {
            facts_.cueBallCrossedHeadStringBeforeFirstContact =
                std::tie(canonical.front().physicsTick,
                    canonical.front().wholeTickTimeOfImpactSeconds) <
                std::tie(firstContactEvidence_.front().physicsTick,
                    firstContactEvidence_.front().
                        wholeTickTimeOfImpactSeconds);
        }
    }
    return facts_;
}

}  // namespace billiardgl::rules
