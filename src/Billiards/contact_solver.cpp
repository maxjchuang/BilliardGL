#include "contact_solver.h"
#include "cushion_contact.h"
#include "surface_motion.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace billiardgl {
namespace {

struct SolverConstraint {
    ContinuousContactCandidate candidate;
    Point3 normal;
    Point3 tangent;
    Point3 firstArmM;
    Point3 secondArmM;
    double targetNormalSpeedCmS = 0.0;
    double lambdaNormalNs = 0.0;
    double lambdaTangentNs = 0.0;
    double inverseNormalEffectiveMass = 0.0;
    double inverseTangentEffectiveMass = 0.0;
    double boundaryPlaneCoordinateCm = 0.0;
    double restitution = 0.0;
    double friction = 0.0;
    BallBallContactRegime regime = BallBallContactRegime::Frictionless;
};

double dot(const Point3& first, const Point3& second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

Point3 cross(const Point3& first, const Point3& second)
{
    return Point3{
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x};
}

Point3 scaled(const Point3& value, double scale)
{
    return Point3{static_cast<float>(value.x * scale),
        static_cast<float>(value.y * scale),
        static_cast<float>(value.z * scale)};
}

Point3 added(const Point3& first, const Point3& second)
{
    return Point3{first.x + second.x, first.y + second.y, first.z + second.z};
}

Point3 subtracted(const Point3& first, const Point3& second)
{
    return Point3{first.x - second.x, first.y - second.y, first.z - second.z};
}

double length(const Point3& value)
{
    return std::sqrt(dot(value, value));
}

Point3 normalized(const Point3& value)
{
    const double magnitude = length(value);
    if (!(magnitude > 1e-12) || !std::isfinite(magnitude)) {
        return Point3{1.0f, 0.0f, 0.0f};
    }
    return scaled(value, 1.0 / magnitude);
}

Point3 deterministicTangent(const Point3& normal)
{
    const Point3 reference = std::fabs(normal.y) < 0.9f
        ? Point3{0.0f, 1.0f, 0.0f}
        : Point3{1.0f, 0.0f, 0.0f};
    return normalized(cross(normal, reference));
}

bool finitePoint(const Point3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

void addScaled(Point3& value, const Point3& direction, double scale)
{
    value.x = static_cast<float>(value.x + direction.x * scale);
    value.y = static_cast<float>(value.y + direction.y * scale);
    value.z = static_cast<float>(value.z + direction.z * scale);
}

double ballInertiaKgM2(const PhysicsProfile& profile)
{
    const double radiusM = profile.ball.radiusCm / 100.0;
    return profile.ball.inertiaFactor * profile.ball.massKg *
        radiusM * radiusM;
}

Point3 contactVelocityCmS(const BallState& ball, const Point3& armM)
{
    return added(ball.velocity, scaled(cross(ball.angularVelocity, armM), 100.0));
}

Point3 relativeVelocity(const GameState& state,
    const SolverConstraint& constraint)
{
    const Point3 first = contactVelocityCmS(
        state.balls[constraint.candidate.firstBall], constraint.firstArmM);
    if (constraint.candidate.secondBall < 0) return first;
    const Point3 second = contactVelocityCmS(
        state.balls[constraint.candidate.secondBall], constraint.secondArmM);
    return subtracted(second, first);
}

double inverseEffectiveMass(const SolverConstraint& constraint,
    const Point3& axis, const PhysicsProfile& profile)
{
    const double inverseMass = 1.0 / profile.ball.massKg;
    const double inverseInertia = 1.0 / ballInertiaKgM2(profile);
    double result = inverseMass +
        dot(cross(constraint.firstArmM, axis),
            cross(constraint.firstArmM, axis)) * inverseInertia;
    if (constraint.candidate.secondBall >= 0) {
        result += inverseMass +
            dot(cross(constraint.secondArmM, axis),
                cross(constraint.secondArmM, axis)) * inverseInertia;
    }
    return result;
}

void applyBallImpulse(BallState& ball, const Point3& impulseNs,
    const Point3& armM, const PhysicsProfile& profile)
{
    const double inverseMass = 1.0 / profile.ball.massKg;
    const double inverseInertia = 1.0 / ballInertiaKgM2(profile);
    addScaled(ball.velocity, impulseNs, 100.0 * inverseMass);
    addScaled(ball.angularVelocity, cross(armM, impulseNs), inverseInertia);
}

void applyConstraintImpulse(GameState& state,
    const SolverConstraint& constraint, const Point3& impulseNs,
    const PhysicsProfile& profile)
{
    if (constraint.candidate.secondBall >= 0) {
        applyBallImpulse(state.balls[constraint.candidate.firstBall],
            scaled(impulseNs, -1.0), constraint.firstArmM, profile);
        applyBallImpulse(state.balls[constraint.candidate.secondBall],
            impulseNs, constraint.secondArmM, profile);
    } else {
        applyBallImpulse(state.balls[constraint.candidate.firstBall],
            impulseNs, constraint.firstArmM, profile);
    }
}

double totalEnergy(const GameState& state, const ContactIsland& island,
    const PhysicsProfile& profile)
{
    const double mass = profile.ball.massKg;
    const double inertia = ballInertiaKgM2(profile);
    double result = 0.0;
    for (int index : island.ballIndices) {
        const BallState& ball = state.balls[index];
        result += 0.5 * mass * dot(ball.velocity, ball.velocity) / 10000.0;
        result += 0.5 * inertia * dot(
            ball.angularVelocity, ball.angularVelocity);
    }
    return result;
}

SolverConstraint makeConstraint(const GameState& state,
    const ContinuousContactCandidate& candidate,
    const PhysicsProfile& profile)
{
    SolverConstraint constraint;
    constraint.candidate = candidate;
    constraint.normal = normalized(candidate.normal);
    const float radiusM = profile.ball.radiusCm / 100.0f;
    if (candidate.secondBall >= 0) {
        constraint.firstArmM = scaled(constraint.normal, radiusM);
        constraint.secondArmM = scaled(constraint.normal, -radiusM);
        constraint.restitution = profile.ball.normalRestitution;
        constraint.friction = profile.ball.frictionCoefficient;
    } else {
        constraint.firstArmM = added(
            scaled(constraint.normal, -radiusM),
            Point3{0.0f,
                radiusM * (profile.cushion.noseHeightRatio - 1.0f), 0.0f});
        constraint.friction = profile.cushion.frictionCoefficient;
        constraint.boundaryPlaneCoordinateCm = dot(
            state.balls[candidate.firstBall].position, constraint.normal) +
            candidate.penetrationCm;
    }
    const Point3 relative = relativeVelocity(state, constraint);
    const double normalSpeed = dot(relative, constraint.normal);
    if (candidate.secondBall < 0) {
        constraint.restitution = cushionRestitution(
            profile.cushion, std::max(0.0, -normalSpeed));
    }
    const Point3 tangential = subtracted(
        relative, scaled(constraint.normal, normalSpeed));
    constraint.tangent = length(tangential) > 1e-9
        ? normalized(tangential) : deterministicTangent(constraint.normal);
    constraint.targetNormalSpeedCmS = normalSpeed < 0.0
        ? -constraint.restitution * normalSpeed : 0.0;
    constraint.inverseNormalEffectiveMass = inverseEffectiveMass(
        constraint, constraint.normal, profile);
    constraint.inverseTangentEffectiveMass = inverseEffectiveMass(
        constraint, constraint.tangent, profile);
    return constraint;
}

}  // namespace

ContactSolverResult solveContactIslandIteration(GameState& state,
    const ContactIsland& island, const PhysicsProfile& profile,
    int velocityIterations, int positionIterations)
{
    ContactSolverResult result;
    if (velocityIterations <= 0 || positionIterations <= 0) {
        result.status = ContactSolverStatus::IterationLimit;
        return result;
    }
    if (island.limitExceeded ||
        static_cast<int>(island.ballIndices.size()) >
            profile.solver.maximumIslandSize) {
        result.status = ContactSolverStatus::IslandLimit;
        return result;
    }

    const GameState velocitySnapshot = state;
    result.totalKineticEnergyBeforeJ = totalEnergy(state, island, profile);
    result.kineticEnergyBeforeJ = result.totalKineticEnergyBeforeJ;
    std::vector<SolverConstraint> constraints;
    constraints.reserve(island.contacts.size());
    result.contacts.resize(island.contacts.size());
    for (std::size_t index = 0; index < island.contacts.size(); ++index) {
        constraints.push_back(makeConstraint(state, island.contacts[index], profile));
        const SolverConstraint& constraint = constraints.back();
        ContactImpulseDiagnostic& diagnostic = result.contacts[index];
        diagnostic.firstBall = constraint.candidate.firstBall;
        diagnostic.secondBall = constraint.candidate.secondBall;
        diagnostic.featureId = constraint.candidate.featureId;
        diagnostic.targetNormalSpeedCmS = constraint.targetNormalSpeedCmS;
        diagnostic.restitution = constraint.restitution;
        diagnostic.frictionCoefficient = constraint.friction;
        diagnostic.normal = constraint.normal;
        diagnostic.tangent = constraint.tangent;
        diagnostic.firstContactArmM = constraint.firstArmM;
        diagnostic.secondContactArmM = constraint.secondArmM;
        diagnostic.normalEffectiveMassKg =
            1.0 / constraint.inverseNormalEffectiveMass;
        diagnostic.tangentEffectiveMassKg =
            1.0 / constraint.inverseTangentEffectiveMass;
        diagnostic.relativeVelocityBeforeCmS = relativeVelocity(state, constraint);
        result.maximumPenetrationCm = std::max(
            result.maximumPenetrationCm, constraint.candidate.penetrationCm);
    }
    if (result.maximumPenetrationCm > profile.solver.maximumPenetrationCm) {
        result.status = ContactSolverStatus::PenetrationLimit;
        return result;
    }

    for (int iteration = 0; iteration < velocityIterations;
            ++iteration) {
        result.velocityIterations = iteration + 1;
        for (int sweep = 0; sweep < 2; ++sweep) {
            const bool reverse = sweep != 0;
            for (std::size_t step = 0; step < constraints.size(); ++step) {
                const std::size_t index = reverse
                    ? constraints.size() - 1 - step : step;
                SolverConstraint& constraint = constraints[index];
                const double normalSpeed = dot(
                    relativeVelocity(state, constraint), constraint.normal);
                const double normalDelta =
                    (constraint.targetNormalSpeedCmS - normalSpeed) / 100.0 /
                    constraint.inverseNormalEffectiveMass;
                const double updatedNormal = std::max(
                    0.0, constraint.lambdaNormalNs + normalDelta);
                const double appliedNormal =
                    updatedNormal - constraint.lambdaNormalNs;
                constraint.lambdaNormalNs = updatedNormal;
                applyConstraintImpulse(state, constraint,
                    scaled(constraint.normal, appliedNormal), profile);

                const double tangentSpeed = dot(
                    relativeVelocity(state, constraint), constraint.tangent);
                const double desiredDelta = -tangentSpeed / 100.0 /
                    constraint.inverseTangentEffectiveMass;
                const double desiredTotal =
                    constraint.lambdaTangentNs + desiredDelta;
                const double cone =
                    constraint.friction * constraint.lambdaNormalNs;
                const double updatedTangent = std::max(
                    -cone, std::min(cone, desiredTotal));
                const double appliedTangent =
                    updatedTangent - constraint.lambdaTangentNs;
                constraint.lambdaTangentNs = updatedTangent;
                applyConstraintImpulse(state, constraint,
                    scaled(constraint.tangent, appliedTangent), profile);
                if (constraint.friction <= 0.0) {
                    constraint.regime = BallBallContactRegime::Frictionless;
                } else if (std::fabs(desiredTotal) <= cone + 1e-12) {
                    constraint.regime = BallBallContactRegime::Stick;
                } else {
                    constraint.regime = BallBallContactRegime::Slip;
                }
            }
        }
        double residual = 0.0;
        for (const SolverConstraint& constraint : constraints) {
            residual = std::max(residual, std::max(0.0,
                constraint.targetNormalSpeedCmS - dot(
                    relativeVelocity(state, constraint), constraint.normal)));
        }
        result.maximumResidualCmS = residual;
        if (residual <= profile.solver.residualToleranceCmS) break;
    }

    const bool validRestitution = std::all_of(constraints.begin(),
        constraints.end(), [](const SolverConstraint& constraint) {
            return constraint.restitution >= 0.0 &&
                constraint.restitution <= 1.0;
        });
    if (validRestitution && totalEnergy(state, island, profile) >
            result.totalKineticEnergyBeforeJ +
                profile.solver.passiveEnergyToleranceJ) {
        const auto restoreAndApply = [&](double scale) {
            for (int ballIndex : island.ballIndices) {
                state.balls[ballIndex].velocity =
                    velocitySnapshot.balls[ballIndex].velocity;
                state.balls[ballIndex].angularVelocity =
                    velocitySnapshot.balls[ballIndex].angularVelocity;
            }
            for (const SolverConstraint& constraint : constraints) {
                const Point3 impulse = added(
                    scaled(constraint.normal,
                        constraint.lambdaNormalNs * scale),
                    scaled(constraint.tangent,
                        constraint.lambdaTangentNs * scale));
                applyConstraintImpulse(state, constraint, impulse, profile);
            }
            return totalEnergy(state, island, profile);
        };
        double lower = 0.0;
        double upper = 1.0;
        for (int iteration = 0; iteration < 60; ++iteration) {
            const double middle = (lower + upper) * 0.5;
            if (restoreAndApply(middle) <=
                result.totalKineticEnergyBeforeJ +
                    profile.solver.passiveEnergyToleranceJ) {
                lower = middle;
            } else {
                upper = middle;
            }
        }
        restoreAndApply(lower);
        for (std::size_t index = 0; index < constraints.size(); ++index) {
            SolverConstraint& constraint = constraints[index];
            constraint.lambdaNormalNs *= lower;
            constraint.lambdaTangentNs *= lower;
            const double achievedNormalSpeed = dot(
                relativeVelocity(state, constraint), constraint.normal);
            constraint.targetNormalSpeedCmS = std::min(
                constraint.targetNormalSpeedCmS, achievedNormalSpeed);
            const double incidentNormalSpeed = -dot(
                result.contacts[index].relativeVelocityBeforeCmS,
                constraint.normal);
            if (incidentNormalSpeed > 1e-12) {
                constraint.restitution = std::max(0.0,
                    achievedNormalSpeed / incidentNormalSpeed);
            }
        }
    }

    for (int iteration = 0; iteration < positionIterations;
            ++iteration) {
        result.positionIterations = iteration + 1;
        for (std::size_t index = 0; index < constraints.size(); ++index) {
            const ContinuousContactCandidate& contact =
                constraints[index].candidate;
            double penetration = contact.penetrationCm;
            if (contact.secondBall >= 0) {
                const Point3 separation = subtracted(
                    state.balls[contact.secondBall].position,
                    state.balls[contact.firstBall].position);
                penetration = std::max(0.0,
                    2.0 * profile.ball.radiusCm - length(separation));
            } else {
                penetration = std::max(0.0,
                    constraints[index].boundaryPlaneCoordinateCm - dot(
                        state.balls[contact.firstBall].position,
                        contact.normal));
            }
            const double excess = std::max(
                0.0, penetration - profile.solver.penetrationSlopCm);
            if (excess <= 0.0) continue;
            const double share = contact.secondBall >= 0 ? 0.5 : 1.0;
            if (contact.secondBall >= 0) {
                addScaled(state.balls[contact.firstBall].position,
                    contact.normal, -excess * share);
                addScaled(state.balls[contact.secondBall].position,
                    contact.normal, excess * share);
            } else {
                addScaled(state.balls[contact.firstBall].position,
                    contact.normal, excess);
            }
            result.contacts[index].projectionCm += excess;
        }
    }

    for (std::size_t index = 0; index < constraints.size(); ++index) {
        const SolverConstraint& constraint = constraints[index];
        ContactImpulseDiagnostic& diagnostic = result.contacts[index];
        diagnostic.accumulatedNormalImpulseNs = constraint.lambdaNormalNs;
        diagnostic.accumulatedTangentialImpulseNs =
            constraint.lambdaTangentNs;
        diagnostic.restitution = constraint.restitution;
        diagnostic.relativeVelocityAfterCmS = relativeVelocity(state, constraint);
        diagnostic.residualCmS = std::max(0.0,
            constraint.targetNormalSpeedCmS - dot(
                diagnostic.relativeVelocityAfterCmS, constraint.normal));
        diagnostic.regime = constraint.lambdaNormalNs <= 0.0
            ? BallBallContactRegime::Separating : constraint.regime;
    }
    result.totalKineticEnergyAfterJ = totalEnergy(state, island, profile);
    result.kineticEnergyAfterJ = result.totalKineticEnergyAfterJ;
    for (int index : island.ballIndices) {
        BallState& ball = state.balls[index];
        if (!finitePoint(ball.velocity) || !finitePoint(ball.position) ||
            !finitePoint(ball.angularVelocity)) {
            result.status = ContactSolverStatus::NonfiniteState;
            return result;
        }
        ball.speed = static_cast<float>(length(ball.velocity));
        ball.motionState = classifySurfaceMotion(
            ball, profile.ball, profile.surface);
    }
    if (result.maximumResidualCmS > profile.solver.residualToleranceCmS) {
        result.status = ContactSolverStatus::IterationLimit;
    }
    return result;
}

ContactSolverResult solveContactIsland(
    GameState& state, const ContactIsland& island,
    const PhysicsProfile& profile)
{
    return solveContactIslandIteration(state, island, profile,
        profile.solver.velocityIterations, profile.solver.positionIterations);
}

const char* contactSolverStatusName(ContactSolverStatus status)
{
    switch (status) {
    case ContactSolverStatus::Converged: return "converged";
    case ContactSolverStatus::IterationLimit: return "iteration_limit";
    case ContactSolverStatus::IslandLimit: return "island_limit";
    case ContactSolverStatus::PenetrationLimit: return "penetration_limit";
    case ContactSolverStatus::NonfiniteState: return "nonfinite_state";
    }
    return "nonfinite_state";
}

}  // namespace billiardgl
