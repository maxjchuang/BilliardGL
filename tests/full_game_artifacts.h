#pragma once

#include "full_game_case_registry.h"

#include <filesystem>

namespace fullgame {

void writeFullGameArtifacts(
    const FullGameCaseResult& result,
    const std::filesystem::path& directory);

}  // namespace fullgame
