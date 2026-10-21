#pragma once

#include "physics_telemetry.h"
#include "rules/table_departure_observer_adapter.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::rules {

// Context that is not present in PhysicsStepTelemetry.  Event bindings are
// evidence for producer-private feature identities; generations and capture
// facts are always reconstructed from GameState, analytic segments, and
// canonical PhysicsContactRecord rows.
struct TableDepartureTelemetryBridgeContext {
    std::uint64_t physicsTick = 0;
    std::string solverProfileId;
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable;
    std::array<int, kBallCount> activeOutsideLandingSurfaceIds = [] {
        std::array<int, kBallCount> values{};
        values.fill(-1);
        return values;
    }();
    std::vector<TableDepartureProducerEventBinding> orderedEventBindings;
    bool successfulStep = false;
    bool complete = false;
};

TableDepartureObserverAdapterResult adaptPhysicsStepTelemetryTick(
    const PhysicsStepTelemetry& telemetry,
    const GameState& before,
    const GameState& after,
    const PhysicsProfile& profile,
    const TableDepartureTelemetryBridgeContext& context);

}  // namespace billiardgl::rules
