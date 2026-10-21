#include "render/ui/system_font_resolver.h"

#include <filesystem>
#include <vector>

namespace billiardgl::render::ui {

SystemFontResolveResult resolveMacSystemHudFont(
    const std::string& familyHint)
{
#if defined(__APPLE__)
    const bool monospace = familyHint == "monospace" ||
        familyHint == "SF Mono" || familyHint == "Menlo";
    const std::vector<std::string> candidates = monospace ?
        std::vector<std::string>{"/System/Library/Fonts/SFNSMono.ttf",
            "/System/Library/Fonts/Menlo.ttc"} :
        std::vector<std::string>{"/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/HelveticaNeue.ttc",
            "/System/Library/Fonts/Hiragino Sans GB.ttc"};
    for (const std::string& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate))
            return {true, candidate, 0, ""};
    }
    return {false, "", 0, "system_font_not_found"};
#else
    (void)familyHint;
    return {false, "", 0, "system_font_resolver_unsupported"};
#endif
}

}  // namespace billiardgl::render::ui
