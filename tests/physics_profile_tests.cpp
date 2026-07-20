#include "physics_profile.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool close(float first, float second)
{
    return std::fabs(first - second) <= 0.000001f;
}

}  // namespace

int main()
{
    const billiardgl::PhysicsProfile production =
        billiardgl::defaultChinesePoolPhysicsProfile();
    expect(production.id == "chinese_pool_legacy_v1",
        "production default must remain on the authorized legacy baseline");
    expect(production.formulaVersion == "legacy_v1",
        "production default formula must remain on the authorized legacy baseline");
    expect(!production.frozenCueContact.enabled,
        "production default must not enable rejected v5 cue contact");

    const billiardgl::PhysicsProfile interactive =
        billiardgl::interactiveChinesePoolPhysicsProfile();
    expect(billiardgl::validatePhysicsProfile(interactive).ok,
        "interactive profile must be valid");
    expect(interactive.id == "chinese_pool_interactive_120hz_v1",
        "interactive profile has a distinct traceable ID");
    expect(close(interactive.solver.timeStepSeconds, 1.0f / 120.0f),
        "interactive profile runs physics at 120 Hz");
    expect(close(production.solver.timeStepSeconds, 0.1f),
        "evidence replay baseline keeps its explicit 100 ms step");
    expect(interactive.formulaVersion == production.formulaVersion &&
        interactive.surface.material == production.surface.material &&
        interactive.cushion.material == production.cushion.material,
        "interactive timing must not silently change the physical model");

    const billiardgl::PhysicsProfile profile =
        billiardgl::phase3V5CandidatePhysicsProfile();
    expect(billiardgl::validatePhysicsProfile(profile).ok,
        "preserved v5 candidate profile is valid");
    expect(profile.id == "chinese_pool_full_game_v5",
        "candidate ID");
    expect(profile.formulaVersion == "phase3_integrated_v5_coupled_cue_contact_v1",
        "formula version");
    expect(profile.ball.radiusCm == billiardgl::kChineseBallRadiusCm,
        "ball radius unit");
    expect(profile.ball.massKg == 0.17f, "legacy telemetry mass");
    expect(close(profile.ball.inertiaFactor, 0.4f),
        "solid sphere inertia compatibility default");
    expect(close(profile.ball.normalRestitution, 0.97f),
        "series-balanced spent-data restitution fit");
    expect(close(profile.ball.frictionCoefficient, 0.1f),
        "series-balanced spent-data friction fit");
    expect(close(profile.cushion.normalRestitution, 0.93f) &&
        close(profile.cushion.restitutionIntercept, 1.0f) &&
        close(profile.cushion.restitutionSlopePerMps, 0.056f) &&
        close(profile.cushion.minimumRestitution, 0.0f) &&
        close(profile.cushion.maximumRestitution, 0.93f) &&
        close(profile.cushion.frictionCoefficient, 0.14f) &&
        close(profile.cushion.noseHeightRatio, 1.4f) &&
        close(profile.cushion.maximumRigidIncidentSpeedCmS, 250.0f),
        "frozen cushion calibration values");
    expect(profile.cushion.material == "mathavan_speed_dependent_cushion_v2",
        "source cushion material is explicit");
    expect(close(profile.tableBoundary.playfieldWidthCm, 127.0f) &&
        close(profile.tableBoundary.playfieldLengthCm, 254.0f) &&
        close(profile.tableBoundary.cornerMouthWidthCm, 13.2f) &&
        close(profile.tableBoundary.sideMouthWidthCm, 8.6f) &&
        close(profile.tableBoundary.cornerThroatWidthCm, 11.0f) &&
        close(profile.tableBoundary.sideThroatWidthCm, 7.0f) &&
        close(profile.tableBoundary.jawRadiusCm, 1.2f) &&
        close(profile.tableBoundary.throatDepthCm, 4.0f) &&
        close(profile.tableBoundary.captureDepthCm, 6.0f) &&
        profile.tableBoundary.geometryId == "wpa_pool_analytic_v1",
        "frozen pocket boundary geometry is explicit");
    expect(close(profile.surface.rollingResistanceAccelerationCmS2,
                 12.499999813735489f),
        "phase-correct spent-data rolling fit");
    expect(close(profile.surface.slidingFrictionCoefficient,
                 0.19999996439955606f),
        "phase-correct spent-data sliding fit");
    expect(profile.surface.material == "mathavan_phase_correct_surface_v2",
        "phase-correct surface material is explicit");
    expect(profile.surface.torsionalSpinDecelerationRadS2 == 0.0f,
        "sidespin decay remains unevidenced");
    expect(profile.solver.maximumIslandSize == 16 &&
        profile.solver.velocityIterations == 64 &&
        profile.solver.positionIterations == 4 &&
        close(profile.solver.maximumPenetrationCm, 0.5f),
        "deterministic multi-contact controls have safe defaults");
    expect(profile.solver.passiveEnergyToleranceJ == 1e-10,
        "passive energy tolerance is explicit in the frozen solver profile");
    billiardgl::PhysicsProfile invalidEnergyTolerance = profile;
    invalidEnergyTolerance.solver.passiveEnergyToleranceJ = -1.0;
    expect(!billiardgl::validatePhysicsProfile(invalidEnergyTolerance).ok,
        "negative passive energy tolerance must be rejected");
    billiardgl::PhysicsProfile invalidSolver = profile;
    invalidSolver.solver.maximumPenetrationCm =
        invalidSolver.solver.penetrationSlopCm;
    expect(!billiardgl::validatePhysicsProfile(invalidSolver).ok,
        "maximum penetration must exceed positional slop");
    billiardgl::PhysicsProfile invalidBoundary = profile;
    invalidBoundary.tableBoundary.cornerThroatWidthCm =
        invalidBoundary.tableBoundary.cornerMouthWidthCm + 1.0f;
    expect(!billiardgl::validatePhysicsProfile(invalidBoundary).ok,
        "a throat cannot be wider than its mouth");
    invalidBoundary = profile;
    invalidBoundary.tableBoundary.captureDepthCm =
        invalidBoundary.tableBoundary.throatDepthCm - 1.0f;
    expect(!billiardgl::validatePhysicsProfile(invalidBoundary).ok,
        "capture depth cannot precede throat depth");
    expect(close(profile.cue.normalRestitution, 0.0f),
        "cue normal restitution compatibility default");
    expect(close(profile.cue.chalkedFrictionCoefficient, 0.6f),
        "chalked cue friction hypothesis");
    expect(close(profile.cue.unchalkedFrictionCoefficient, 0.1f),
        "unchalked cue friction hypothesis");
    expect(close(profile.cue.maximumReliableOffsetRadius, 0.8f),
        "miscue reliability boundary");
    expect(close(profile.cue.cueSpeedPerPowerUnitCmS, 1.34f),
        "versioned shot power mapping");

    billiardgl::PhysicsProfile coupled = profile;
    coupled.frozenCueContact.enabled = true;
    coupled.frozenCueContact.normalStiffnessNPerM32 = 1.25e7;
    coupled.frozenCueContact.normalDissipationSPerM = 0.05;
    coupled.frozenCueContact.tangentialStiffnessNPerM = 4.0e5;
    coupled.frozenCueContact.tangentialDampingNsPerM = 25.0;
    coupled.frozenCueContact.microstepSeconds = 0.00001;
    coupled.frozenCueContact.maximumContactSeconds = 0.006;
    coupled.frozenCueContact.releaseCompressionM = 1e-8;
    coupled.frozenCueContact.maximumCompressionM = 0.004;
    coupled.frozenCueContact.maximumNormalForceN = 10000.0;
    expect(billiardgl::validatePhysicsProfile(coupled).ok,
        "finite ordered frozen-contact controls are valid");
    const std::string coupledText =
        billiardgl::canonicalPhysicsProfileText(coupled);
    expect(coupledText.find(
        "frozen_cue_contact.normal_stiffness_n_per_m32=12500000") !=
        std::string::npos,
        "canonical profile binds normal stiffness with units");
    coupled.frozenCueContact.microstepSeconds =
        coupled.frozenCueContact.maximumContactSeconds;
    expect(!billiardgl::validatePhysicsProfile(coupled).ok,
        "microstep must be smaller than maximum contact duration");

    expect(billiardgl::canonicalPhysicsProfileText(profile) ==
        billiardgl::canonicalPhysicsProfileText(profile),
        "deterministic serialization");

    billiardgl::PhysicsProfile invalid = profile;
    invalid.ball.massKg = std::numeric_limits<float>::quiet_NaN();
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "nonfinite mass rejected");

    invalid = profile;
    invalid.id = "../unsafe";
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "unsafe ID rejected");

    invalid = profile;
    invalid.ball.inertiaFactor = 0.0f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "nonpositive inertia factor rejected");

    invalid = profile;
    invalid.ball.normalRestitution = 1.01f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "ball restitution above one rejected");

    invalid = profile;
    invalid.ball.frictionCoefficient = -0.1f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "negative ball contact friction rejected");

    invalid = profile;
    invalid.cushion.noseHeightRatio = 2.0f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "invalid cushion nose height rejected");

    invalid = profile;
    invalid.cushion.maximumRigidIncidentSpeedCmS = 0.0f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "nonpositive rigid cushion speed domain rejected");

    invalid = profile;
    invalid.cushion.restitutionSlopePerMps = -0.1f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "negative cushion restitution slope rejected");

    invalid = profile;
    invalid.cushion.minimumRestitution = 0.9f;
    invalid.cushion.maximumRestitution = 0.8f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "cushion restitution clamp bounds must be ordered");

    invalid = profile;
    invalid.cushion.material = "../unsafe";
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "unsafe cushion material rejected");

    invalid = profile;
    invalid.surface.slidingFrictionCoefficient = -0.1f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "negative friction rejected");

    invalid = profile;
    invalid.cue.maximumReliableOffsetRadius = 1.0f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "cue offset boundary must remain inside the ball radius");
    return 0;
}
