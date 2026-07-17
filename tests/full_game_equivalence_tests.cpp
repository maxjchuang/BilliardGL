#include "full_game_case_registry.h"
#include "full_game_test_profile.h"

#include <iostream>
#include <stdexcept>

namespace {

fullgame::FullGameCaseResult run(const std::string& id, std::uint32_t seed)
{
    const fullgame::FullGameCase* selected = fullgame::findFullGameCase(id);
    if (selected == nullptr || selected->run == nullptr)
        throw std::runtime_error(id + " is registered");
    billiardgl::GameRuntime runtime = fullgame::phase3V5CandidateRuntime();
    fullgame::FullGameRunOptions options;
    options.repeats = 1;
    options.ticksPerRepeat = 180;
    return selected->run(runtime, seed, options);
}

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main()
{
    try {
        const fullgame::FullGameCaseResult cadence =
            run("cadence_equivalence", 77);
        expect(cadence.passed && cadence.stateMismatches == 0 &&
            cadence.eventMismatches == 0,
            "render cadence changes wall time only");
        const fullgame::FullGameCaseResult load =
            run("host_load_equivalence", 77);
        expect(load.passed && load.stateMismatches == 0 &&
            load.eventMismatches == 0,
            "host load changes wall time only");
        expect(cadence.deterministicHash ==
            run("cadence_equivalence", 77).deterministicHash,
            "cadence equivalence rerun is deterministic");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
