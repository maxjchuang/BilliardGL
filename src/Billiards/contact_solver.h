#pragma once

#include "contact_island.h"
#include "physics_profile.h"

#include <vector>

namespace billiardgl {

enum class ContactSolverStatus {
    Converged,
    IterationLimit,
    IslandLimit,
    PenetrationLimit,
    NonfiniteState
};

struct ContactImpulseDiagnostic {
    int firstBall = -1;
    int secondBall = -1;
    int featureId = -1;
    double accumulatedNormalImpulseNs = 0.0;
    double targetNormalSpeedCmS = 0.0;
    double residualCmS = 0.0;
    double projectionCm = 0.0;
    Point3 relativeVelocityBeforeCmS;
    Point3 relativeVelocityAfterCmS;
};

struct ContactSolverResult {
    ContactSolverStatus status = ContactSolverStatus::Converged;
    int velocityIterations = 0;
    int positionIterations = 0;
    double maximumResidualCmS = 0.0;
    double maximumPenetrationCm = 0.0;
    double kineticEnergyBeforeJ = 0.0;
    double kineticEnergyAfterJ = 0.0;
    std::vector<ContactImpulseDiagnostic> contacts;
};

ContactSolverResult solveContactIsland(
    GameState& state, const ContactIsland& island,
    const PhysicsProfile& profile);
const char* contactSolverStatusName(ContactSolverStatus status);

}  // namespace billiardgl
