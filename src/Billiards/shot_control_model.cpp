#include "shot_control_model.h"

#include "shot.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

constexpr float kMaximumRequestedPower = 200.0f;
constexpr float kMaximumStrokeSpeedCmS = 2500.0f;
constexpr float kMaximumElevationDegrees = 85.0f;

bool finite(float value)
{
    return std::isfinite(value);
}

}  // namespace

ShotControlModel legacyShotControls(float yawRadians, float shotPower) noexcept
{
    ShotControlModel model;
    model.schemaVersion = ShotControlSchemaVersion::LegacyYawPowerV1;
    model.yawRadians = yawRadians;
    model.requestedPower = shotPower;
    model.powerMode = ShotPowerMode::RequestedPower;
    model.tipOffsetX = 0.0f;
    model.tipOffsetY = 0.0f;
    model.cueElevationDegrees = 0.0f;
    model.chalk = ChalkIntent::DefaultChalked;
    return model;
}

ShotControlConversion convertShotControls(
    const ShotControlModel& model,
    const ShotControlCapabilities& capabilities,
    const PhysicsProfile& profile) noexcept
{
    ShotControlConversion result;
    const bool intentV2 = model.schemaVersion == ShotControlSchemaVersion::IntentV2;
    result.visibleControls.cueElevation = intentV2 && capabilities.cueElevation;
    result.visibleControls.chalkState = intentV2 && capabilities.chalkState;
    result.command.schemaVersion = static_cast<std::uint32_t>(model.schemaVersion);
    result.command.assistLevel = model.assistLevel;

    if (!finite(model.yawRadians) || !finite(model.tipOffsetX) ||
        !finite(model.tipOffsetY) || !finite(model.requestedPower) ||
        !finite(model.strokeSpeedCmS) || !finite(model.cueElevationDegrees)) {
        result.error = "shot_control_nonfinite";
        return result;
    }
    if (model.schemaVersion != ShotControlSchemaVersion::LegacyYawPowerV1 &&
        model.schemaVersion != ShotControlSchemaVersion::IntentV2) {
        result.error = "shot_control_schema_unsupported";
        return result;
    }
    if (model.requestedPower < 0.0f || model.requestedPower > kMaximumRequestedPower) {
        result.error = "requested_power_out_of_range";
        return result;
    }
    if (model.schemaVersion == ShotControlSchemaVersion::LegacyYawPowerV1) {
        if (model.powerMode != ShotPowerMode::RequestedPower) {
            result.error = "legacy_shot_requires_requested_power";
            return result;
        }
        if (std::fabs(model.tipOffsetX) > 0.000001f ||
            std::fabs(model.tipOffsetY) > 0.000001f ||
            std::fabs(model.cueElevationDegrees) > 0.000001f ||
            model.chalk != ChalkIntent::DefaultChalked) {
            result.error = "legacy_shot_contains_v2_fields";
            return result;
        }
        result.command.cueImpact = cueImpactFromShotControls(
            model.yawRadians, model.requestedPower, profile);
        result.ok = true;
        return result;
    }

    if (std::fabs(model.cueElevationDegrees) > 0.000001f && !capabilities.cueElevation) {
        result.error = "cue_elevation_capability_required";
        return result;
    }
    if (std::fabs(model.cueElevationDegrees) > kMaximumElevationDegrees) {
        result.error = "cue_elevation_out_of_range";
        return result;
    }
    if (model.chalk == ChalkIntent::Unchalked && !capabilities.chalkState) {
        result.error = "chalk_state_capability_required";
        return result;
    }
    const float offsetMagnitude = std::hypot(model.tipOffsetX, model.tipOffsetY);
    if (offsetMagnitude > profile.cue.maximumReliableOffsetRadius + 0.000001f) {
        result.error = "tip_offset_out_of_range";
        return result;
    }
    if (model.powerMode == ShotPowerMode::StrokeSpeedCmS &&
        (model.strokeSpeedCmS < 0.0f || model.strokeSpeedCmS > kMaximumStrokeSpeedCmS)) {
        result.error = "stroke_speed_out_of_range";
        return result;
    }

    CueImpactInput input = cueImpactFromShotControls(
        model.yawRadians,
        model.powerMode == ShotPowerMode::RequestedPower ? model.requestedPower : 0.0f,
        profile);
    if (model.powerMode == ShotPowerMode::StrokeSpeedCmS)
        input.cueSpeedCmS = model.strokeSpeedCmS;
    input.elevationDegrees = model.cueElevationDegrees;
    input.tipOffsetRadius = {{model.tipOffsetX, model.tipOffsetY}};
    input.tipOffsetCm = {{
        static_cast<double>(model.tipOffsetX) * profile.ball.radiusCm,
        static_cast<double>(model.tipOffsetY) * profile.ball.radiusCm,
    }};
    input.chalkState = model.chalk == ChalkIntent::Unchalked ? "UNCHALKED" : "CHALKED";
    result.command.cueImpact = input;
    result.ok = true;
    return result;
}

}  // namespace billiardgl
