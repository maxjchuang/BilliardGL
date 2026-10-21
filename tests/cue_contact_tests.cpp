#include "cue_contact.h"
#include "coupled_cue_contact.h"
#include "frozen_cue_topology.h"
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
        std::exit(EXIT_FAILURE);
    }
}

bool close(double first, double second, double tolerance = 0.0001)
{
    return std::fabs(first - second) <= tolerance;
}

billiardgl::CueImpactInput inputAt(double sideRadius, double verticalRadius)
{
    billiardgl::CueImpactInput input;
    input.cueSpeedCmS = 200.0;
    input.cueMassKg = 0.5;
    input.direction = {{1.0, 0.0, 0.0}};
    input.tipOffsetRadius = {{sideRadius, verticalRadius}};
    input.tipOffsetCm = {{sideRadius * billiardgl::kBallRadius,
        verticalRadius * billiardgl::kBallRadius}};
    input.chalkState = "CHALKED";
    return input;
}

double ballEnergy(const billiardgl::BallState& ball,
    const billiardgl::BallProperties& properties)
{
    const double v2 = (ball.velocity.x * ball.velocity.x +
        ball.velocity.y * ball.velocity.y + ball.velocity.z * ball.velocity.z) / 10000.0;
    const double w2 = ball.angularVelocity.x * ball.angularVelocity.x +
        ball.angularVelocity.y * ball.angularVelocity.y +
        ball.angularVelocity.z * ball.angularVelocity.z;
    const double radiusM = properties.radiusCm / 100.0;
    const double inertia = 0.4 * properties.massKg * radiusM * radiusM;
    return 0.5 * properties.massKg * v2 + 0.5 * inertia * w2;
}

void testCenterHitIsFiniteSpinFreeAndDissipative()
{
    const billiardgl::PhysicsProfile profile = billiardgl::defaultChinesePoolPhysicsProfile();
    billiardgl::BallState ball;
    const billiardgl::CueImpactInput input = inputAt(0.0, 0.0);
    const double energyBefore = 0.5 * input.cueMassKg * 4.0;
    const billiardgl::CueContactResult result = billiardgl::resolveCueContact(
        ball, input, profile.ball, profile.cue);

    expect(result.regime == billiardgl::CueContactRegime::Stick, "center hit sticks");
    expect(result.applied && std::isfinite(result.normalImpulseNs), "center impulse is finite");
    expect(ball.velocity.x > 0.0f && close(ball.velocity.y, 0.0) && close(ball.velocity.z, 0.0),
        "center hit launches in cue direction");
    expect(close(ball.angularVelocity.x, 0.0) && close(ball.angularVelocity.y, 0.0) &&
        close(ball.angularVelocity.z, 0.0), "center hit adds no spin");
    const double cueAfter2 = result.cueVelocityAfterMS[0] * result.cueVelocityAfterMS[0] +
        result.cueVelocityAfterMS[1] * result.cueVelocityAfterMS[1] +
        result.cueVelocityAfterMS[2] * result.cueVelocityAfterMS[2];
    expect(ballEnergy(ball, profile.ball) + 0.5 * input.cueMassKg * cueAfter2 <=
        energyBefore + 1e-9, "cue plus ball energy does not increase");
}

void testOffsetsProduceMirroredSpinAndRespectFrictionCone()
{
    const billiardgl::PhysicsProfile profile = billiardgl::defaultChinesePoolPhysicsProfile();
    billiardgl::BallState left;
    billiardgl::BallState right;
    const billiardgl::CueContactResult leftResult = billiardgl::resolveCueContact(
        left, inputAt(-0.2, 0.0), profile.ball, profile.cue);
    const billiardgl::CueContactResult rightResult = billiardgl::resolveCueContact(
        right, inputAt(0.2, 0.0), profile.ball, profile.cue);
    expect(leftResult.regime == billiardgl::CueContactRegime::Stick &&
        rightResult.regime == billiardgl::CueContactRegime::Stick, "small offsets stick");
    expect(close(left.angularVelocity.y, -right.angularVelocity.y, 0.0002) &&
        std::fabs(left.angularVelocity.y) > 0.0, "side offsets mirror sidespin");

    billiardgl::BallState top;
    billiardgl::BallState bottom;
    billiardgl::resolveCueContact(top, inputAt(0.0, 0.2), profile.ball, profile.cue);
    billiardgl::resolveCueContact(bottom, inputAt(0.0, -0.2), profile.ball, profile.cue);
    expect(close(top.angularVelocity.z, -bottom.angularVelocity.z, 0.0002) &&
        std::fabs(top.angularVelocity.z) > 0.0, "vertical offsets mirror top and bottom spin");

    billiardgl::BallState slipBall;
    const billiardgl::CueContactResult slip = billiardgl::resolveCueContact(
        slipBall, inputAt(0.7, 0.0), profile.ball, profile.cue);
    expect(slip.regime == billiardgl::CueContactRegime::Slip && slip.applied,
        "large horizontal offset slips but remains 2.5D-compatible");
    expect(slip.tangentialImpulseNs <=
        profile.cue.chalkedFrictionCoefficient * slip.normalImpulseNs + 1e-10,
        "slip impulse respects friction cone");
}

void testChalkMiscueAndUnsupportedInputsAreExplicitAndAtomic()
{
    billiardgl::PhysicsProfile profile = billiardgl::defaultChinesePoolPhysicsProfile();
    billiardgl::CueImpactInput unchalked = inputAt(0.3, 0.0);
    unchalked.chalkState = "UNCHALKED";
    billiardgl::BallState ball;
    const billiardgl::CueContactResult slip = billiardgl::resolveCueContact(
        ball, unchalked, profile.ball, profile.cue);
    expect(slip.regime == billiardgl::CueContactRegime::Slip,
        "unchalked contact reaches slip at a smaller offset");

    billiardgl::BallState untouched;
    untouched.velocity.x = 3.0f;
    const billiardgl::CueContactResult miscue = billiardgl::resolveCueContact(
        untouched, inputAt(0.81, 0.0), profile.ball, profile.cue);
    expect(miscue.regime == billiardgl::CueContactRegime::Miscue && !miscue.applied &&
        close(untouched.velocity.x, 3.0), "miscue does not mutate the ball");

    billiardgl::BallState verticalSlip;
    const billiardgl::CueContactResult unsupported = billiardgl::resolveCueContact(
        verticalSlip, inputAt(0.0, 0.7), profile.ball, profile.cue);
    expect(unsupported.regime == billiardgl::CueContactRegime::Unsupported &&
        unsupported.error == "vertical_ball_impulse_requires_3d" &&
        close(verticalSlip.velocity.x, 0.0), "vertical slip is rejected atomically");

    billiardgl::CueImpactInput elevated = inputAt(0.0, 0.0);
    elevated.elevationDegrees = 5.0;
    const billiardgl::CueContactResult elevation = billiardgl::resolveCueContact(
        verticalSlip, elevated, profile.ball, profile.cue);
    expect(elevation.error == "cue_elevation_requires_3d" && !elevation.applied,
        "elevated cue is explicitly unsupported");
}

void testMovingSpinningBallAndNonApproachAreHandled()
{
    const billiardgl::PhysicsProfile profile = billiardgl::defaultChinesePoolPhysicsProfile();
    billiardgl::BallState ball;
    ball.velocity.x = 20.0f;
    ball.angularVelocity.y = 4.0f;
    const billiardgl::CueContactResult result = billiardgl::resolveCueContact(
        ball, inputAt(0.2, 0.0), profile.ball, profile.cue);
    expect(result.applied && std::isfinite(ball.velocity.x) &&
        std::isfinite(ball.angularVelocity.y), "moving spinning ball remains finite");

    billiardgl::CueImpactInput zero = inputAt(0.0, 0.0);
    zero.cueSpeedCmS = 0.0;
    billiardgl::BallState receding;
    receding.velocity.x = 1.0f;
    const billiardgl::CueContactResult noImpact = billiardgl::resolveCueContact(
        receding, zero, profile.ball, profile.cue);
    expect(!noImpact.applied && noImpact.error == "cue_not_approaching" &&
        close(receding.velocity.x, 1.0), "nonapproaching input is atomic");
}

void testCanonicalStrokeObservationFoundation()
{
    billiardgl::CanonicalCueTipStrokeObservation observation;
    observation.shotIdentity = "shot.1";
    observation.actorPlayerId = 0;
    observation.rackIdentity = "rack.1";
    observation.rulesStateHash = "state.1";
    observation.observationStartedBeforeFirstContact = true;
    observation.observationEndedAfterAuthoritativeRelease = true;
    observation.noDroppedIntervals = true;
    observation.successfulLaunchCommit = true;
    billiardgl::CanonicalCueTipContactInterval interval;
    interval.relativeTipBallPathLengthCm=0.2;
    interval.relativeTipBallPathLengthAttested=true;
    interval.ordinal = 0;interval.startTimeSeconds = 0.0;
    interval.endTimeSeconds = 0.001;
    interval.startTickOffsetSeconds=0.0;
    interval.endTickOffsetSeconds=0.001;
    interval.origin = billiardgl::CueTipIntervalOrigin::AnalyticCueContactSolver;
    observation.intervals.push_back(interval);
    const auto validation = billiardgl::validateCanonicalCueTipStrokeObservation(
        observation);
    expect(validation.complete && validation.error.empty(),
        "canonical complete single interval validates without foul classification");
    billiardgl::CanonicalCueTipStrokeObserver observer;
    billiardgl::CanonicalCueTipStrokeObservation identity=observation;
    identity.intervals.clear();identity.endTick=identity.startTick;
    expect(observer.begin(identity)&&observer.append(interval)&&
        observer.append(interval)&&observer.observation().intervals.size()==1,
        "observer canonically dedupes exact interval retry");
    expect(observer.finish(1).complete&&observer.committed(),
        "authoritative motion end is required for full stroke commit");
    const std::string digest=observer.observation().digest;
    expect(observer.finish(1).complete&&observer.observation().digest==digest,
        "exact observation commit retry returns the immutable digest");
    billiardgl::CanonicalCueTipContactInterval conflict=interval;
    conflict.ordinal=0;conflict.endTimeSeconds=0.002;
    expect(!observer.append(conflict)&&observer.observation().digest==digest,
        "conflicting retry is rejected with zero observation delta");
    observer.rollback();
    expect(!observer.active()&&!observer.committed(),
        "rollback removes uncommitted observer state");
    expect(observer.begin(identity)&&observer.append(interval),
        "observer restarts after rollback");
    billiardgl::CanonicalCueTipContactInterval contiguous=interval;
    contiguous.startTimeSeconds=interval.endTimeSeconds;
    contiguous.endTimeSeconds=0.002;
    contiguous.startTickOffsetSeconds=interval.endTickOffsetSeconds;
    contiguous.endTickOffsetSeconds=0.002;
    contiguous.solverEventIds={2};
    expect(observer.append(contiguous)&&observer.observation().intervals.size()==1&&
        observer.observation().intervals.front().endTimeSeconds==0.002,
        "contiguous same-origin samples merge into one uninterrupted interval");
    billiardgl::CanonicalCueTipContactInterval separated=contiguous;
    separated.startTimeSeconds=0.003;separated.endTimeSeconds=0.004;
    separated.startTickOffsetSeconds=0.003;separated.endTickOffsetSeconds=0.004;
    separated.origin=billiardgl::CueTipIntervalOrigin::AuthoritativeCueMotionObserver;
    expect(observer.append(separated)&&observer.observation().intervals.size()==2,
        "separated recontact creates a second canonical interval");
    auto invalid=identity;invalid.intervals={interval};
    invalid.observationEndedAfterAuthoritativeRelease=true;
    invalid.intervals[0].origin=static_cast<billiardgl::CueTipIntervalOrigin>(99);
    expect(!billiardgl::validateCanonicalCueTipStrokeObservation(invalid).valid,
        "unknown interval origin fails closed");
    invalid=observation;invalid.intervals[0].normalImpulseNs=
        std::numeric_limits<double>::quiet_NaN();
    expect(!billiardgl::validateCanonicalCueTipStrokeObservation(invalid).valid,
        "non-finite interval measurement fails closed");
    invalid=observation;invalid.intervals.push_back(interval);
    invalid.intervals[1].ordinal=1;
    expect(!billiardgl::validateCanonicalCueTipStrokeObservation(invalid).valid,
        "overlapping canonical intervals fail closed");
    invalid=observation;invalid.noDroppedIntervals=false;
    const auto dropped=
        billiardgl::validateCanonicalCueTipStrokeObservation(invalid);
    expect(dropped.valid&&!dropped.complete&&
        !invalid.fullStrokeComplete,
        "dropped intervals remain diagnostic-only and incomplete");
    invalid=observation;invalid.observationStartedBeforeFirstContact=false;
    expect(!billiardgl::validateCanonicalCueTipStrokeObservation(invalid).complete,
        "late observation start cannot become a complete stroke");
    invalid=observation;invalid.intervals.clear();
    expect(billiardgl::validateCanonicalCueTipStrokeObservation(invalid).complete,
        "authoritatively observed no-contact stroke may be complete and empty");
}

}  // namespace

int main()
{
    // Additive Phase 5 interval coverage lives outside the frozen Phase 3
    // convergence-contract file, whose bytes are historical release evidence.
    {
        auto profile = billiardgl::defaultChinesePoolPhysicsProfile();
        profile.frozenCueContact.enabled = true;
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (auto& ball : state.balls) {
            ball.pocketed = true;
            ball.position = {};
            ball.velocity = {};
            ball.angularVelocity = {};
        }
        state.balls[0].pocketed = false;
        state.balls[1].pocketed = false;
        state.balls[1].position.x = 2.0f * profile.ball.radiusCm;
        const auto topology = billiardgl::detectFrozenCueTopology(state, 0,
            profile, billiardgl::PhysicsBoundaryMode::Unbounded);
        state.balls[1].pocketed = true;
        const auto centered = billiardgl::solveCoupledCueContact(
            state, topology, inputAt(0.0, 0.0), profile);
        expect(centered.status == billiardgl::CoupledCueContactStatus::Released &&
            centered.contact.applied && !centered.contact.microsteps.empty() &&
            centered.contact.canonicalTipIntervals.size() == 1 &&
            centered.contact.canonicalTipIntervals[0].origin ==
                billiardgl::CueTipIntervalOrigin::CoupledCueContactSolver &&
            centered.contact.canonicalTipIntervals[0].endTimeSeconds > 0.0 &&
            centered.contact.canonicalTipIntervals[0].relativeTipBallPathLengthAttested &&
            centered.contact.canonicalTipIntervals[0].relativeTipBallPathLengthCm > 0.0,
            "released coupled contact carries its actual canonical tip path interval");
    }
    testCenterHitIsFiniteSpinFreeAndDissipative();
    testOffsetsProduceMirroredSpinAndRespectFrictionCone();
    testChalkMiscueAndUnsupportedInputsAreExplicitAndAtomic();
    testMovingSpinningBallAndNonApproachAreHandled();
    testCanonicalStrokeObservationFoundation();
    return EXIT_SUCCESS;
}
