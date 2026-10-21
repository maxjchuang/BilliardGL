#include "rules/table_departure_telemetry_bridge.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace billiardgl::rules {
namespace {

bool finitePoint(const Point3& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z);
}

bool closePoint(const Point3& first, const Point3& second, double tolerance)
{
    return std::fabs(static_cast<double>(first.x) - second.x) <= tolerance &&
        std::fabs(static_cast<double>(first.y) - second.y) <= tolerance &&
        std::fabs(static_cast<double>(first.z) - second.z) <= tolerance;
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

std::string contactIdentity(std::uint64_t tick, int contactOrdinal,
    const PhysicsContactRecord& contact)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "canonical_pocket_contact:" << tick << ':' << contactOrdinal
           << ':' << contact.firstBall << ':' << contact.solverEventId << ':'
           << contact.pocketId << ':' << contact.pocketCaptureSequence;
    return stream.str();
}

const SurfaceMotionAnalyticSegment* uniquePrecedingSegment(
    const PhysicsStepTelemetry& telemetry, int ballId,
    std::uint64_t generation, double time, const Point3& position,
    const PhysicsProfile& profile)
{
    const SurfaceMotionAnalyticSegment* match = nullptr;
    for (const auto& segment : telemetry.analyticSurfaceMotion) {
        const double end = segment.wholeTickStartOffsetSeconds +
            segment.durationSeconds;
        if (segment.ballIndex != ballId ||
            segment.trajectoryGeneration != generation ||
            !segment.analyticPathComplete ||
            std::fabs(end - time) > profile.solver.toiToleranceSeconds ||
            !closePoint(segment.endPositionCm, position,
                profile.solver.penetrationSlopCm))
            continue;
        if (match) return nullptr;
        match = &segment;
    }
    return match;
}

const SurfaceMotionAnalyticSegment* uniqueFollowingSegment(
    const PhysicsStepTelemetry& telemetry, int ballId,
    std::uint64_t generation, double time, const Point3& position,
    const PhysicsProfile& profile)
{
    const SurfaceMotionAnalyticSegment* match = nullptr;
    for (const auto& segment : telemetry.analyticSurfaceMotion) {
        if (segment.ballIndex != ballId ||
            segment.trajectoryGeneration != generation ||
            !segment.analyticPathComplete ||
            std::fabs(segment.wholeTickStartOffsetSeconds - time) >
                profile.solver.toiToleranceSeconds ||
            !closePoint(segment.startPositionCm, position,
                profile.solver.penetrationSlopCm))
            continue;
        if (match) return nullptr;
        match = &segment;
    }
    return match;
}

bool zeroVelocity(const Point3& velocity)
{
    return velocity.x == 0.0f && velocity.y == 0.0f &&
        velocity.z == 0.0f;
}

struct SourceGroup {
    CanonicalTableDepartureSourceReceipt receipt;
    std::vector<CanonicalTableDepartureEvent> events;
};

bool sourceBefore(const SourceGroup& first, const SourceGroup& second)
{
    if (first.receipt.wholeTickTimeSeconds !=
            second.receipt.wholeTickTimeSeconds)
        return first.receipt.wholeTickTimeSeconds <
            second.receipt.wholeTickTimeSeconds;
    if (first.receipt.sourceOrigin != second.receipt.sourceOrigin)
        return first.receipt.sourceOrigin < second.receipt.sourceOrigin;
    if (first.receipt.privateEventOrdinal !=
            second.receipt.privateEventOrdinal)
        return first.receipt.privateEventOrdinal <
            second.receipt.privateEventOrdinal;
    if (first.receipt.canonicalContactOrdinal !=
            second.receipt.canonicalContactOrdinal)
        return first.receipt.canonicalContactOrdinal <
            second.receipt.canonicalContactOrdinal;
    if (first.receipt.sourceSegmentOrdinal !=
            second.receipt.sourceSegmentOrdinal)
        return first.receipt.sourceSegmentOrdinal <
            second.receipt.sourceSegmentOrdinal;
    return first.receipt.ballId < second.receipt.ballId;
}

bool exactBinding(const TableDepartureProducerEventBinding& first,
    const TableDepartureProducerEventBinding& second)
{
    return first.sourceOrdinal == second.sourceOrdinal &&
        first.sourceIdentity == second.sourceIdentity &&
        first.canonicalSolverEventId == second.canonicalSolverEventId &&
        first.boundaryFeatureId == second.boundaryFeatureId &&
        first.railFeatureId == second.railFeatureId &&
        first.landingSurfaceId == second.landingSurfaceId &&
        first.pocketId == second.pocketId &&
        first.physicsContactOrdinal == second.physicsContactOrdinal &&
        first.canonicalContactOrdinal == second.canonicalContactOrdinal &&
        first.transitionPositionCm.x == second.transitionPositionCm.x &&
        first.transitionPositionCm.y == second.transitionPositionCm.y &&
        first.transitionPositionCm.z == second.transitionPositionCm.z &&
        first.transitionPositionComplete ==
            second.transitionPositionComplete &&
        first.clothReturnProvenance == second.clothReturnProvenance &&
        first.complete == second.complete;
}

bool nativeSupportContactMatches(const PhysicsStepTelemetry& telemetry,
    const TableDepartureEvent& source)
{
    if (source.linkedPhysicsContactOrdinal < 0 ||
        static_cast<std::size_t>(source.linkedPhysicsContactOrdinal) >=
            telemetry.contacts.size())
        return false;
    const auto& contact = telemetry.contacts[static_cast<std::size_t>(
        source.linkedPhysicsContactOrdinal)];
    return contact.kind == PhysicsContactKind::Support &&
        contact.firstBall == source.ballIndex && contact.secondBall == -1 &&
        contact.solverEventId == source.solverEventId &&
        contact.wholeTickTimeOfImpactSeconds ==
            source.absoluteWholeTickTimeSeconds;
}

bool nativeRailContactMatches(const PhysicsStepTelemetry& telemetry,
    const TableDepartureEvent& source)
{
    if (source.linkedPhysicsContactOrdinal < 0 ||
        static_cast<std::size_t>(source.linkedPhysicsContactOrdinal) >=
            telemetry.contacts.size())
        return false;
    const auto& contact = telemetry.contacts[static_cast<std::size_t>(
        source.linkedPhysicsContactOrdinal)];
    return contact.kind == PhysicsContactKind::Rail &&
        contact.firstBall == source.ballIndex && contact.secondBall == -1 &&
        contact.solverEventId == source.solverEventId &&
        contact.wholeTickTimeOfImpactSeconds ==
            source.absoluteWholeTickTimeSeconds;
}

bool buildNativeBindings(const PhysicsStepTelemetry& telemetry,
    std::vector<TableDepartureProducerEventBinding>& bindings,
    std::string& error)
{
    std::map<std::pair<int, std::uint64_t>, int> activeRailFeatures;
    std::vector<int> canonicalContactOrdinals(telemetry.contacts.size(), -1);
    int captureOrdinal = 0;
    for (std::size_t index = 0; index < telemetry.contacts.size(); ++index) {
        const auto& contact = telemetry.contacts[index];
        if (contact.kind == PhysicsContactKind::Pocket &&
            contact.pocketBoundaryEvent == PocketBoundaryEventKind::Capture)
            canonicalContactOrdinals[index] = captureOrdinal++;
    }

    bindings.reserve(telemetry.tableDepartureEvents.size());
    for (const auto& source : telemetry.tableDepartureEvents) {
        TableDepartureProducerEventBinding binding;
        binding.sourceOrdinal = source.ordinal;
        binding.sourceIdentity = source.stableIdentity;
        binding.canonicalSolverEventId = source.solverEventId;
        binding.boundaryFeatureId = source.boundaryFeatureId;
        binding.railFeatureId = source.railFeatureId;
        binding.landingSurfaceId = source.landingSurfaceId;
        binding.pocketId = source.pocketId;
        binding.clothReturnProvenance = source.clothReturnProvenance;
        if (source.linkedPhysicsContactOrdinal >= 0 &&
            static_cast<std::size_t>(source.linkedPhysicsContactOrdinal) <
                telemetry.contacts.size()) {
            const auto& linked = telemetry.contacts[static_cast<std::size_t>(
                source.linkedPhysicsContactOrdinal)];
            if (linked.kind == PhysicsContactKind::Support) {
                binding.transitionPositionCm =
                    linked.firstPositionAtContactCm;
                binding.transitionPositionComplete =
                    finitePoint(binding.transitionPositionCm);
            }
        }

        const auto key = std::make_pair(source.ballIndex,
            source.trajectoryGeneration);
        switch (source.kind) {
        case TableDepartureEventKind::OuterDeparture:
            binding.complete = source.boundaryFeatureId >= 0 &&
                source.solverEventId >= -1;
            break;
        case TableDepartureEventKind::RailTopLanding:
            binding.physicsContactOrdinal =
                source.linkedPhysicsContactOrdinal;
            binding.complete = source.railFeatureId >= 0 &&
                source.solverEventId >= 0 &&
                nativeSupportContactMatches(telemetry, source);
            if (binding.complete)
                activeRailFeatures[key] = source.railFeatureId;
            break;
        case TableDepartureEventKind::OutsideLanding:
            binding.physicsContactOrdinal =
                source.linkedPhysicsContactOrdinal;
            binding.complete = source.landingSurfaceId >= 0 &&
                source.solverEventId >= 0 &&
                nativeSupportContactMatches(telemetry, source);
            break;
        case TableDepartureEventKind::ClothReturn:
            binding.physicsContactOrdinal =
                source.linkedPhysicsContactOrdinal;
            if (binding.clothReturnProvenance ==
                    TableDepartureClothReturnProvenance::RailTopLanding) {
                const auto rail = activeRailFeatures.find(key);
                if (binding.railFeatureId < 0 &&
                    rail != activeRailFeatures.end())
                    binding.railFeatureId = rail->second;
            }
            binding.complete = source.solverEventId >= 0 &&
                nativeSupportContactMatches(telemetry, source) &&
                binding.clothReturnProvenance !=
                    TableDepartureClothReturnProvenance::NotApplicable &&
                ((binding.clothReturnProvenance ==
                        TableDepartureClothReturnProvenance::
                            DirectAirborneLanding &&
                    binding.railFeatureId == -1) ||
                 (binding.clothReturnProvenance ==
                        TableDepartureClothReturnProvenance::RailTopLanding &&
                    binding.railFeatureId >= 0));
            break;
        case TableDepartureEventKind::PocketEntry: {
            binding.physicsContactOrdinal =
                source.linkedPhysicsContactOrdinal;
            if (source.linkedPhysicsContactOrdinal < 0) {
                binding.complete = source.pocketId >= 0 &&
                    source.solverEventId == -1 &&
                    source.pocketCaptureSequence == 0;
                break;
            }
            if (
                static_cast<std::size_t>(source.linkedPhysicsContactOrdinal) >=
                    telemetry.contacts.size())
                break;
            const std::size_t contactIndex = static_cast<std::size_t>(
                source.linkedPhysicsContactOrdinal);
            const auto& contact = telemetry.contacts[contactIndex];
            binding.canonicalContactOrdinal =
                canonicalContactOrdinals[contactIndex];
            binding.complete = binding.canonicalContactOrdinal >= 0 &&
                contact.kind == PhysicsContactKind::Pocket &&
                contact.pocketBoundaryEvent ==
                    PocketBoundaryEventKind::Capture &&
                contact.firstBall == source.ballIndex &&
                contact.pocketId == source.pocketId &&
                contact.solverEventId == source.solverEventId &&
                contact.pocketCaptureSequence ==
                    source.pocketCaptureSequence &&
                contact.trajectoryGenerationBefore ==
                    source.trajectoryGeneration &&
                contact.wholeTickTimeOfImpactSeconds >=
                    source.absoluteWholeTickTimeSeconds;
            break;
        }
        case TableDepartureEventKind::CushionNoseContact:
            binding.physicsContactOrdinal =
                source.linkedPhysicsContactOrdinal;
            binding.complete = source.solverEventId >= 0 &&
                nativeRailContactMatches(telemetry, source);
            break;
        case TableDepartureEventKind::CrossDomainBallContact:
            binding.complete = source.solverEventId >= -1;
            break;
        }
        if (!binding.complete) {
            std::ostringstream detail;
            detail.imbue(std::locale::classic());
            detail << "table_departure_native_binding_incomplete:ordinal="
                   << source.ordinal << ":kind="
                   << static_cast<int>(source.kind) << ":ball="
                   << source.ballIndex << ":generation="
                   << source.trajectoryGeneration << ":time="
                   << std::setprecision(17)
                   << source.absoluteWholeTickTimeSeconds << ":solver="
                   << source.solverEventId << ":contact="
                   << source.linkedPhysicsContactOrdinal << ":provenance="
                   << static_cast<int>(source.clothReturnProvenance)
                   << ":rail=" << binding.railFeatureId;
            if (source.linkedPhysicsContactOrdinal >= 0 &&
                static_cast<std::size_t>(source.linkedPhysicsContactOrdinal) <
                    telemetry.contacts.size()) {
                const auto& linked = telemetry.contacts[
                    static_cast<std::size_t>(
                        source.linkedPhysicsContactOrdinal)];
                detail << ":linked_kind=" << static_cast<int>(linked.kind)
                       << ":linked_ball=" << linked.firstBall
                       << ":linked_solver=" << linked.solverEventId
                       << ":linked_time=" << std::setprecision(17)
                       << linked.wholeTickTimeOfImpactSeconds;
            }
            error = detail.str();
            return false;
        }
        bindings.push_back(std::move(binding));
    }
    return true;
}

}  // namespace

TableDepartureObserverAdapterResult adaptPhysicsStepTelemetryTick(
    const PhysicsStepTelemetry& telemetry,
    const GameState& before,
    const GameState& after,
    const PhysicsProfile& profile,
    const TableDepartureTelemetryBridgeContext& context)
{
    TableDepartureObserverAdapterResult result;
    if (telemetry.stepStatus != PhysicsStepStatus::Succeeded ||
        telemetry.failureCode != PhysicsFailureCode::None ||
        !context.successfulStep || !context.complete ||
        context.physicsTick == 0 || context.solverProfileId.empty() ||
        context.solverProfileId != profile.id ||
        context.boundaryMode != PhysicsBoundaryMode::ProductionTable ||
        !std::isfinite(telemetry.wholeTickDurationSeconds) ||
        telemetry.wholeTickDurationSeconds < 0.0 ||
        (!context.orderedEventBindings.empty() &&
            context.orderedEventBindings.size() !=
                telemetry.tableDepartureEvents.size())) {
        result.error = "table_departure_telemetry_context_incomplete";
        return result;
    }

    std::vector<TableDepartureProducerEventBinding> nativeBindings;
    if (!buildNativeBindings(telemetry, nativeBindings, result.error))
        return result;
    if (!context.orderedEventBindings.empty()) {
        for (std::size_t index = 0; index < nativeBindings.size(); ++index) {
            if (!exactBinding(nativeBindings[index],
                    context.orderedEventBindings[index])) {
                result.error = "table_departure_external_binding_mismatch";
                return result;
            }
        }
    }
    const auto& bindings = nativeBindings;

    std::array<std::vector<std::pair<double, std::uint64_t>>, kBallCount>
        generationTransitions;
    for (const auto& contact : telemetry.contacts) {
        if (!contact.administrativeResetAfterCapture) continue;
        if (contact.firstBall < 0 || contact.firstBall >= kBallCount ||
            contact.trajectoryGenerationBefore ==
                std::numeric_limits<std::uint64_t>::max() ||
            contact.trajectoryGenerationAfter !=
                contact.trajectoryGenerationBefore + 1 ||
            !std::isfinite(contact.wholeTickTimeOfImpactSeconds)) {
            result.error = "table_departure_generation_transition_invalid";
            return result;
        }
        generationTransitions[static_cast<std::size_t>(contact.firstBall)].
            push_back({contact.wholeTickTimeOfImpactSeconds,
                contact.trajectoryGenerationAfter});
    }
    for (std::size_t ball = 0; ball < generationTransitions.size(); ++ball) {
        auto& transitions = generationTransitions[ball];
        std::sort(transitions.begin(), transitions.end());
        std::uint64_t generation = before.balls[ball].trajectoryGeneration;
        for (const auto& transition : transitions) {
            if (generation == std::numeric_limits<std::uint64_t>::max() ||
                transition.second != generation + 1) {
                result.error = "table_departure_generation_chain_invalid";
                return result;
            }
            generation = transition.second;
        }
        if (after.balls[ball].trajectoryGeneration != generation) {
            result.error = "table_departure_terminal_generation_invalid";
            return result;
        }
        for (const auto& segment : telemetry.analyticSurfaceMotion) {
            if (segment.ballIndex != static_cast<int>(ball)) continue;
            std::uint64_t expected = before.balls[ball].trajectoryGeneration;
            for (const auto& transition : transitions) {
                if (segment.wholeTickStartOffsetSeconds +
                        profile.solver.toiToleranceSeconds >=
                    transition.first)
                    expected = transition.second;
            }
            if (segment.trajectoryGeneration != expected) {
                std::ostringstream detail;
                detail.imbue(std::locale::classic());
                detail << "table_departure_segment_generation_invalid:ball="
                       << ball << ":ordinal=" << segment.segmentOrdinal
                       << ":start=" << std::setprecision(17)
                       << segment.wholeTickStartOffsetSeconds << ":duration="
                       << segment.durationSeconds << ":actual="
                       << segment.trajectoryGeneration << ":expected="
                       << expected << ":transitions="
                       << transitions.size();
                if (!transitions.empty())
                    detail << ":first_transition=" << std::setprecision(17)
                           << transitions.front().first << ':'
                           << transitions.front().second;
                result.error = detail.str();
                return result;
            }
        }
    }

    TableDepartureStepResult producer;
    producer.ok = true;
    producer.failureCode = TableDepartureFailureCode::None;
    producer.analyticSegments = telemetry.analyticSurfaceMotion;
    TableDepartureObserverAdapterContext producerContext;
    producerContext.physicsTick = context.physicsTick;
    producerContext.wholeTickDurationSeconds =
        telemetry.wholeTickDurationSeconds;
    producerContext.solverProfileId = context.solverProfileId;
    producerContext.boundaryMode = context.boundaryMode;
    producerContext.activeOutsideLandingSurfaceIds =
        context.activeOutsideLandingSurfaceIds;
    producerContext.successfulStep = true;
    producerContext.complete = true;
    for (std::size_t index = 0; index < telemetry.contacts.size(); ++index) {
        const auto& contact = telemetry.contacts[index];
        if (!contact.administrativeResetAfterCapture) continue;
        if (contact.kind != PhysicsContactKind::Pocket ||
            contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture ||
            contact.firstBall < 0 || contact.firstBall >= kBallCount ||
            contact.secondBall != -1 || contact.solverEventId < -1 ||
            !contact.pocketPassable ||
            contact.pocketPhaseAfter != PocketInteractionPhase::Captured ||
            !std::isfinite(contact.wholeTickTimeOfImpactSeconds) ||
            !finitePoint(contact.administrativeResetPositionCm)) {
            result.error = "table_departure_generation_transition_source_invalid";
            return result;
        }
        TableDepartureTrajectoryGenerationTransition transition;
        transition.ballId = contact.firstBall;
        transition.physicsContactOrdinal = static_cast<int>(index);
        transition.generationBefore = contact.trajectoryGenerationBefore;
        transition.generationAfter = contact.trajectoryGenerationAfter;
        transition.wholeTickTimeSeconds =
            contact.wholeTickTimeOfImpactSeconds;
        transition.resetPositionCm = contact.administrativeResetPositionCm;
        transition.complete = true;
        producerContext.trajectoryGenerationTransitions.push_back(transition);
    }
    const auto exactPoint = [](const Point3& first, const Point3& second) {
        return first.x == second.x && first.y == second.y &&
            first.z == second.z;
    };
    for (const auto& contact : telemetry.contacts) {
        if (!contact.positionCorrected &&
            exactPoint(contact.firstPositionCorrectionCm, Point3{}) &&
            exactPoint(contact.secondPositionCorrectionCm, Point3{}))
            continue;
        int matchingProjectionEvents = 0;
        for (const auto& event : telemetry.solverEvents) {
            if (event.eventId == contact.solverEventId &&
                event.islandId == contact.solverIslandId &&
                !event.positionProjections.empty() &&
                std::fabs(event.wholeTickTimeOfImpactSeconds -
                    contact.wholeTickTimeOfImpactSeconds) <=
                    profile.solver.toiToleranceSeconds)
                ++matchingProjectionEvents;
        }
        if (matchingProjectionEvents != 1) {
            result.error =
                "table_departure_position_projection_source_missing";
            return result;
        }
    }
    std::set<std::pair<int, int>> projectionEventKeys;
    for (const auto& event : telemetry.solverEvents) {
        if (event.positionProjections.empty()) continue;
        if (event.eventId < 0 || event.islandId < 0 ||
            !event.failureCode || std::string(event.failureCode) != "converged" ||
            event.islandLimitExceeded || event.contactCount <= 0 ||
            event.positionIterations <= 0 ||
            !std::isfinite(event.wholeTickTimeOfImpactSeconds) ||
            event.wholeTickTimeOfImpactSeconds < 0.0 ||
            event.wholeTickTimeOfImpactSeconds >
                telemetry.wholeTickDurationSeconds ||
            !projectionEventKeys.insert(
                {event.eventId, event.islandId}).second) {
            result.error =
                "table_departure_position_projection_event_invalid";
            return result;
        }
        std::vector<const PhysicsContactRecord*> eventContacts;
        for (const auto& contact : telemetry.contacts) {
            if (contact.solverEventId == event.eventId &&
                contact.solverIslandId == event.islandId &&
                std::fabs(contact.wholeTickTimeOfImpactSeconds -
                    event.wholeTickTimeOfImpactSeconds) <=
                    profile.solver.toiToleranceSeconds)
                eventContacts.push_back(&contact);
        }
        if (eventContacts.size() !=
                static_cast<std::size_t>(event.contactCount)) {
            result.error =
                "table_departure_position_projection_contact_set_invalid";
            return result;
        }
        int expectedOrdinal = 0;
        std::tuple<int, int, int> priorProjection{-1, -1, -1};
        for (const auto& receipt : event.positionProjections) {
            const bool contactIndexValid = receipt.contactIndex >= 0 &&
                static_cast<std::size_t>(receipt.contactIndex) <
                    eventContacts.size();
            int participant = -1;
            if (contactIndexValid) {
                const auto& contact = *eventContacts[
                    static_cast<std::size_t>(receipt.contactIndex)];
                participant = receipt.ballIndex == contact.firstBall ? 0 :
                    (receipt.ballIndex == contact.secondBall ? 1 : -1);
            }
            const auto projectionKey = std::make_tuple(receipt.iteration,
                receipt.contactIndex, participant);
            if (receipt.ordinal != expectedOrdinal++ ||
                receipt.iteration < 0 ||
                receipt.iteration >= event.positionIterations ||
                !contactIndexValid || participant < 0 ||
                projectionKey <= priorProjection ||
                !eventContacts[static_cast<std::size_t>(receipt.contactIndex)]->
                    positionCorrected ||
                receipt.ballIndex < 0 || receipt.ballIndex >= kBallCount ||
                !finitePoint(receipt.beforeCm) ||
                !finitePoint(receipt.afterCm) ||
                (receipt.beforeCm.x == receipt.afterCm.x &&
                    receipt.beforeCm.y == receipt.afterCm.y &&
                    receipt.beforeCm.z == receipt.afterCm.z)) {
                result.error =
                    "table_departure_position_projection_receipt_invalid";
                return result;
            }
            priorProjection = projectionKey;
            TableDeparturePositionProjectionTransition transition;
            transition.ballId = receipt.ballIndex;
            transition.solverEventId = event.eventId;
            transition.solverIslandId = event.islandId;
            transition.projectionOrdinal = receipt.ordinal;
            transition.solverIteration = receipt.iteration;
            transition.solverContactIndex = receipt.contactIndex;
            transition.trajectoryGeneration = receipt.trajectoryGeneration;
            transition.wholeTickTimeSeconds =
                event.wholeTickTimeOfImpactSeconds;
            transition.beforeCm = receipt.beforeCm;
            transition.afterCm = receipt.afterCm;
            transition.complete = true;
            producerContext.positionProjectionTransitions.push_back(
                transition);
        }
    }
    for (std::size_t ball = 0; ball < before.balls.size(); ++ball)
        producerContext.trajectoryGenerations[ball] =
            before.balls[ball].trajectoryGeneration;

    std::vector<int> projectedToPrivateOrdinal;
    std::vector<std::size_t> pocketEntryIndices;
    int priorOrdinal = -1;
    double priorTime = -std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < telemetry.tableDepartureEvents.size();
            ++index) {
        const auto& source = telemetry.tableDepartureEvents[index];
        const auto& binding = bindings[index];
        if (source.ordinal != static_cast<int>(index) ||
            source.ordinal <= priorOrdinal ||
            binding.sourceOrdinal != source.ordinal ||
            binding.sourceIdentity != source.stableIdentity ||
            source.absoluteWholeTickTimeSeconds < priorTime) {
            result.error = "table_departure_telemetry_private_order_invalid";
            return result;
        }
        priorOrdinal = source.ordinal;
        priorTime = source.absoluteWholeTickTimeSeconds;
        const std::size_t ball = static_cast<std::size_t>(source.ballIndex);
        if (source.ballIndex < 0 || source.ballIndex >= kBallCount ||
            source.trajectoryGeneration !=
                before.balls[ball].trajectoryGeneration ||
            (source.solverEventId != -1 &&
                source.solverEventId != binding.canonicalSolverEventId) ||
            (source.boundaryFeatureId != -1 &&
                source.boundaryFeatureId != binding.boundaryFeatureId) ||
            (source.railFeatureId != -1 &&
                source.railFeatureId != binding.railFeatureId) ||
            (source.landingSurfaceId != -1 &&
                source.landingSurfaceId != binding.landingSurfaceId)) {
            result.error = "table_departure_telemetry_native_metadata_mismatch";
            return result;
        }
        producerContext.trajectoryGenerations[ball] =
            source.trajectoryGeneration;
        if (source.kind == TableDepartureEventKind::PocketEntry) {
            if ((source.pocketId != -1 &&
                    source.pocketId != binding.pocketId) ||
                (source.linkedPhysicsContactOrdinal != -1 &&
                    source.linkedPhysicsContactOrdinal !=
                        binding.physicsContactOrdinal)) {
                result.error =
                    "table_departure_telemetry_native_pocket_mismatch";
                return result;
            }
            pocketEntryIndices.push_back(index);
            continue;
        }
        auto projectedSource = source;
        projectedSource.ordinal = static_cast<int>(producer.events.size());
        auto projectedBinding = binding;
        projectedBinding.sourceOrdinal = projectedSource.ordinal;
        producer.events.push_back(projectedSource);
        producerContext.orderedEventBindings.push_back(projectedBinding);
        projectedToPrivateOrdinal.push_back(source.ordinal);
    }

    const auto adapted = adaptTableDepartureProducerTick(producer, before,
        after, profile, producerContext);
    if (!adapted.complete) {
        result.error = adapted.error;
        return result;
    }

    std::vector<SourceGroup> groups;
    for (const auto& receipt : adapted.tick.attestation.orderedSourceReceipts) {
        SourceGroup group;
        group.receipt = receipt;
        if (receipt.sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::PrivateEvent) {
            if (receipt.privateEventOrdinal < 0 ||
                static_cast<std::size_t>(receipt.privateEventOrdinal) >=
                    projectedToPrivateOrdinal.size()) {
                result.error = "table_departure_telemetry_private_mapping_invalid";
                return result;
            }
            group.receipt.privateEventOrdinal = projectedToPrivateOrdinal[
                static_cast<std::size_t>(receipt.privateEventOrdinal)];
        }
        if (receipt.canonicalEventOrdinal >= 0) {
            const std::size_t ordinal = static_cast<std::size_t>(
                receipt.canonicalEventOrdinal);
            if (receipt.canonicalEventCount != 1 ||
                ordinal >= adapted.tick.events.size()) {
                result.error = "table_departure_telemetry_projection_invalid";
                return result;
            }
            group.events.push_back(adapted.tick.events[ordinal]);
        }
        groups.push_back(std::move(group));
    }

    struct CaptureBinding {
        int physicsContactOrdinal = -1;
        int canonicalContactOrdinal = -1;
        const PhysicsContactRecord* contact = nullptr;
        Point3 velocityBefore;
        Point3 resetPosition;
    };
    std::vector<CaptureBinding> captures;
    double priorCaptureTime = -std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < telemetry.contacts.size(); ++index) {
        const auto& contact = telemetry.contacts[index];
        if (contact.kind != PhysicsContactKind::Pocket ||
            contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture)
            continue;
        if (contact.firstBall < 0 || contact.firstBall >= kBallCount ||
            contact.secondBall != -1 || contact.solverEventId < -1 ||
            contact.pocketId < 0 || !contact.pocketPassable ||
            contact.pocketPhaseAfter != PocketInteractionPhase::Captured ||
            contact.pocketCaptureSequence == 0 ||
            !std::isfinite(contact.wholeTickTimeOfImpactSeconds) ||
            contact.wholeTickTimeOfImpactSeconds < priorCaptureTime ||
            contact.wholeTickTimeOfImpactSeconds < 0.0 ||
            contact.wholeTickTimeOfImpactSeconds >
                telemetry.wholeTickDurationSeconds ||
            !finitePoint(contact.firstPositionAtContactCm)) {
            result.error = "table_departure_capture_contact_invalid";
            return result;
        }
        const bool hasVerticalShaftNormal = contact.normal.x == 0.0f &&
            contact.normal.y == -1.0f && contact.normal.z == 0.0f;
        if ((contact.pocketVerticalShaft && !hasVerticalShaftNormal) ||
            (!contact.pocketVerticalShaft &&
                (hasVerticalShaftNormal ||
                 before.balls[static_cast<std::size_t>(contact.firstBall)].
                     physicalDomain == PhysicalBallDomain::PocketChannel))) {
            result.error = "table_departure_capture_shaft_provenance_invalid";
            return result;
        }
        priorCaptureTime = contact.wholeTickTimeOfImpactSeconds;
        const std::size_t ball = static_cast<std::size_t>(contact.firstBall);
        if (before.balls[ball].trajectoryGeneration !=
                contact.trajectoryGenerationBefore) {
            result.error = "table_departure_capture_generation_invalid";
            return result;
        }
        const auto* preceding = uniquePrecedingSegment(telemetry,
            contact.firstBall, contact.trajectoryGenerationBefore,
            contact.wholeTickTimeOfImpactSeconds,
            contact.firstPositionAtContactCm, profile);
        Point3 velocityBefore;
        if (preceding) {
            velocityBefore = terminalVelocity(*preceding);
        } else if (contact.wholeTickTimeOfImpactSeconds <=
                profile.solver.toiToleranceSeconds &&
            before.balls[ball].trajectoryGeneration ==
                contact.trajectoryGenerationBefore &&
            closePoint(before.balls[ball].position,
                contact.firstPositionAtContactCm,
                profile.solver.penetrationSlopCm) &&
            finitePoint(before.balls[ball].velocity)) {
            // A capture already eligible at the tick boundary owns no
            // positive-duration analytic segment.  The exact pre-step state
            // is the only authoritative generation-matched left witness.
            velocityBefore = before.balls[ball].velocity;
        } else {
            result.error = "table_departure_capture_velocity_incomplete";
            return result;
        }

        Point3 resetPosition{};
        if (contact.administrativeResetAfterCapture) {
            if (contact.firstBall != 0 ||
                contact.trajectoryGenerationBefore ==
                    std::numeric_limits<std::uint64_t>::max() ||
                contact.trajectoryGenerationAfter !=
                    contact.trajectoryGenerationBefore + 1 ||
                after.balls[ball].trajectoryGeneration !=
                    contact.trajectoryGenerationAfter ||
                !finitePoint(contact.administrativeResetPositionCm)) {
                result.error = "table_departure_reset_generation_invalid";
                return result;
            }
            const auto* following = uniqueFollowingSegment(telemetry,
                contact.firstBall, contact.trajectoryGenerationAfter,
                contact.wholeTickTimeOfImpactSeconds,
                contact.administrativeResetPositionCm, profile);
            if (following) {
                if (!zeroVelocity(following->startVelocityCmS)) {
                    result.error = "table_departure_reset_velocity_invalid";
                    return result;
                }
                resetPosition = following->startPositionCm;
            } else if (after.balls[ball].trajectoryGeneration ==
                    contact.trajectoryGenerationAfter &&
                closePoint(after.balls[ball].position,
                    contact.administrativeResetPositionCm,
                    profile.solver.penetrationSlopCm) &&
                zeroVelocity(after.balls[ball].velocity)) {
                resetPosition = after.balls[ball].position;
            } else {
                result.error = "table_departure_reset_witness_incomplete";
                return result;
            }
        } else if (contact.trajectoryGenerationAfter !=
                contact.trajectoryGenerationBefore ||
            !after.balls[ball].pocketed ||
            after.balls[ball].physicalDomain !=
                PhysicalBallDomain::PocketChannel) {
            result.error = "table_departure_capture_terminal_invalid";
            return result;
        }

        CaptureBinding capture;
        capture.physicsContactOrdinal = static_cast<int>(index);
        capture.canonicalContactOrdinal = static_cast<int>(captures.size());
        capture.contact = &contact;
        capture.velocityBefore = velocityBefore;
        capture.resetPosition = resetPosition;
        captures.push_back(capture);
    }

    for (const auto& capture : captures) {
        const auto& contact = *capture.contact;
        const std::string identity = contactIdentity(context.physicsTick,
            capture.physicsContactOrdinal, contact);
        CanonicalTableDepartureEvent captured;
        captured.kind = CanonicalTableDepartureEventKind::PocketCapture;
        captured.ballId = contact.firstBall;
        captured.physicsTick = context.physicsTick;
        captured.solverEventId = contact.solverEventId;
        captured.trajectoryGeneration = contact.trajectoryGenerationBefore;
        captured.wholeTickTimeSeconds =
            contact.wholeTickTimeOfImpactSeconds;
        captured.sourceDomain = contact.pocketVerticalShaft ?
            PhysicalBallDomain::PocketChannel :
            before.balls[static_cast<std::size_t>(
                contact.firstBall)].physicalDomain;
        captured.targetDomain = PhysicalBallDomain::PocketChannel;
        captured.positionCm = contact.firstPositionAtContactCm;
        captured.velocityBeforeCmS = capture.velocityBefore;
        captured.velocityAfterCmS = {};
        captured.ballRadiusCm = profile.ball.radiusCm;
        captured.pocketId = contact.pocketId;
        captured.pocketCaptureSequence = contact.pocketCaptureSequence;
        captured.complete = true;
        captured.stableIdentity = identity + ":capture";

        SourceGroup group;
        group.receipt.sourceOrigin =
            CanonicalTableDepartureSourceOrigin::CanonicalContact;
        group.receipt.physicsContactOrdinal = capture.physicsContactOrdinal;
        group.receipt.canonicalContactOrdinal =
            capture.canonicalContactOrdinal;
        group.receipt.sourceIdentity = identity;
        group.receipt.kind =
            CanonicalTableDeparturePrivateEventKind::PocketCapture;
        group.receipt.ballId = contact.firstBall;
        group.receipt.solverEventId = contact.solverEventId;
        group.receipt.trajectoryGeneration =
            contact.trajectoryGenerationBefore;
        group.receipt.wholeTickTimeSeconds =
            contact.wholeTickTimeOfImpactSeconds;
        group.receipt.transitionPositionCm = contact.firstPositionAtContactCm;
        group.receipt.velocityBeforeCmS = capture.velocityBefore;
        group.receipt.velocityAfterCmS = {};
        group.receipt.sourceDomain = captured.sourceDomain;
        group.receipt.targetDomain = captured.targetDomain;
        group.receipt.disposition =
            CanonicalTableDepartureProjectionDisposition::EmittedCanonical;
        group.receipt.complete = true;
        group.events.push_back(captured);

        if (contact.administrativeResetAfterCapture) {
            CanonicalTableDepartureEvent reset;
            reset.kind =
                CanonicalTableDepartureEventKind::AdministrativeReset;
            reset.ballId = contact.firstBall;
            reset.physicsTick = context.physicsTick;
            reset.solverEventId = contact.solverEventId;
            reset.previousTrajectoryGeneration =
                contact.trajectoryGenerationBefore;
            reset.trajectoryGeneration = contact.trajectoryGenerationAfter;
            reset.wholeTickTimeSeconds =
                contact.wholeTickTimeOfImpactSeconds;
            reset.sourceDomain = PhysicalBallDomain::PocketChannel;
            reset.targetDomain = PhysicalBallDomain::ClothSupported;
            reset.positionCm = capture.resetPosition;
            reset.velocityBeforeCmS = {};
            reset.velocityAfterCmS = {};
            reset.ballRadiusCm = profile.ball.radiusCm;
            reset.pocketId = contact.pocketId;
            reset.pocketCaptureSequence = contact.pocketCaptureSequence;
            reset.complete = true;
            reset.stableIdentity = identity + ":reset";
            group.events.push_back(reset);
        }
        groups.push_back(std::move(group));
    }

    for (std::size_t privateIndex : pocketEntryIndices) {
        const auto& source = telemetry.tableDepartureEvents[privateIndex];
        const auto& binding = bindings[privateIndex];
        const auto capture = std::find_if(captures.begin(), captures.end(),
            [&source, &binding](const auto& candidate) {
                const auto& contact = *candidate.contact;
                return candidate.canonicalContactOrdinal ==
                        binding.canonicalContactOrdinal &&
                    candidate.physicsContactOrdinal ==
                        binding.physicsContactOrdinal &&
                    contact.firstBall == source.ballIndex &&
                    contact.pocketId == binding.pocketId &&
                    contact.solverEventId == binding.canonicalSolverEventId &&
                    contact.wholeTickTimeOfImpactSeconds >=
                        source.absoluteWholeTickTimeSeconds;
            });
        // PocketEntry carries no absolute position.  Its generation must be
        // witnessed by exactly one analytic segment ending at its timestamp.
        const SurfaceMotionAnalyticSegment* generationWitness = nullptr;
        for (const auto& segment : telemetry.analyticSurfaceMotion) {
            const double end = segment.wholeTickStartOffsetSeconds +
                segment.durationSeconds;
            if (segment.ballIndex == source.ballIndex &&
                segment.trajectoryGeneration ==
                    source.trajectoryGeneration &&
                segment.analyticPathComplete &&
                std::fabs(end - source.absoluteWholeTickTimeSeconds) <=
                    profile.solver.toiToleranceSeconds) {
                if (generationWitness) {
                    result.error =
                        "table_departure_pocket_entry_generation_ambiguous";
                    return result;
                }
                generationWitness = &segment;
            }
        }
        if (!generationWitness) {
            result.error = "table_departure_pocket_entry_generation_incomplete";
            return result;
        }
        SourceGroup group;
        group.receipt.sourceOrigin =
            CanonicalTableDepartureSourceOrigin::PrivateEvent;
        group.receipt.privateEventOrdinal = source.ordinal;
        group.receipt.sourceIdentity = source.stableIdentity;
        group.receipt.kind =
            CanonicalTableDeparturePrivateEventKind::PocketEntryDiagnostic;
        group.receipt.ballId = source.ballIndex;
        group.receipt.solverEventId = binding.canonicalSolverEventId;
        group.receipt.trajectoryGeneration = source.trajectoryGeneration;
        group.receipt.wholeTickTimeSeconds =
            source.absoluteWholeTickTimeSeconds;
        group.receipt.sourceDomain = source.sourceDomain;
        group.receipt.targetDomain = source.targetDomain;
        group.receipt.velocityAfterCmS = source.velocityAfterCmS;
        if (capture == captures.end()) {
            group.receipt.disposition =
                CanonicalTableDepartureProjectionDisposition::
                    ObservedPendingPocketEntry;
            group.receipt.exclusionReason =
                CanonicalTableDepartureExclusionReason::
                    PocketEntryAwaitingCapture;
            group.receipt.complete = true;
            groups.push_back(std::move(group));
            continue;
        }
        group.receipt.canonicalContactOrdinal =
            capture->canonicalContactOrdinal;
        group.receipt.disposition =
            CanonicalTableDepartureProjectionDisposition::ExcludedDiagnostic;
        group.receipt.exclusionReason =
            CanonicalTableDepartureExclusionReason::
                PocketEntryBoundToCanonicalCapture;
        group.receipt.complete = true;
        groups.push_back(std::move(group));
    }

    std::sort(groups.begin(), groups.end(), sourceBefore);
    CanonicalTableDepartureTick tick;
    tick.physicsTick = context.physicsTick;
    tick.wholeTickDurationSeconds = telemetry.wholeTickDurationSeconds;
    tick.solverProfileId = context.solverProfileId;
    tick.boundaryMode = context.boundaryMode;
    for (std::size_t sourceIndex = 0; sourceIndex < groups.size();
            ++sourceIndex) {
        auto& group = groups[sourceIndex];
        group.receipt.sourceOrdinal = static_cast<int>(sourceIndex);
        if (!group.events.empty()) {
            group.receipt.canonicalEventOrdinal =
                static_cast<int>(tick.events.size());
            group.receipt.canonicalEventCount =
                static_cast<int>(group.events.size());
            for (auto& event : group.events) {
                event.eventOrdinal = static_cast<int>(tick.events.size());
                tick.events.push_back(event);
            }
            group.receipt.canonicalEventDigest =
                canonicalTableDepartureEventDigest(group.events);
        }
        tick.attestation.orderedSourceReceipts.push_back(group.receipt);
    }

    auto& attestation = tick.attestation;
    attestation.physicsTick = tick.physicsTick;
    attestation.wholeTickDurationSeconds = tick.wholeTickDurationSeconds;
    attestation.solverProfileId = tick.solverProfileId;
    attestation.boundaryMode = tick.boundaryMode;
    attestation.sourceEventCount = tick.events.size();
    attestation.privatePhysicsEventCount =
        telemetry.tableDepartureEvents.size();
    attestation.analyticTransitionSourceCount =
        adapted.tick.attestation.analyticTransitionSourceCount;
    attestation.canonicalContactSourceCount = captures.size();
    attestation.excludedPhysicsDiagnosticEventCount =
        static_cast<std::size_t>(std::count_if(
            attestation.orderedSourceReceipts.begin(),
            attestation.orderedSourceReceipts.end(), [](const auto& receipt) {
                return receipt.disposition ==
                    CanonicalTableDepartureProjectionDisposition::
                        ExcludedDiagnostic;
            }));
    attestation.nonSettlingSupportContactSourceCount =
        static_cast<std::size_t>(std::count_if(
            attestation.orderedSourceReceipts.begin(),
            attestation.orderedSourceReceipts.end(), [](const auto& receipt) {
                return receipt.disposition ==
                    CanonicalTableDepartureProjectionDisposition::
                        ObservedNonSettlingSupport;
            }));
    attestation.pendingPocketEntrySourceCount =
        static_cast<std::size_t>(std::count_if(
            attestation.orderedSourceReceipts.begin(),
            attestation.orderedSourceReceipts.end(), [](const auto& receipt) {
                return receipt.disposition ==
                    CanonicalTableDepartureProjectionDisposition::
                        ObservedPendingPocketEntry;
            }));
    attestation.emittedEventCount = tick.events.size();
    attestation.orderedSourceDigest = canonicalTableDepartureSourceDigest(
        attestation.orderedSourceReceipts);
    attestation.orderedEventDigest =
        canonicalTableDepartureEventDigest(tick.events);
    attestation.successfulStep = true;
    attestation.canonicalOrder = true;
    attestation.noDroppedSamples = true;
    attestation.complete = true;
    result.tick = std::move(tick);
    result.complete = true;
    return result;
}

}  // namespace billiardgl::rules
