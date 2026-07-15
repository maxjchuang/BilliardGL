#pragma once

#include "ball_ball_contact.h"
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
    double accumulatedTangentialImpulseNs = 0.0;
    double targetNormalSpeedCmS = 0.0;
    double residualCmS = 0.0;
    double projectionCm = 0.0;
    double normalEffectiveMassKg = 0.0;
    double tangentEffectiveMassKg = 0.0;
    double restitution = 0.0;
    double frictionCoefficient = 0.0;
    BallBallContactRegime regime = BallBallContactRegime::NoContact;
    Point3 normal;
    Point3 tangent;
    Point3 firstContactArmM;
    Point3 secondContactArmM;
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
    double totalKineticEnergyBeforeJ = 0.0;
    double totalKineticEnergyAfterJ = 0.0;
    std::vector<ContactImpulseDiagnostic> contacts;
};

ContactSolverResult solveContactIsland(
    GameState& state, const ContactIsland& island,
    const PhysicsProfile& profile);
ContactSolverResult solveContactIslandIteration(GameState& state,
    const ContactIsland& island, const PhysicsProfile& profile,
    int velocityIterations, int positionIterations);
const char* contactSolverStatusName(ContactSolverStatus status);

}  // namespace billiardgl
