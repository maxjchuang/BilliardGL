#pragma once

#include "cue_impact.h"
#include "physics_profile.h"

#include <cstdint>
#include <string>

namespace billiardgl {

enum class ShotControlSchemaVersion : std::uint32_t {
    LegacyYawPowerV1 = 1,
    IntentV2 = 2,
};

enum class ShotPowerMode : std::uint8_t {
    RequestedPower,
    StrokeSpeedCmS,
};

enum class ChalkIntent : std::uint8_t {
    DefaultChalked,
    Chalked,
    Unchalked,
};

enum class AssistLevel : std::uint8_t {
    Off,
    Basic,
    Training,
};

struct ShotControlModel {
    ShotControlSchemaVersion schemaVersion = ShotControlSchemaVersion::IntentV2;
    float yawRadians = 1.5707963f;
    // Tip offsets are measured in ball-radius units, matching CueImpactInput.
    float tipOffsetX = 0.0f;
    float tipOffsetY = 0.0f;
    ShotPowerMode powerMode = ShotPowerMode::RequestedPower;
    float requestedPower = 60.0f;
    float strokeSpeedCmS = 0.0f;
    float cueElevationDegrees = 0.0f;
    ChalkIntent chalk = ChalkIntent::DefaultChalked;
    AssistLevel assistLevel = AssistLevel::Basic;
};

struct ShotControlCapabilities {
    bool cueElevation = false;
    bool chalkState = false;
};

struct ShotControlVisibility {
    bool cueElevation = false;
    bool chalkState = false;
};

struct ShotCommand {
    std::uint32_t schemaVersion = 0;
    CueImpactInput cueImpact;
    AssistLevel assistLevel = AssistLevel::Basic;
};

struct ShotControlConversion {
    bool ok = false;
    ShotCommand command;
    ShotControlVisibility visibleControls;
    std::string error;
};

ShotControlModel legacyShotControls(float yawRadians, float shotPower) noexcept;

// This is the single conversion boundary for the versioned model. UI, cue
// presentation and runtime commands consume this result rather than deriving
// direction, offsets or speed independently.
ShotControlConversion convertShotControls(
    const ShotControlModel& model,
    const ShotControlCapabilities& capabilities,
    const PhysicsProfile& profile) noexcept;

}  // namespace billiardgl
