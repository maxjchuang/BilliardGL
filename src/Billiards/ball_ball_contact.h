#pragma once

#include "game_state.h"
#include "physics_profile.h"

#include <array>

namespace billiardgl {

enum class BallBallContactRegime {
    NoContact,
    Separating,
    Frictionless,
    Stick,
    Slip
};

struct BallBallContactResult {
    BallBallContactRegime regime = BallBallContactRegime::NoContact;
    bool velocityImpulseApplied = false;
    bool positionCorrected = false;
    double penetrationM = 0.0;
    double positionSlopM = 0.0000001;
    double restitution = 0.0;
    double frictionCoefficient = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double normalRelativeSpeedBeforeMS = 0.0;
    double normalRelativeSpeedAfterMS = 0.0;
    double kineticEnergyBeforeJ = 0.0;
    double kineticEnergyAfterJ = 0.0;
    std::array<double, 3> contactNormal{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactTangent{{0.0, 0.0, 0.0}};
    std::array<double, 3> firstContactArmM{{0.0, 0.0, 0.0}};
    std::array<double, 3> secondContactArmM{{0.0, 0.0, 0.0}};
    std::array<double, 3> relativeContactVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> relativeContactVelocityAfterMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> impulseOnSecondNs{{0.0, 0.0, 0.0}};
    std::array<double, 3> firstPositionCorrectionM{{0.0, 0.0, 0.0}};
    std::array<double, 3> secondPositionCorrectionM{{0.0, 0.0, 0.0}};
};

BallBallContactResult resolveBallBallContact(BallState& first, BallState& second,
    const BallProperties& firstProperties,
    const BallProperties& secondProperties);
const char* ballBallContactRegimeName(BallBallContactRegime regime);

}  // namespace billiardgl
