#pragma once

#include "table_specs.h"

#include <string>

namespace billiardgl {

struct BallProperties {
    float radiusCm = kChineseBallRadiusCm;
    float massKg = 0.17f;
    std::string material = "phenolic_resin";
};

struct SurfaceProperties {
    float legacyFrictionAccelerationCmS2 = 4.0f;
    float slidingFrictionCoefficient = 0.0f;
    float rollingResistanceAccelerationCmS2 = 4.0f;
    float torsionalSpinDecelerationRadS2 = 0.0f;
    float slipSpeedEpsilonCmS = 0.0001f;
    float stopEnergyThresholdJ = 0.000000001f;
    std::string material = "production_cloth_legacy";
};

struct CueProperties {
    float effectiveMassKg = 0.5f;
};

struct CushionProperties {
    float normalRestitution = 1.0f;
    float frictionCoefficient = 0.0f;
};

struct SolverSettings {
    float timeStepSeconds = 0.1f;
    int maximumEventsPerTick = 64;
};

struct PhysicsProfile {
    std::string id;
    std::string formulaVersion;
    BallProperties ball;
    SurfaceProperties surface;
    CueProperties cue;
    CushionProperties cushion;
    SolverSettings solver;
};

struct PhysicsProfileValidation {
    bool ok = false;
    std::string error;
};

PhysicsProfile defaultChinesePoolPhysicsProfile();
PhysicsProfileValidation validatePhysicsProfile(const PhysicsProfile& profile);
std::string canonicalPhysicsProfileText(const PhysicsProfile& profile);

}  // namespace billiardgl
