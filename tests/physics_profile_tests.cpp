#include "physics_profile.h"

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

}  // namespace

int main()
{
    const billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    expect(billiardgl::validatePhysicsProfile(profile).ok,
        "default profile is valid");
    expect(profile.id == "chinese_pool_legacy_v1",
        "stable default profile ID");
    expect(profile.ball.radiusCm == billiardgl::kChineseBallRadiusCm,
        "ball radius unit");
    expect(profile.ball.massKg == 0.17f, "legacy telemetry mass");
    expect(profile.surface.legacyFrictionAccelerationCmS2 == 4.0f,
        "theme zero preserves legacy friction");
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
