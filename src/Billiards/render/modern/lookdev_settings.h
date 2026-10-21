#pragma once

#include "render/modern/render_data.h"

#include <string>
#include <vector>

namespace billiardgl::render {

struct LookdevSettings {
    Float4 feltColor{{0.0044f, 0.0835f, 0.1128f, 1.0f}};
    Float4 woodColor{{0.0134f, 0.0044f, 0.0022f, 1.0f}};
    Float4 ballBaseColor{{0.82f, 0.78f, 0.66f, 1.0f}};
    Float3 lampEmissive{{1.0f, 0.79f, 0.59f}};
    float ballRoughness = 0.12f;
    float ballClearcoat = 0.9f;
    float ballClearcoatRoughness = 0.06f;
    float exposureEv100 = 8.1f;
    float lampWidthMetres = 1.85f;
    float lampDepthMetres = 0.65f;
    std::string manifestId = "built_in_graybox_fallback";
};

struct LookdevLoadResult {
    LookdevSettings settings;
    bool loadedManifest = false;
    std::vector<std::string> warnings;
};

LookdevLoadResult loadLookdevSettings(const std::string& path);

}  // namespace billiardgl::render
