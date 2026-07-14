#pragma once

#include "cue_impact.h"
#include "game_state.h"
#include "physics_profile.h"

#include <array>
#include <string>

namespace billiardgl {

enum class CueContactRegime {
    Unsupported,
    Stick,
    Slip,
    Miscue
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
};

CueContactResult resolveCueContact(BallState& ball, const CueImpactInput& input,
    const BallProperties& ballProperties, const CueProperties& cueProperties);
const char* cueContactRegimeName(CueContactRegime regime);

}  // namespace billiardgl
