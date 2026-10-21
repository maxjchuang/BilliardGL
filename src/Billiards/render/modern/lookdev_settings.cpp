#include "render/modern/lookdev_settings.h"

#include "automation_json.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace billiardgl::render {
namespace {

Float4 color4(const json::Value& value, const std::string& field)
{
    const auto& values = value.asArray();
    if (values.size() != 4) throw std::runtime_error(field + " must contain four channels");
    Float4 result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<float>(values[index].asNumber());
    }
    return result;
}

Float3 color3(const json::Value& value, const std::string& field)
{
    const auto& values = value.asArray();
    if (values.size() != 3) throw std::runtime_error(field + " must contain three channels");
    Float3 result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<float>(values[index].asNumber());
    }
    return result;
}

float target(const json::Value& value) { return static_cast<float>(value.at("target").asNumber()); }

}  // namespace

LookdevLoadResult loadLookdevSettings(const std::string& path)
{
    LookdevLoadResult result;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        result.warnings.push_back("lookdev manifest unavailable; using explicit graybox fallback: " + path);
        return result;
    }
    std::ostringstream text;
    text << input.rdbuf();
    const json::ParseResult parsed = json::parse(text.str());
    if (!parsed.ok) throw std::runtime_error("lookdev manifest parse failed: " + path + ": " + parsed.error);
    try {
        const auto& root = parsed.value;
        const auto& materials = root.at("materials");
        const auto& ball = materials.at("ball_resin");
        const auto& lamp = materials.at("lamp_diffuser");
        result.settings.manifestId = root.at("manifest_id").asString();
        result.settings.feltColor = color4(materials.at("table_felt").at("base_color_factor_linear_srgb"), "table_felt.base_color");
        result.settings.woodColor = color4(materials.at("table_wood").at("base_color_factor_linear_srgb"), "table_wood.base_color");
        result.settings.ballBaseColor = color4(ball.at("base_color_factor_linear_srgb"), "ball_resin.base_color");
        result.settings.ballRoughness = target(ball.at("roughness"));
        result.settings.ballClearcoat = target(ball.at("clearcoat").at("factor"));
        result.settings.ballClearcoatRoughness = target(ball.at("clearcoat").at("roughness"));
        result.settings.lampEmissive = color3(lamp.at("emissive_color_linear_srgb"), "lamp_diffuser.emissive");
        result.settings.exposureEv100 = static_cast<float>(root.at("exposure").at("reference_ev100").asNumber());
        const auto& lampSize = root.at("lighting").at("key_light").at("size_m").asArray();
        if (lampSize.size() != 2) throw std::runtime_error("lighting.key_light.size_m must contain two values");
        result.settings.lampWidthMetres = static_cast<float>(lampSize[0].asNumber());
        result.settings.lampDepthMetres = static_cast<float>(lampSize[1].asNumber());
        result.loadedManifest = true;
    } catch (const std::exception& error) {
        throw std::runtime_error("lookdev manifest required field error: " + path + ": " + error.what());
    }
    return result;
}

}  // namespace billiardgl::render
