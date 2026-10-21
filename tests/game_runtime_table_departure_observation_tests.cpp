#include "game_runtime.h"
#include "shot.h"
#include "table_departure_geometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;

void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

CueImpactInput fixtureCueImpact(const PhysicsProfile& profile)
{
    constexpr float kHalfPi = 1.57079632679489661923f;
    return cueImpactFromShotControls(kHalfPi, 200.0f, profile);
}

void runMaximumBreakRoundedRailRegression()
{
    const PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
    struct RunReceipt {
        int contactTick = -1;
        PhysicsContactRecord contact;
    };
    const auto run = [&]() {
        GameRuntime runtime;
        require(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
            "Chinese rules mode selected for maximum break");
        require(runtime.replaceStateForScenario(runtime.state(), profile).ok,
            "shipping profile selected for maximum break");
        require(runtime.applyCueImpact(fixtureCueImpact(profile)).ok,
            "maximum break begins through public cue impact");
        RunReceipt receipt;
        int steps = 0;
        while (runtime.state().ballsMoving && steps < 240) {
            const ActionResult result = runtime.step(1);
            if (!result.ok) {
                std::cerr << "maximum_break_step_error=" << result.errorCode
                          << " observation="
                          << runtime.tableDepartureObservationError()
                          << " tick=" << runtime.tick();
                if (runtime.hasRecentPhysicsStep()) {
                    const auto& telemetry =
                        runtime.latestPhysicsStep().telemetry;
                    std::cerr << " physics_status="
                              << static_cast<int>(telemetry.stepStatus)
                              << " physics_code="
                              << static_cast<int>(telemetry.failureCode)
                              << " physics_detail=" << telemetry.failureDetail
                              << " contacts=" << telemetry.contacts.size()
                              << " private="
                              << telemetry.tableDepartureEvents.size();
                }
                const TableDepartureEarliestBatch batch =
                    queryEarliestTableDepartureBatch(runtime.state(),
                        profile.solver.timeStepSeconds, profile,
                        PhysicsBoundaryMode::ProductionTable);
                std::cerr << " query_ok=" << batch.ok << " query_code="
                          << static_cast<int>(batch.failureCode)
                          << " query_pair=" << batch.failingFirstBall << ':'
                          << batch.failingSecondBall << '\n';
            }
            require(result.ok, "maximum break physical tick succeeds");
            if (runtime.hasRecentPhysicsStep()) {
                for (const auto& contact :
                        runtime.latestPhysicsStep().telemetry.contacts) {
                    if (receipt.contactTick < 0 &&
                        contact.kind == PhysicsContactKind::Rail &&
                        contact.firstBall == 0 && contact.normal.y > 0.0f &&
                        contact.normal.z > 0.0f &&
                        contact.normalImpulseNs > 0.0) {
                        receipt.contactTick = runtime.tick();
                        receipt.contact = contact;
                    }
                }
            }
            ++steps;
        }
        require(receipt.contactTick >= 0,
            "maximum break records a real rounded end-rail impulse");
        require(receipt.contact.normalRelativeSpeedBeforeCmS < 0.0 &&
                receipt.contact.normalRelativeSpeedAfterCmS >= 0.0 &&
                receipt.contact.kineticEnergyAfterJ <=
                    receipt.contact.kineticEnergyBeforeJ + 1e-12,
            "rounded end-rail impulse must be approaching and passive");
        return receipt;
    };
    const RunReceipt first = run();
    const RunReceipt repeat = run();
    require(first.contactTick == repeat.contactTick &&
            first.contact.solverEventId == repeat.contact.solverEventId &&
            first.contact.solverIslandId == repeat.contact.solverIslandId &&
            first.contact.boundaryFeatureId ==
                repeat.contact.boundaryFeatureId &&
            first.contact.normal.x == repeat.contact.normal.x &&
            first.contact.normal.y == repeat.contact.normal.y &&
            first.contact.normal.z == repeat.contact.normal.z &&
            first.contact.normalImpulseNs == repeat.contact.normalImpulseNs &&
            first.contact.normalRelativeSpeedBeforeCmS ==
                repeat.contact.normalRelativeSpeedBeforeCmS &&
            first.contact.normalRelativeSpeedAfterCmS ==
                repeat.contact.normalRelativeSpeedAfterCmS,
        "maximum break rounded edge receipt is exactly deterministic");
    require(first.contact.normal.y > 0.0f && first.contact.normal.z > 0.0f,
        "maximum break records a real rounded end-rail impulse");
    std::cout << "maximum_break_contact_tick=" << first.contactTick
              << " rounded_rail=1\n";
}

void runObjectOffTableObservation(int objectBall)
{
    GameRuntime runtime;
    require(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "Chinese rules mode selected");
    const PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);
    GameState scenario = runtime.state();
    const int kObject = objectBall;
    BallState& object = scenario.balls[kObject];
    object.position = {geometry.playfieldHalfWidthCm - 8.0f,
        geometry.clothSupportedCenterYCm + 12.0f, 0.0f};
    object.velocity = {};
    object.speed = 0.0f;
    object.motionState = BallMotionState::Stationary;
    object.physicalDomain = PhysicalBallDomain::Airborne;
    require(runtime.replaceStateForScenario(scenario, profile).ok,
        "stationary physical preset installed");
    require(runtime.applyCueImpact(fixtureCueImpact(profile)).ok,
        "public cue impact begins authoritative observation");
    object.velocity = {420.0f, 180.0f, 0.0f};
    object.speed = std::sqrt(object.velocity.x * object.velocity.x +
        object.velocity.y * object.velocity.y);
    object.motionState = BallMotionState::Sliding;
    require(runtime.setBall(kObject, object).ok,
        "public scenario seam starts the airborne object after shot authority");

    int steps = 0;
    while (runtime.state().ballsMoving && steps < 7200) {
        const ActionResult result = runtime.step(1);
        if (!result.ok) {
            std::cerr << "object_off_table_step_error=" << result.errorCode
                      << " detail=" << runtime.tableDepartureObservationError()
                      << " tick=" << runtime.tick();
            if (runtime.hasRecentPhysicsStep()) {
                const auto& telemetry = runtime.latestPhysicsStep().telemetry;
                std::cerr << " physics=" << telemetry.failureDetail
                          << " private="
                          << telemetry.tableDepartureEvents.size();
            }
            for (int ball = 0; ball < kBallCount; ++ball) {
                const auto& failed = runtime.state().balls[ball];
                if (failed.pocketed || failed.outOfPlay) continue;
                std::cerr << " ball=" << ball << ":domain="
                          << static_cast<int>(failed.physicalDomain)
                          << ":p=" << failed.position.x << ','
                          << failed.position.y << ',' << failed.position.z
                          << ":v=" << failed.velocity.x << ','
                          << failed.velocity.y << ',' << failed.velocity.z
                          << ":pocketPhase="
                          << static_cast<int>(failed.pocketInteraction.phase)
                          << ":pocketId=" << failed.pocketInteraction.pocketId;
            }
            std::cerr << '\n';
        }
        require(result.ok, "object off-table physical tick succeeds");
        ++steps;
    }
    if (runtime.state().ballsMoving) {
        std::cerr << "object_off_table_timeout object=" << kObject
                  << " steps=" << steps;
        for (int ball = 0; ball < kBallCount; ++ball) {
            const auto& state = runtime.state().balls[ball];
            if (!state.pocketed && !state.outOfPlay && state.speed > 0.0f)
                std::cerr << " moving=" << ball << ':' << state.speed << ':'
                          << static_cast<int>(state.physicalDomain);
        }
        std::cerr << '\n';
    }
    require(!runtime.state().ballsMoving && steps > 0,
        "object off-table shot reaches physical rest");
    require(runtime.hasTableDepartureObservation(),
        "runtime commits table-departure observation");
    const auto& observation = runtime.tableDepartureObservation();
    require(observation.observationComplete,
        "committed table-departure observation is complete");
    bool found = false;
    for (const auto& row : observation.balls) {
        if (row.ballId == kObject && row.cameToRestOffTable) found = true;
    }
    require(found, "object off-table measurement is physical and complete");
    require(runtime.hasLastShotFacts() &&
            runtime.lastShotFacts().objectBallsOffTable ==
                std::vector<int>{kObject},
        "physical observation populates object off-table shot fact");
    require(runtime.state().balls[kObject].outOfPlay &&
            !runtime.state().balls[kObject].pocketed,
        "object off-table adjudication removes without inventing capture");
    require(runtime.hasLastAdjudication() &&
            runtime.lastAdjudication().foul,
        "object off-table physical fact reaches rules foul");
    if (kObject == 8) {
        require(runtime.lastAdjudication().gameOver &&
                runtime.lastAdjudication().winner == 1 &&
                runtime.matchState().phase == rules::MatchPhase::Complete &&
                runtime.state().gameOver,
            "opening-break 8-ball off table is an immediate terminal loss");
        require(!runtime.groupChoiceDecisionOffer() &&
                !runtime.breakFoulDecisionPending() &&
                !runtime.eightOnBreakDecisionPending() &&
                !runtime.ballInHandPlacementPending(),
            "terminal 8-ball departure creates no post-shot decision authority");
        const ActionResult blocked = runtime.applyCueImpact(
            fixtureCueImpact(profile));
        require(!blocked.ok &&
                std::string(blocked.errorCode) == "game_over",
            "terminal 8-ball departure cannot be resumed as a pending break foul");
    }
    std::cout << "object=" << kObject << " object_ticks=" << steps
              << " observation=" << observation.digest << '\n';
}

void runCueOffTableObservation()
{
    GameRuntime runtime;
    require(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "Chinese rules mode selected for cue test");
    const PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
    require(runtime.replaceStateForScenario(runtime.state(), profile).ok,
        "shipping profile selected for cue test");
    require(runtime.applyCueImpact(fixtureCueImpact(profile)).ok,
        "public cue impact begins cue observation");
    BallState cue = runtime.state().balls[0];
    cue.velocity = {420.0f, 180.0f, 0.0f};
    cue.speed = std::sqrt(cue.velocity.y * cue.velocity.y +
        cue.velocity.z * cue.velocity.z);
    cue.motionState = BallMotionState::Sliding;
    cue.physicalDomain = PhysicalBallDomain::Airborne;
    require(runtime.setBall(0, cue).ok,
        "public scenario seam supplies airborne cue launch");

    int steps = 0;
    while (runtime.state().ballsMoving && steps < 7200) {
        const ActionResult result = runtime.step(1);
        if (!result.ok)
            std::cerr << "cue_off_table_step_error=" << result.errorCode
                      << " detail="
                      << runtime.tableDepartureObservationError()
                      << " tick=" << runtime.tick() << '\n';
        require(result.ok, "cue off-table physical tick succeeds");
        ++steps;
    }
    if (runtime.state().ballsMoving ||
        !runtime.hasTableDepartureObservation() ||
        !runtime.tableDepartureObservation().observationComplete) {
        std::cerr << "cue_off_table_incomplete steps=" << steps
                  << " moving=" << runtime.state().ballsMoving
                  << " hasObservation="
                  << runtime.hasTableDepartureObservation()
                  << " observationError="
                  << runtime.tableDepartureObservationError()
                  << " hasFacts=" << runtime.hasLastShotFacts()
                  << " hasAdjudication=" << runtime.hasLastAdjudication()
                  << '\n';
        for (int ball = 0; ball < kBallCount; ++ball) {
            const auto& pending = runtime.state().balls[ball];
            if (pending.pocketed || pending.outOfPlay ||
                pending.speed == 0.0f)
                continue;
            std::cerr << " cue_pending_ball=" << ball << ":speed="
                      << pending.speed << ":domain="
                      << static_cast<int>(pending.physicalDomain)
                      << ":p=" << pending.position.x << ','
                      << pending.position.y << ',' << pending.position.z
                      << ":v=" << pending.velocity.x << ','
                      << pending.velocity.y << ',' << pending.velocity.z
                      << '\n';
        }
    }
    require(!runtime.state().ballsMoving &&
            runtime.hasTableDepartureObservation() &&
            runtime.tableDepartureObservation().observationComplete,
        "cue off-table observation completes");
    bool found = false;
    for (const auto& row : runtime.tableDepartureObservation().balls)
        if (row.ballId == 0 && row.cameToRestOffTable) found = true;
    require(found && runtime.hasLastShotFacts() &&
            runtime.lastShotFacts().cueBallOffTable &&
            runtime.state().balls[0].outOfPlay &&
            !runtime.state().balls[0].pocketed,
        "physical cue departure reaches facts and removal without capture");
    require(runtime.hasLastAdjudication() &&
            runtime.lastAdjudication().foul,
        "cue off-table physical fact reaches rules foul");
    std::cout << "cue_ticks=" << steps << " observation="
              << runtime.tableDepartureObservation().digest << '\n';
}

}  // namespace

int main()
{
    try {
        runMaximumBreakRoundedRailRegression();
        runObjectOffTableObservation(3);
        runObjectOffTableObservation(8);
        runCueOffTableObservation();
        std::cout << "game runtime table-departure observation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
