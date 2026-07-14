#include "cushion_contact.h"

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

billiardgl::BallState ball(double vxCmS, double vzCmS)
{
    billiardgl::BallState value;
    value.velocity = billiardgl::Point3(
        static_cast<float>(vxCmS), 0.0f, static_cast<float>(vzCmS));
    value.speed = static_cast<float>(std::hypot(vxCmS, vzCmS));
    return value;
}

bool finite(const billiardgl::CushionContactResult& result)
{
    const double scalars[] = {
        result.penetrationM, result.positionSlopM, result.restitution,
        result.frictionCoefficient, result.noseHeightRatio,
        result.incidentSpeedMS, result.maximumRigidIncidentSpeedMS,
        result.normalImpulseNs, result.tangentialImpulseNs,
        result.normalRelativeSpeedBeforeMS, result.normalRelativeSpeedAfterMS,
        result.kineticEnergyBeforeJ, result.kineticEnergyAfterJ,
    };
    for (double value : scalars) if (!std::isfinite(value)) return false;
    const std::array<double, 3>* vectors[] = {
        &result.contactNormal, &result.contactTangent, &result.contactArmM,
        &result.contactVelocityBeforeMS, &result.contactVelocityAfterMS,
        &result.impulseOnBallNs, &result.positionCorrectionM,
        &result.linearVelocityChangeMS, &result.angularVelocityChangeRadS,
    };
    for (const std::array<double, 3>* vector : vectors) {
        for (double value : *vector) if (!std::isfinite(value)) return false;
    }
    return true;
}

}  // namespace

int main()
{
    billiardgl::BallProperties properties;
    billiardgl::CushionProperties cushion;
    cushion.normalRestitution = 0.8f;
    cushion.frictionCoefficient = 0.0f;
    cushion.noseHeightRatio = 1.0f;
    cushion.maximumRigidIncidentSpeedCmS = 250.0f;
    const billiardgl::Point3 rightRailNormal(-1.0f, 0.0f, 0.0f);

    billiardgl::BallState headOn = ball(100.0, 0.0);
    const billiardgl::CushionContactResult elastic =
        billiardgl::resolveCushionContact(
            headOn, rightRailNormal, 0.0, properties, cushion);
    expect(elastic.velocityImpulseApplied &&
        elastic.regime == billiardgl::CushionContactRegime::Frictionless,
        "approaching head-on cushion contact applies once");
    expect(close(headOn.velocity.x, -80.0, 1e-4) &&
        close(elastic.normalRelativeSpeedAfterMS, 0.8, 1e-6),
        "head-on rebound obeys restitution");
    expect(elastic.kineticEnergyAfterJ <= elastic.kineticEnergyBeforeJ + 1e-9,
        "inelastic cushion contact does not create energy");

    billiardgl::CushionProperties rough = cushion;
    rough.normalRestitution = 0.9f;
    rough.frictionCoefficient = 0.2f;
    rough.noseHeightRatio = 1.4f;
    billiardgl::BallState oblique = ball(100.0, 40.0);
    const billiardgl::CushionContactResult coupled =
        billiardgl::resolveCushionContact(
            oblique, rightRailNormal, 0.0002, properties, rough);
    expect(coupled.regime == billiardgl::CushionContactRegime::Stick ||
        coupled.regime == billiardgl::CushionContactRegime::Slip,
        "rough oblique cushion contact classifies tangent regime");
    expect(coupled.tangentialImpulseNs <=
        coupled.frictionCoefficient * coupled.normalImpulseNs + 1e-10,
        "cushion tangent impulse stays inside Coulomb cone");
    expect(std::fabs(oblique.angularVelocity.y) > 0.0f &&
        std::fabs(oblique.angularVelocity.z) > 0.0f,
        "rail friction and nose height couple translation to three-dimensional spin");
    expect(coupled.contactArmM[1] > 0.0 && coupled.positionCorrectionM[0] < 0.0,
        "nose height and inward penetration correction are explicit");
    expect(coupled.kineticEnergyAfterJ <= coupled.kineticEnergyBeforeJ + 1e-9,
        "rough cushion contact does not create energy");

    billiardgl::BallState mirrored = ball(-100.0, 40.0);
    const billiardgl::CushionContactResult mirrorResult =
        billiardgl::resolveCushionContact(
            mirrored, billiardgl::Point3(1.0f, 0.0f, 0.0f),
            0.0002, properties, rough);
    expect(close(oblique.velocity.x, -mirrored.velocity.x) &&
        close(oblique.velocity.z, mirrored.velocity.z) &&
        close(oblique.angularVelocity.y, -mirrored.angularVelocity.y) &&
        close(oblique.angularVelocity.z, -mirrored.angularVelocity.z),
        "opposite table boundaries produce mirrored motion and spin");
    expect(close(coupled.normalImpulseNs, mirrorResult.normalImpulseNs),
        "mirror preserves normal impulse magnitude");

    billiardgl::BallState spinning = ball(100.0, 0.0);
    spinning.angularVelocity.y = 20.0f;
    const billiardgl::CushionContactResult spinFriction =
        billiardgl::resolveCushionContact(
            spinning, rightRailNormal, 0.0, properties, rough);
    expect(spinFriction.tangentialImpulseNs > 0.0 &&
        std::fabs(spinning.velocity.z) > 0.0,
        "sidespin changes along-rail translation in the physical direction");

    billiardgl::BallState topspin = ball(100.0, 0.0);
    billiardgl::BallState backspin = ball(100.0, 0.0);
    topspin.angularVelocity.z = 10.0f;
    backspin.angularVelocity.z = -10.0f;
    const billiardgl::CushionContactResult topResult =
        billiardgl::resolveCushionContact(
            topspin, rightRailNormal, 0.0, properties, rough);
    const billiardgl::CushionContactResult backResult =
        billiardgl::resolveCushionContact(
            backspin, rightRailNormal, 0.0, properties, rough);
    expect(topResult.normalImpulseNs < backResult.normalImpulseNs &&
        topspin.velocity.x > backspin.velocity.x,
        "nose-height contact gives topspin and backspin opposite normal effects");

    billiardgl::BallState receding = ball(-20.0, 0.0);
    const billiardgl::CushionContactResult separated =
        billiardgl::resolveCushionContact(
            receding, rightRailNormal, 0.001, properties, rough);
    expect(separated.regime == billiardgl::CushionContactRegime::Separating &&
        !separated.velocityImpulseApplied && separated.positionCorrected &&
        close(receding.velocity.x, -20.0),
        "receding overlap is corrected without repeat velocity impulse");

    billiardgl::BallState fast = ball(300.0, 0.0);
    const billiardgl::CushionContactResult outsideDomain =
        billiardgl::resolveCushionContact(
            fast, rightRailNormal, 0.0, properties, cushion);
    expect(outsideDomain.velocityImpulseApplied && outsideDomain.rigidDomainExceeded,
        "above-domain contact still executes and is labeled rather than clamped");

    billiardgl::BallState invalid = ball(100.0, 0.0);
    const billiardgl::CushionContactResult invalidNormal =
        billiardgl::resolveCushionContact(
            invalid, billiardgl::Point3(2.0f, 0.0f, 0.0f),
            0.0, properties, cushion);
    expect(invalidNormal.regime == billiardgl::CushionContactRegime::NoContact &&
        !invalidNormal.velocityImpulseApplied && close(invalid.velocity.x, 100.0),
        "invalid normal leaves state unchanged");
    expect(finite(elastic) && finite(coupled) && finite(mirrorResult) &&
        finite(spinFriction) && finite(topResult) && finite(backResult) &&
        finite(separated) && finite(outsideDomain) &&
        finite(invalidNormal), "all cushion diagnostics remain finite");
    return 0;
}
