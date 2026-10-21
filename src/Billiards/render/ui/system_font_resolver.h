#pragma once

#include <string>

namespace billiardgl::render::ui {

struct SystemFontResolveResult {
    bool ok = false;
    std::string path;
    long faceIndex = 0;
    std::string errorCode;
};

// Resolves an installed macOS font path only. It never copies or modifies the
// font and returns unsupported on other platforms.
SystemFontResolveResult resolveMacSystemHudFont(
    const std::string& familyHint = "system-ui");

}  // namespace billiardgl::render::ui
