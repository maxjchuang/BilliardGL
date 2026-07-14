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
    const billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    expect(billiardgl::validatePhysicsProfile(profile).ok,
        "default profile is valid");
    expect(profile.id == "chinese_pool_full_game_v1",
        "candidate ID");
    expect(profile.formulaVersion == "full_game_integration_v1",
        "formula version");
    expect(profile.ball.radiusCm == billiardgl::kChineseBallRadiusCm,
        "ball radius unit");
    expect(profile.ball.massKg == 0.17f, "legacy telemetry mass");
    expect(close(profile.ball.inertiaFactor, 0.4f),
        "solid sphere inertia compatibility default");
    expect(close(profile.ball.normalRestitution, 0.36f),
        "Domenech billiard calibration restitution");
    expect(close(profile.ball.frictionCoefficient, 0.25f),
        "Domenech billiard calibration friction");
    expect(close(profile.cushion.normalRestitution, 0.9248723120650503f) &&
        close(profile.cushion.frictionCoefficient, 0.14f) &&
        close(profile.cushion.noseHeightRatio, 1.4f) &&
        close(profile.cushion.maximumRigidIncidentSpeedCmS, 250.0f),
        "frozen cushion calibration values");
    expect(profile.cushion.material == "riley_renaissance_snooker_cushion",
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
    expect(close(profile.surface.rollingResistanceAccelerationCmS2, 12.5f),
        "Mathavan rolling calibration midpoint");
    expect(close(profile.surface.slidingFrictionCoefficient, 0.20f),
        "preregistered independent sliding hypothesis");
    expect(profile.surface.torsionalSpinDecelerationRadS2 == 0.0f,
        "sidespin decay remains unevidenced");
    expect(profile.solver.maximumIslandSize == 16 &&
        profile.solver.velocityIterations == 64 &&
        profile.solver.positionIterations == 4 &&
        close(profile.solver.maximumPenetrationCm, 0.5f),
        "deterministic multi-contact controls have safe defaults");
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
