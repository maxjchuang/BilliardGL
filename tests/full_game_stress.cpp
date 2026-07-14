#include "full_game_case_registry.h"
#include "full_game_cli.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

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
    const fullgame::FullGameCaseResult result = selected->run(
        runtime, command.seed, fullgame::FullGameRunOptions{});
    if (!command.writeDirectory.empty()) {
        std::filesystem::create_directories(command.writeDirectory);
        std::ofstream output(command.writeDirectory / "summary.txt");
        if (!output) return EXIT_FAILURE;
        output << "case=" << command.caseId << '\n'
            << "seed=" << result.seed << '\n'
            << "passed=" << (result.passed ? "true" : "false") << '\n'
            << "hash=" << result.deterministicHash << '\n';
    }
    if (!result.passed) {
        std::cerr << result.failure << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
