#pragma once

#include "game_state.h"

#include <string>

namespace billiardgl {

std::string sha256Hex(const std::string& bytes);
std::string canonicalGameState(const GameState& state);
std::string gameStateSha256(const GameState& state);

}  // namespace billiardgl
