#pragma once

#include "rules/table_departure_observer.h"
#include "table_departure_geometry.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::rules {

struct TableDepartureProducerEventBinding {
    int sourceOrdinal = -1;
    std::string sourceIdentity;
    int canonicalSolverEventId = -1;
    int boundaryFeatureId = -1;
    int railFeatureId = -1;
    int landingSurfaceId = -1;
    int pocketId = -1;
    int physicsContactOrdinal = -1;
    int canonicalContactOrdinal = -1;
    Point3 transitionPositionCm;
    bool transitionPositionComplete = false;
    billiardgl::TableDepartureClothReturnProvenance clothReturnProvenance =
        billiardgl::TableDepartureClothReturnProvenance::NotApplicable;
    bool complete = false;
};

struct TableDepartureTrajectoryGenerationTransition {
    int ballId = -1;
    int physicsContactOrdinal = -1;
    std::uint64_t generationBefore = 0;
    std::uint64_t generationAfter = 0;
    double wholeTickTimeSeconds = 0.0;
    Point3 resetPositionCm;
    bool complete = false;
};

struct TableDeparturePositionProjectionTransition {
    int ballId = -1;
    int solverEventId = -1;
    int solverIslandId = -1;
    int projectionOrdinal = -1;
    int solverIteration = -1;
    int solverContactIndex = -1;
    std::uint64_t trajectoryGeneration = 0;
    double wholeTickTimeSeconds = 0.0;
    Point3 beforeCm;
    Point3 afterCm;
    bool complete = false;
};

struct TableDepartureObserverAdapterContext {
    std::uint64_t physicsTick = 0;
    double wholeTickDurationSeconds = 0.0;
    std::string solverProfileId;
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable;
    std::array<std::uint64_t, kBallCount> trajectoryGenerations{};
    std::array<int, kBallCount> activeOutsideLandingSurfaceIds = [] {
        std::array<int, kBallCount> values{};
        values.fill(-1);
        return values;
    }();
    std::vector<TableDepartureProducerEventBinding> orderedEventBindings;
    std::vector<TableDepartureTrajectoryGenerationTransition>
        trajectoryGenerationTransitions;
    std::vector<TableDeparturePositionProjectionTransition>
        positionProjectionTransitions;
    bool successfulStep = false;
    bool complete = false;
};

struct TableDepartureObserverAdapterResult {
    bool complete = false;
    std::string error;
    CanonicalTableDepartureTick tick;
};

TableDepartureObserverAdapterResult adaptTableDepartureProducerTick(
    const TableDepartureStepResult& producer,
    const GameState& before,
    const GameState& after,
    const PhysicsProfile& profile,
    const TableDepartureObserverAdapterContext& context);

}  // namespace billiardgl::rules
