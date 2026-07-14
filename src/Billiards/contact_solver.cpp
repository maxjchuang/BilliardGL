#include "contact_solver.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

double dot(const Point3& first, const Point3& second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

bool finitePoint(const Point3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void addScaled(Point3& value, const Point3& direction, double scale)
{
    value.x = static_cast<float>(value.x + direction.x * scale);
    value.y = static_cast<float>(value.y + direction.y * scale);
    value.z = static_cast<float>(value.z + direction.z * scale);
}

double energy(const GameState& state, const ContactIsland& island,
    double massKg)
{
    double result = 0.0;
    for (int index : island.ballIndices) {
        const Point3& velocity = state.balls[index].velocity;
        const double speedMS2 = dot(velocity, velocity) / 10000.0;
        result += 0.5 * massKg * speedMS2;
    }
    return result;
}

double normalSpeed(const GameState& state,
    const ContinuousContactCandidate& contact)
{
    if (contact.secondBall >= 0) {
        Point3 relative{
            state.balls[contact.secondBall].velocity.x -
                state.balls[contact.firstBall].velocity.x,
            state.balls[contact.secondBall].velocity.y -
                state.balls[contact.firstBall].velocity.y,
            state.balls[contact.secondBall].velocity.z -
                state.balls[contact.firstBall].velocity.z};
        return dot(relative, contact.normal);
    }
    return dot(state.balls[contact.firstBall].velocity, contact.normal);
}

}  // namespace

ContactSolverResult solveContactIsland(
    GameState& state, const ContactIsland& island,
    const PhysicsProfile& profile)
{
    ContactSolverResult result;
    if (island.limitExceeded ||
        static_cast<int>(island.ballIndices.size()) > profile.solver.maximumIslandSize) {
        result.status = ContactSolverStatus::IslandLimit;
        return result;
    }
    const double mass = profile.ball.massKg;
    const double inverseMass = 1.0 / mass;
    result.kineticEnergyBeforeJ = energy(state, island, mass);
    result.contacts.resize(island.contacts.size());
    std::vector<double> accumulated(island.contacts.size(), 0.0);
    std::vector<double> target(island.contacts.size(), 0.0);
    for (std::size_t index = 0; index < island.contacts.size(); ++index) {
        const ContinuousContactCandidate& contact = island.contacts[index];
        ContactImpulseDiagnostic& diagnostic = result.contacts[index];
        diagnostic.firstBall = contact.firstBall;
        diagnostic.secondBall = contact.secondBall;
        diagnostic.featureId = contact.featureId;
        const double restitution = contact.secondBall >= 0
            ? profile.ball.normalRestitution : profile.cushion.normalRestitution;
        const double before = normalSpeed(state, contact);
        target[index] = before < 0.0 ? -restitution * before : 0.0;
        diagnostic.targetNormalSpeedCmS = target[index];
        result.maximumPenetrationCm = std::max(
            result.maximumPenetrationCm, contact.penetrationCm);
    }
    if (result.maximumPenetrationCm > profile.solver.maximumPenetrationCm) {
        result.status = ContactSolverStatus::PenetrationLimit;
        return result;
    }

    for (int iteration = 0; iteration < profile.solver.velocityIterations; ++iteration) {
        result.velocityIterations = iteration + 1;
        const bool reverse = (iteration % 2) != 0;
        for (std::size_t step = 0; step < island.contacts.size(); ++step) {
            const std::size_t index = reverse
                ? island.contacts.size() - 1 - step : step;
            const ContinuousContactCandidate& contact = island.contacts[index];
            const double current = normalSpeed(state, contact);
            const double effectiveInverseMass = contact.secondBall >= 0
                ? 2.0 * inverseMass : inverseMass;
            const double deltaNs = (target[index] - current) / 100.0 /
                effectiveInverseMass;
            const double updated = std::max(0.0, accumulated[index] + deltaNs);
            const double applied = updated - accumulated[index];
            accumulated[index] = updated;
            const double deltaVelocityCmS = applied * inverseMass * 100.0;
            if (contact.secondBall >= 0) {
                addScaled(state.balls[contact.firstBall].velocity,
                    contact.normal, -deltaVelocityCmS);
                addScaled(state.balls[contact.secondBall].velocity,
                    contact.normal, deltaVelocityCmS);
            } else {
                addScaled(state.balls[contact.firstBall].velocity,
                    contact.normal, deltaVelocityCmS);
            }
        }
        double residual = 0.0;
        for (std::size_t index = 0; index < island.contacts.size(); ++index) {
            residual = std::max(residual, std::max(
                0.0, target[index] - normalSpeed(state, island.contacts[index])));
        }
        result.maximumResidualCmS = residual;
        if (residual <= profile.solver.residualToleranceCmS) break;
    }

    for (int iteration = 0; iteration < profile.solver.positionIterations; ++iteration) {
        result.positionIterations = iteration + 1;
        bool corrected = false;
        for (std::size_t index = 0; index < island.contacts.size(); ++index) {
            const ContinuousContactCandidate& contact = island.contacts[index];
            const double excess = std::max(0.0,
                contact.penetrationCm - profile.solver.penetrationSlopCm);
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
            corrected = true;
        }
        if (!corrected) break;
        break;
    }

    for (std::size_t index = 0; index < island.contacts.size(); ++index) {
        result.contacts[index].accumulatedNormalImpulseNs = accumulated[index];
        result.contacts[index].residualCmS = std::max(
            0.0, target[index] - normalSpeed(state, island.contacts[index]));
    }
    result.kineticEnergyAfterJ = energy(state, island, mass);
    for (int index : island.ballIndices) {
        if (!finitePoint(state.balls[index].velocity) ||
            !finitePoint(state.balls[index].position)) {
            result.status = ContactSolverStatus::NonfiniteState;
            return result;
        }
        state.balls[index].speed = static_cast<float>(std::sqrt(
            dot(state.balls[index].velocity, state.balls[index].velocity)));
    }
    if (result.maximumResidualCmS > profile.solver.residualToleranceCmS) {
        result.status = ContactSolverStatus::IterationLimit;
    }
    return result;
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
