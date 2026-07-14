#include "full_game_case_registry.h"

#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

fullgame::FullGameCaseResult run(const std::string& id, std::uint32_t seed)
{
    const fullgame::FullGameCase* selected = fullgame::findFullGameCase(id);
    expect(selected != nullptr && selected->run != nullptr, id + " is registered");
    billiardgl::GameRuntime runtime;
    fullgame::FullGameRunOptions options;
    options.repeats = 1;
    options.ticksPerRepeat = 360;
    return selected->run(runtime, seed, options);
}

}  // namespace

int main()
{
    try {
        const fullgame::FullGameCaseResult breakResult = run("seeded_break", 100);
        expect(breakResult.passed && breakResult.ballCollisions >= 1 &&
            breakResult.completedShots == 1,
            "seeded break reaches the rack and terminates");

        const fullgame::FullGameCaseResult scoring = run("continuous_scoring", 100);
        expect(scoring.passed && scoring.objectBallCaptures >= 3,
            "three objects are captured");
        expect(scoring.scoreEvents >= 3, "score updates follow captures");
        expect(scoring.cueBallCaptures == 0 && scoring.foulEvents == 0 &&
            scoring.turnTransfers == 0,
            "legal scoring retains the turn without a scratch");

        const fullgame::FullGameCaseResult scratch = run("cue_ball_scratch", 101);
        expect(scratch.passed && scratch.cueBallCaptures == 1,
            "cue ball enters one pocket");
        expect(scratch.foulEvents == 1 && scratch.turnTransfers == 1,
            "scratch causes one foul and one turn transfer");

        const fullgame::FullGameCaseResult random =
            run("randomized_legal_sequence", 0x12345678u);
        expect(random.passed && random.illegalActionAttempts == 0,
            "only operationally legal actions emit");
        expect(random.completedShots == random.declaredShots || random.gameOver,
            "random sequence reaches its declared terminal condition");
        const fullgame::FullGameCaseResult replay =
            run("randomized_legal_sequence", 0x12345678u);
        expect(random.deterministicHash == replay.deterministicHash,
            "randomized sequence is replay deterministic");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
