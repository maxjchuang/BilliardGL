#include "deterministic_hash.h"
#include "game_state.h"
#include "physics.h"
#include "table_departure_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

GameState simultaneousRailLandingState(const PhysicsProfile& profile)
{
    const TableDepartureGeometry geometry =
        buildTableDepartureGeometry(profile);
    const double diameter = 2.0 * profile.ball.radiusCm;
    constexpr double kVerticalSeparationCm = 1.0;
    const double horizontalSeparation = std::sqrt(
        diameter * diameter -
        kVerticalSeparationCm * kVerticalSeparationCm);

    GameState state;
    initializeBalls(state);
    for (auto& ball : state.balls) {
        resetBallMotion(ball);
        resetPocketInteraction(ball);
        ball.pocketed = true;
    }

    BallState& landing = state.balls[0];
    landing.pocketed = false;
    landing.position = {68.0f, geometry.railTopSupportedCenterYCm, 30.0f};
    landing.velocity = {0.0f, -1.0f, 0.0f};
    landing.speed = 1.0f;
    landing.physicalDomain = PhysicalBallDomain::Airborne;

    BallState& striker = state.balls[1];
    striker.pocketed = false;
    striker.position = {static_cast<float>(68.0 - horizontalSeparation),
        static_cast<float>(geometry.railTopSupportedCenterYCm -
            kVerticalSeparationCm), 30.0f};
    striker.velocity = {50.0f, 0.0f, 0.0f};
    striker.speed = 50.0f;
    striker.physicalDomain = PhysicalBallDomain::Airborne;

    state.pocketedBallCount = kBallCount - 2;
    state.ballsMoving = true;
    clearGameplayEvents(state);
    return state;
}

bool canonicalOutputsEmpty(const PhysicsStepTelemetry& telemetry)
{
    return telemetry.contacts.empty() && telemetry.solverEvents.empty() &&
        telemetry.tableDepartureEvents.empty() &&
        telemetry.surfaceMotion.empty() &&
        telemetry.analyticSurfaceMotion.empty() &&
        telemetry.planeCrossings.empty() &&
        telemetry.planeAttestations.empty();
}

}  // namespace

int main()
{
    try {
        PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
        profile.solver.maximumEventsPerTick = 2;
        GameState state = simultaneousRailLandingState(profile);
        const GameState initial = state;
        const double diameter = 2.0 * profile.ball.radiusCm;
        const Point3 initialSeparation{
            initial.balls[0].position.x - initial.balls[1].position.x,
            initial.balls[0].position.y - initial.balls[1].position.y,
            initial.balls[0].position.z - initial.balls[1].position.z};
        expect(std::fabs(std::sqrt(double(initialSeparation.x) *
                    initialSeparation.x + double(initialSeparation.y) *
                    initialSeparation.y + double(initialSeparation.z) *
                    initialSeparation.z) - diameter) <=
                profile.solver.penetrationSlopCm,
            "fixture must begin at one nonpenetrating t0 ball contact");

        const PhysicsStepTelemetry telemetry = updatePhysics(state, 0.001f,
            profile, PhysicsBoundaryMode::ProductionTable);
        expect(telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
                telemetry.failureCode == PhysicsFailureCode::None,
            "t0 landing and cross-domain contact must solve successfully");
        expect(telemetry.tableDepartureEvents.size() == 1,
            "fixture must publish exactly one private landing transition");
        const TableDepartureEvent& landing =
            telemetry.tableDepartureEvents.front();
        expect(landing.kind == TableDepartureEventKind::RailTopLanding &&
                landing.ballIndex == 0 &&
                landing.absoluteWholeTickTimeSeconds == 0.0 &&
                landing.sourceDomain == PhysicalBallDomain::Airborne &&
                landing.targetDomain == PhysicalBallDomain::Airborne &&
                landing.transitionWitnessComplete &&
                landing.velocityAfterCmS.y >
                    profile.solver.residualToleranceCmS,
            "landing witness must retain the real positive post-island release");

        const auto pair = std::find_if(telemetry.contacts.begin(),
            telemetry.contacts.end(), [](const PhysicsContactRecord& contact) {
                return contact.kind == PhysicsContactKind::BallBall &&
                    contact.firstBall == 0 && contact.secondBall == 1;
            });
        const auto support = std::find_if(telemetry.contacts.begin(),
            telemetry.contacts.end(), [](const PhysicsContactRecord& contact) {
                return contact.kind == PhysicsContactKind::Support &&
                    contact.firstBall == 0;
            });
        expect(pair != telemetry.contacts.end() &&
                support != telemetry.contacts.end() &&
                pair->velocityImpulseApplied && pair->normalImpulseNs > 0.0 &&
                pair->wholeTickTimeOfImpactSeconds == 0.0 &&
                support->wholeTickTimeOfImpactSeconds == 0.0 &&
                pair->solverEventId == support->solverEventId &&
                pair->solverIslandId == support->solverIslandId &&
                landing.solverEventId == support->solverEventId,
            "landing support and ball contact must share one real t0 island");
        expect(landing.linkedPhysicsContactOrdinal >= 0 &&
                landing.linkedPhysicsContactOrdinal <
                    static_cast<int>(telemetry.contacts.size()) &&
                &telemetry.contacts[landing.linkedPhysicsContactOrdinal] ==
                    &*support,
            "private landing must bind the exact canonical Support contact");
        const auto solver = std::find_if(telemetry.solverEvents.begin(),
            telemetry.solverEvents.end(), [&](const SolverEventRecord& event) {
                return event.eventId == support->solverEventId &&
                    event.islandId == support->solverIslandId;
            });
        expect(solver != telemetry.solverEvents.end() &&
                solver->contactCount == 2 &&
                solver->wholeTickTimeOfImpactSeconds == 0.0,
            "shared solver event must account for the pair and support exactly");
        expect(state.balls[0].physicalDomain == PhysicalBallDomain::Airborne &&
                state.balls[0].velocity.y > 0.0f &&
                state.balls[0].position.y >
                    initial.balls[0].position.y,
            "positive post-island release must remain airborne through the tail");

        GameState repeated = simultaneousRailLandingState(profile);
        const PhysicsStepTelemetry repeatedTelemetry = updatePhysics(repeated,
            0.001f, profile, PhysicsBoundaryMode::ProductionTable);
        expect(repeatedTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
                gameStateSha256(repeated) == gameStateSha256(state) &&
                repeatedTelemetry.contacts.size() == telemetry.contacts.size() &&
                repeatedTelemetry.tableDepartureEvents.front().stableIdentity ==
                    landing.stableIdentity,
            "fixed t0 release must repeat exactly");

        PhysicsProfile exhaustedProfile = profile;
        exhaustedProfile.solver.maximumEventsPerTick = 1;
        GameState exhausted = simultaneousRailLandingState(exhaustedProfile);
        const std::string beforeSha = gameStateSha256(exhausted);
        const PhysicsStepTelemetry exhaustedTelemetry = updatePhysics(exhausted,
            0.001f, exhaustedProfile,
            PhysicsBoundaryMode::ProductionTable);
        expect(exhaustedTelemetry.stepStatus == PhysicsStepStatus::Failed &&
                exhaustedTelemetry.failureCode ==
                    PhysicsFailureCode::EventBudget &&
                canonicalOutputsEmpty(exhaustedTelemetry) &&
                gameStateSha256(exhausted) == beforeSha,
            "one landing slot must reserve the second event for its shared island or rollback atomically");

        std::cout << "t0_rail_landing_release witness_vy="
                  << landing.velocityAfterCmS.y << " final_vy="
                  << state.balls[0].velocity.y << " contacts="
                  << telemetry.contacts.size() << " event_budget=2/2\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
