#include "cushion_contact.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

using Vector = std::array<double, 3>;

Vector vector(const Point3& value)
{
    return {{value.x, value.y, value.z}};
}

Vector velocityM(const Point3& value)
{
    return {{value.x / 100.0, value.y / 100.0, value.z / 100.0}};
}

Vector add(const Vector& first, const Vector& second)
{
    return {{first[0] + second[0], first[1] + second[1], first[2] + second[2]}};
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

bool finite(const Vector& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

bool valid(const BallProperties& ball, const CushionProperties& cushion)
{
    return std::isfinite(ball.radiusCm) && ball.radiusCm > 0.0f &&
        std::isfinite(ball.massKg) && ball.massKg > 0.0f &&
        std::isfinite(ball.inertiaFactor) && ball.inertiaFactor > 0.0f &&
        std::isfinite(cushion.normalRestitution) &&
        cushion.normalRestitution >= 0.0f && cushion.normalRestitution <= 1.0f &&
        std::isfinite(cushion.frictionCoefficient) &&
        cushion.frictionCoefficient >= 0.0f &&
        std::isfinite(cushion.noseHeightRatio) &&
        cushion.noseHeightRatio > 0.0f && cushion.noseHeightRatio < 2.0f &&
        std::isfinite(cushion.maximumRigidIncidentSpeedCmS) &&
        cushion.maximumRigidIncidentSpeedCmS > 0.0f;
}

double inertia(const BallProperties& properties)
{
    const double radiusM = properties.radiusCm / 100.0;
    return properties.inertiaFactor * properties.massKg * radiusM * radiusM;
}

double kineticEnergy(const BallState& ball, const BallProperties& properties)
{
    const Vector velocity = velocityM(ball.velocity);
    const Vector angular = vector(ball.angularVelocity);
    return 0.5 * properties.massKg * dot(velocity, velocity) +
        0.5 * inertia(properties) * dot(angular, angular);
}

Vector contactVelocity(const BallState& ball, const Vector& arm)
{
    return add(velocityM(ball.velocity), cross(vector(ball.angularVelocity), arm));
}

void applyLinearChange(BallState& ball, const Vector& change)
{
    ball.velocity.x += static_cast<float>(change[0] * 100.0);
    ball.velocity.y += static_cast<float>(change[1] * 100.0);
    ball.velocity.z += static_cast<float>(change[2] * 100.0);
    ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x +
        ball.velocity.y * ball.velocity.y + ball.velocity.z * ball.velocity.z);
}

void applyAngularChange(BallState& ball, const Vector& change)
{
    ball.angularVelocity.x += static_cast<float>(change[0]);
    ball.angularVelocity.y += static_cast<float>(change[1]);
    ball.angularVelocity.z += static_cast<float>(change[2]);
}

void applyPositionCorrection(BallState& ball, const Vector& correction)
{
    ball.position.x += static_cast<float>(correction[0] * 100.0);
    ball.position.y += static_cast<float>(correction[1] * 100.0);
    ball.position.z += static_cast<float>(correction[2] * 100.0);
}

}  // namespace

const char* cushionContactRegimeName(CushionContactRegime regime)
{
    switch (regime) {
    case CushionContactRegime::NoContact: return "no_contact";
    case CushionContactRegime::Separating: return "separating";
    case CushionContactRegime::Frictionless: return "frictionless";
    case CushionContactRegime::Stick: return "stick";
    case CushionContactRegime::Slip: return "slip";
    }
    return "no_contact";
}

CushionContactResult resolveCushionContact(
    BallState& ball, const Point3& inwardNormal, double penetrationM,
    const BallProperties& ballProperties,
    const CushionProperties& cushionProperties)
{
    CushionContactResult result;
    const Vector normal = vector(inwardNormal);
    const Vector velocity = velocityM(ball.velocity);
    const Vector angular = vector(ball.angularVelocity);
    if (!valid(ballProperties, cushionProperties) || !finite(normal) ||
        !finite(velocity) || !finite(angular) || !std::isfinite(penetrationM) ||
        penetrationM < 0.0 || std::fabs(magnitude(normal) - 1.0) > 1e-6 ||
        std::fabs(normal[1]) > 1e-9) {
        return result;
    }

    result.contactNormal = normal;
    result.contactTangent = cross(Vector{{0.0, 1.0, 0.0}}, normal);
    result.penetrationM = penetrationM;
    result.restitution = cushionProperties.normalRestitution;
    result.frictionCoefficient = cushionProperties.frictionCoefficient;
    result.noseHeightRatio = cushionProperties.noseHeightRatio;
    result.maximumRigidIncidentSpeedMS =
        cushionProperties.maximumRigidIncidentSpeedCmS / 100.0;
    const double radiusM = ballProperties.radiusCm / 100.0;
    result.contactArmM = add(
        multiply(normal, -radiusM),
        Vector{{0.0, radiusM * (result.noseHeightRatio - 1.0), 0.0}});
    result.kineticEnergyBeforeJ = kineticEnergy(ball, ballProperties);
    result.kineticEnergyAfterJ = result.kineticEnergyBeforeJ;

    const double correction = std::max(0.0, penetrationM - result.positionSlopM);
    if (correction > 0.0) {
        result.positionCorrectionM = multiply(normal, correction);
        applyPositionCorrection(ball, result.positionCorrectionM);
        result.positionCorrected = true;
    }

    result.contactVelocityBeforeMS = contactVelocity(ball, result.contactArmM);
    const double normalSpeed = dot(result.contactVelocityBeforeMS, normal);
    result.normalRelativeSpeedBeforeMS = normalSpeed;
    result.incidentSpeedMS = std::max(0.0, -normalSpeed);
    result.rigidDomainExceeded =
        result.incidentSpeedMS > result.maximumRigidIncidentSpeedMS + 1e-12;
    if (normalSpeed >= -1e-12) {
        result.regime = CushionContactRegime::Separating;
        result.contactVelocityAfterMS = result.contactVelocityBeforeMS;
        result.normalRelativeSpeedAfterMS = normalSpeed;
        return result;
    }

    const double rotationalInertia = inertia(ballProperties);
    const Vector armCrossNormal = cross(result.contactArmM, normal);
    const double normalInverseEffectiveMass = 1.0 / ballProperties.massKg +
        dot(armCrossNormal, armCrossNormal) / rotationalInertia;
    result.normalImpulseNs =
        -(1.0 + result.restitution) * normalSpeed /
        normalInverseEffectiveMass;
    Vector impulse = multiply(normal, result.normalImpulseNs);

    const double tangentSpeed =
        dot(result.contactVelocityBeforeMS, result.contactTangent);
    if (std::fabs(tangentSpeed) <= 1e-12 ||
        result.frictionCoefficient <= 0.0) {
        result.regime = CushionContactRegime::Frictionless;
    } else {
        const Vector armCrossTangent = cross(
            result.contactArmM, result.contactTangent);
        const double tangentInverseEffectiveMass =
            1.0 / ballProperties.massKg +
            dot(armCrossTangent, armCrossTangent) / rotationalInertia;
        const double desired = -tangentSpeed / tangentInverseEffectiveMass;
        const double maximum =
            result.frictionCoefficient * result.normalImpulseNs;
        const double applied = std::max(-maximum, std::min(maximum, desired));
        result.tangentialImpulseNs = std::fabs(applied);
        impulse = add(impulse, multiply(result.contactTangent, applied));
        result.regime = std::fabs(desired) <= maximum + 1e-12 ?
            CushionContactRegime::Stick : CushionContactRegime::Slip;
    }

    result.impulseOnBallNs = impulse;
    result.linearVelocityChangeMS = multiply(
        impulse, 1.0 / ballProperties.massKg);
    result.angularVelocityChangeRadS = multiply(
        cross(result.contactArmM, impulse), 1.0 / rotationalInertia);
    applyLinearChange(ball, result.linearVelocityChangeMS);
    applyAngularChange(ball, result.angularVelocityChangeRadS);
    result.velocityImpulseApplied = true;
    result.contactVelocityAfterMS = contactVelocity(ball, result.contactArmM);
    result.normalRelativeSpeedAfterMS =
        dot(result.contactVelocityAfterMS, normal);
    result.kineticEnergyAfterJ = kineticEnergy(ball, ballProperties);
    return result;
}

}  // namespace billiardgl
