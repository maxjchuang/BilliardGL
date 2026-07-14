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
    expect(profile.id == "chinese_pool_surface_motion_v1",
        "candidate ID");
    expect(profile.formulaVersion == "surface_motion_v1",
        "formula version");
    expect(profile.ball.radiusCm == billiardgl::kChineseBallRadiusCm,
        "ball radius unit");
    expect(profile.ball.massKg == 0.17f, "legacy telemetry mass");
    expect(close(profile.surface.rollingResistanceAccelerationCmS2, 12.5f),
        "Mathavan rolling calibration midpoint");
    expect(close(profile.surface.slidingFrictionCoefficient, 0.20f),
        "preregistered independent sliding hypothesis");
    expect(profile.surface.torsionalSpinDecelerationRadS2 == 0.0f,
        "sidespin decay remains unevidenced");
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
    invalid.surface.slidingFrictionCoefficient = -0.1f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok,
        "negative friction rejected");
    return 0;
}
