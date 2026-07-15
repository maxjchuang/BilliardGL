#pragma once

#include "cue_impact.h"
#include "game_state.h"
#include "physics_profile.h"

#include <array>
#include <string>
#include <vector>

namespace billiardgl {

enum class CueContactRegime {
    Unsupported,
    Stick,
    Slip,
    Miscue,
    Released
};

struct CueContactBallSample {
    int index = -1;
    Point3 positionCm;
    Point3 velocityCmS;
    Point3 accelerationCmS2;
    Point3 angularVelocityRadS;
};

struct CueContactConstraintSample {
    int kind = 0;
    int firstBall = -1;
    int secondBall = -1;
    int featureId = -1;
    Point3 normal;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double penetrationCm = 0.0;
    double residualCmS = 0.0;
};

struct CueContactMicrostep {
    int index = 0;
    double timeSeconds = 0.0;
    double cuePositionM = 0.0;
    double cueVelocityMS = 0.0;
    double cueAccelerationMS2 = 0.0;
    double compressionM = 0.0;
    double compressionRateMS = 0.0;
    double normalForceN = 0.0;
    double tangentialForceN = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double kineticEnergyJ = 0.0;
    double elasticEnergyJ = 0.0;
    double dissipatedEnergyJ = 0.0;
    double energyResidualJ = 0.0;
    double maximumPenetrationCm = 0.0;
    double solverResidualCmS = 0.0;
    int solverIterations = 0;
    CueContactRegime regime = CueContactRegime::Stick;
    std::array<CueContactBallSample, kBallCount> balls;
    std::vector<CueContactConstraintSample> contacts;
};

struct CueContactResult {
    CueContactRegime regime = CueContactRegime::Unsupported;
    bool applied = false;
    std::string error;
    double frictionCoefficient = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double normalRelativeSpeedBeforeMS = 0.0;
    double tangentialRelativeSpeedBeforeMS = 0.0;
    double inputKineticEnergyJ = 0.0;
    double outputKineticEnergyJ = 0.0;
    std::array<double, 3> tangentialRelativeVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactArmM{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactNormal{{0.0, 0.0, 0.0}};
    std::array<double, 3> impulseNs{{0.0, 0.0, 0.0}};
    std::array<double, 3> cueVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> cueVelocityAfterMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballVelocityAfterMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballAngularVelocityBeforeRadS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballAngularVelocityAfterRadS{{0.0, 0.0, 0.0}};
    int microtraceSchemaVersion = 1;
    std::vector<CueContactMicrostep> microsteps;
};

CueContactResult resolveCueContact(BallState& ball, const CueImpactInput& input,
    const BallProperties& ballProperties, const CueProperties& cueProperties);
const char* cueContactRegimeName(CueContactRegime regime);

}  // namespace billiardgl
