#include "ball_ball_contact.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool close(double first, double second, double tolerance = 1e-5)
{
    return std::fabs(first - second) <= tolerance;
}

bool finite(const billiardgl::BallBallContactResult& result)
{
    const double scalars[] = {
        result.penetrationM, result.positionSlopM,
        result.normalImpulseNs, result.tangentialImpulseNs,
        result.normalRelativeSpeedBeforeMS, result.normalRelativeSpeedAfterMS,
        result.kineticEnergyBeforeJ, result.kineticEnergyAfterJ,
        result.restitution, result.frictionCoefficient,
    };
    for (double value : scalars) if (!std::isfinite(value)) return false;
    const std::array<double, 3>* vectors[] = {
        &result.contactNormal, &result.contactTangent,
        &result.firstContactArmM, &result.secondContactArmM,
        &result.relativeContactVelocityBeforeMS,
        &result.relativeContactVelocityAfterMS, &result.impulseOnSecondNs,
        &result.firstPositionCorrectionM, &result.secondPositionCorrectionM,
    };
    for (const std::array<double, 3>* vector : vectors) {
        for (double value : *vector) if (!std::isfinite(value)) return false;
    }
    return true;
}

billiardgl::BallState ball(double xCm, double zCm, double vxCmS, double vzCmS)
{
    billiardgl::BallState value;
    value.position = billiardgl::Point3(
        static_cast<float>(xCm), 0.0f, static_cast<float>(zCm));
    value.velocity = billiardgl::Point3(
        static_cast<float>(vxCmS), 0.0f, static_cast<float>(vzCmS));
    value.speed = static_cast<float>(std::hypot(vxCmS, vzCmS));
    return value;
}

double momentumX(const billiardgl::BallState& first,
    const billiardgl::BallState& second,
    const billiardgl::BallProperties& firstProperties,
    const billiardgl::BallProperties& secondProperties)
{
    return (firstProperties.massKg * first.velocity.x +
        secondProperties.massKg * second.velocity.x) / 100.0;
}

}  // namespace

int main()
{
    billiardgl::BallProperties equal;
    billiardgl::BallState first = ball(0.0, 0.0, 100.0, 0.0);
    billiardgl::BallState second = ball(5.715, 0.0, 0.0, 0.0);
    const billiardgl::BallBallContactResult elastic =
        billiardgl::resolveBallBallContact(first, second, equal, equal);
    expect(elastic.regime == billiardgl::BallBallContactRegime::Frictionless &&
        elastic.velocityImpulseApplied, "equal-mass elastic contact applies once");
    expect(close(first.velocity.x, 0.0) && close(second.velocity.x, 100.0),
        "equal-mass elastic head-on velocities exchange");
    expect(close(elastic.kineticEnergyBeforeJ, elastic.kineticEnergyAfterJ, 1e-8),
        "elastic head-on contact conserves energy");
    expect(elastic.normalRelativeSpeedBeforeMS < 0.0 &&
        elastic.normalRelativeSpeedAfterMS >= -1e-9,
        "approaching contact separates after impulse");

    billiardgl::BallProperties light = equal;
    billiardgl::BallProperties heavy = equal;
    light.massKg = 0.1f;
    light.radiusCm = 2.0f;
    light.normalRestitution = 0.8f;
    heavy.massKg = 0.3f;
    heavy.radiusCm = 3.0f;
    heavy.normalRestitution = 0.9f;
    first = ball(0.0, 0.0, 100.0, 0.0);
    second = ball(4.9, 0.0, 0.0, 0.0);
    const double momentumBefore = momentumX(first, second, light, heavy);
    const billiardgl::BallBallContactResult unequal =
        billiardgl::resolveBallBallContact(first, second, light, heavy);
    expect(close(unequal.restitution, 0.8) &&
        close(momentumBefore, momentumX(first, second, light, heavy), 1e-7),
        "unequal contact uses conservative restitution and conserves momentum");
    expect(unequal.firstPositionCorrectionM[0] < 0.0 &&
        unequal.secondPositionCorrectionM[0] > 0.0 &&
        std::fabs(unequal.firstPositionCorrectionM[0]) >
            std::fabs(unequal.secondPositionCorrectionM[0]),
        "penetration correction is inverse-mass weighted");
    expect(unequal.kineticEnergyAfterJ <= unequal.kineticEnergyBeforeJ + 1e-8,
        "inelastic unequal collision does not create energy");

    billiardgl::BallProperties rough = equal;
    rough.normalRestitution = 0.9f;
    rough.frictionCoefficient = 0.2f;
    first = ball(0.0, 0.0, 100.0, 40.0);
    second = ball(5.715, 0.0, 0.0, 0.0);
    const billiardgl::BallBallContactResult oblique =
        billiardgl::resolveBallBallContact(first, second, rough, rough);
    expect(oblique.regime == billiardgl::BallBallContactRegime::Slip ||
        oblique.regime == billiardgl::BallBallContactRegime::Stick,
        "rough oblique contact classifies tangent regime");
    expect(oblique.tangentialImpulseNs <=
        oblique.frictionCoefficient * oblique.normalImpulseNs + 1e-10,
        "tangential impulse stays inside Coulomb cone");
    expect(std::fabs(first.angularVelocity.y) > 0.0 &&
        std::fabs(second.angularVelocity.y) > 0.0,
        "oblique friction transfers angular momentum");
    expect(oblique.kineticEnergyAfterJ <= oblique.kineticEnergyBeforeJ + 1e-8,
        "rough oblique collision does not create energy");

    billiardgl::BallProperties lowInertia = rough;
    billiardgl::BallProperties highInertia = rough;
    lowInertia.inertiaFactor = 0.2f;
    highInertia.inertiaFactor = 0.8f;
    billiardgl::BallState lowFirst = ball(0.0, 0.0, 100.0, 40.0);
    billiardgl::BallState lowSecond = ball(5.715, 0.0, 0.0, 0.0);
    billiardgl::BallState highFirst = lowFirst;
    billiardgl::BallState highSecond = lowSecond;
    billiardgl::resolveBallBallContact(
        lowFirst, lowSecond, lowInertia, lowInertia);
    billiardgl::resolveBallBallContact(
        highFirst, highSecond, highInertia, highInertia);
    expect(std::fabs(lowFirst.angularVelocity.y) >
        std::fabs(highFirst.angularVelocity.y),
        "ball contact angular response should honor the configured inertia factor");

    billiardgl::BallProperties sticky = rough;
    sticky.frictionCoefficient = 1.0f;
    billiardgl::BallState stickFirst = ball(0.0, 0.0, 100.0, 5.0);
    billiardgl::BallState stickSecond = ball(5.715, 0.0, 0.0, 0.0);
    const billiardgl::BallBallContactResult stick =
        billiardgl::resolveBallBallContact(stickFirst, stickSecond, sticky, sticky);
    expect(stick.regime == billiardgl::BallBallContactRegime::Stick,
        "small tangential motion inside the cone sticks");

    billiardgl::BallState spinFirst = ball(0.0, 0.0, 100.0, 0.0);
    billiardgl::BallState spinSecond = ball(5.715, 0.0, 0.0, 0.0);
    spinFirst.angularVelocity.y = 20.0f;
    const billiardgl::BallBallContactResult spinTransfer =
        billiardgl::resolveBallBallContact(spinFirst, spinSecond, rough, rough);
    expect(spinTransfer.tangentialImpulseNs > 0.0 &&
        std::fabs(spinSecond.velocity.z) > 0.0,
        "surface spin transfers into tangential translation");

    billiardgl::BallState mirroredFirst = ball(0.0, 0.0, 100.0, -40.0);
    billiardgl::BallState mirroredSecond = ball(5.715, 0.0, 0.0, 0.0);
    const billiardgl::BallBallContactResult mirrored =
        billiardgl::resolveBallBallContact(
            mirroredFirst, mirroredSecond, rough, rough);
    expect(close(first.velocity.x, mirroredFirst.velocity.x) &&
        close(first.velocity.z, -mirroredFirst.velocity.z) &&
        close(first.angularVelocity.y, -mirroredFirst.angularVelocity.y),
        "left-right mirror produces mirrored output");

    billiardgl::BallState originalFirst = ball(0.0, 0.0, 100.0, 40.0);
    billiardgl::BallState originalSecond = ball(4.9, 0.0, 0.0, 0.0);
    billiardgl::BallState swappedFirst = originalSecond;
    billiardgl::BallState swappedSecond = originalFirst;
    const billiardgl::BallBallContactResult ordered =
        billiardgl::resolveBallBallContact(originalFirst, originalSecond, light, heavy);
    const billiardgl::BallBallContactResult swapped =
        billiardgl::resolveBallBallContact(swappedFirst, swappedSecond, heavy, light);
    expect(close(originalFirst.velocity.x, swappedSecond.velocity.x) &&
        close(originalFirst.velocity.z, swappedSecond.velocity.z) &&
        close(originalSecond.velocity.x, swappedFirst.velocity.x) &&
        close(originalSecond.velocity.z, swappedFirst.velocity.z) &&
        close(ordered.normalImpulseNs, swapped.normalImpulseNs),
        "index permutation preserves physical result");

    first = ball(0.0, 0.0, 0.0, 0.0);
    second = ball(5.0, 0.0, 0.0, 0.0);
    const billiardgl::BallBallContactResult stationary =
        billiardgl::resolveBallBallContact(first, second, equal, equal);
    expect(stationary.regime == billiardgl::BallBallContactRegime::Separating &&
        !stationary.velocityImpulseApplied && stationary.positionCorrected,
        "stationary overlap is separated without velocity impulse");

    first = ball(0.0, 0.0, -20.0, 0.0);
    second = ball(5.0, 0.0, 20.0, 0.0);
    const double recedingFirstVelocity = first.velocity.x;
    const double recedingSecondVelocity = second.velocity.x;
    const billiardgl::BallBallContactResult receding =
        billiardgl::resolveBallBallContact(first, second, equal, equal);
    expect(receding.regime == billiardgl::BallBallContactRegime::Separating &&
        !receding.velocityImpulseApplied &&
        close(first.velocity.x, recedingFirstVelocity) &&
        close(second.velocity.x, recedingSecondVelocity),
        "receding overlap never receives a repeat velocity impulse");

    first = ball(0.0, 0.0, 0.0, 0.0);
    second = ball(20.0, 0.0, 0.0, 0.0);
    const billiardgl::BallBallContactResult absent =
        billiardgl::resolveBallBallContact(first, second, equal, equal);
    expect(absent.regime == billiardgl::BallBallContactRegime::NoContact &&
        !absent.positionCorrected, "separated balls have no contact");

    first = ball(0.0, 0.0, 0.0, 0.0);
    second = ball(0.0, 0.0, 0.0, 0.0);
    const billiardgl::BallBallContactResult degenerate =
        billiardgl::resolveBallBallContact(first, second, equal, equal);
    expect(degenerate.regime == billiardgl::BallBallContactRegime::NoContact &&
        finite(degenerate), "zero-distance degeneracy remains finite and atomic");
    expect(finite(elastic) && finite(unequal) && finite(oblique) && finite(stick) &&
        finite(spinTransfer) && finite(mirrored),
        "all supported contact diagnostics are finite");
    return 0;
}
