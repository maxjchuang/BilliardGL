#pragma once

#include "table_specs.h"

#include <string>

namespace billiardgl {

struct BallProperties {
    float radiusCm = kChineseBallRadiusCm;
    float massKg = 0.17f;
    float inertiaFactor = 0.4f;
    float normalRestitution = 1.0f;
    float frictionCoefficient = 0.0f;
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
    float normalRestitution = 0.0f;
    float chalkedFrictionCoefficient = 0.6f;
    float unchalkedFrictionCoefficient = 0.1f;
    float maximumReliableOffsetRadius = 0.8f;
    float cueSpeedPerPowerUnitCmS = 1.34f;
};

struct CushionProperties {
    float normalRestitution = 1.0f;
    float frictionCoefficient = 0.0f;
    float noseHeightRatio = 1.0f;
    float maximumRigidIncidentSpeedCmS = 1000000000.0f;
    std::string material = "legacy_rigid_rail";
};

struct TableBoundaryProperties {
    float playfieldWidthCm = kChinesePlayfieldWidthCm;
    float playfieldLengthCm = kChinesePlayfieldLengthCm;
    float cornerMouthWidthCm = kChineseCornerPocketMouthWidthCm;
    float sideMouthWidthCm = kChineseSidePocketMouthWidthCm;
    float cornerThroatWidthCm = kChineseCornerPocketMouthWidthCm;
    float sideThroatWidthCm = kChineseSidePocketMouthWidthCm;
    float jawRadiusCm = 0.0001f;
    float throatDepthCm = kChinesePocketDropZoneDepthCm;
    float captureDepthCm = kChinesePocketDropZoneDepthCm;
    std::string geometryId = "legacy_opening_band";
    std::string material = "legacy_table_boundary";
};

struct SolverSettings {
    float timeStepSeconds = 0.1f;
    int maximumEventsPerTick = 64;
    float toiToleranceSeconds = 0.0000001f;
    int maximumIslandSize = 16;
    int velocityIterations = 12;
    int positionIterations = 4;
    float penetrationSlopCm = 0.001f;
    float maximumPenetrationCm = 0.5f;
    float residualToleranceCmS = 0.001f;
};

struct PhysicsProfile {
    std::string id;
    std::string formulaVersion;
    BallProperties ball;
    SurfaceProperties surface;
    CueProperties cue;
    CushionProperties cushion;
    TableBoundaryProperties tableBoundary;
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
