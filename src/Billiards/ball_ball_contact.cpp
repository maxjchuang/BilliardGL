#include "ball_ball_contact.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

using Vector = std::array<double, 3>;

Vector pointM(const Point3& point)
{
    return {{point.x / 100.0, point.y / 100.0, point.z / 100.0}};
}

Vector angular(const Point3& point)
{
    return {{point.x, point.y, point.z}};
}

Vector add(const Vector& first, const Vector& second)
{
    return {{first[0] + second[0], first[1] + second[1], first[2] + second[2]}};
}

Vector subtract(const Vector& first, const Vector& second)
{
    return {{first[0] - second[0], first[1] - second[1], first[2] - second[2]}};
}

Vector multiply(const Vector& value, double scale)
{
    return {{value[0] * scale, value[1] * scale, value[2] * scale}};
}

double dot(const Vector& first, const Vector& second)
{
    return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
}

Vector cross(const Vector& first, const Vector& second)
{
    return {{
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0]}};
}

double magnitude(const Vector& value)
{
    return std::sqrt(dot(value, value));
}

bool finiteVector(const Vector& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

bool validProperties(const BallProperties& properties)
{
    return std::isfinite(properties.radiusCm) && properties.radiusCm > 0.0f &&
        std::isfinite(properties.massKg) && properties.massKg > 0.0f &&
        std::isfinite(properties.inertiaFactor) && properties.inertiaFactor > 0.0f &&
        std::isfinite(properties.normalRestitution) &&
        properties.normalRestitution >= 0.0f && properties.normalRestitution <= 1.0f &&
        std::isfinite(properties.frictionCoefficient) &&
        properties.frictionCoefficient >= 0.0f;
}

double inertia(const BallProperties& properties)
{
    const double radiusM = properties.radiusCm / 100.0;
    return properties.inertiaFactor * properties.massKg * radiusM * radiusM;
}

double kineticEnergy(const Vector& velocity, const Vector& angularVelocity,
    const BallProperties& properties)
{
    return 0.5 * properties.massKg * dot(velocity, velocity) +
        0.5 * inertia(properties) * dot(angularVelocity, angularVelocity);
}

Vector contactVelocity(const BallState& ball, const Vector& arm)
{
    return add(pointM(ball.velocity), cross(angular(ball.angularVelocity), arm));
}

void addVelocity(BallState& ball, const Vector& deltaMS)
{
    ball.velocity.x += static_cast<float>(deltaMS[0] * 100.0);
    ball.velocity.y += static_cast<float>(deltaMS[1] * 100.0);
    ball.velocity.z += static_cast<float>(deltaMS[2] * 100.0);
    ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x +
        ball.velocity.y * ball.velocity.y + ball.velocity.z * ball.velocity.z);
}

void addAngularVelocity(BallState& ball, const Vector& delta)
{
    ball.angularVelocity.x += static_cast<float>(delta[0]);
    ball.angularVelocity.y += static_cast<float>(delta[1]);
    ball.angularVelocity.z += static_cast<float>(delta[2]);
}

void addPosition(BallState& ball, const Vector& deltaM)
{
    ball.position.x += static_cast<float>(deltaM[0] * 100.0);
    ball.position.y += static_cast<float>(deltaM[1] * 100.0);
    ball.position.z += static_cast<float>(deltaM[2] * 100.0);
}

}  // namespace

const char* ballBallContactRegimeName(BallBallContactRegime regime)
{
    switch (regime) {
    case BallBallContactRegime::NoContact: return "no_contact";
    case BallBallContactRegime::Separating: return "separating";
    case BallBallContactRegime::Frictionless: return "frictionless";
    case BallBallContactRegime::Stick: return "stick";
    case BallBallContactRegime::Slip: return "slip";
    }
    return "no_contact";
}

BallBallContactResult resolveBallBallContact(BallState& first, BallState& second,
    const BallProperties& firstProperties,
    const BallProperties& secondProperties)
{
    BallBallContactResult result;
    const Vector firstPosition = pointM(first.position);
    const Vector secondPosition = pointM(second.position);
    const Vector firstVelocity = pointM(first.velocity);
    const Vector secondVelocity = pointM(second.velocity);
    const Vector firstAngular = angular(first.angularVelocity);
    const Vector secondAngular = angular(second.angularVelocity);
    if (!validProperties(firstProperties) || !validProperties(secondProperties) ||
        !finiteVector(firstPosition) || !finiteVector(secondPosition) ||
        !finiteVector(firstVelocity) || !finiteVector(secondVelocity) ||
        !finiteVector(firstAngular) || !finiteVector(secondAngular)) {
        return result;
    }

    result.restitution = std::min(
        static_cast<double>(firstProperties.normalRestitution),
        static_cast<double>(secondProperties.normalRestitution));
    result.frictionCoefficient = std::sqrt(
        static_cast<double>(firstProperties.frictionCoefficient) *
        static_cast<double>(secondProperties.frictionCoefficient));
    result.kineticEnergyBeforeJ =
        kineticEnergy(firstVelocity, firstAngular, firstProperties) +
        kineticEnergy(secondVelocity, secondAngular, secondProperties);
    result.kineticEnergyAfterJ = result.kineticEnergyBeforeJ;

    const Vector separation = subtract(secondPosition, firstPosition);
    const double distance = magnitude(separation);
    const double firstRadiusM = firstProperties.radiusCm / 100.0;
    const double secondRadiusM = secondProperties.radiusCm / 100.0;
    const double radiusSumM = firstRadiusM + secondRadiusM;
    if (distance <= 1e-12 || distance > radiusSumM + 1e-9) return result;

    const Vector normal = multiply(separation, 1.0 / distance);
    const Vector firstArm = multiply(normal, firstRadiusM);
    const Vector secondArm = multiply(normal, -secondRadiusM);
    result.contactNormal = normal;
    result.firstContactArmM = firstArm;
    result.secondContactArmM = secondArm;
    result.penetrationM = std::max(0.0, radiusSumM - distance);

    const double correctionMagnitude = std::max(
        0.0, result.penetrationM - result.positionSlopM);
    if (correctionMagnitude > 0.0) {
        const double firstInverseMass = 1.0 / firstProperties.massKg;
        const double secondInverseMass = 1.0 / secondProperties.massKg;
        const double inverseMassSum = firstInverseMass + secondInverseMass;
        result.firstPositionCorrectionM = multiply(
            normal, -correctionMagnitude * firstInverseMass / inverseMassSum);
        result.secondPositionCorrectionM = multiply(
            normal, correctionMagnitude * secondInverseMass / inverseMassSum);
        addPosition(first, result.firstPositionCorrectionM);
        addPosition(second, result.secondPositionCorrectionM);
        result.positionCorrected = true;
    }

    const Vector relativeBefore = subtract(
        contactVelocity(second, secondArm), contactVelocity(first, firstArm));
    result.relativeContactVelocityBeforeMS = relativeBefore;
    const double normalSpeed = dot(relativeBefore, normal);
    result.normalRelativeSpeedBeforeMS = normalSpeed;
    const Vector tangentVelocity = subtract(
        relativeBefore, multiply(normal, normalSpeed));
    const double tangentSpeed = magnitude(tangentVelocity);
    if (tangentSpeed > 1e-12) {
        result.contactTangent = multiply(tangentVelocity, 1.0 / tangentSpeed);
    }

    if (normalSpeed >= -1e-12) {
        result.regime = BallBallContactRegime::Separating;
        result.relativeContactVelocityAfterMS = relativeBefore;
        result.normalRelativeSpeedAfterMS = normalSpeed;
        return result;
    }

    const double inverseMassSum = 1.0 / firstProperties.massKg +
        1.0 / secondProperties.massKg;
    result.normalImpulseNs =
        -(1.0 + result.restitution) * normalSpeed / inverseMassSum;
    Vector impulse = multiply(normal, result.normalImpulseNs);

    if (tangentSpeed <= 1e-12 || result.frictionCoefficient <= 0.0) {
        result.regime = BallBallContactRegime::Frictionless;
    } else {
        const Vector tangent = result.contactTangent;
        const Vector firstArmCrossTangent = cross(firstArm, tangent);
        const Vector secondArmCrossTangent = cross(secondArm, tangent);
        const double tangentInverseEffectiveMass = inverseMassSum +
            dot(firstArmCrossTangent, firstArmCrossTangent) /
                inertia(firstProperties) +
            dot(secondArmCrossTangent, secondArmCrossTangent) /
                inertia(secondProperties);
        const double desiredSignedImpulse =
            -dot(relativeBefore, tangent) / tangentInverseEffectiveMass;
        const double maximumTangentialImpulse =
            result.frictionCoefficient * result.normalImpulseNs;
        const double appliedSignedImpulse = std::max(
            -maximumTangentialImpulse,
            std::min(maximumTangentialImpulse, desiredSignedImpulse));
        result.tangentialImpulseNs = std::fabs(appliedSignedImpulse);
        impulse = add(impulse, multiply(tangent, appliedSignedImpulse));
        result.regime = std::fabs(desiredSignedImpulse) <=
            maximumTangentialImpulse + 1e-12 ?
            BallBallContactRegime::Stick : BallBallContactRegime::Slip;
    }

    result.impulseOnSecondNs = impulse;
    addVelocity(first, multiply(impulse, -1.0 / firstProperties.massKg));
    addVelocity(second, multiply(impulse, 1.0 / secondProperties.massKg));
    addAngularVelocity(first, multiply(
        cross(firstArm, multiply(impulse, -1.0)), 1.0 / inertia(firstProperties)));
    addAngularVelocity(second, multiply(
        cross(secondArm, impulse), 1.0 / inertia(secondProperties)));
    result.velocityImpulseApplied = true;

    result.relativeContactVelocityAfterMS = subtract(
        contactVelocity(second, secondArm), contactVelocity(first, firstArm));
    result.normalRelativeSpeedAfterMS =
        dot(result.relativeContactVelocityAfterMS, normal);
    result.kineticEnergyAfterJ =
        kineticEnergy(pointM(first.velocity), angular(first.angularVelocity),
            firstProperties) +
        kineticEnergy(pointM(second.velocity), angular(second.angularVelocity),
            secondProperties);
    return result;
}

}  // namespace billiardgl
