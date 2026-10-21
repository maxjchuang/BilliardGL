#include "contact_solver.h"
#include "continuous_collision.h"
#include "game_state.h"
#include "physics.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

struct BallSample {
    billiardgl::Point3 position;
    billiardgl::Point3 velocity;
    billiardgl::Point3 angularVelocity;
};

// Exact committed state immediately before the failed attempt at tick 339 of
// the native power-65 product break.  The product runtime rolled that attempt
// back atomically, so this is a stable, authoritative reproducer input.
const std::array<BallSample, billiardgl::kBallCount> kPower65Tick338{{
    {{-1.0225250662188046e-05f, 89.341476440429688f, 57.430755615234375f}, {-0.00013214307546149939f, 0.0f, -7.4926314353942871f}, {1.2488594055175781f, 0.0f, 2.6223937311442569e-05f}},
    {{-2.8749265670776367f, 89.341476440429688f, 78.388587951660156f}, {-0.4279215931892395f, 0.0f, 1.0022610425949097f}, {0.35074752569198608f, 0.0f, 0.14975383877754211f}},
    {{5.7204551696777344f, 89.341476440429688f, 83.359855651855469f}, {0.13665780425071716f, 0.0f, 1.6297276020050049f}, {0.57033336162567139f, 0.0f, -0.047824252396821976f}},
    {{-5.8356518745422363f, 89.341476440429688f, 73.500335693359375f}, {-3.2567250728607178f, 0.0f, 2.7312328815460205f}, {0.95581197738647461f, 0.169843390583992f, 1.1397112607955933f}},
    {{5.8355093002319336f, 89.341476440429688f, 73.500389099121094f}, {3.2528848648071289f, 0.0f, 2.7326881885528564f}, {0.95632129907608032f, -0.16061367094516754f, -1.1383674144744873f}},
    {{-11.51111888885498f, 89.341476440429688f, 83.425247192382812f}, {-2.1950139999389648f, 0.0f, 3.4616231918334961f}, {1.2114166021347046f, 0.0f, 0.76815885305404663f}},
    {{8.6705379486083984f, 89.341476440429688f, 78.46453857421875f}, {2.6671843528747559f, 0.0f, 3.1515998840332031f}, {1.1029220819473267f, 0.0f, -0.93339782953262329f}},
    {{-0.00011161389556946233f, 89.341476440429688f, 63.159706115722656f}, {-0.0032749378588050604f, 0.0f, -9.9318504333496094f}, {-3.4757130146026611f, 0.0f, 0.0011460849782451987f}},
    {{3.6631988677982008e-06f, 89.341476440429688f, 73.446640014648438f}, {9.1426394646987319e-05f, 0.0f, 1.1969603300094604f}, {0.41888374090194702f, 0.0f, -3.1995237804949284e-05f}},
    {{2.8748838901519775f, 89.341476440429688f, 78.388694763183594f}, {0.42701569199562073f, 0.0f, 1.0055524110794067f}, {0.35189935564994812f, 0.0f, -0.14943680167198181f}},
    {{11.511116027832031f, 89.341476440429688f, 83.425239562988281f}, {2.1952273845672607f, 0.0f, 3.4619417190551758f}, {1.2115281820297241f, 0.0f, -0.7682335376739502f}},
    {{3.6941648431820795e-05f, 89.341476440429688f, 83.328544616699219f}, {0.00084114825585857034f, 0.0f, 0.71069419384002686f}, {0.24871186912059784f, 0.0f, -0.0002943650761153549f}},
    {{-8.6705789566040039f, 89.341476440429688f, 78.464714050292969f}, {-2.6685576438903809f, 0.0f, 3.1577413082122803f}, {1.1050713062286377f, 0.0f, 0.93387842178344727f}},
    {{2.9671101570129395f, 89.341476440429688f, 68.547904968261719f}, {2.9763975143432617f, 0.0f, 2.684497594833374f}, {0.93945670127868652f, 0.23033638298511505f, -1.0416089296340942f}},
    {{-2.9666368961334229f, 89.341476440429688f, 68.548049926757812f}, {-2.9622859954833984f, 0.0f, 2.6867654323577881f}, {0.94025033712387085f, -0.21838606894016266f, 1.036670446395874f}},
    {{-5.7206039428710938f, 89.341476440429688f, 83.359954833984375f}, {-0.14062404632568359f, 0.0f, 1.6320129632949829f}, {0.57113313674926758f, 0.0f, 0.049212262034416199f}},
}};

}  // namespace

int main()
{
    auto profile = billiardgl::interactiveChinesePoolPhysicsProfile();
    {
        billiardgl::GameState projection;
        billiardgl::initializeBalls(projection);
        for (auto& ball : projection.balls) ball.pocketed = true;
        auto& first = projection.balls[0];
        auto& second = projection.balls[1];
        first.pocketed = second.pocketed = false;
        first.position = {0.0f, 89.341476440429688f, 57.430755615234375f};
        const float minimumStored = static_cast<float>(first.position.z +
            2.0 * profile.ball.radiusCm - profile.solver.penetrationSlopCm);
        second.position = {0.0f, first.position.y, std::nextafter(minimumStored,
            -std::numeric_limits<float>::infinity())};
        first.velocity.z = -1.0f;
        second.velocity.z = 1.0f;
        auto candidate = billiardgl::sweptBallBallCandidate(first, 0, second, 1,
            0.0, 2.0 * profile.ball.radiusCm,
            profile.solver.toiToleranceSeconds, true,
            profile.solver.residualToleranceCmS);
        billiardgl::ContactIsland island;
        island.ballIndices = {0, 1};
        island.contacts = {candidate};
        const auto beforeFirst = first.position;
        const auto beforeSecond = second.position;
        const auto corrected = billiardgl::solveContactIsland(
            projection, island, profile);
        const auto separation = projection.balls[1].position.z -
            projection.balls[0].position.z;
        if (corrected.status != billiardgl::ContactSolverStatus::Converged ||
            corrected.positionProjections.empty() ||
            !(separation >= 2.0 * profile.ball.radiusCm -
                profile.solver.penetrationSlopCm) ||
            (projection.balls[0].position.z == beforeFirst.z &&
             projection.balls[1].position.z == beforeSecond.z)) {
            std::cerr << "sub-ULP over-slop projection made no representable progress\n";
            return EXIT_FAILURE;
        }

        projection.balls[0].position = beforeFirst;
        projection.balls[1].position = beforeSecond;
        projection.balls[1].position.z = minimumStored;
        candidate = billiardgl::sweptBallBallCandidate(projection.balls[0], 0,
            projection.balls[1], 1, 0.0, 2.0 * profile.ball.radiusCm,
            profile.solver.toiToleranceSeconds, true,
            profile.solver.residualToleranceCmS);
        island.contacts = {candidate};
        const auto withinSlopFirst = projection.balls[0].position;
        const auto withinSlopSecond = projection.balls[1].position;
        const auto withinSlop = billiardgl::solveContactIsland(
            projection, island, profile);
        if (withinSlop.status != billiardgl::ContactSolverStatus::Converged ||
            !withinSlop.positionProjections.empty() ||
            projection.balls[0].position.z != withinSlopFirst.z ||
            projection.balls[1].position.z != withinSlopSecond.z) {
            std::cerr << "within-slop separating contact must not be projected\n";
            return EXIT_FAILURE;
        }
    }
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        auto& ball = state.balls[index];
        const auto& sample = kPower65Tick338[index];
        ball.pocketed = false;
        ball.outOfPlay = false;
        ball.physicalDomain = billiardgl::PhysicalBallDomain::ClothSupported;
        ball.position = sample.position;
        ball.velocity = sample.velocity;
        ball.angularVelocity = sample.angularVelocity;
        ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x +
            ball.velocity.y * ball.velocity.y + ball.velocity.z * ball.velocity.z);
    }
    state.ballsMoving = true;
    state.players.shotTaken = true;
    const auto before = state;
    const auto telemetry = billiardgl::updatePhysics(state,
        profile.solver.timeStepSeconds, profile,
        billiardgl::PhysicsBoundaryMode::ProductionTable);
    if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed) {
        std::cerr << billiardgl::physicsFailureCodeName(telemetry.failureCode)
                  << ':' << telemetry.failureDetail << '\n';
        if (telemetry.failureCode == billiardgl::PhysicsFailureCode::EventBudget &&
            (state.balls[0].position.x != before.balls[0].position.x ||
             state.balls[0].position.z != before.balls[0].position.z)) {
            std::cerr << "event-budget failure did not roll back atomically\n";
        }
        return EXIT_FAILURE;
    }
    if (!(state.balls[0].position.z < before.balls[0].position.z) ||
        telemetry.contacts.empty() || telemetry.solverEvents.empty()) {
        std::cerr << "power-65 continuation did not publish the expected finite contact step\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
