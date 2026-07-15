#include "physics_profile.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace billiardgl {
namespace {

bool safeId(const std::string& value)
{
    if (value.empty() || !std::isalnum(static_cast<unsigned char>(value[0]))) {
        return false;
    }
    for (char character : value) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (!std::isalnum(byte) && character != '_' && character != '-' && character != '.') {
            return false;
        }
    }
    return true;
}

bool finiteNonnegative(float value)
{
    return std::isfinite(value) && value >= 0.0f;
}

PhysicsProfileValidation invalid(const std::string& error)
{
    PhysicsProfileValidation result;
    result.error = error;
    return result;
}

}  // namespace

PhysicsProfile defaultChinesePoolPhysicsProfile()
{
    PhysicsProfile profile;
#include "generated/phase3_v3_profile.inc"
    return profile;
}

PhysicsProfileValidation validatePhysicsProfile(const PhysicsProfile& profile)
{
    if (!safeId(profile.id)) return invalid("physics profile id is not a safe stable ID");
    if (!safeId(profile.formulaVersion)) {
        return invalid("physics formula version is not a safe stable ID");
    }
    if (!safeId(profile.ball.material) || !safeId(profile.surface.material) ||
        !safeId(profile.cushion.material) ||
        !safeId(profile.tableBoundary.geometryId) ||
        !safeId(profile.tableBoundary.material)) {
        return invalid("physics material is not a safe stable ID");
    }
    if (!std::isfinite(profile.ball.radiusCm) || profile.ball.radiusCm <= 0.0f) {
        return invalid("ball radius_cm must be finite and positive");
    }
    if (!std::isfinite(profile.ball.massKg) || profile.ball.massKg <= 0.0f) {
        return invalid("ball mass_kg must be finite and positive");
    }
    if (!std::isfinite(profile.ball.inertiaFactor) || profile.ball.inertiaFactor <= 0.0f) {
        return invalid("ball inertia factor must be finite and positive");
    }
    if (!std::isfinite(profile.ball.normalRestitution) ||
        profile.ball.normalRestitution < 0.0f || profile.ball.normalRestitution > 1.0f) {
        return invalid("ball normal restitution must be between zero and one");
    }
    if (!finiteNonnegative(profile.ball.frictionCoefficient)) {
        return invalid("ball friction coefficient must be finite and nonnegative");
    }
    if (!finiteNonnegative(profile.surface.legacyFrictionAccelerationCmS2)) {
        return invalid("legacy friction acceleration_cm_s2 must be finite and nonnegative");
    }
    if (!finiteNonnegative(profile.surface.slidingFrictionCoefficient)) {
        return invalid("sliding friction coefficient must be finite and nonnegative");
    }
    if (!finiteNonnegative(profile.surface.rollingResistanceAccelerationCmS2)) {
        return invalid("rolling resistance acceleration_cm_s2 must be finite and nonnegative");
    }
    if (!finiteNonnegative(profile.surface.torsionalSpinDecelerationRadS2)) {
        return invalid("torsional spin deceleration_rad_s2 must be finite and nonnegative");
    }
    if (!finiteNonnegative(profile.surface.slipSpeedEpsilonCmS)) {
        return invalid("slip speed epsilon_cm_s must be finite and nonnegative");
    }
    if (!finiteNonnegative(profile.surface.stopEnergyThresholdJ)) {
        return invalid("stop energy threshold_j must be finite and nonnegative");
    }
    if (!std::isfinite(profile.cue.effectiveMassKg) || profile.cue.effectiveMassKg <= 0.0f) {
        return invalid("cue effective mass_kg must be finite and positive");
    }
    if (!std::isfinite(profile.cue.normalRestitution) ||
        profile.cue.normalRestitution < 0.0f || profile.cue.normalRestitution > 1.0f) {
        return invalid("cue normal restitution must be between zero and one");
    }
    if (!finiteNonnegative(profile.cue.chalkedFrictionCoefficient) ||
        !finiteNonnegative(profile.cue.unchalkedFrictionCoefficient)) {
        return invalid("cue friction coefficients must be finite and nonnegative");
    }
    if (!std::isfinite(profile.cue.maximumReliableOffsetRadius) ||
        profile.cue.maximumReliableOffsetRadius <= 0.0f ||
        profile.cue.maximumReliableOffsetRadius >= 1.0f) {
        return invalid("cue maximum reliable offset radius must be between zero and one");
    }
    if (!std::isfinite(profile.cue.cueSpeedPerPowerUnitCmS) ||
        profile.cue.cueSpeedPerPowerUnitCmS <= 0.0f) {
        return invalid("cue speed per power unit must be finite and positive");
    }
    if (!std::isfinite(profile.cushion.normalRestitution) ||
        profile.cushion.normalRestitution < 0.0f ||
        profile.cushion.normalRestitution > 1.0f) {
        return invalid("cushion normal restitution must be between zero and one");
    }
    if (!std::isfinite(profile.cushion.restitutionIntercept) ||
        !finiteNonnegative(profile.cushion.restitutionSlopePerMps) ||
        !std::isfinite(profile.cushion.minimumRestitution) ||
        !std::isfinite(profile.cushion.maximumRestitution) ||
        profile.cushion.minimumRestitution < 0.0f ||
        profile.cushion.minimumRestitution > profile.cushion.maximumRestitution ||
        profile.cushion.maximumRestitution > 1.0f) {
        return invalid("cushion affine restitution law is invalid");
    }
    if (!finiteNonnegative(profile.cushion.frictionCoefficient)) {
        return invalid("cushion friction coefficient must be finite and nonnegative");
    }
    if (!std::isfinite(profile.cushion.noseHeightRatio) ||
        profile.cushion.noseHeightRatio <= 0.0f ||
        profile.cushion.noseHeightRatio >= 2.0f) {
        return invalid("cushion nose height ratio must be between zero and two");
    }
    if (!std::isfinite(profile.cushion.maximumRigidIncidentSpeedCmS) ||
        profile.cushion.maximumRigidIncidentSpeedCmS <= 0.0f) {
        return invalid("cushion maximum rigid incident speed must be finite and positive");
    }
    const TableBoundaryProperties& boundary = profile.tableBoundary;
    if (!std::isfinite(boundary.playfieldWidthCm) || boundary.playfieldWidthCm <= 0.0f ||
        !std::isfinite(boundary.playfieldLengthCm) || boundary.playfieldLengthCm <= 0.0f ||
        !std::isfinite(boundary.cornerMouthWidthCm) || boundary.cornerMouthWidthCm <= 0.0f ||
        !std::isfinite(boundary.sideMouthWidthCm) || boundary.sideMouthWidthCm <= 0.0f ||
        !std::isfinite(boundary.cornerThroatWidthCm) || boundary.cornerThroatWidthCm <= 0.0f ||
        !std::isfinite(boundary.sideThroatWidthCm) || boundary.sideThroatWidthCm <= 0.0f ||
        !std::isfinite(boundary.jawRadiusCm) || boundary.jawRadiusCm <= 0.0f ||
        !std::isfinite(boundary.throatDepthCm) || boundary.throatDepthCm <= 0.0f ||
        !std::isfinite(boundary.captureDepthCm) ||
        boundary.captureDepthCm < boundary.throatDepthCm) {
        return invalid("table boundary dimensions and depths must be finite, positive, and ordered");
    }
    if (boundary.cornerThroatWidthCm > boundary.cornerMouthWidthCm ||
        boundary.sideThroatWidthCm > boundary.sideMouthWidthCm ||
        boundary.cornerMouthWidthCm >= boundary.playfieldWidthCm ||
        boundary.sideMouthWidthCm >= boundary.playfieldLengthCm) {
        return invalid("table boundary throat and mouth widths are inconsistent");
    }
    if (!std::isfinite(profile.solver.timeStepSeconds) ||
        profile.solver.timeStepSeconds <= 0.0f) {
        return invalid("solver time_step_seconds must be finite and positive");
    }
    if (!std::isfinite(profile.solver.passiveEnergyToleranceJ) ||
        profile.solver.passiveEnergyToleranceJ < 0.0) {
        return invalid("solver passive_energy_tolerance_j must be finite and nonnegative");
    }
    if (profile.solver.maximumEventsPerTick <= 0) {
        return invalid("solver maximum_events_per_tick must be positive");
    }
    if (!std::isfinite(profile.solver.toiToleranceSeconds) ||
        profile.solver.toiToleranceSeconds < 0.0f ||
        profile.solver.toiToleranceSeconds >= profile.solver.timeStepSeconds ||
        profile.solver.maximumIslandSize < 1 ||
        profile.solver.maximumIslandSize > 1024 ||
        profile.solver.velocityIterations < 1 ||
        profile.solver.positionIterations < 1 ||
        !finiteNonnegative(profile.solver.penetrationSlopCm) ||
        !std::isfinite(profile.solver.maximumPenetrationCm) ||
        profile.solver.maximumPenetrationCm <= profile.solver.penetrationSlopCm ||
        !finiteNonnegative(profile.solver.residualToleranceCmS)) {
        return invalid("multi-contact solver controls are invalid or inconsistent");
    }
    PhysicsProfileValidation result;
    result.ok = true;
    return result;
}

std::string canonicalPhysicsProfileText(const PhysicsProfile& profile)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10)
        << "id=" << profile.id << '\n'
        << "formula_version=" << profile.formulaVersion << '\n'
        << "ball.radius_cm=" << profile.ball.radiusCm << '\n'
        << "ball.mass_kg=" << profile.ball.massKg << '\n'
        << "ball.inertia_factor=" << profile.ball.inertiaFactor << '\n'
        << "ball.normal_restitution=" << profile.ball.normalRestitution << '\n'
        << "ball.friction_coefficient=" << profile.ball.frictionCoefficient << '\n'
        << "ball.material=" << profile.ball.material << '\n'
        << "surface.legacy_friction_acceleration_cm_s2="
        << profile.surface.legacyFrictionAccelerationCmS2 << '\n'
        << "surface.sliding_friction_coefficient="
        << profile.surface.slidingFrictionCoefficient << '\n'
        << "surface.rolling_resistance_acceleration_cm_s2="
        << profile.surface.rollingResistanceAccelerationCmS2 << '\n'
        << "surface.torsional_spin_deceleration_rad_s2="
        << profile.surface.torsionalSpinDecelerationRadS2 << '\n'
        << "surface.slip_speed_epsilon_cm_s="
        << profile.surface.slipSpeedEpsilonCmS << '\n'
        << "surface.stop_energy_threshold_j="
        << profile.surface.stopEnergyThresholdJ << '\n'
        << "surface.material=" << profile.surface.material << '\n'
        << "cue.effective_mass_kg=" << profile.cue.effectiveMassKg << '\n'
        << "cue.normal_restitution=" << profile.cue.normalRestitution << '\n'
        << "cue.chalked_friction_coefficient="
        << profile.cue.chalkedFrictionCoefficient << '\n'
        << "cue.unchalked_friction_coefficient="
        << profile.cue.unchalkedFrictionCoefficient << '\n'
        << "cue.maximum_reliable_offset_radius="
        << profile.cue.maximumReliableOffsetRadius << '\n'
        << "cue.cue_speed_per_power_unit_cm_s="
        << profile.cue.cueSpeedPerPowerUnitCmS << '\n'
        << "cushion.normal_restitution=" << profile.cushion.normalRestitution << '\n'
        << "cushion.restitution_intercept="
        << profile.cushion.restitutionIntercept << '\n'
        << "cushion.restitution_slope_per_mps="
        << profile.cushion.restitutionSlopePerMps << '\n'
        << "cushion.minimum_restitution="
        << profile.cushion.minimumRestitution << '\n'
        << "cushion.maximum_restitution="
        << profile.cushion.maximumRestitution << '\n'
        << "cushion.friction_coefficient=" << profile.cushion.frictionCoefficient << '\n'
        << "cushion.nose_height_ratio=" << profile.cushion.noseHeightRatio << '\n'
        << "cushion.maximum_rigid_incident_speed_cm_s="
        << profile.cushion.maximumRigidIncidentSpeedCmS << '\n'
        << "cushion.material=" << profile.cushion.material << '\n'
        << "table_boundary.playfield_width_cm="
        << profile.tableBoundary.playfieldWidthCm << '\n'
        << "table_boundary.playfield_length_cm="
        << profile.tableBoundary.playfieldLengthCm << '\n'
        << "table_boundary.corner_mouth_width_cm="
        << profile.tableBoundary.cornerMouthWidthCm << '\n'
        << "table_boundary.side_mouth_width_cm="
        << profile.tableBoundary.sideMouthWidthCm << '\n'
        << "table_boundary.corner_throat_width_cm="
        << profile.tableBoundary.cornerThroatWidthCm << '\n'
        << "table_boundary.side_throat_width_cm="
        << profile.tableBoundary.sideThroatWidthCm << '\n'
        << "table_boundary.jaw_radius_cm="
        << profile.tableBoundary.jawRadiusCm << '\n'
        << "table_boundary.throat_depth_cm="
        << profile.tableBoundary.throatDepthCm << '\n'
        << "table_boundary.capture_depth_cm="
        << profile.tableBoundary.captureDepthCm << '\n'
        << "table_boundary.geometry_id="
        << profile.tableBoundary.geometryId << '\n'
        << "table_boundary.material="
        << profile.tableBoundary.material << '\n'
        << "solver.time_step_seconds=" << profile.solver.timeStepSeconds << '\n'
        << "solver.maximum_events_per_tick=" << profile.solver.maximumEventsPerTick << '\n'
        << "solver.toi_tolerance_seconds=" << profile.solver.toiToleranceSeconds << '\n'
        << "solver.maximum_island_size=" << profile.solver.maximumIslandSize << '\n'
        << "solver.velocity_iterations=" << profile.solver.velocityIterations << '\n'
        << "solver.position_iterations=" << profile.solver.positionIterations << '\n'
        << "solver.penetration_slop_cm=" << profile.solver.penetrationSlopCm << '\n'
        << "solver.maximum_penetration_cm=" << profile.solver.maximumPenetrationCm << '\n'
        << "solver.residual_tolerance_cm_s=" << profile.solver.residualToleranceCmS << '\n'
        << "solver.passive_energy_tolerance_j="
        << profile.solver.passiveEnergyToleranceJ << '\n';
    return output.str();
}

std::string canonicalPhysicsProfileJson(const PhysicsProfile& profile)
{
    const auto quote = [](const std::string& value) {
        std::string result = "\"";
        for (const unsigned char byte : value) {
            switch (byte) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += static_cast<char>(byte); break;
            }
        }
        return result + "\"";
    };
    return "{\"id\":" + quote(profile.id) +
        ",\"formula_version\":" + quote(profile.formulaVersion) +
        ",\"canonical_text\":" + quote(canonicalPhysicsProfileText(profile)) + "}\n";
}

}  // namespace billiardgl
