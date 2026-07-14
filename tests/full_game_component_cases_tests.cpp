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
    options.ticksPerRepeat = 240;
    return selected->run(runtime, seed, options);
}

}  // namespace

int main()
{
    try {
        const char* ids[] = {"cue_center_hit", "cue_near_miscue",
            "sliding_to_rolling", "oblique_ball_collision",
            "rail_rebound", "side_pocket_capture"};
        for (const char* id : ids) {
            const fullgame::FullGameCaseResult first = run(id, 7);
            const fullgame::FullGameCaseResult second = run(id, 7);
            expect(first.passed, std::string(id) + " passes invariants: " + first.failure);
            expect(first.deterministicHash == second.deterministicHash,
                std::string(id) + " is deterministic");
            expect(!first.frames.empty(), std::string(id) + " retains frames");
        }
        expect(run("cue_center_hit", 7).cueContactApplied,
            "center cue contact is applied");
        const fullgame::FullGameCaseResult near = run("cue_near_miscue", 7);
        expect(near.cueContactApplied && !near.cueContactMiscue,
            "near-miscue remains inside the modeled envelope");
        expect(run("sliding_to_rolling", 7).surfaceTransitions >= 1,
            "sliding to rolling transition is observed");
        expect(run("oblique_ball_collision", 7).ballCollisions == 1,
            "one oblique ball collision is observed");
        expect(run("rail_rebound", 7).railCollisions >= 1,
            "straight rail rebound is observed");
        expect(run("side_pocket_capture", 7).objectBallCaptures == 1,
            "one object ball is captured by a side pocket");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
