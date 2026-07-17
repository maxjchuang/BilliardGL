#include "game_state.h"
#include "pocket_boundary.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

}  // namespace

int main()
{
    using namespace billiardgl;
    PocketInteractionState state;

    PocketTransitionResult result = advancePocketInteraction(
        state, 2, PocketBoundaryEventKind::Mouth,
        PocketBoundaryRegion::Outside, 0);
    if (!result.changed || state.phase != PocketInteractionPhase::Approaching ||
        state.pocketId != 2) {
        return fail("crossing a passable mouth must bind the pocket interaction");
    }
    state = PocketInteractionState{};

    result = advancePocketInteraction(
        state, 2, PocketBoundaryEventKind::None,
        PocketBoundaryRegion::Approaching, 0);
    if (!result.changed || state.phase != PocketInteractionPhase::Approaching ||
        state.pocketId != 2) {
        return fail("mouth approach should bind the interaction to one pocket");
    }

    result = advancePocketInteraction(state, 2, PocketBoundaryEventKind::Mouth,
        PocketBoundaryRegion::Solid, 0);
    if (!result.changed || state.phase != PocketInteractionPhase::Rejected) {
        return fail("leaving the mouth channel through solid geometry must reject the attempt");
    }
    state = PocketInteractionState{};
    advancePocketInteraction(state, 2, PocketBoundaryEventKind::None,
        PocketBoundaryRegion::Approaching, 0);

    result = advancePocketInteraction(state, 3, PocketBoundaryEventKind::Throat,
        PocketBoundaryRegion::Throat, 0);
    if (result.changed || state.pocketId != 2) {
        return fail("an active interaction must not teleport across pockets");
    }

    advancePocketInteraction(state, 2, PocketBoundaryEventKind::LeftJaw,
        PocketBoundaryRegion::Approaching, 0);
    if (state.phase != PocketInteractionPhase::JawContact) {
        return fail("jaw event should enter jaw-contact state");
    }
    advancePocketInteraction(state, 2, PocketBoundaryEventKind::None,
        PocketBoundaryRegion::Solid, 0);
    if (state.phase != PocketInteractionPhase::Rejected) {
        return fail("leaving a jaw through solid geometry should reject the attempt");
    }

    state = PocketInteractionState{};
    advancePocketInteraction(state, 1, PocketBoundaryEventKind::None,
        PocketBoundaryRegion::Approaching, 0);
    advancePocketInteraction(state, 1, PocketBoundaryEventKind::Throat,
        PocketBoundaryRegion::Throat, 0);
    result = advancePocketInteraction(state, 1, PocketBoundaryEventKind::Capture,
        PocketBoundaryRegion::Capture, 17);
    if (!result.captureEmitted || state.phase != PocketInteractionPhase::Captured ||
        state.captureSequence != 17) {
        return fail("valid throat/capture order should emit one capture sequence");
    }
    result = advancePocketInteraction(state, 1, PocketBoundaryEventKind::Capture,
        PocketBoundaryRegion::Capture, 18);
    if (result.captureEmitted || result.changed || state.captureSequence != 17) {
        return fail("capture must be irreversible and emitted exactly once");
    }

    PocketInteractionState direct;
    result = advancePocketInteraction(direct, 0, PocketBoundaryEventKind::Capture,
        PocketBoundaryRegion::Capture, 1);
    if (result.changed || result.captureEmitted) {
        return fail("outside state cannot jump directly to capture");
    }

    PocketInteractionState rejected;
    advancePocketInteraction(rejected, 4, PocketBoundaryEventKind::Throat,
        PocketBoundaryRegion::Throat, 0);
    advancePocketInteraction(rejected, 4, PocketBoundaryEventKind::None,
        PocketBoundaryRegion::Approaching, 0);
    if (rejected.phase != PocketInteractionPhase::Rejected) {
        return fail("a ball retreating after throat crossing should be rejected");
    }

    BallState ball;
    ball.pocketInteraction = state;
    resetPocketInteraction(ball);
    if (ball.pocketInteraction.phase != PocketInteractionPhase::Outside ||
        ball.pocketInteraction.pocketId != -1 ||
        ball.pocketInteraction.captureSequence != 0) {
        return fail("reset should clear all pocket interaction identity");
    }

    GameState game;
    game.balls[0].pocketInteraction = state;
    initializeBalls(game);
    if (game.balls[0].pocketInteraction.phase != PocketInteractionPhase::Outside) {
        return fail("game initialization should reset pocket interaction state");
    }

    return 0;
}
