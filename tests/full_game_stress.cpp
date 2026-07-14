#include "full_game_case_registry.h"
#include "full_game_cli.h"
#include "full_game_artifacts.h"
#include "full_game_invariants.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sys/resource.h>

namespace {

std::uint64_t peakRssBytes()
{
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
}

}  // namespace

int main(int argc, char** argv)
{
    fullgame::FullGameCommand command;
    try {
        command = fullgame::parseFullGameCommand(argc,
            const_cast<const char* const*>(argv));
    } catch (const std::invalid_argument& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    if (command.mode == fullgame::FullGameMode::ListCases) {
        for (const std::string& id : fullgame::fullGameCaseIds())
            std::cout << id << '\n';
        return EXIT_SUCCESS;
    }
    if (command.mode == fullgame::FullGameMode::Matrix) {
        std::cerr << "matrix execution is not implemented yet\n";
        return EXIT_FAILURE;
    }
    const fullgame::FullGameCase* selected =
        fullgame::findFullGameCase(command.caseId);
    if (selected == nullptr || selected->run == nullptr) return EXIT_FAILURE;
    billiardgl::GameRuntime runtime;
    const auto started = std::chrono::steady_clock::now();
    fullgame::FullGameCaseResult result = selected->run(
        runtime, command.seed, fullgame::FullGameRunOptions{});
    result.caseId = command.caseId;
    result.wallSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    result.peakRssBytes = peakRssBytes();
    const fullgame::FullGameInvariantResult invariants =
        fullgame::evaluateFullGameInvariants(result.frames, result.events,
            result.droppedTraceFrames, result.wallSeconds,
            result.peakRssBytes);
    result.maximumPenetrationCm = invariants.maximumPenetrationCm;
    result.maximumResidualCmS = invariants.maximumResidualCmS;
    result.duplicateContacts = invariants.duplicateContacts;
    result.stepFailures = invariants.stepFailures;
    result.deterministicHash = invariants.deterministicHash;
    if (!invariants.passed) {
        result.passed = false;
        if (result.failure.empty() && !invariants.failures.empty())
            result.failure = invariants.failures.front();
    }
    if (!command.writeDirectory.empty()) {
        try {
            fullgame::writeFullGameArtifacts(result, command.writeDirectory);
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    if (!result.passed) {
        std::cerr << result.failure << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
