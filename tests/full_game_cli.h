#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace fullgame {

enum class FullGameMode {
    ListCases,
    SingleCase,
    Matrix
};

struct FullGameCommand {
    FullGameMode mode = FullGameMode::ListCases;
    std::string caseId;
    std::uint32_t seed = 0;
    bool hasSeed = false;
    std::filesystem::path matrixPath;
    std::filesystem::path writeDirectory;
};

FullGameCommand parseFullGameCommand(int argc, const char* const* argv);

}  // namespace fullgame
