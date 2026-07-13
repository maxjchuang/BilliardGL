#pragma once
#include "game_state.h"
#include <string>
namespace billiardgl {
enum class RunMode { Interactive, AutomationHeadless, AutomationRendered };
struct LaunchOptions { bool ok=true; std::string error; RunMode mode=RunMode::Interactive; std::string transport="stdio"; RuntimeConfig runtime; };
LaunchOptions parseLaunchOptions(int argc, char* argv[]);
}
