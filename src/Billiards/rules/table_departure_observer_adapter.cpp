#include "rules/table_departure_observer_adapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace billiardgl::rules {
namespace {

class AdapterHash {
public:
    void addU64(std::uint64_t value)
    {
        for (int shift = 0; shift < 64; shift += 8) {
            value_ ^= static_cast<unsigned char>((value >> shift) & 0xffu);
            value_ *= 1099511628211ull;
        }
    }

    void addI64(std::int64_t value)
    {
        addU64(static_cast<std::uint64_t>(value));
    }

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

    std::string finish() const
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::hex << std::setfill('0') << std::setw(16) << value_;
        return stream.str();
    }

private:
    std::uint64_t value_ = 1469598103934665603ull;
};

bool finitePoint(const Point3& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z);
}

Point3 terminalVelocity(const SurfaceMotionAnalyticSegment& segment)
{
    const float duration = static_cast<float>(segment.durationSeconds);
    return {
        segment.startVelocityCmS.x +
            segment.constantAccelerationCmS2.x * duration,
        segment.startVelocityCmS.y +
            segment.constantAccelerationCmS2.y * duration,
        segment.startVelocityCmS.z +
            segment.constantAccelerationCmS2.z * duration,
    };
}

Point3 analyticPosition(const SurfaceMotionAnalyticSegment& segment,
    double absoluteTime)
{
    const double localTime = absoluteTime -
        segment.wholeTickStartOffsetSeconds;
    return {
        static_cast<float>(segment.startPositionCm.x +
            segment.startVelocityCmS.x * localTime +
            .5 * segment.constantAccelerationCmS2.x * localTime * localTime),
        static_cast<float>(segment.startPositionCm.y +
            segment.startVelocityCmS.y * localTime +
            .5 * segment.constantAccelerationCmS2.y * localTime * localTime),
        static_cast<float>(segment.startPositionCm.z +
            segment.startVelocityCmS.z * localTime +
            .5 * segment.constantAccelerationCmS2.z * localTime * localTime),
    };
}

void hashPoint(AdapterHash& hash, const Point3& point)
{
    hash.addFloat(point.x);
    hash.addFloat(point.y);
    hash.addFloat(point.z);
}

bool closePoint(const Point3& first, const Point3& second, double tolerance)
{
    return std::fabs(static_cast<double>(first.x) - second.x) <= tolerance &&
        std::fabs(static_cast<double>(first.y) - second.y) <= tolerance &&
        std::fabs(static_cast<double>(first.z) - second.z) <= tolerance;
}

bool analyticPartitionCompleteForBall(
    const TableDepartureStepResult& producer, const GameState& before,
    const GameState& after, int ballId, double duration,
    const PhysicsProfile& profile,
    const std::vector<TableDepartureTrajectoryGenerationTransition>&
        generationTransitions,
    const std::vector<TableDeparturePositionProjectionTransition>&
        positionProjections,
    std::string* failureDetail = nullptr)
{
    const auto fail = [failureDetail](const std::string& detail) {
        if (failureDetail) *failureDetail = detail;
        return false;
    };
    std::vector<const SurfaceMotionAnalyticSegment*> segments;
    for (const auto& segment : producer.analyticSegments) {
        if (segment.ballIndex == ballId) segments.push_back(&segment);
    }
    if (segments.empty()) return fail("empty");
    std::sort(segments.begin(), segments.end(), [](const auto* first,
            const auto* second) {
        if (first->wholeTickStartOffsetSeconds !=
                second->wholeTickStartOffsetSeconds)
            return first->wholeTickStartOffsetSeconds <
                second->wholeTickStartOffsetSeconds;
        return first->segmentOrdinal < second->segmentOrdinal;
    });
    std::vector<const TableDepartureTrajectoryGenerationTransition*>
        transitions;
    for (const auto& transition : generationTransitions) {
        if (transition.ballId == ballId) transitions.push_back(&transition);
    }
    std::sort(transitions.begin(), transitions.end(), [](const auto* first,
            const auto* second) {
        if (first->wholeTickTimeSeconds != second->wholeTickTimeSeconds)
            return first->wholeTickTimeSeconds < second->wholeTickTimeSeconds;
        return first->generationAfter < second->generationAfter;
    });
    std::vector<const TableDeparturePositionProjectionTransition*> projections;
    for (const auto& projection : positionProjections) {
        if (projection.ballId == ballId) projections.push_back(&projection);
    }
    std::vector<bool> consumedProjections(projections.size(), false);
    std::vector<bool> consumedTransitions(transitions.size(), false);
    double covered = 0.0;
    Point3 position = before.balls[static_cast<std::size_t>(ballId)].position;
    std::uint64_t generation = before.balls[
        static_cast<std::size_t>(ballId)].trajectoryGeneration;
    const auto applyProjections = [&](double time) {
        for (std::size_t index = 0; index < projections.size(); ++index) {
            if (consumedProjections[index]) continue;
            const auto& projection = *projections[index];
            if (std::fabs(projection.wholeTickTimeSeconds - time) >
                    profile.solver.toiToleranceSeconds ||
                projection.trajectoryGeneration != generation)
                continue;
            if (!projection.complete || projection.solverEventId < 0 ||
                projection.solverIslandId < 0 ||
                projection.projectionOrdinal < 0 ||
                projection.solverIteration < 0 ||
                projection.solverContactIndex < 0 ||
                !finitePoint(projection.beforeCm) ||
                !finitePoint(projection.afterCm) ||
                projection.beforeCm.x != position.x ||
                projection.beforeCm.y != position.y ||
                projection.beforeCm.z != position.z)
                return false;
            position = projection.afterCm;
            consumedProjections[index] = true;
        }
        return true;
    };
    const auto applyTransitions = [&](double time,
                                      std::uint64_t targetGeneration) {
        if (targetGeneration < generation) return false;
        while (generation < targetGeneration) {
            std::size_t match = transitions.size();
            for (std::size_t index = 0; index < transitions.size(); ++index) {
                if (consumedTransitions[index]) continue;
                const auto& transition = *transitions[index];
                if (transition.generationBefore == generation &&
                    transition.wholeTickTimeSeconds == time) {
                    if (match != transitions.size()) return false;
                    match = index;
                }
            }
            if (match == transitions.size()) return false;
            const auto& transition = *transitions[match];
            if (!transition.complete || transition.physicsContactOrdinal < 0 ||
                generation == std::numeric_limits<std::uint64_t>::max() ||
                transition.generationAfter != generation + 1 ||
                !finitePoint(transition.resetPositionCm))
                return false;
            generation = transition.generationAfter;
            position = transition.resetPositionCm;
            consumedTransitions[match] = true;
            if (!applyProjections(time)) return false;
        }
        return true;
    };
    int priorOrdinal = -1;
    for (const auto* segmentPointer : segments) {
        const auto& segment = *segmentPointer;
        if (segment.segmentOrdinal < 0 ||
            segment.segmentOrdinal <= priorOrdinal)
            return fail("ordinal");
        if (!segment.analyticPathComplete)
            return fail("path_incomplete");
        if (!std::isfinite(segment.wholeTickStartOffsetSeconds) ||
            !std::isfinite(segment.durationSeconds) ||
            !(segment.durationSeconds > 0.0))
            return fail("time_invalid");
        if (std::fabs(segment.wholeTickStartOffsetSeconds - covered) >
                profile.solver.toiToleranceSeconds)
            return fail("time_gap");
        if (!applyProjections(segment.wholeTickStartOffsetSeconds))
            return fail("position_projection_invalid");
        if (!applyTransitions(segment.wholeTickStartOffsetSeconds,
                segment.trajectoryGeneration))
            return fail("generation_transition_invalid");
        if (!applyProjections(segment.wholeTickStartOffsetSeconds))
            return fail("post_reset_position_projection_invalid");
        if (!finitePoint(segment.startPositionCm) ||
            !finitePoint(segment.endPositionCm) ||
            !finitePoint(segment.startVelocityCmS) ||
            !finitePoint(segment.constantAccelerationCmS2))
            return fail("nonfinite");
        if (!closePoint(position, segment.startPositionCm,
                profile.solver.penetrationSlopCm)) {
            std::ostringstream detail;
            detail.imbue(std::locale::classic());
            detail << "position_gap:ordinal=" << segment.segmentOrdinal
                   << ":generation=" << segment.trajectoryGeneration
                   << ":start=" << std::setprecision(17)
                   << segment.wholeTickStartOffsetSeconds << ":expected="
                   << position.x << ',' << position.y << ',' << position.z
                   << ":actual=" << segment.startPositionCm.x << ','
                   << segment.startPositionCm.y << ','
                   << segment.startPositionCm.z;
            return fail(detail.str());
        }
        priorOrdinal = segment.segmentOrdinal;
        const double t = segment.durationSeconds;
        const Point3 integrated{
            static_cast<float>(segment.startPositionCm.x +
                segment.startVelocityCmS.x * t +
                .5 * segment.constantAccelerationCmS2.x * t * t),
            static_cast<float>(segment.startPositionCm.y +
                segment.startVelocityCmS.y * t +
                .5 * segment.constantAccelerationCmS2.y * t * t),
            static_cast<float>(segment.startPositionCm.z +
                segment.startVelocityCmS.z * t +
                .5 * segment.constantAccelerationCmS2.z * t * t),
        };
        if (!closePoint(integrated, segment.endPositionCm,
                profile.solver.penetrationSlopCm))
            return fail("integration_mismatch");
        covered = segment.wholeTickStartOffsetSeconds + t;
        position = segment.endPositionCm;
    }
    const auto& finalBall = after.balls[static_cast<std::size_t>(ballId)];
    if (!applyProjections(covered))
        return fail("terminal_position_projection_invalid");
    if (!applyTransitions(covered, finalBall.trajectoryGeneration))
        return fail("terminal_generation_transition_invalid");
    if (!applyProjections(covered))
        return fail("terminal_post_reset_projection_invalid");
    if (std::find(consumedTransitions.begin(), consumedTransitions.end(),
            false) != consumedTransitions.end())
        return fail("generation_transition_unconsumed");
    if (std::find(consumedProjections.begin(), consumedProjections.end(),
            false) != consumedProjections.end())
        return fail("position_projection_unconsumed");
    if (finalBall.physicalDomain == PhysicalBallDomain::PocketChannel)
        return true;
    if (std::fabs(covered - duration) >
            profile.solver.toiToleranceSeconds)
        return fail("terminal_time");
    if (!closePoint(position, finalBall.position,
            profile.solver.penetrationSlopCm))
        return fail("terminal_position");
    if (generation != finalBall.trajectoryGeneration)
        return fail("terminal_generation");
    if (!closePoint(terminalVelocity(*segments.back()), finalBall.velocity,
            profile.solver.residualToleranceCmS))
        return fail("terminal_velocity");
    return true;
}

std::string analyticPartitionDigest(const TableDepartureStepResult& producer,
    const GameState& before, const GameState& after, int ballId,
    double duration)
{
    AdapterHash hash;
    hash.addI64(ballId);
    hash.addDouble(duration);
    const auto& first = before.balls[static_cast<std::size_t>(ballId)];
    const auto& last = after.balls[static_cast<std::size_t>(ballId)];
    hashPoint(hash, first.position);
    hashPoint(hash, first.velocity);
    hash.addI64(static_cast<int>(first.physicalDomain));
    std::size_t count = 0;
    for (const auto& segment : producer.analyticSegments)
        if (segment.ballIndex == ballId) ++count;
    hash.addU64(count);
    for (const auto& segment : producer.analyticSegments) {
        if (segment.ballIndex != ballId) continue;
        hash.addI64(segment.segmentOrdinal);
        hash.addU64(segment.trajectoryGeneration);
        hash.addI64(static_cast<int>(segment.motionState));
        hash.addDouble(segment.wholeTickStartOffsetSeconds);
        hash.addDouble(segment.durationSeconds);
        hashPoint(hash, segment.startPositionCm);
        hashPoint(hash, segment.endPositionCm);
        hashPoint(hash, segment.startVelocityCmS);
        hashPoint(hash, segment.constantAccelerationCmS2);
        hash.addU64(segment.analyticPathComplete ? 1u : 0u);
    }
    hashPoint(hash, last.position);
    hashPoint(hash, last.velocity);
    hash.addI64(static_cast<int>(last.physicalDomain));
    return hash.finish();
}

const SurfaceMotionAnalyticSegment* precedingSegment(
    const TableDepartureStepResult& producer, int ballId,
    std::uint64_t trajectoryGeneration, double time, double tolerance)
{
    const SurfaceMotionAnalyticSegment* containingMatch = nullptr;
    const SurfaceMotionAnalyticSegment* exactMatch = nullptr;
    const SurfaceMotionAnalyticSegment* toleranceMatch = nullptr;
    bool exactAmbiguous = false;
    bool toleranceAmbiguous = false;
    for (const auto& segment : producer.analyticSegments) {
        if (segment.ballIndex != ballId ||
            segment.trajectoryGeneration != trajectoryGeneration ||
            !segment.analyticPathComplete)
            continue;
        const double start = segment.wholeTickStartOffsetSeconds;
        const double end = segment.wholeTickStartOffsetSeconds +
            segment.durationSeconds;
        // Analytic partitions own (start,end]. A transition at a shared
        // boundary therefore belongs to the segment that reached it, not the
        // next segment. This remains unambiguous when float-derived adjacent
        // endpoints are all within the broader TOI grouping tolerance.
        if (start < time && time <= end) {
            if (containingMatch) return nullptr;
            containingMatch = &segment;
        }
        if (std::fabs(end - time) > tolerance) continue;
        if (end == time) {
            if (exactMatch) exactAmbiguous = true;
            else exactMatch = &segment;
        } else {
            if (toleranceMatch) toleranceAmbiguous = true;
            else toleranceMatch = &segment;
        }
    }
    if (containingMatch) return containingMatch;
    if (exactAmbiguous) return nullptr;
    if (exactMatch) return exactMatch;
    return toleranceAmbiguous ? nullptr : toleranceMatch;
}

CanonicalTableDeparturePrivateEventKind privateKind(
    TableDepartureEventKind kind)
{
    switch (kind) {
    case TableDepartureEventKind::CushionNoseContact:
        return CanonicalTableDeparturePrivateEventKind::NoseContactDiagnostic;
    case TableDepartureEventKind::OuterDeparture:
        return CanonicalTableDeparturePrivateEventKind::OuterDeparture;
    case TableDepartureEventKind::RailTopLanding:
        return CanonicalTableDeparturePrivateEventKind::RailTopLanding;
    case TableDepartureEventKind::OutsideLanding:
        return CanonicalTableDeparturePrivateEventKind::OutsideLanding;
    case TableDepartureEventKind::ClothReturn:
        return CanonicalTableDeparturePrivateEventKind::ClothReturn;
    case TableDepartureEventKind::PocketEntry:
        return CanonicalTableDeparturePrivateEventKind::PocketCapture;
    case TableDepartureEventKind::CrossDomainBallContact:
        return CanonicalTableDeparturePrivateEventKind::CrossContactDiagnostic;
    }
    return CanonicalTableDeparturePrivateEventKind::CrossContactDiagnostic;
}

bool diagnosticKind(TableDepartureEventKind kind)
{
    return kind == TableDepartureEventKind::CushionNoseContact ||
        kind == TableDepartureEventKind::CrossDomainBallContact;
}

bool bindingShapeValid(const TableDepartureEvent& source,
    const TableDepartureProducerEventBinding& binding, std::size_t index)
{
    if (binding.sourceOrdinal != static_cast<int>(index) ||
        binding.sourceIdentity != source.stableIdentity ||
        binding.canonicalSolverEventId < -1 || !binding.complete)
        return false;
    if (source.kind != TableDepartureEventKind::OuterDeparture &&
        binding.boundaryFeatureId != -1)
        return false;
    if (source.kind != TableDepartureEventKind::RailTopLanding &&
        source.kind != TableDepartureEventKind::ClothReturn &&
        binding.railFeatureId != -1)
        return false;
    if (source.kind != TableDepartureEventKind::OutsideLanding &&
        binding.landingSurfaceId != -1)
        return false;
    if (source.kind != TableDepartureEventKind::PocketEntry &&
        (binding.pocketId != -1 || binding.canonicalContactOrdinal != -1))
        return false;
    if (binding.physicsContactOrdinal != -1 &&
        source.kind != TableDepartureEventKind::PocketEntry &&
        source.kind != TableDepartureEventKind::RailTopLanding &&
        source.kind != TableDepartureEventKind::ClothReturn &&
        source.kind != TableDepartureEventKind::OutsideLanding &&
        source.kind != TableDepartureEventKind::CushionNoseContact)
        return false;
    const bool supportedLanding =
        source.kind == TableDepartureEventKind::RailTopLanding ||
        source.kind == TableDepartureEventKind::ClothReturn ||
        source.kind == TableDepartureEventKind::OutsideLanding;
    if ((binding.transitionPositionComplete &&
            (!supportedLanding || !finitePoint(binding.transitionPositionCm))) ||
        (supportedLanding && binding.physicsContactOrdinal >= 0 &&
            !binding.transitionPositionComplete))
        return false;
    if (source.kind != TableDepartureEventKind::ClothReturn &&
        binding.clothReturnProvenance !=
            TableDepartureClothReturnProvenance::NotApplicable)
        return false;
    return true;
}

std::string pendingRestIdentity(std::uint64_t tick, int ballId,
    int segmentOrdinal, double time)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "analytic_outside_rest:" << tick << ':' << ballId << ':'
           << segmentOrdinal << ':' << time;
    return stream.str();
}

}  // namespace

TableDepartureObserverAdapterResult adaptTableDepartureProducerTick(
    const TableDepartureStepResult& producer,
    const GameState& before,
    const GameState& after,
    const PhysicsProfile& profile,
    const TableDepartureObserverAdapterContext& context)
{
    TableDepartureObserverAdapterResult result;
    if (!producer.ok || producer.failureCode != TableDepartureFailureCode::None ||
        !context.successfulStep || !context.complete ||
        context.physicsTick == 0 || context.solverProfileId.empty() ||
        context.solverProfileId != profile.id ||
        context.boundaryMode != PhysicsBoundaryMode::ProductionTable ||
        !std::isfinite(context.wholeTickDurationSeconds) ||
        context.wholeTickDurationSeconds < 0.0 ||
        context.orderedEventBindings.size() != producer.events.size()) {
        result.error = "table_departure_adapter_context_incomplete";
        return result;
    }

    CanonicalTableDepartureTick stagedTick;
    stagedTick.physicsTick = context.physicsTick;
    stagedTick.wholeTickDurationSeconds = context.wholeTickDurationSeconds;
    stagedTick.solverProfileId = context.solverProfileId;
    stagedTick.boundaryMode = context.boundaryMode;
    std::set<std::string> sourceIdentities;
    std::set<int> validatedPartitions;
    auto outsideLandingSurfaceIds =
        context.activeOutsideLandingSurfaceIds;
    double priorTime = -std::numeric_limits<double>::infinity();
    const double tolerance = profile.solver.toiToleranceSeconds;

    for (std::size_t index = 0; index < producer.events.size(); ++index) {
        const auto& source = producer.events[index];
        const auto& binding = context.orderedEventBindings[index];
        if (source.scope != TableDepartureEventScope::PhysicsPrivateDiagnostic ||
            source.ordinal != static_cast<int>(index) ||
            source.ballIndex < 0 || source.ballIndex >= kBallCount ||
            before.balls[static_cast<std::size_t>(source.ballIndex)].pocketed ||
            before.balls[static_cast<std::size_t>(source.ballIndex)].
                physicalDomain == PhysicalBallDomain::PocketChannel ||
            !source.transitionWitnessComplete || source.stableIdentity.empty() ||
            !sourceIdentities.insert(source.stableIdentity).second ||
            !std::isfinite(source.absoluteWholeTickTimeSeconds) ||
            source.absoluteWholeTickTimeSeconds < 0.0 ||
            source.absoluteWholeTickTimeSeconds >
                context.wholeTickDurationSeconds ||
            source.absoluteWholeTickTimeSeconds < priorTime ||
            source.trajectoryGeneration != context.trajectoryGenerations[
                static_cast<std::size_t>(source.ballIndex)] ||
            !finitePoint(source.velocityAfterCmS) ||
            (source.velocityBeforeWitnessComplete &&
                !finitePoint(source.velocityBeforeCmS)) ||
            !bindingShapeValid(source, binding, index)) {
            result.error = "table_departure_adapter_source_invalid";
            return result;
        }
        priorTime = source.absoluteWholeTickTimeSeconds;
        std::string partitionFailure;
        if (validatedPartitions.insert(source.ballIndex).second &&
            !analyticPartitionCompleteForBall(producer, before, after,
                source.ballIndex, context.wholeTickDurationSeconds, profile,
                context.trajectoryGenerationTransitions,
                context.positionProjectionTransitions,
                &partitionFailure)) {
            result.error = "table_departure_adapter_partition_incomplete:ball=" +
                std::to_string(source.ballIndex) + ":reason=" +
                partitionFailure;
            return result;
        }

        CanonicalTableDepartureSourceReceipt receipt;
        receipt.sourceOrdinal = static_cast<int>(index);
        receipt.sourceOrigin =
            CanonicalTableDepartureSourceOrigin::PrivateEvent;
        receipt.privateEventOrdinal = static_cast<int>(index);
        receipt.sourceIdentity = source.stableIdentity;
        receipt.kind = privateKind(source.kind);
        receipt.ballId = source.ballIndex;
        receipt.solverEventId = binding.canonicalSolverEventId;
        receipt.physicsContactOrdinal = binding.physicsContactOrdinal;
        receipt.trajectoryGeneration = context.trajectoryGenerations[
            static_cast<std::size_t>(source.ballIndex)];
        receipt.wholeTickTimeSeconds = source.absoluteWholeTickTimeSeconds;
        receipt.complete = true;

        if (diagnosticKind(source.kind)) {
            receipt.disposition =
                CanonicalTableDepartureProjectionDisposition::
                    ExcludedDiagnostic;
            receipt.exclusionReason = source.kind ==
                    TableDepartureEventKind::CushionNoseContact ?
                CanonicalTableDepartureExclusionReason::
                    NoseContactNotTableDepartureMeasurement :
                CanonicalTableDepartureExclusionReason::
                    CrossContactNotTableDepartureMeasurement;
            stagedTick.attestation.orderedSourceReceipts.push_back(
                std::move(receipt));
            continue;
        }
        const bool supportTransition =
            source.kind == TableDepartureEventKind::RailTopLanding ||
            source.kind == TableDepartureEventKind::ClothReturn ||
            source.kind == TableDepartureEventKind::OutsideLanding;
        const bool supportIdentityComplete =
            (source.kind == TableDepartureEventKind::RailTopLanding &&
                binding.railFeatureId >= 0) ||
            (source.kind == TableDepartureEventKind::OutsideLanding &&
                binding.landingSurfaceId >= 0) ||
            (source.kind == TableDepartureEventKind::ClothReturn &&
                binding.clothReturnProvenance !=
                    TableDepartureClothReturnProvenance::NotApplicable &&
                ((binding.clothReturnProvenance ==
                        TableDepartureClothReturnProvenance::
                            DirectAirborneLanding &&
                    binding.railFeatureId == -1) ||
                 (binding.clothReturnProvenance ==
                        TableDepartureClothReturnProvenance::RailTopLanding &&
                    binding.railFeatureId >= 0)));
        if (supportTransition &&
            supportIdentityComplete &&
            source.sourceDomain == PhysicalBallDomain::Airborne &&
            source.targetDomain == PhysicalBallDomain::Airborne &&
            source.velocityAfterCmS.y > 0.0f &&
            binding.physicsContactOrdinal >= 0 &&
            binding.canonicalSolverEventId >= 0) {
            receipt.disposition =
                CanonicalTableDepartureProjectionDisposition::
                    ObservedNonSettlingSupport;
            receipt.exclusionReason =
                CanonicalTableDepartureExclusionReason::
                    SupportContactImmediateRelease;
            receipt.sourceDomain = source.sourceDomain;
            receipt.targetDomain = source.targetDomain;
            receipt.transitionPositionCm = binding.transitionPositionCm;
            receipt.velocityBeforeCmS =
                source.velocityBeforeWitnessComplete ?
                    source.velocityBeforeCmS : Point3{};
            receipt.velocityAfterCmS = source.velocityAfterCmS;
            stagedTick.attestation.orderedSourceReceipts.push_back(
                std::move(receipt));
            continue;
        }
        if (source.kind == TableDepartureEventKind::PocketEntry) {
            result.error =
                "table_departure_pocket_entry_is_not_capture_evidence";
            return result;
        }

        const auto* preceding = precedingSegment(producer, source.ballIndex,
            source.trajectoryGeneration,
            source.absoluteWholeTickTimeSeconds, tolerance);
        const bool nativeZeroDurationSupportWitness = supportTransition &&
            binding.transitionPositionComplete &&
            binding.physicsContactOrdinal >= 0 &&
            binding.canonicalSolverEventId >= 0 &&
            source.velocityBeforeWitnessComplete &&
            finitePoint(source.velocityBeforeCmS);
        if ((!preceding && !nativeZeroDurationSupportWitness) ||
            (preceding && (!finitePoint(preceding->endPositionCm) ||
                !finitePoint(preceding->startVelocityCmS) ||
                !finitePoint(preceding->constantAccelerationCmS2)))) {
            std::ostringstream detail;
            detail << "table_departure_adapter_event_witness_incomplete:ball="
                   << source.ballIndex << ":time="
                   << source.absoluteWholeTickTimeSeconds;
            result.error = detail.str();
            return result;
        }
        const Point3 analyticTransitionPosition = preceding ?
            analyticPosition(*preceding,
                source.absoluteWholeTickTimeSeconds) :
            binding.transitionPositionCm;
        if (!finitePoint(analyticTransitionPosition) ||
            (preceding && binding.transitionPositionComplete &&
             (!finitePoint(binding.transitionPositionCm) ||
              !closePoint(analyticTransitionPosition,
                  binding.transitionPositionCm,
                  profile.solver.penetrationSlopCm)))) {
            result.error =
                "table_departure_adapter_contact_position_mismatch";
            return result;
        }

        CanonicalTableDepartureEvent event;
        event.ballId = source.ballIndex;
        event.physicsTick = context.physicsTick;
        event.eventOrdinal = static_cast<int>(stagedTick.events.size());
        event.solverEventId = binding.canonicalSolverEventId;
        event.trajectoryGeneration = receipt.trajectoryGeneration;
        event.wholeTickTimeSeconds = source.absoluteWholeTickTimeSeconds;
        event.sourceDomain = source.sourceDomain;
        event.targetDomain = source.targetDomain;
        event.positionCm = binding.transitionPositionComplete ?
            binding.transitionPositionCm : analyticTransitionPosition;
        event.velocityBeforeCmS = source.velocityBeforeWitnessComplete ?
            source.velocityBeforeCmS : terminalVelocity(*preceding);
        event.velocityAfterCmS = source.velocityAfterCmS;
        event.ballRadiusCm = profile.ball.radiusCm;
        event.origin = "AUTHORITATIVE_CONTINUOUS_TABLE_DEPARTURE_SOLVER_V1";
        event.complete = true;
        event.stableIdentity = source.stableIdentity;

        switch (source.kind) {
        case TableDepartureEventKind::OuterDeparture:
            if (source.targetDomain != PhysicalBallDomain::Airborne ||
                binding.boundaryFeatureId < 0) {
                result.error = "table_departure_boundary_binding_incomplete";
                return result;
            }
            event.kind = CanonicalTableDepartureEventKind::OuterDeparture;
            event.boundaryFeatureId = binding.boundaryFeatureId;
            break;
        case TableDepartureEventKind::RailTopLanding:
            if (source.targetDomain != PhysicalBallDomain::RailTopSupported ||
                binding.railFeatureId < 0 ||
                source.velocityAfterCmS.y != 0.0f) {
                result.error = "table_departure_rail_binding_incomplete";
                return result;
            }
            event.kind = CanonicalTableDepartureEventKind::RailTopLanding;
            event.railFeatureId = binding.railFeatureId;
            break;
        case TableDepartureEventKind::ClothReturn:
            if (source.targetDomain != PhysicalBallDomain::ClothSupported ||
                source.velocityAfterCmS.y != 0.0f ||
                binding.clothReturnProvenance ==
                    TableDepartureClothReturnProvenance::NotApplicable ||
                (binding.clothReturnProvenance ==
                        TableDepartureClothReturnProvenance::
                            DirectAirborneLanding &&
                    binding.railFeatureId != -1) ||
                (binding.clothReturnProvenance ==
                        TableDepartureClothReturnProvenance::RailTopLanding &&
                    binding.railFeatureId < 0)) {
                std::ostringstream detail;
                detail << "table_departure_cloth_provenance_incomplete:ball="
                       << source.ballIndex << ":source="
                       << static_cast<int>(source.sourceDomain) << ":target="
                       << static_cast<int>(source.targetDomain) << ":vy="
                       << source.velocityAfterCmS.y << ":provenance="
                       << static_cast<int>(binding.clothReturnProvenance)
                       << ":rail=" << binding.railFeatureId;
                result.error = detail.str();
                return result;
            }
            event.kind = CanonicalTableDepartureEventKind::ClothReturn;
            event.railFeatureId = binding.railFeatureId;
            break;
        case TableDepartureEventKind::OutsideLanding:
            if (source.targetDomain !=
                    PhysicalBallDomain::OutsideLandingSupported ||
                binding.landingSurfaceId < 0 ||
                source.velocityAfterCmS.y != 0.0f) {
                result.error = "table_departure_outside_binding_incomplete";
                return result;
            }
            event.kind = CanonicalTableDepartureEventKind::OutsideLanding;
            event.landingSurfaceId = binding.landingSurfaceId;
            outsideLandingSurfaceIds[static_cast<std::size_t>(
                source.ballIndex)] = binding.landingSurfaceId;
            break;
        case TableDepartureEventKind::CushionNoseContact:
        case TableDepartureEventKind::PocketEntry:
        case TableDepartureEventKind::CrossDomainBallContact:
            result.error = "table_departure_adapter_internal_mapping_error";
            return result;
        }

        stagedTick.events.push_back(event);
        receipt.transitionPositionCm = event.positionCm;
        receipt.velocityBeforeCmS = event.velocityBeforeCmS;
        receipt.velocityAfterCmS = event.velocityAfterCmS;
        receipt.sourceDomain = event.sourceDomain;
        receipt.targetDomain = event.targetDomain;
        receipt.canonicalEventOrdinal = event.eventOrdinal;
        receipt.canonicalEventCount = 1;
        receipt.canonicalEventDigest =
            canonicalTableDepartureEventDigest({event});
        stagedTick.attestation.orderedSourceReceipts.push_back(
            std::move(receipt));
    }

    std::size_t analyticTransitionCount = 0;
    for (const auto& segment : producer.analyticSegments) {
        if (segment.ballIndex < 0 || segment.ballIndex >= kBallCount ||
            segment.motionState != BallMotionState::Stationary)
            continue;
        const std::size_t ball = static_cast<std::size_t>(segment.ballIndex);
        if (after.balls[ball].physicalDomain !=
                PhysicalBallDomain::OutsideLandingSupported)
            continue;
        const SurfaceMotionAnalyticSegment* movingPredecessor = nullptr;
        for (const auto& candidate : producer.analyticSegments) {
            if (candidate.ballIndex != segment.ballIndex ||
                &candidate == &segment ||
                std::fabs(candidate.wholeTickStartOffsetSeconds +
                        candidate.durationSeconds -
                    segment.wholeTickStartOffsetSeconds) > tolerance)
                continue;
            if (movingPredecessor) {
                result.error =
                    "table_departure_analytic_rest_witness_ambiguous";
                return result;
            }
            movingPredecessor = &candidate;
        }
        const TableDepartureEvent* landingAtRest = nullptr;
        for (const auto& source : producer.events) {
            if (source.ballIndex == segment.ballIndex &&
                source.kind == TableDepartureEventKind::OutsideLanding &&
                std::fabs(source.absoluteWholeTickTimeSeconds -
                    segment.wholeTickStartOffsetSeconds) <= tolerance) {
                if (landingAtRest) {
                    result.error =
                        "table_departure_analytic_rest_landing_ambiguous";
                    return result;
                }
                landingAtRest = &source;
            }
        }
        if (!movingPredecessor) {
            if (segment.wholeTickStartOffsetSeconds > tolerance) {
                result.error =
                    "table_departure_analytic_rest_predecessor_missing";
                return result;
            }
            continue;
        }
        if (movingPredecessor->motionState != BallMotionState::Rolling &&
            (!landingAtRest ||
                landingAtRest->targetDomain !=
                    PhysicalBallDomain::OutsideLandingSupported ||
                landingAtRest->velocityAfterCmS.x != 0.0f ||
                landingAtRest->velocityAfterCmS.y != 0.0f ||
                landingAtRest->velocityAfterCmS.z != 0.0f)) {
            result.error =
                "table_departure_analytic_rest_predecessor_not_rolling";
            return result;
        }
        if (!segment.analyticPathComplete ||
            !(segment.durationSeconds > 0.0) ||
            !finitePoint(segment.startPositionCm) ||
            !finitePoint(segment.endPositionCm) ||
            !finitePoint(segment.startVelocityCmS) ||
            !finitePoint(segment.constantAccelerationCmS2) ||
            segment.startVelocityCmS.x != 0.0f ||
            segment.startVelocityCmS.y != 0.0f ||
            segment.startVelocityCmS.z != 0.0f ||
            segment.constantAccelerationCmS2.x != 0.0f ||
            segment.constantAccelerationCmS2.y != 0.0f ||
            segment.constantAccelerationCmS2.z != 0.0f) {
            result.error =
                "table_departure_analytic_rest_source_state_incomplete";
            return result;
        }
        if ((before.balls[ball].physicalDomain !=
                    PhysicalBallDomain::OutsideLandingSupported &&
                !landingAtRest) ||
            !analyticPartitionCompleteForBall(producer, before, after,
                segment.ballIndex, context.wholeTickDurationSeconds, profile,
                context.trajectoryGenerationTransitions,
                context.positionProjectionTransitions)) {
            result.error =
                "table_departure_analytic_rest_partition_incomplete";
            return result;
        }
        if (outsideLandingSurfaceIds[ball] < 0) {
            result.error =
                "table_departure_analytic_rest_surface_binding_incomplete";
            return result;
        }
        const Point3 movingTerminal = terminalVelocity(*movingPredecessor);
        if (!closePoint(movingPredecessor->endPositionCm,
                segment.startPositionCm,
                profile.solver.penetrationSlopCm) ||
            (movingPredecessor->motionState == BallMotionState::Rolling &&
                std::sqrt(static_cast<double>(movingTerminal.x) *
                        movingTerminal.x +
                    static_cast<double>(movingTerminal.y) * movingTerminal.y +
                    static_cast<double>(movingTerminal.z) * movingTerminal.z) >
                        profile.solver.residualToleranceCmS)) {
            result.error =
                "table_departure_analytic_rest_transition_incomplete";
            return result;
        }
        if (!finitePoint(after.balls[ball].position) ||
            !finitePoint(after.balls[ball].velocity) ||
            after.balls[ball].velocity.x != 0.0f ||
            after.balls[ball].velocity.y != 0.0f ||
            after.balls[ball].velocity.z != 0.0f ||
            after.balls[ball].speed != 0.0f ||
            std::fabs(after.balls[ball].position.x -
                segment.endPositionCm.x) >
                    profile.solver.penetrationSlopCm ||
            std::fabs(after.balls[ball].position.y -
                segment.endPositionCm.y) >
                    profile.solver.penetrationSlopCm ||
            std::fabs(after.balls[ball].position.z -
                segment.endPositionCm.z) >
                    profile.solver.penetrationSlopCm) {
            result.error =
                "table_departure_analytic_rest_terminal_state_incomplete";
            return result;
        }
        CanonicalTableDepartureEvent rest;
        rest.kind = CanonicalTableDepartureEventKind::OutsideRest;
        rest.ballId = segment.ballIndex;
        rest.physicsTick = context.physicsTick;
        rest.solverEventId = -1;
        rest.trajectoryGeneration = context.trajectoryGenerations[ball];
        rest.wholeTickTimeSeconds = segment.wholeTickStartOffsetSeconds;
        rest.sourceDomain = PhysicalBallDomain::OutsideLandingSupported;
        rest.targetDomain = PhysicalBallDomain::OutsideLandingSupported;
        rest.positionCm = segment.startPositionCm;
        // The producer snaps the supported stop state to exact zero before
        // emitting the following stationary segment. The independently
        // integrated rolling endpoint above must agree within the frozen
        // residual tolerance; publish the exact source-side snapped state.
        rest.velocityBeforeCmS = segment.startVelocityCmS;
        rest.velocityAfterCmS = {};
        rest.ballRadiusCm = profile.ball.radiusCm;
        rest.landingSurfaceId = outsideLandingSurfaceIds[ball];
        rest.origin = "AUTHORITATIVE_CONTINUOUS_TABLE_DEPARTURE_SOLVER_V1";
        rest.complete = true;
        rest.stableIdentity = pendingRestIdentity(context.physicsTick,
            rest.ballId, segment.segmentOrdinal,
            rest.wholeTickTimeSeconds);
        stagedTick.events.push_back(rest);

        CanonicalTableDepartureSourceReceipt receipt;
        receipt.sourceOrigin =
            CanonicalTableDepartureSourceOrigin::AnalyticTransition;
        receipt.precedingSegmentOrdinal =
            movingPredecessor->segmentOrdinal;
        receipt.sourceSegmentOrdinal = segment.segmentOrdinal;
        receipt.sourceIdentity = rest.stableIdentity;
        receipt.kind =
            CanonicalTableDeparturePrivateEventKind::OutsideRest;
        receipt.ballId = rest.ballId;
        receipt.solverEventId = -1;
        receipt.trajectoryGeneration = rest.trajectoryGeneration;
        receipt.wholeTickTimeSeconds = rest.wholeTickTimeSeconds;
        receipt.transitionPositionCm = rest.positionCm;
        receipt.velocityBeforeCmS = rest.velocityBeforeCmS;
        receipt.velocityAfterCmS = rest.velocityAfterCmS;
        receipt.sourceDomain = rest.sourceDomain;
        receipt.targetDomain = rest.targetDomain;
        receipt.sourceAnalyticState = movingPredecessor->motionState ==
                BallMotionState::Rolling ?
            CanonicalTableDepartureAnalyticState::OutsideRolling :
            CanonicalTableDepartureAnalyticState::OutsideLandingPostState;
        receipt.targetAnalyticState =
            CanonicalTableDepartureAnalyticState::OutsideStationary;
        receipt.analyticPartitionDigest = analyticPartitionDigest(
            producer, before, after, rest.ballId,
            context.wholeTickDurationSeconds);
        receipt.disposition =
            CanonicalTableDepartureProjectionDisposition::EmittedCanonical;
        receipt.canonicalEventCount = 1;
        receipt.complete = true;
        stagedTick.attestation.orderedSourceReceipts.push_back(receipt);
        ++analyticTransitionCount;
    }

    auto& receipts = stagedTick.attestation.orderedSourceReceipts;
    std::sort(receipts.begin(), receipts.end(), [](const auto& first,
            const auto& second) {
        if (first.wholeTickTimeSeconds != second.wholeTickTimeSeconds)
            return first.wholeTickTimeSeconds < second.wholeTickTimeSeconds;
        if (first.sourceOrigin != second.sourceOrigin)
            return first.sourceOrigin < second.sourceOrigin;
        if (first.privateEventOrdinal != second.privateEventOrdinal)
            return first.privateEventOrdinal < second.privateEventOrdinal;
        if (first.sourceSegmentOrdinal != second.sourceSegmentOrdinal)
            return first.sourceSegmentOrdinal < second.sourceSegmentOrdinal;
        return first.ballId < second.ballId;
    });
    std::map<std::string, int> sourceRanks;
    for (std::size_t index = 0; index < receipts.size(); ++index) {
        receipts[index].sourceOrdinal = static_cast<int>(index);
        sourceRanks[receipts[index].sourceIdentity] =
            static_cast<int>(index);
    }
    std::sort(stagedTick.events.begin(), stagedTick.events.end(),
        [&sourceRanks](const auto& first, const auto& second) {
            return sourceRanks.at(first.stableIdentity) <
                sourceRanks.at(second.stableIdentity);
        });
    for (std::size_t index = 0; index < stagedTick.events.size(); ++index) {
        auto& event = stagedTick.events[index];
        event.eventOrdinal = static_cast<int>(index);
        for (auto& receipt : receipts) {
            if (receipt.sourceIdentity != event.stableIdentity) continue;
            receipt.canonicalEventOrdinal = event.eventOrdinal;
            receipt.canonicalEventDigest =
                canonicalTableDepartureEventDigest({event});
        }
    }

    auto& attestation = stagedTick.attestation;
    attestation.physicsTick = context.physicsTick;
    attestation.wholeTickDurationSeconds = context.wholeTickDurationSeconds;
    attestation.solverProfileId = context.solverProfileId;
    attestation.boundaryMode = context.boundaryMode;
    attestation.privatePhysicsEventCount = producer.events.size();
    attestation.analyticTransitionSourceCount = analyticTransitionCount;
    attestation.sourceEventCount = stagedTick.events.size();
    attestation.excludedPhysicsDiagnosticEventCount = static_cast<std::size_t>(
        std::count_if(producer.events.begin(), producer.events.end(),
            [](const auto& event) { return diagnosticKind(event.kind); }));
    attestation.nonSettlingSupportContactSourceCount =
        static_cast<std::size_t>(std::count_if(
            attestation.orderedSourceReceipts.begin(),
            attestation.orderedSourceReceipts.end(), [](const auto& receipt) {
                return receipt.disposition ==
                    CanonicalTableDepartureProjectionDisposition::
                        ObservedNonSettlingSupport;
            }));
    attestation.emittedEventCount = stagedTick.events.size();
    attestation.orderedSourceDigest = canonicalTableDepartureSourceDigest(
        attestation.orderedSourceReceipts);
    attestation.orderedEventDigest =
        canonicalTableDepartureEventDigest(stagedTick.events);
    attestation.successfulStep = true;
    attestation.canonicalOrder = true;
    attestation.noDroppedSamples = true;
    attestation.complete = true;
    result.tick = std::move(stagedTick);
    result.complete = true;
    return result;
}

}  // namespace billiardgl::rules
