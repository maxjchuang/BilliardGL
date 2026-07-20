#include "coupled_cue_contact.h"

#include "contact_solver.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace billiardgl {
namespace {

using Vector = std::array<double, 3>;

Vector add(const Vector& a, const Vector& b)
{
    return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}};
}

Vector subtract(const Vector& a, const Vector& b)
{
    return {{a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
}

Vector multiply(const Vector& value, double scale)
{
    return {{value[0] * scale, value[1] * scale, value[2] * scale}};
}

double dot(const Vector& a, const Vector& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vector cross(const Vector& a, const Vector& b)
{
    return {{a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]}};
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

Vector velocityM(const BallState& ball)
{
    return {{ball.velocity.x / 100.0, ball.velocity.y / 100.0,
        ball.velocity.z / 100.0}};
}

Vector angular(const BallState& ball)
{
    return {{ball.angularVelocity.x, ball.angularVelocity.y,
        ball.angularVelocity.z}};
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

double ballEnergy(const BallState& ball, const BallProperties& properties)
{
    if (ball.pocketed) return 0.0;
    const Vector velocity = velocityM(ball);
    const Vector spin = angular(ball);
    const double radiusM = properties.radiusCm / 100.0;
    const double inertia = 0.4 * properties.massKg * radiusM * radiusM;
    return 0.5 * properties.massKg * dot(velocity, velocity) +
        0.5 * inertia * dot(spin, spin);
}

double totalBallEnergy(const GameState& state, const BallProperties& properties)
{
    double result = 0.0;
    for (const BallState& ball : state.balls) result += ballEnergy(ball, properties);
    return result;
}

bool finiteBall(const BallState& ball)
{
    return std::isfinite(ball.position.x) && std::isfinite(ball.position.y) &&
        std::isfinite(ball.position.z) && std::isfinite(ball.velocity.x) &&
        std::isfinite(ball.velocity.y) && std::isfinite(ball.velocity.z) &&
        std::isfinite(ball.angularVelocity.x) &&
        std::isfinite(ball.angularVelocity.y) &&
        std::isfinite(ball.angularVelocity.z);
}

void setFailure(CoupledCueContactResult& result,
    CoupledCueContactStatus status, const char* error)
{
    result.status = status;
    result.error = error;
    result.contact.error = error;
    result.contact.applied = false;
}

}  // namespace

double huntCrossleyNormalForce(double compressionM,
    double compressionRateMS, double stiffnessNPerM32,
    double dissipationSPerM)
{
    if (!(compressionM > 0.0) || !(stiffnessNPerM32 > 0.0) ||
        !std::isfinite(compressionM) || !std::isfinite(compressionRateMS) ||
        !std::isfinite(stiffnessNPerM32) || !std::isfinite(dissipationSPerM)) {
        return 0.0;
    }
    return std::max(0.0, stiffnessNPerM32 * std::pow(compressionM, 1.5) *
        (1.0 + dissipationSPerM * compressionRateMS));
}

CoupledCueContactResult solveCoupledCueContact(const GameState& state,
    const FrozenCueTopology& topology, const CueImpactInput& input,
    const PhysicsProfile& profile)
{
    CoupledCueContactResult result;
    result.state = state;
    const FrozenCueContactProperties& settings = profile.frozenCueContact;
    if (!settings.enabled || topology.status != FrozenCueTopologyStatus::Valid ||
        !topology.frozen || input.cueBallIndex < 0 ||
        input.cueBallIndex >= kBallCount || input.cueMassKg <= 0.0 ||
        input.cueSpeedCmS <= 0.0 || std::fabs(input.elevationDegrees) > 1e-9 ||
        !std::isfinite(input.cueMassKg) || !std::isfinite(input.cueSpeedCmS)) {
        setFailure(result, CoupledCueContactStatus::InvalidInput,
            "invalid_coupled_cue_contact_input");
        return result;
    }

    Vector direction = input.direction;
    if (!finite(direction) || std::fabs(direction[1]) > 1e-9) {
        setFailure(result, CoupledCueContactStatus::InvalidInput,
            "cue_elevation_requires_3d");
        return result;
    }
    const double directionLength = magnitude(direction);
    if (!(directionLength > 0.0)) {
        setFailure(result, CoupledCueContactStatus::InvalidInput,
            "invalid_cue_direction");
        return result;
    }
    direction = multiply(direction, 1.0 / directionLength);
    const double sideOffset = input.tipOffsetRadius[0];
    const double verticalOffset = input.tipOffsetRadius[1];
    const double offsetFraction = std::sqrt(
        sideOffset * sideOffset + verticalOffset * verticalOffset);
    if (!std::isfinite(offsetFraction) || offsetFraction >= 1.0) {
        setFailure(result, CoupledCueContactStatus::InvalidInput,
            "cue_offset_outside_ball");
        return result;
    }

    BallState& ball = result.state.balls[input.cueBallIndex];
    if (ball.pocketed || !finiteBall(ball)) {
        setFailure(result, CoupledCueContactStatus::NonfiniteState,
            "nonfinite_state");
        return result;
    }
    const double radiusM = profile.ball.radiusCm / 100.0;
    const double inertia = 0.4 * profile.ball.massKg * radiusM * radiusM;
    const Vector side = {{-direction[2], 0.0, direction[0]}};
    const Vector up = {{0.0, 1.0, 0.0}};
    const Vector arm = add(multiply(direction,
        -radiusM * std::sqrt(std::max(0.0,
            1.0 - offsetFraction * offsetFraction))),
        add(multiply(side, sideOffset * radiusM),
            multiply(up, verticalOffset * radiusM)));
    const Vector normal = multiply(arm, -1.0 / radiusM);
    Vector relative = subtract(multiply(direction, input.cueSpeedCmS / 100.0),
        add(velocityM(ball), cross(angular(ball), arm)));
    Vector tangentVelocity = subtract(relative, multiply(normal, dot(relative, normal)));
    const double tangentMagnitude = magnitude(tangentVelocity);
    Vector tangent = tangentMagnitude > 1e-12 ?
        multiply(tangentVelocity, 1.0 / tangentMagnitude) : side;
    if (magnitude(tangent) <= 1e-12) tangent = {{0.0, 0.0, 1.0}};

    const double friction = input.chalkState == "UNCHALKED" ?
        profile.cue.unchalkedFrictionCoefficient :
        profile.cue.chalkedFrictionCoefficient;
    result.contact.frictionCoefficient = friction;
    result.contact.contactArmM = arm;
    result.contact.contactNormal = normal;
    result.contact.cueVelocityBeforeMS = multiply(
        direction, input.cueSpeedCmS / 100.0);
    result.contact.ballVelocityBeforeMS = velocityM(ball);
    result.contact.ballAngularVelocityBeforeRadS = angular(ball);
    result.contact.normalRelativeSpeedBeforeMS = dot(relative, normal);
    result.contact.tangentialRelativeVelocityBeforeMS = tangentVelocity;
    result.contact.tangentialRelativeSpeedBeforeMS = tangentMagnitude;

    double cuePositionM = 0.0;
    double cueVelocityMS = input.cueSpeedCmS / 100.0;
    double compressionM = 0.0;
    double tangentHistoryM = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double signedTangentialImpulseNs = 0.0;
    double dissipatedEnergyJ = 0.0;
    double tangentialProjectionEnergyJ = 0.0;
    bool compressed = false;
    const double initialEnergy = totalBallEnergy(result.state, profile.ball) +
        0.5 * input.cueMassKg * cueVelocityMS * cueVelocityMS;
    result.contact.inputKineticEnergyJ = initialEnergy;
    const int maximumSteps = static_cast<int>(std::ceil(
        settings.maximumContactSeconds / settings.microstepSeconds));
    ContactIsland rigidIsland = topology.island;
    rigidIsland.ballIndices.erase(std::remove_if(
        rigidIsland.ballIndices.begin(), rigidIsland.ballIndices.end(),
        [&](int index) { return result.state.balls[index].pocketed; }),
        rigidIsland.ballIndices.end());
    rigidIsland.contacts.erase(std::remove_if(
        rigidIsland.contacts.begin(), rigidIsland.contacts.end(),
        [&](const ContinuousContactCandidate& contact) {
            return result.state.balls[contact.firstBall].pocketed ||
                (contact.secondBall >= 0 &&
                    result.state.balls[contact.secondBall].pocketed);
        }), rigidIsland.contacts.end());
    std::sort(rigidIsland.contacts.begin(), rigidIsland.contacts.end(),
        continuousContactLess);

    for (int stepIndex = 0; stepIndex < maximumSteps; ++stepIndex) {
        const double dt = settings.microstepSeconds;
        std::array<Point3, kBallCount> velocityBeforeStep;
        for (int index = 0; index < kBallCount; ++index) {
            velocityBeforeStep[index] = result.state.balls[index].velocity;
        }
        relative = subtract(multiply(direction, cueVelocityMS),
            add(velocityM(ball), cross(angular(ball), arm)));
        const double compressionRateMS = dot(relative, normal);
        const double tangentSpeedMS = dot(relative, tangent);
        compressionM = std::max(0.0, compressionM + compressionRateMS * dt);
        tangentHistoryM += tangentSpeedMS * dt;
        compressed = compressed || compressionM > settings.releaseCompressionM;
        const double elasticNormalForce = settings.normalStiffnessNPerM32 *
            std::pow(compressionM, 1.5);
        const double normalForceN = huntCrossleyNormalForce(compressionM,
            compressionRateMS, settings.normalStiffnessNPerM32,
            settings.normalDissipationSPerM);
        const double tangentialTrialN =
            -settings.tangentialStiffnessNPerM * tangentHistoryM -
            settings.tangentialDampingNsPerM * tangentSpeedMS;
        const double tangentialForceN = std::clamp(tangentialTrialN,
            -friction * normalForceN, friction * normalForceN);
        const bool slipping = std::fabs(tangentialTrialN) >
            friction * normalForceN;
        const bool releaseCandidate = compressed &&
            compressionM <= settings.releaseCompressionM &&
            normalForceN <= 0.0 && compressionRateMS < 0.0;
        if (slipping && settings.tangentialStiffnessNPerM > 0.0) {
            tangentHistoryM = -(tangentialForceN +
                settings.tangentialDampingNsPerM * tangentSpeedMS) /
                settings.tangentialStiffnessNPerM;
        }
        if (compressionM > settings.maximumCompressionM) {
            setFailure(result, CoupledCueContactStatus::CompressionLimit,
                "compression_limit");
            return result;
        }
        if (normalForceN > settings.maximumNormalForceN) {
            setFailure(result, CoupledCueContactStatus::ForceLimit,
                "compression_limit");
            return result;
        }

        const Vector force = add(multiply(normal, normalForceN),
            multiply(tangent, tangentialForceN));
        const Vector impulse = multiply(force, dt);
        addVelocity(ball, multiply(impulse, 1.0 / profile.ball.massKg));
        addAngularVelocity(ball, multiply(cross(arm, impulse), 1.0 / inertia));
        const double cueAccelerationMS2 = -dot(force, direction) / input.cueMassKg;
        cueVelocityMS += cueAccelerationMS2 * dt;
        cuePositionM += cueVelocityMS * dt;
        const ContactSolverResult rigid = solveContactIslandIteration(
            result.state, rigidIsland, profile,
            profile.solver.velocityIterations, 1);
        if (rigid.status != ContactSolverStatus::Converged) {
            switch (rigid.status) {
            case ContactSolverStatus::IslandLimit:
                setFailure(result, CoupledCueContactStatus::ContactIslandLimit,
                    "contact_island_limit");
                break;
            case ContactSolverStatus::PenetrationLimit:
                setFailure(result, CoupledCueContactStatus::PenetrationLimit,
                    "compression_limit");
                break;
            case ContactSolverStatus::NonfiniteState:
                setFailure(result, CoupledCueContactStatus::NonfiniteState,
                    "nonfinite_state");
                break;
            case ContactSolverStatus::IterationLimit:
                setFailure(result, CoupledCueContactStatus::Nonconvergence,
                    "cue_contact_nonconvergence");
                break;
            case ContactSolverStatus::Converged:
                break;
            }
            return result;
        }
        for (int ballIndex : rigidIsland.ballIndices) {
            BallState& movingBall = result.state.balls[ballIndex];
            if (movingBall.pocketed) continue;
            movingBall.position.x +=
                static_cast<float>(movingBall.velocity.x * dt);
            movingBall.position.y +=
                static_cast<float>(movingBall.velocity.y * dt);
            movingBall.position.z +=
                static_cast<float>(movingBall.velocity.z * dt);
        }
        normalImpulseNs += normalForceN * dt;
        tangentialImpulseNs += std::fabs(tangentialForceN) * dt;
        signedTangentialImpulseNs += tangentialForceN * dt;

        const double normalDampingPower = std::max(0.0,
            elasticNormalForce * settings.normalDissipationSPerM *
            compressionRateMS * compressionRateMS);
        const double tangentialDampingPower = std::max(0.0,
            settings.tangentialDampingNsPerM *
            tangentSpeedMS * tangentSpeedMS);
        const double slipPower = slipping ?
            std::max(0.0, -tangentialForceN * tangentSpeedMS) : 0.0;
        dissipatedEnergyJ +=
            (normalDampingPower + tangentialDampingPower + slipPower) * dt;
        double kineticEnergy = totalBallEnergy(result.state, profile.ball) +
            0.5 * input.cueMassKg * cueVelocityMS * cueVelocityMS;
        double tangentialElasticEnergy =
            0.5 * settings.tangentialStiffnessNPerM *
                tangentHistoryM * tangentHistoryM;
        if (slipping) {
            tangentialProjectionEnergyJ = tangentialElasticEnergy;
        }
        double elasticEnergy =
            settings.normalStiffnessNPerM32 * std::pow(compressionM, 2.5) / 2.5 +
            tangentialElasticEnergy;
        if (settings.normalDissipationSPerM == 0.0 &&
            settings.tangentialDampingNsPerM == 0.0 && !slipping) {
            if (releaseCandidate && tangentialElasticEnergy <= 1e-14) {
                compressionM = 0.0;
                elasticEnergy = 0.0;
                const double ballKineticEnergy =
                    totalBallEnergy(result.state, profile.ball);
                const double cueKineticEnergy =
                    std::max(0.0, initialEnergy - ballKineticEnergy);
                cueVelocityMS = std::copysign(std::sqrt(
                    2.0 * cueKineticEnergy / input.cueMassKg), cueVelocityMS);
                kineticEnergy = ballKineticEnergy + cueKineticEnergy;
            } else if (!(compressionRateMS < 0.0 &&
                compressionM <= -compressionRateMS * dt +
                    settings.releaseCompressionM)) {
                const double targetNormalElasticEnergy =
                    initialEnergy - kineticEnergy - tangentialElasticEnergy;
                if (targetNormalElasticEnergy >= 0.0) {
                    compressionM = std::pow(
                        targetNormalElasticEnergy * 2.5 /
                            settings.normalStiffnessNPerM32,
                        0.4);
                    elasticEnergy = targetNormalElasticEnergy +
                        tangentialElasticEnergy;
                }
            }
        }

        double normalElasticEnergy =
            settings.normalStiffnessNPerM32 *
                std::pow(compressionM, 2.5) / 2.5;
        double recoverableTangentialEnergy = std::max(
            0.0, tangentialElasticEnergy - tangentialProjectionEnergyJ);
        const double availableElasticEnergy =
            std::max(0.0, initialEnergy - kineticEnergy);
        if (normalElasticEnergy + recoverableTangentialEnergy >
            availableElasticEnergy) {
            if (normalElasticEnergy > availableElasticEnergy) {
                compressionM = std::pow(
                    availableElasticEnergy * 2.5 /
                        settings.normalStiffnessNPerM32,
                    0.4);
                normalElasticEnergy = availableElasticEnergy;
                recoverableTangentialEnergy = 0.0;
            } else {
                recoverableTangentialEnergy =
                    availableElasticEnergy - normalElasticEnergy;
            }
            if (settings.tangentialStiffnessNPerM > 0.0) {
                tangentialElasticEnergy = tangentialProjectionEnergyJ +
                    recoverableTangentialEnergy;
                tangentHistoryM = std::copysign(std::sqrt(
                    2.0 * tangentialElasticEnergy /
                        settings.tangentialStiffnessNPerM),
                    tangentHistoryM);
            }
            elasticEnergy = normalElasticEnergy + tangentialElasticEnergy;
        }

        CueContactMicrostep step;
        step.index = stepIndex;
        step.timeSeconds = (stepIndex + 1) * dt;
        step.cuePositionM = cuePositionM;
        step.cueVelocityMS = cueVelocityMS;
        step.cueAccelerationMS2 = cueAccelerationMS2;
        step.compressionM = compressionM;
        step.compressionRateMS = compressionRateMS;
        step.normalForceN = normalForceN;
        step.tangentialForceN = tangentialForceN;
        step.normalImpulseNs = normalImpulseNs;
        step.tangentialImpulseNs = tangentialImpulseNs;
        step.kineticEnergyJ = kineticEnergy;
        step.elasticEnergyJ = elasticEnergy;
        step.dissipatedEnergyJ = dissipatedEnergyJ;
        const double recoverableElasticEnergy =
            normalElasticEnergy +
            recoverableTangentialEnergy;
        step.energyResidualJ =
            kineticEnergy + recoverableElasticEnergy - initialEnergy;
        step.maximumPenetrationCm = rigid.maximumPenetrationCm;
        step.solverResidualCmS = rigid.maximumResidualCmS;
        step.solverIterations = rigid.velocityIterations;
        step.regime = slipping ? CueContactRegime::Slip : CueContactRegime::Stick;
        for (int index = 0; index < kBallCount; ++index) {
            CueContactBallSample& sample = step.balls[index];
            sample.index = index;
            sample.positionCm = result.state.balls[index].position;
            sample.velocityCmS = result.state.balls[index].velocity;
            sample.angularVelocityRadS = result.state.balls[index].angularVelocity;
        }
        for (int ballIndex : rigidIsland.ballIndices) {
            const Point3& before = velocityBeforeStep[ballIndex];
            const Point3& after = result.state.balls[ballIndex].velocity;
            step.balls[ballIndex].accelerationCmS2 = Point3{
                static_cast<float>((after.x - before.x) / dt),
                static_cast<float>((after.y - before.y) / dt),
                static_cast<float>((after.z - before.z) / dt)};
        }
        for (std::size_t contactIndex = 0;
             contactIndex < rigid.contacts.size(); ++contactIndex) {
            const ContactImpulseDiagnostic& diagnostic =
                rigid.contacts[contactIndex];
            CueContactConstraintSample sample;
            sample.kind = static_cast<int>(
                rigidIsland.contacts[contactIndex].kind);
            sample.firstBall = diagnostic.firstBall;
            sample.secondBall = diagnostic.secondBall;
            sample.featureId = diagnostic.featureId;
            sample.normal = diagnostic.normal;
            sample.normalImpulseNs =
                diagnostic.accumulatedNormalImpulseNs;
            sample.tangentialImpulseNs =
                diagnostic.accumulatedTangentialImpulseNs;
            sample.penetrationCm =
                rigidIsland.contacts[contactIndex].penetrationCm;
            sample.residualCmS = diagnostic.residualCmS;
            step.contacts.push_back(sample);
        }
        result.contact.microsteps.push_back(step);

        if (!finiteBall(ball) || !std::isfinite(cueVelocityMS) ||
            !std::isfinite(step.energyResidualJ)) {
            setFailure(result, CoupledCueContactStatus::NonfiniteState,
                "nonfinite_state");
            return result;
        }
        if (step.energyResidualJ > 1e-6) {
            setFailure(result, CoupledCueContactStatus::PassiveEnergyGain,
                "passive_energy_gain");
            return result;
        }
        if (releaseCandidate) {
            result.status = CoupledCueContactStatus::Released;
            result.contact.regime = CueContactRegime::Released;
            result.contact.applied = true;
            result.contact.normalImpulseNs = normalImpulseNs;
            result.contact.tangentialImpulseNs = tangentialImpulseNs;
            result.contact.impulseNs = add(multiply(normal, normalImpulseNs),
                multiply(tangent, signedTangentialImpulseNs));
            result.contact.cueVelocityAfterMS = multiply(direction, cueVelocityMS);
            result.contact.ballVelocityAfterMS = velocityM(ball);
            result.contact.ballAngularVelocityAfterRadS = angular(ball);
            result.contact.outputKineticEnergyJ = kineticEnergy;
            return result;
        }
    }

    setFailure(result, CoupledCueContactStatus::NoRelease,
        "cue_contact_no_release");
    return result;
}

}  // namespace billiardgl
