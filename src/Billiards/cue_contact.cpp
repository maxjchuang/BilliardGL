#include "cue_contact.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

using Vector = std::array<double, 3>;

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
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

CueContactResult unsupported(const char* error)
{
    CueContactResult result;
    result.error = error;
    return result;
}

double ballKineticEnergy(const Vector& velocity, const Vector& angularVelocity,
    double massKg, double inertia)
{
    return 0.5 * massKg * dot(velocity, velocity) +
        0.5 * inertia * dot(angularVelocity, angularVelocity);
}

}  // namespace

const char* cueContactRegimeName(CueContactRegime regime)
{
    switch (regime) {
    case CueContactRegime::Stick: return "stick";
    case CueContactRegime::Slip: return "slip";
    case CueContactRegime::Miscue: return "miscue";
    case CueContactRegime::Unsupported: return "unsupported";
    }
    return "unsupported";
}

CueContactResult resolveCueContact(BallState& ball, const CueImpactInput& input,
    const BallProperties& ballProperties, const CueProperties& cueProperties)
{
    if (!std::isfinite(input.cueSpeedCmS) || input.cueSpeedCmS < 0.0 ||
        !std::isfinite(input.cueMassKg) || input.cueMassKg <= 0.0 ||
        !finiteVector(input.direction) || !std::isfinite(input.elevationDegrees) ||
        !std::isfinite(input.tipOffsetRadius[0]) || !std::isfinite(input.tipOffsetRadius[1]) ||
        ballProperties.massKg <= 0.0f || ballProperties.radiusCm <= 0.0f) {
        return unsupported("invalid_cue_contact_input");
    }
    if (std::fabs(input.elevationDegrees) > 1e-9 || std::fabs(input.direction[1]) > 1e-9) {
        return unsupported("cue_elevation_requires_3d");
    }

    Vector direction = input.direction;
    const double directionLength = magnitude(direction);
    if (directionLength <= 0.0) return unsupported("invalid_cue_direction");
    direction = multiply(direction, 1.0 / directionLength);

    const double sideOffset = input.tipOffsetRadius[0];
    const double verticalOffset = input.tipOffsetRadius[1];
    const double offsetFraction = std::sqrt(
        sideOffset * sideOffset + verticalOffset * verticalOffset);
    if (offsetFraction >= 1.0) return unsupported("cue_offset_outside_ball");
    const double radiusM = ballProperties.radiusCm / 100.0;
    const double inertia = 0.4 * ballProperties.massKg * radiusM * radiusM;
    const Vector ballVelocity = {{ball.velocity.x / 100.0,
        ball.velocity.y / 100.0, ball.velocity.z / 100.0}};
    const Vector angularVelocity = {{ball.angularVelocity.x,
        ball.angularVelocity.y, ball.angularVelocity.z}};
    const Vector cueVelocity = multiply(direction, input.cueSpeedCmS / 100.0);
    if (offsetFraction > cueProperties.maximumReliableOffsetRadius) {
        CueContactResult result;
        result.regime = CueContactRegime::Miscue;
        result.error = "miscue_offset_exceeds_reliable_radius";
        result.frictionCoefficient = input.chalkState == "UNCHALKED" ?
            cueProperties.unchalkedFrictionCoefficient :
            cueProperties.chalkedFrictionCoefficient;
        result.cueVelocityBeforeMS = cueVelocity;
        result.cueVelocityAfterMS = cueVelocity;
        result.ballVelocityBeforeMS = ballVelocity;
        result.ballVelocityAfterMS = ballVelocity;
        result.ballAngularVelocityBeforeRadS = angularVelocity;
        result.ballAngularVelocityAfterRadS = angularVelocity;
        result.inputKineticEnergyJ = ballKineticEnergy(
            ballVelocity, angularVelocity, ballProperties.massKg, inertia) +
            0.5 * input.cueMassKg * dot(cueVelocity, cueVelocity);
        result.outputKineticEnergyJ = result.inputKineticEnergyJ;
        return result;
    }

    const Vector side = {{-direction[2], 0.0, direction[0]}};
    const Vector up = {{0.0, 1.0, 0.0}};
    const Vector offset = add(multiply(side, sideOffset * radiusM),
        multiply(up, verticalOffset * radiusM));
    const Vector arm = add(multiply(direction,
        -radiusM * std::sqrt(std::max(0.0, 1.0 - offsetFraction * offsetFraction))), offset);
    const Vector normal = multiply(arm, -1.0 / radiusM);

    const Vector contactVelocity = add(ballVelocity, cross(angularVelocity, arm));
    const Vector relativeVelocity = subtract(cueVelocity, contactVelocity);
    const double approachSpeed = dot(relativeVelocity, direction);
    if (approachSpeed <= 0.0) return unsupported("cue_not_approaching");

    const Vector armCrossDirection = cross(arm, direction);
    const double inverseEffectiveMass = 1.0 / input.cueMassKg +
        1.0 / ballProperties.massKg + dot(armCrossDirection, armCrossDirection) / inertia;
    const double desiredMagnitude =
        (1.0 + cueProperties.normalRestitution) * approachSpeed / inverseEffectiveMass;
    const Vector desiredImpulse = multiply(direction, desiredMagnitude);
    const double normalImpulse = dot(desiredImpulse, normal);
    if (normalImpulse <= 0.0) return unsupported("cue_not_approaching_contact_normal");
    const Vector desiredTangent = subtract(desiredImpulse, multiply(normal, normalImpulse));
    const double desiredTangentialMagnitude = magnitude(desiredTangent);
    const double friction = input.chalkState == "UNCHALKED" ?
        cueProperties.unchalkedFrictionCoefficient : cueProperties.chalkedFrictionCoefficient;

    CueContactResult result;
    result.frictionCoefficient = friction;
    result.normalImpulseNs = normalImpulse;
    result.normalRelativeSpeedBeforeMS = dot(relativeVelocity, normal);
    result.contactArmM = arm;
    result.contactNormal = normal;
    result.tangentialRelativeVelocityBeforeMS = subtract(
        relativeVelocity, multiply(normal, dot(relativeVelocity, normal)));
    result.tangentialRelativeSpeedBeforeMS =
        magnitude(result.tangentialRelativeVelocityBeforeMS);
    result.cueVelocityBeforeMS = cueVelocity;
    result.cueVelocityAfterMS = cueVelocity;
    result.ballVelocityBeforeMS = ballVelocity;
    result.ballVelocityAfterMS = ballVelocity;
    result.ballAngularVelocityBeforeRadS = angularVelocity;
    result.ballAngularVelocityAfterRadS = angularVelocity;
    result.inputKineticEnergyJ = ballKineticEnergy(
        ballVelocity, angularVelocity, ballProperties.massKg, inertia) +
        0.5 * input.cueMassKg * dot(cueVelocity, cueVelocity);
    result.outputKineticEnergyJ = result.inputKineticEnergyJ;

    Vector impulse = desiredImpulse;
    if (desiredTangentialMagnitude <= friction * normalImpulse + 1e-12) {
        result.regime = CueContactRegime::Stick;
        result.tangentialImpulseNs = desiredTangentialMagnitude;
    } else {
        result.regime = CueContactRegime::Slip;
        result.tangentialImpulseNs = friction * normalImpulse;
        impulse = add(multiply(normal, normalImpulse), multiply(
            desiredTangent, result.tangentialImpulseNs / desiredTangentialMagnitude));
        if (std::fabs(impulse[1]) > 1e-9) {
            result.regime = CueContactRegime::Unsupported;
            result.error = "vertical_ball_impulse_requires_3d";
            return result;
        }
    }

    const Vector deltaVelocity = multiply(impulse, 1.0 / ballProperties.massKg);
    const Vector deltaAngularVelocity = multiply(cross(arm, impulse), 1.0 / inertia);
    ball.velocity.x += static_cast<float>(deltaVelocity[0] * 100.0);
    ball.velocity.y += static_cast<float>(deltaVelocity[1] * 100.0);
    ball.velocity.z += static_cast<float>(deltaVelocity[2] * 100.0);
    ball.angularVelocity.x += static_cast<float>(deltaAngularVelocity[0]);
    ball.angularVelocity.y += static_cast<float>(deltaAngularVelocity[1]);
    ball.angularVelocity.z += static_cast<float>(deltaAngularVelocity[2]);
    ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x +
        ball.velocity.y * ball.velocity.y + ball.velocity.z * ball.velocity.z);

    result.impulseNs = impulse;
    result.cueVelocityAfterMS = subtract(cueVelocity,
        multiply(impulse, 1.0 / input.cueMassKg));
    result.ballVelocityAfterMS = add(ballVelocity, deltaVelocity);
    result.ballAngularVelocityAfterRadS = add(
        angularVelocity, deltaAngularVelocity);
    result.outputKineticEnergyJ = ballKineticEnergy(
        result.ballVelocityAfterMS, result.ballAngularVelocityAfterRadS,
        ballProperties.massKg, inertia) + 0.5 * input.cueMassKg *
        dot(result.cueVelocityAfterMS, result.cueVelocityAfterMS);
    result.applied = true;
    return result;
}

}  // namespace billiardgl
