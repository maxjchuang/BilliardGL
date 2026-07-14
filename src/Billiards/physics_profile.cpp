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
    profile.id = "chinese_pool_legacy_v1";
    profile.formulaVersion = "legacy_v1";
    return profile;
}

PhysicsProfileValidation validatePhysicsProfile(const PhysicsProfile& profile)
{
    if (!safeId(profile.id)) return invalid("physics profile id is not a safe stable ID");
    if (!safeId(profile.formulaVersion)) {
        return invalid("physics formula version is not a safe stable ID");
    }
    if (!safeId(profile.ball.material) || !safeId(profile.surface.material)) {
        return invalid("physics material is not a safe stable ID");
    }
    if (!std::isfinite(profile.ball.radiusCm) || profile.ball.radiusCm <= 0.0f) {
        return invalid("ball radius_cm must be finite and positive");
    }
    if (!std::isfinite(profile.ball.massKg) || profile.ball.massKg <= 0.0f) {
        return invalid("ball mass_kg must be finite and positive");
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
    if (!std::isfinite(profile.cushion.normalRestitution) ||
        profile.cushion.normalRestitution < 0.0f ||
        profile.cushion.normalRestitution > 1.0f) {
        return invalid("cushion normal restitution must be between zero and one");
    }
    if (!finiteNonnegative(profile.cushion.frictionCoefficient)) {
        return invalid("cushion friction coefficient must be finite and nonnegative");
    }
    if (!std::isfinite(profile.solver.timeStepSeconds) ||
        profile.solver.timeStepSeconds <= 0.0f) {
        return invalid("solver time_step_seconds must be finite and positive");
    }
    if (profile.solver.maximumEventsPerTick <= 0) {
        return invalid("solver maximum_events_per_tick must be positive");
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
        << "cushion.normal_restitution=" << profile.cushion.normalRestitution << '\n'
        << "cushion.friction_coefficient=" << profile.cushion.frictionCoefficient << '\n'
        << "solver.time_step_seconds=" << profile.solver.timeStepSeconds << '\n'
        << "solver.maximum_events_per_tick=" << profile.solver.maximumEventsPerTick << '\n';
    return output.str();
}

}  // namespace billiardgl
