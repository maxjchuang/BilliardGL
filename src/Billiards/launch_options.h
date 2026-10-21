#pragma once
#include "game_state.h"
#include <string>
namespace billiardgl {
enum class RunMode { Interactive, AutomationHeadless, AutomationRendered };
// Compatibility is retained only for historical fixtures/entry points.
// The modern-only production entry must select Product explicitly.
enum class LaunchPolicy { Compatibility, Product };
struct LaunchOptions {
    bool ok = true;
    bool printPhysicsProfile = false;
    // Compatibility fixtures retain opt-in lifecycle behavior. Product policy
    // resolves ordinary interaction to the shell without changing RuntimeConfig.
    bool productMode = false;
    bool aiOpponent = false;
    std::string aiDifficulty = "club";
    std::string productSettingsPath;
    std::string productFramePerformancePath;
    std::string error;
    RunMode mode = RunMode::Interactive;
    std::string transport = "stdio";
    RuntimeConfig runtime;
};
LaunchOptions parseLaunchOptions(int argc, char* argv[],
    LaunchPolicy policy = LaunchPolicy::Compatibility);
}
