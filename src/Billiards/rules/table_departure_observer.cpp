#include "rules/table_departure_observer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <tuple>

namespace billiardgl::rules {
namespace {

class CanonicalHash {
public:
    void addU64(std::uint64_t value)
    {
        for (int shift = 0; shift < 64; shift += 8) {
            hash_ ^= static_cast<unsigned char>((value >> shift) & 0xffu);
            hash_ *= 1099511628211ull;
        }
    }

    void addI64(std::int64_t value)
    {
        addU64(static_cast<std::uint64_t>(value));
    }

    void addBool(bool value) { addU64(value ? 1u : 0u); }

    void addFloat(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        addU64(bits);
    }

    void addDouble(double value)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        addU64(bits);
    }

    void addString(const std::string& value)
    {
        addU64(value.size());
        for (unsigned char byte : value) {
            hash_ ^= byte;
            hash_ *= 1099511628211ull;
        }
    }

    std::string finish() const
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::hex << std::setfill('0') << std::setw(16) << hash_;
        return stream.str();
    }

private:
    std::uint64_t hash_ = 1469598103934665603ull;
};

bool finitePoint(const Point3& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z);
}

double speed(const Point3& velocity)
{
    return std::sqrt(static_cast<double>(velocity.x) * velocity.x +
        static_cast<double>(velocity.y) * velocity.y +
        static_cast<double>(velocity.z) * velocity.z);
}

void hashPoint(CanonicalHash& hash, const Point3& point)
{
    hash.addFloat(point.x);
    hash.addFloat(point.y);
    hash.addFloat(point.z);
}

void hashEvent(CanonicalHash& hash,
    const CanonicalTableDepartureEvent& event, bool includeIdentity)
{
    hash.addI64(static_cast<int>(event.kind));
    hash.addI64(event.ballId);
    hash.addU64(event.physicsTick);
    hash.addI64(event.eventOrdinal);
    hash.addI64(event.solverEventId);
    hash.addU64(event.trajectoryGeneration);
    hash.addU64(event.previousTrajectoryGeneration);
    hash.addDouble(event.wholeTickTimeSeconds);
    hash.addI64(static_cast<int>(event.sourceDomain));
    hash.addI64(static_cast<int>(event.targetDomain));
    hashPoint(hash, event.positionCm);
    hashPoint(hash, event.velocityBeforeCmS);
    hashPoint(hash, event.velocityAfterCmS);
    hash.addFloat(event.ballRadiusCm);
    hash.addI64(event.boundaryFeatureId);
    hash.addI64(event.railFeatureId);
    hash.addI64(event.landingSurfaceId);
    hash.addI64(event.pocketId);
    hash.addU64(event.pocketCaptureSequence);
    hash.addString(event.origin);
    hash.addBool(event.complete);
    if (includeIdentity) hash.addString(event.stableIdentity);
}

std::string stableIdentity(const std::string& shotIdentity,
    const CanonicalTableDepartureEvent& event)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << shotIdentity << ':' << event.ballId << ':'
           << event.trajectoryGeneration << ':' << event.physicsTick << ':'
           << event.eventOrdinal << ':' << event.solverEventId;
    return stream.str();
}

std::string tickDigest(const CanonicalTableDepartureTick& tick)
{
    CanonicalHash hash;
    hash.addU64(tick.physicsTick);
    hash.addDouble(tick.wholeTickDurationSeconds);
    hash.addString(tick.solverProfileId);
    hash.addI64(static_cast<int>(tick.boundaryMode));
    hash.addString(canonicalTableDepartureEventDigest(tick.events));
    const auto& attestation = tick.attestation;
    hash.addU64(attestation.physicsTick);
    hash.addDouble(attestation.wholeTickDurationSeconds);
    hash.addString(attestation.solverProfileId);
    hash.addI64(static_cast<int>(attestation.boundaryMode));
    hash.addU64(attestation.sourceEventCount);
    hash.addU64(attestation.privatePhysicsEventCount);
    hash.addU64(attestation.analyticTransitionSourceCount);
    hash.addU64(attestation.canonicalContactSourceCount);
    hash.addU64(attestation.excludedPhysicsDiagnosticEventCount);
    hash.addU64(attestation.nonSettlingSupportContactSourceCount);
    hash.addU64(attestation.pendingPocketEntrySourceCount);
    hash.addU64(attestation.emittedEventCount);
    hash.addString(attestation.orderedSourceDigest);
    hash.addString(attestation.orderedEventDigest);
    hash.addBool(attestation.successfulStep);
    hash.addBool(attestation.canonicalOrder);
    hash.addBool(attestation.noDroppedSamples);
    hash.addBool(attestation.complete);
    return hash.finish();
}

bool privateKindMapsToCanonical(
    CanonicalTableDeparturePrivateEventKind privateKind,
    CanonicalTableDepartureEventKind canonicalKind)
{
    switch (privateKind) {
    case CanonicalTableDeparturePrivateEventKind::OuterDeparture:
        return canonicalKind == CanonicalTableDepartureEventKind::OuterDeparture;
    case CanonicalTableDeparturePrivateEventKind::RailTopLanding:
        return canonicalKind == CanonicalTableDepartureEventKind::RailTopLanding;
    case CanonicalTableDeparturePrivateEventKind::ClothReturn:
        return canonicalKind == CanonicalTableDepartureEventKind::ClothReturn;
    case CanonicalTableDeparturePrivateEventKind::OutsideLanding:
        return canonicalKind == CanonicalTableDepartureEventKind::OutsideLanding;
    case CanonicalTableDeparturePrivateEventKind::OutsideRest:
        return canonicalKind == CanonicalTableDepartureEventKind::OutsideRest;
    case CanonicalTableDeparturePrivateEventKind::PocketCapture:
        return canonicalKind == CanonicalTableDepartureEventKind::PocketCapture;
    case CanonicalTableDeparturePrivateEventKind::AdministrativeReset:
        return canonicalKind ==
            CanonicalTableDepartureEventKind::AdministrativeReset;
    case CanonicalTableDeparturePrivateEventKind::PocketEntryDiagnostic:
    case CanonicalTableDeparturePrivateEventKind::NoseContactDiagnostic:
    case CanonicalTableDeparturePrivateEventKind::CrossContactDiagnostic:
        return false;
    }
    return false;
}

bool privateSourceProjectionValid(const CanonicalTableDepartureTick& tick)
{
    const auto& attestation = tick.attestation;
    const auto& receipts = attestation.orderedSourceReceipts;
    if (attestation.orderedSourceDigest !=
            canonicalTableDepartureSourceDigest(receipts))
        return false;

    std::size_t emitted = 0;
    std::size_t excluded = 0;
    std::size_t nonSettling = 0;
    std::size_t pendingPocketEntries = 0;
    std::size_t privateEvents = 0;
    std::size_t analyticTransitions = 0;
    std::size_t canonicalContacts = 0;
    double priorTime = -std::numeric_limits<double>::infinity();
    std::set<std::string> identities;
    std::set<int> physicsContactOrdinals;
    for (std::size_t index = 0; index < receipts.size(); ++index) {
        const auto& receipt = receipts[index];
        if (receipt.sourceOrdinal != static_cast<int>(index) ||
            receipt.sourceIdentity.empty() || !receipt.complete ||
            receipt.ballId < 0 || receipt.ballId >= kBallCount ||
            !std::isfinite(receipt.wholeTickTimeSeconds) ||
            receipt.wholeTickTimeSeconds < 0.0 ||
            receipt.wholeTickTimeSeconds > tick.wholeTickDurationSeconds ||
            receipt.wholeTickTimeSeconds < priorTime ||
            !identities.insert(receipt.sourceIdentity).second)
            return false;
        priorTime = receipt.wholeTickTimeSeconds;

        if (receipt.sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::PrivateEvent) {
            if (receipt.privateEventOrdinal !=
                    static_cast<int>(privateEvents) ||
                receipt.sourceSegmentOrdinal != -1 ||
                receipt.precedingSegmentOrdinal != -1 ||
                (receipt.physicsContactOrdinal != -1 &&
                    receipt.kind !=
                        CanonicalTableDeparturePrivateEventKind::
                            RailTopLanding &&
                    receipt.kind !=
                        CanonicalTableDeparturePrivateEventKind::ClothReturn &&
                    receipt.kind !=
                        CanonicalTableDeparturePrivateEventKind::
                            OutsideLanding &&
                    receipt.kind !=
                        CanonicalTableDeparturePrivateEventKind::
                            NoseContactDiagnostic) ||
                receipt.solverEventId < -1 ||
                !receipt.analyticPartitionDigest.empty() ||
                (receipt.canonicalContactOrdinal != -1 &&
                    receipt.kind != CanonicalTableDeparturePrivateEventKind::
                        PocketEntryDiagnostic) ||
                receipt.sourceAnalyticState !=
                    CanonicalTableDepartureAnalyticState::NotApplicable ||
                receipt.targetAnalyticState !=
                    CanonicalTableDepartureAnalyticState::NotApplicable)
                return false;
            if (receipt.physicsContactOrdinal >= 0 &&
                !physicsContactOrdinals.insert(
                    receipt.physicsContactOrdinal).second)
                return false;
            ++privateEvents;
        } else if (receipt.sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::AnalyticTransition) {
            ++analyticTransitions;
            if (receipt.kind !=
                    CanonicalTableDeparturePrivateEventKind::OutsideRest ||
                receipt.privateEventOrdinal != -1 ||
                receipt.physicsContactOrdinal != -1 ||
                receipt.precedingSegmentOrdinal < 0 ||
                receipt.sourceSegmentOrdinal < 0 ||
                receipt.precedingSegmentOrdinal ==
                    receipt.sourceSegmentOrdinal ||
                receipt.solverEventId != -1 ||
                receipt.disposition !=
                    CanonicalTableDepartureProjectionDisposition::
                        EmittedCanonical ||
                receipt.exclusionReason !=
                    CanonicalTableDepartureExclusionReason::None ||
                receipt.analyticPartitionDigest.empty() ||
                !finitePoint(receipt.transitionPositionCm) ||
                !finitePoint(receipt.velocityBeforeCmS) ||
                !finitePoint(receipt.velocityAfterCmS) ||
                speed(receipt.velocityBeforeCmS) > 1e-6 ||
                speed(receipt.velocityAfterCmS) > 1e-6 ||
                receipt.sourceDomain !=
                    PhysicalBallDomain::OutsideLandingSupported ||
                receipt.targetDomain !=
                    PhysicalBallDomain::OutsideLandingSupported ||
                (receipt.sourceAnalyticState !=
                        CanonicalTableDepartureAnalyticState::OutsideRolling &&
                    receipt.sourceAnalyticState !=
                        CanonicalTableDepartureAnalyticState::
                            OutsideLandingPostState) ||
                receipt.targetAnalyticState !=
                    CanonicalTableDepartureAnalyticState::OutsideStationary)
                return false;
        } else if (receipt.sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::CanonicalContact) {
            if (receipt.privateEventOrdinal != -1 ||
                receipt.physicsContactOrdinal < 0 ||
                !physicsContactOrdinals.insert(
                    receipt.physicsContactOrdinal).second ||
                receipt.canonicalContactOrdinal !=
                    static_cast<int>(canonicalContacts) ||
                receipt.precedingSegmentOrdinal != -1 ||
                receipt.sourceSegmentOrdinal != -1 ||
                receipt.kind !=
                    CanonicalTableDeparturePrivateEventKind::PocketCapture ||
                receipt.solverEventId < -1 ||
                !receipt.analyticPartitionDigest.empty() ||
                receipt.sourceAnalyticState !=
                    CanonicalTableDepartureAnalyticState::NotApplicable ||
                receipt.targetAnalyticState !=
                    CanonicalTableDepartureAnalyticState::NotApplicable)
                return false;
            ++canonicalContacts;
        } else {
            return false;
        }

        if (receipt.disposition ==
                CanonicalTableDepartureProjectionDisposition::EmittedCanonical) {
            if (receipt.exclusionReason !=
                    CanonicalTableDepartureExclusionReason::None ||
                receipt.canonicalEventOrdinal !=
                    static_cast<int>(emitted) ||
                receipt.canonicalEventCount <= 0 ||
                emitted + static_cast<std::size_t>(receipt.canonicalEventCount) >
                    tick.events.size())
                return false;
            const auto& event = tick.events[emitted];
            std::vector<CanonicalTableDepartureEvent> projectedEvents(
                tick.events.begin() + static_cast<std::ptrdiff_t>(emitted),
                tick.events.begin() + static_cast<std::ptrdiff_t>(emitted) +
                    receipt.canonicalEventCount);
            if (receipt.sourceOrigin ==
                    CanonicalTableDepartureSourceOrigin::CanonicalContact) {
                if ((receipt.canonicalEventCount != 1 &&
                        receipt.canonicalEventCount != 2) ||
                    projectedEvents[0].kind !=
                        CanonicalTableDepartureEventKind::PocketCapture ||
                    (receipt.canonicalEventCount == 2 &&
                        (projectedEvents[1].kind !=
                            CanonicalTableDepartureEventKind::
                                AdministrativeReset ||
                         projectedEvents[0].ballId !=
                            projectedEvents[1].ballId ||
                         projectedEvents[0].solverEventId !=
                            projectedEvents[1].solverEventId ||
                         projectedEvents[0].wholeTickTimeSeconds !=
                            projectedEvents[1].wholeTickTimeSeconds ||
                         projectedEvents[0].pocketId !=
                            projectedEvents[1].pocketId ||
                         projectedEvents[0].pocketCaptureSequence !=
                            projectedEvents[1].pocketCaptureSequence ||
                         projectedEvents[0].trajectoryGeneration ==
                            std::numeric_limits<std::uint64_t>::max() ||
                         projectedEvents[1].previousTrajectoryGeneration !=
                            projectedEvents[0].trajectoryGeneration ||
                         projectedEvents[1].trajectoryGeneration !=
                            projectedEvents[0].trajectoryGeneration + 1)))
                    return false;
            } else if (receipt.canonicalEventCount != 1 ||
                !privateKindMapsToCanonical(receipt.kind, event.kind)) {
                return false;
            }
            if (receipt.ballId != event.ballId ||
                receipt.solverEventId != event.solverEventId ||
                receipt.trajectoryGeneration != event.trajectoryGeneration ||
                receipt.wholeTickTimeSeconds != event.wholeTickTimeSeconds ||
                receipt.canonicalEventDigest !=
                    canonicalTableDepartureEventDigest(projectedEvents))
                return false;
            if (receipt.sourceOrigin ==
                    CanonicalTableDepartureSourceOrigin::AnalyticTransition &&
                (event.positionCm.x != receipt.transitionPositionCm.x ||
                    event.positionCm.y != receipt.transitionPositionCm.y ||
                    event.positionCm.z != receipt.transitionPositionCm.z ||
                    event.velocityBeforeCmS.x != receipt.velocityBeforeCmS.x ||
                    event.velocityBeforeCmS.y != receipt.velocityBeforeCmS.y ||
                    event.velocityBeforeCmS.z != receipt.velocityBeforeCmS.z ||
                    event.velocityAfterCmS.x != receipt.velocityAfterCmS.x ||
                    event.velocityAfterCmS.y != receipt.velocityAfterCmS.y ||
                    event.velocityAfterCmS.z != receipt.velocityAfterCmS.z ||
                    event.sourceDomain != receipt.sourceDomain ||
                    event.targetDomain != receipt.targetDomain))
                return false;
            if (receipt.sourceOrigin ==
                    CanonicalTableDepartureSourceOrigin::PrivateEvent &&
                (!finitePoint(receipt.transitionPositionCm) ||
                    !finitePoint(receipt.velocityBeforeCmS) ||
                    !finitePoint(receipt.velocityAfterCmS) ||
                    event.positionCm.x != receipt.transitionPositionCm.x ||
                    event.positionCm.y != receipt.transitionPositionCm.y ||
                    event.positionCm.z != receipt.transitionPositionCm.z ||
                    event.velocityBeforeCmS.x != receipt.velocityBeforeCmS.x ||
                    event.velocityBeforeCmS.y != receipt.velocityBeforeCmS.y ||
                    event.velocityBeforeCmS.z != receipt.velocityBeforeCmS.z ||
                    event.velocityAfterCmS.x != receipt.velocityAfterCmS.x ||
                    event.velocityAfterCmS.y != receipt.velocityAfterCmS.y ||
                    event.velocityAfterCmS.z != receipt.velocityAfterCmS.z ||
                    event.sourceDomain != receipt.sourceDomain ||
                    event.targetDomain != receipt.targetDomain))
                return false;
            if (receipt.sourceOrigin ==
                    CanonicalTableDepartureSourceOrigin::CanonicalContact &&
                (!finitePoint(receipt.transitionPositionCm) ||
                    !finitePoint(receipt.velocityBeforeCmS) ||
                    !finitePoint(receipt.velocityAfterCmS) ||
                    event.positionCm.x != receipt.transitionPositionCm.x ||
                    event.positionCm.y != receipt.transitionPositionCm.y ||
                    event.positionCm.z != receipt.transitionPositionCm.z ||
                    event.velocityBeforeCmS.x != receipt.velocityBeforeCmS.x ||
                    event.velocityBeforeCmS.y != receipt.velocityBeforeCmS.y ||
                    event.velocityBeforeCmS.z != receipt.velocityBeforeCmS.z ||
                    event.velocityAfterCmS.x != receipt.velocityAfterCmS.x ||
                    event.velocityAfterCmS.y != receipt.velocityAfterCmS.y ||
                    event.velocityAfterCmS.z != receipt.velocityAfterCmS.z ||
                    event.sourceDomain != receipt.sourceDomain ||
                    event.targetDomain != receipt.targetDomain))
                return false;
            emitted += static_cast<std::size_t>(receipt.canonicalEventCount);
            continue;
        }

        if (receipt.disposition ==
                CanonicalTableDepartureProjectionDisposition::
                    ObservedPendingPocketEntry) {
            if (receipt.sourceOrigin !=
                    CanonicalTableDepartureSourceOrigin::PrivateEvent ||
                receipt.kind != CanonicalTableDeparturePrivateEventKind::
                    PocketEntryDiagnostic ||
                receipt.physicsContactOrdinal != -1 ||
                receipt.canonicalContactOrdinal != -1 ||
                receipt.solverEventId != -1 ||
                receipt.exclusionReason !=
                    CanonicalTableDepartureExclusionReason::
                        PocketEntryAwaitingCapture ||
                receipt.canonicalEventOrdinal != -1 ||
                receipt.canonicalEventCount != 0 ||
                !receipt.canonicalEventDigest.empty() ||
                receipt.sourceDomain != PhysicalBallDomain::Airborne ||
                receipt.targetDomain != PhysicalBallDomain::PocketChannel)
                return false;
            ++pendingPocketEntries;
            continue;
        }

        if (receipt.disposition ==
                CanonicalTableDepartureProjectionDisposition::
                    ObservedNonSettlingSupport) {
            if (receipt.sourceOrigin !=
                    CanonicalTableDepartureSourceOrigin::PrivateEvent ||
                (receipt.kind !=
                        CanonicalTableDeparturePrivateEventKind::RailTopLanding &&
                 receipt.kind !=
                        CanonicalTableDeparturePrivateEventKind::ClothReturn &&
                 receipt.kind !=
                        CanonicalTableDeparturePrivateEventKind::OutsideLanding) ||
                receipt.physicsContactOrdinal < 0 ||
                receipt.solverEventId < 0 ||
                receipt.exclusionReason !=
                    CanonicalTableDepartureExclusionReason::
                        SupportContactImmediateRelease ||
                receipt.canonicalEventOrdinal != -1 ||
                receipt.canonicalEventCount != 0 ||
                !receipt.canonicalEventDigest.empty() ||
                !finitePoint(receipt.transitionPositionCm) ||
                !finitePoint(receipt.velocityAfterCmS) ||
                receipt.sourceDomain != PhysicalBallDomain::Airborne ||
                receipt.targetDomain != PhysicalBallDomain::Airborne ||
                !(receipt.velocityAfterCmS.y > 0.0f))
                return false;
            ++nonSettling;
            continue;
        }

        if (receipt.sourceOrigin !=
                CanonicalTableDepartureSourceOrigin::PrivateEvent ||
            receipt.disposition !=
                CanonicalTableDepartureProjectionDisposition::ExcludedDiagnostic ||
            receipt.canonicalEventOrdinal != -1 ||
            receipt.canonicalEventCount != 0 ||
            !receipt.canonicalEventDigest.empty())
            return false;
        if (receipt.kind ==
                CanonicalTableDeparturePrivateEventKind::NoseContactDiagnostic) {
            if (receipt.exclusionReason !=
                    CanonicalTableDepartureExclusionReason::
                        NoseContactNotTableDepartureMeasurement)
                return false;
        } else if (receipt.kind ==
                CanonicalTableDeparturePrivateEventKind::CrossContactDiagnostic) {
            if (receipt.exclusionReason !=
                    CanonicalTableDepartureExclusionReason::
                        CrossContactNotTableDepartureMeasurement)
                return false;
        } else if (receipt.kind ==
                CanonicalTableDeparturePrivateEventKind::PocketEntryDiagnostic) {
            if (receipt.exclusionReason !=
                    CanonicalTableDepartureExclusionReason::
                        PocketEntryBoundToCanonicalCapture ||
                receipt.canonicalContactOrdinal < 0)
                return false;
        } else {
            return false;
        }
        ++excluded;
    }
    for (const auto& receipt : receipts) {
        if (receipt.kind !=
                CanonicalTableDeparturePrivateEventKind::PocketEntryDiagnostic ||
            receipt.disposition !=
                CanonicalTableDepartureProjectionDisposition::
                    ExcludedDiagnostic)
            continue;
        const auto contact = std::find_if(receipts.begin(), receipts.end(),
            [&receipt](const auto& candidate) {
                return candidate.sourceOrigin ==
                        CanonicalTableDepartureSourceOrigin::CanonicalContact &&
                    candidate.canonicalContactOrdinal ==
                        receipt.canonicalContactOrdinal;
            });
        if (contact == receipts.end() || contact->ballId != receipt.ballId ||
            contact->trajectoryGeneration != receipt.trajectoryGeneration ||
            contact->solverEventId != receipt.solverEventId ||
            contact->wholeTickTimeSeconds < receipt.wholeTickTimeSeconds)
            return false;
    }
    return emitted == tick.events.size() &&
        attestation.sourceEventCount == emitted &&
        attestation.emittedEventCount == emitted &&
        attestation.excludedPhysicsDiagnosticEventCount == excluded &&
        attestation.nonSettlingSupportContactSourceCount == nonSettling &&
        attestation.pendingPocketEntrySourceCount == pendingPocketEntries &&
        attestation.privatePhysicsEventCount == privateEvents &&
        attestation.analyticTransitionSourceCount == analyticTransitions &&
        attestation.canonicalContactSourceCount == canonicalContacts;
}

using BallGeneration = std::pair<int, std::uint64_t>;

bool eventShapeValid(const CanonicalTableDepartureEvent& event,
    const CanonicalTableDepartureTick& tick)
{
    if (event.ballId < 0 || event.ballId >= kBallCount ||
        event.physicsTick != tick.physicsTick || event.eventOrdinal < 0 ||
        !event.complete ||
        event.origin !=
            "AUTHORITATIVE_CONTINUOUS_TABLE_DEPARTURE_SOLVER_V1" ||
        !std::isfinite(event.wholeTickTimeSeconds) ||
        event.wholeTickTimeSeconds < 0.0 ||
        event.wholeTickTimeSeconds > tick.wholeTickDurationSeconds ||
        !finitePoint(event.positionCm) ||
        !finitePoint(event.velocityBeforeCmS) ||
        !finitePoint(event.velocityAfterCmS) ||
        !std::isfinite(event.ballRadiusCm) || event.ballRadiusCm <= 0.0f)
        return false;

    switch (event.kind) {
    case CanonicalTableDepartureEventKind::OuterDeparture:
        return event.solverEventId >= -1 && event.boundaryFeatureId >= 0 &&
            event.railFeatureId == -1 &&
            event.landingSurfaceId == -1 && event.pocketId == -1 &&
            event.pocketCaptureSequence == 0 &&
            event.previousTrajectoryGeneration == 0 &&
            event.sourceDomain == PhysicalBallDomain::Airborne &&
            event.targetDomain == PhysicalBallDomain::Airborne;
    case CanonicalTableDepartureEventKind::RailTopLanding:
        return event.solverEventId >= 0 && event.railFeatureId >= 0 &&
            event.boundaryFeatureId == -1 &&
            event.landingSurfaceId == -1 && event.pocketId == -1 &&
            event.pocketCaptureSequence == 0 &&
            event.previousTrajectoryGeneration == 0 &&
            event.sourceDomain == PhysicalBallDomain::Airborne &&
            event.targetDomain == PhysicalBallDomain::RailTopSupported &&
            event.velocityAfterCmS.y == 0.0f;
    case CanonicalTableDepartureEventKind::ClothReturn:
        return event.solverEventId >= 0 && event.railFeatureId >= -1 &&
            event.boundaryFeatureId == -1 &&
            event.landingSurfaceId == -1 && event.pocketId == -1 &&
            event.pocketCaptureSequence == 0 &&
            event.previousTrajectoryGeneration == 0 &&
            event.sourceDomain == PhysicalBallDomain::Airborne &&
            event.targetDomain == PhysicalBallDomain::ClothSupported &&
            event.velocityAfterCmS.y == 0.0f;
    case CanonicalTableDepartureEventKind::OutsideLanding:
        return event.solverEventId >= 0 && event.landingSurfaceId >= 0 &&
            event.boundaryFeatureId == -1 &&
            event.railFeatureId == -1 && event.pocketId == -1 &&
            event.pocketCaptureSequence == 0 &&
            event.previousTrajectoryGeneration == 0 &&
            event.sourceDomain == PhysicalBallDomain::Airborne &&
            event.targetDomain ==
                PhysicalBallDomain::OutsideLandingSupported &&
            event.velocityAfterCmS.y == 0.0f;
    case CanonicalTableDepartureEventKind::OutsideRest:
        return event.solverEventId == -1 &&
            event.landingSurfaceId >= 0 && event.boundaryFeatureId == -1 &&
            event.railFeatureId == -1 && event.pocketId == -1 &&
            event.pocketCaptureSequence == 0 &&
            event.previousTrajectoryGeneration == 0 &&
            event.sourceDomain ==
                PhysicalBallDomain::OutsideLandingSupported &&
            event.targetDomain ==
                PhysicalBallDomain::OutsideLandingSupported &&
            speed(event.velocityAfterCmS) <= 1e-6;
    case CanonicalTableDepartureEventKind::PocketCapture:
        return event.solverEventId >= -1 && event.pocketId >= 0 &&
            event.pocketCaptureSequence > 0 &&
            event.boundaryFeatureId == -1 && event.railFeatureId == -1 &&
            event.landingSurfaceId == -1 &&
            event.previousTrajectoryGeneration == 0 &&
            event.targetDomain == PhysicalBallDomain::PocketChannel;
    case CanonicalTableDepartureEventKind::AdministrativeReset:
        return event.solverEventId >= -1 && event.ballId == 0 &&
            event.pocketId >= 0 &&
            event.pocketCaptureSequence > 0 &&
            event.boundaryFeatureId == -1 && event.railFeatureId == -1 &&
            event.landingSurfaceId == -1 &&
            event.trajectoryGeneration ==
                event.previousTrajectoryGeneration + 1 &&
            event.sourceDomain == PhysicalBallDomain::PocketChannel &&
            event.targetDomain == PhysicalBallDomain::ClothSupported &&
            speed(event.velocityBeforeCmS) <= 1e-6 &&
            speed(event.velocityAfterCmS) <= 1e-6;
    }
    return false;
}

bool rebuildMeasurements(CanonicalTableDepartureShotObservation& observation)
{
    struct Capture {
        std::uint64_t physicsTick = 0;
        double wholeTickTimeSeconds = 0.0;
        int pocketId = -1;
        std::uint64_t pocketCaptureSequence = 0;
    };
    std::map<BallGeneration, CanonicalBallTableDepartureMeasurement> rows;
    std::map<BallGeneration, int> stages;
    std::map<BallGeneration, int> activeRailFeatures;
    std::map<BallGeneration, Capture> captures;
    auto currentGenerations =
        observation.identity.initialTrajectoryGenerations;
    for (const auto& event : observation.events) {
        if (event.kind ==
                CanonicalTableDepartureEventKind::AdministrativeReset) {
            if (event.ballId != 0 ||
                event.previousTrajectoryGeneration !=
                    currentGenerations[0] ||
                event.previousTrajectoryGeneration ==
                    std::numeric_limits<std::uint64_t>::max() ||
                event.trajectoryGeneration !=
                    event.previousTrajectoryGeneration + 1)
                return false;
            const BallGeneration previous{0,
                event.previousTrajectoryGeneration};
            const auto capture = captures.find(previous);
            if (capture == captures.end() ||
                capture->second.physicsTick != event.physicsTick ||
                capture->second.wholeTickTimeSeconds !=
                    event.wholeTickTimeSeconds ||
                capture->second.pocketId != event.pocketId ||
                capture->second.pocketCaptureSequence !=
                    event.pocketCaptureSequence)
                return false;
            currentGenerations[0] = event.trajectoryGeneration;
        } else if (event.trajectoryGeneration !=
                currentGenerations[static_cast<std::size_t>(event.ballId)]) {
            return false;
        }
        const BallGeneration key{event.ballId, event.trajectoryGeneration};
        auto& row = rows[key];
        row.ballId = event.ballId;
        row.ballKind = event.ballId == 0 ?
            CanonicalTableDepartureBallKind::CueBall :
            CanonicalTableDepartureBallKind::ObjectBall;
        row.trajectoryGeneration = event.trajectoryGeneration;
        int& stage = stages[key];
        switch (event.kind) {
        case CanonicalTableDepartureEventKind::OuterDeparture:
            if (stage != 0) return false;
            row.departureObserved = true;
            stage = 1;
            break;
        case CanonicalTableDepartureEventKind::RailTopLanding:
            if (stage != 0 && stage != 1) return false;
            row.railTopLandingObserved = true;
            activeRailFeatures[key] = event.railFeatureId;
            stage = 2;
            break;
        case CanonicalTableDepartureEventKind::ClothReturn:
            if (stage == 2) {
                if (event.railFeatureId < 0 ||
                    activeRailFeatures[key] != event.railFeatureId)
                    return false;
                row.returnedOverRailTop = true;
                activeRailFeatures.erase(key);
            } else if (stage == 0) {
                if (event.railFeatureId != -1) return false;
                row.directClothLandingObserved = true;
            } else {
                return false;
            }
            stage = 0;
            break;
        case CanonicalTableDepartureEventKind::OutsideLanding:
            if (stage != 1) return false;
            row.outsideLandingObserved = true;
            stage = 4;
            break;
        case CanonicalTableDepartureEventKind::OutsideRest:
            if (stage != 4) return false;
            row.outsideRestObserved = true;
            row.cameToRestOffTable = true;
            stage = 5;
            break;
        case CanonicalTableDepartureEventKind::PocketCapture:
            if (stage != 0) return false;
            row.capturedInPocket = true;
            captures[key] = {event.physicsTick, event.wholeTickTimeSeconds,
                event.pocketId, event.pocketCaptureSequence};
            stage = 6;
            break;
        case CanonicalTableDepartureEventKind::AdministrativeReset: {
            if (stage != 0) return false;
            row.administrativeResetObserved = true;
            break;
        }
        }
    }

    observation.balls.clear();
    for (auto& entry : rows) {
        auto& row = entry.second;
        const int stage = stages[entry.first];
        if (stage == 5 ||
            (stage == 0 && (row.directClothLandingObserved ||
                row.returnedOverRailTop ||
                row.administrativeResetObserved)) ||
            (stage == 6 && row.ballId != 0)) {
            row.complete = true;
        } else if (stage == 6 && row.ballId == 0) {
            const BallGeneration resetKey{row.ballId,
                row.trajectoryGeneration + 1};
            row.complete = rows.count(resetKey) != 0 &&
                rows[resetKey].administrativeResetObserved;
        } else {
            row.complete = false;
        }
        observation.balls.push_back(row);
    }
    return true;
}

void refreshObservationDigest(CanonicalTableDepartureShotObservation& observation)
{
    CanonicalHash hash;
    hash.addString(observation.schemaId);
    hash.addString(observation.identity.shotIdentity);
    hash.addString(observation.identity.rackIdentity);
    hash.addString(observation.identity.solverProfileId);
    hash.addI64(static_cast<int>(observation.identity.boundaryMode));
    hash.addU64(observation.identity.firstPhysicsTick);
    for (std::uint64_t generation :
            observation.identity.initialTrajectoryGenerations)
        hash.addU64(generation);
    hash.addString(canonicalTableDepartureEventDigest(observation.events));
    hash.addU64(observation.orderedSuccessfulTickDigests.size());
    for (const auto& digest : observation.orderedSuccessfulTickDigests)
        hash.addString(digest);
    hash.addU64(observation.lastSuccessfulPhysicsTick);
    hash.addBool(observation.successfulPhysicsTicksContiguous);
    hash.addBool(observation.shotEndObserved);
    hash.addBool(observation.canonicalOrder);
    hash.addBool(observation.observationComplete);
    hash.addU64(observation.pendingPocketEntryCount);
    for (const auto& ball : observation.balls) {
        hash.addI64(ball.ballId);
        hash.addI64(static_cast<int>(ball.ballKind));
        hash.addU64(ball.trajectoryGeneration);
        hash.addBool(ball.departureObserved);
        hash.addBool(ball.railTopLandingObserved);
        hash.addBool(ball.directClothLandingObserved);
        hash.addBool(ball.returnedOverRailTop);
        hash.addBool(ball.outsideLandingObserved);
        hash.addBool(ball.outsideRestObserved);
        hash.addBool(ball.cameToRestOffTable);
        hash.addBool(ball.capturedInPocket);
        hash.addBool(ball.administrativeResetObserved);
        hash.addBool(ball.complete);
    }
    observation.digest = hash.finish();
}

}  // namespace

std::string canonicalTableDepartureEventDigest(
    const std::vector<CanonicalTableDepartureEvent>& events)
{
    CanonicalHash hash;
    hash.addU64(events.size());
    for (const auto& event : events) hashEvent(hash, event, false);
    return hash.finish();
}

std::string canonicalTableDepartureSourceDigest(
    const std::vector<CanonicalTableDepartureSourceReceipt>& receipts)
{
    CanonicalHash hash;
    hash.addU64(receipts.size());
    for (const auto& receipt : receipts) {
        hash.addI64(receipt.sourceOrdinal);
        hash.addI64(static_cast<int>(receipt.sourceOrigin));
        hash.addI64(receipt.privateEventOrdinal);
        hash.addI64(receipt.physicsContactOrdinal);
        hash.addI64(receipt.canonicalContactOrdinal);
        hash.addI64(receipt.precedingSegmentOrdinal);
        hash.addI64(receipt.sourceSegmentOrdinal);
        hash.addString(receipt.sourceIdentity);
        hash.addI64(static_cast<int>(receipt.kind));
        hash.addI64(receipt.ballId);
        hash.addI64(receipt.solverEventId);
        hash.addU64(receipt.trajectoryGeneration);
        hash.addDouble(receipt.wholeTickTimeSeconds);
        hashPoint(hash, receipt.transitionPositionCm);
        hashPoint(hash, receipt.velocityBeforeCmS);
        hashPoint(hash, receipt.velocityAfterCmS);
        hash.addI64(static_cast<int>(receipt.sourceDomain));
        hash.addI64(static_cast<int>(receipt.targetDomain));
        hash.addI64(static_cast<int>(receipt.sourceAnalyticState));
        hash.addI64(static_cast<int>(receipt.targetAnalyticState));
        hash.addString(receipt.analyticPartitionDigest);
        hash.addI64(static_cast<int>(receipt.disposition));
        hash.addI64(receipt.canonicalEventOrdinal);
        hash.addI64(receipt.canonicalEventCount);
        hash.addString(receipt.canonicalEventDigest);
        hash.addI64(static_cast<int>(receipt.exclusionReason));
        hash.addBool(receipt.complete);
    }
    return hash.finish();
}

bool CanonicalTableDepartureObserver::begin(
    const CanonicalTableDepartureObserverIdentity& identity)
{
    if (active_ || identity.shotIdentity.empty() ||
        identity.rackIdentity.empty() || identity.solverProfileId.empty() ||
        identity.boundaryMode != PhysicsBoundaryMode::ProductionTable ||
        identity.firstPhysicsTick == 0)
        return false;
    rollback();
    active_ = true;
    observation_.identity = identity;
    observation_.successfulPhysicsTicksContiguous = true;
    observation_.canonicalOrder = true;
    observation_.lastSuccessfulPhysicsTick = identity.firstPhysicsTick - 1;
    refreshObservationDigest(observation_);
    return true;
}

bool CanonicalTableDepartureObserver::consumeSuccessfulPhysicsTick(
    const CanonicalTableDepartureTick& tick)
{
    if (!active_ || tick.physicsTick == 0 ||
        tick.solverProfileId != observation_.identity.solverProfileId ||
        tick.boundaryMode != observation_.identity.boundaryMode ||
        !std::isfinite(tick.wholeTickDurationSeconds) ||
        tick.wholeTickDurationSeconds < 0.0)
        return false;
    const auto& attestation = tick.attestation;
    if (attestation.physicsTick != tick.physicsTick ||
        attestation.wholeTickDurationSeconds != tick.wholeTickDurationSeconds ||
        attestation.solverProfileId != tick.solverProfileId ||
        attestation.boundaryMode != tick.boundaryMode ||
        !privateSourceProjectionValid(tick) ||
        attestation.orderedEventDigest !=
            canonicalTableDepartureEventDigest(tick.events) ||
        !attestation.successfulStep || !attestation.canonicalOrder ||
        !attestation.noDroppedSamples || !attestation.complete)
        return false;

    const std::string digest = tickDigest(tick);
    const auto prior = successfulTickDigests_.find(tick.physicsTick);
    if (prior != successfulTickDigests_.end()) return prior->second == digest;
    if (tick.physicsTick != observation_.lastSuccessfulPhysicsTick + 1)
        return false;

    auto priorOrder = std::tuple<double, int, int, int>{
        -std::numeric_limits<double>::infinity(), -1, -1, -1};
    std::set<std::tuple<int, std::uint64_t, std::uint64_t, int, int>> keys;
    for (std::size_t index = 0; index < tick.events.size(); ++index) {
        const auto& event = tick.events[index];
        if (!eventShapeValid(event, tick) ||
            event.eventOrdinal != static_cast<int>(index))
            return false;
        const auto order = std::make_tuple(event.wholeTickTimeSeconds,
            event.eventOrdinal, event.ballId, event.solverEventId);
        if (order < priorOrder) return false;
        priorOrder = order;
        if (!keys.insert({event.ballId, event.trajectoryGeneration,
                event.physicsTick, event.eventOrdinal,
                event.solverEventId}).second)
            return false;
    }

    CanonicalTableDepartureShotObservation staged = observation_;
    auto stagedPendingPocketEntries = pendingPocketEntries_;
    for (const auto& receipt :
            tick.attestation.orderedSourceReceipts) {
        const BallGeneration key{receipt.ballId,
            receipt.trajectoryGeneration};
        if (receipt.disposition ==
                CanonicalTableDepartureProjectionDisposition::
                    ObservedPendingPocketEntry) {
            if (!stagedPendingPocketEntries.insert(key).second)
                return false;
        } else if (receipt.sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::CanonicalContact &&
            receipt.kind ==
                CanonicalTableDeparturePrivateEventKind::PocketCapture) {
            stagedPendingPocketEntries.erase(key);
        }
    }
    for (auto event : tick.events) {
        event.stableIdentity = stableIdentity(
            staged.identity.shotIdentity, event);
        staged.events.push_back(std::move(event));
    }
    if (!rebuildMeasurements(staged)) return false;
    staged.lastSuccessfulPhysicsTick = tick.physicsTick;
    staged.orderedSuccessfulTickDigests.push_back(digest);
    staged.canonicalOrder = true;
    staged.pendingPocketEntryCount = stagedPendingPocketEntries.size();
    refreshObservationDigest(staged);
    observation_ = std::move(staged);
    pendingPocketEntries_ = std::move(stagedPendingPocketEntries);
    successfulTickDigests_[tick.physicsTick] = digest;
    return true;
}

CanonicalTableDepartureObservationValidation
CanonicalTableDepartureObserver::finish(bool shotEndObserved)
{
    CanonicalTableDepartureObservationValidation result;
    if (!active_) {
        result.error = "table_departure_observer_not_active";
        return result;
    }
    observation_.shotEndObserved = shotEndObserved;
    const bool allComplete = std::all_of(observation_.balls.begin(),
        observation_.balls.end(), [](const auto& ball) {
            return ball.complete;
        });
    observation_.observationComplete = shotEndObserved &&
        observation_.successfulPhysicsTicksContiguous &&
        observation_.canonicalOrder && allComplete &&
        pendingPocketEntries_.empty();
    refreshObservationDigest(observation_);
    result.valid = true;
    result.complete = observation_.observationComplete;
    if (!result.complete) result.error = "table_departure_observation_incomplete";
    if (result.complete) {
        active_ = false;
        committed_ = true;
    }
    return result;
}

void CanonicalTableDepartureObserver::rollback()
{
    active_ = false;
    committed_ = false;
    successfulTickDigests_.clear();
    pendingPocketEntries_.clear();
    observation_ = CanonicalTableDepartureShotObservation{};
}

}  // namespace billiardgl::rules
