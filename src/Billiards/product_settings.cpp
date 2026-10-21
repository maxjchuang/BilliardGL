#include "product_settings.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace billiardgl::settings {
namespace {

ValidationResult success()
{
    ValidationResult result;
    result.ok = true;
    return result;
}

ValidationResult failure(const std::string& code, const std::string& message,
    const std::string& first = {}, const std::string& second = {})
{
    ValidationResult result;
    result.errorCode = code;
    result.message = message;
    result.firstAction = first;
    result.secondAction = second;
    return result;
}

std::string normalized(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(),
        [](unsigned char c) { return std::isspace(c) != 0; }), value.end());
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool contextsOverlap(const std::string& first, const std::string& second)
{
    const std::string a = normalized(first);
    const std::string b = normalized(second);
    if (a == "global" || b == "global" || a == b) return true;
    if (a == "play") return b == "aim" || b == "observe" || b == "training";
    if (b == "play") return a == "aim" || a == "observe" || a == "training";
    return false;
}

const char* colorVisionName(ColorVisionMode value)
{
    switch (value) {
    case ColorVisionMode::Default: return "default";
    case ColorVisionMode::Deuteranopia: return "deuteranopia";
    case ColorVisionMode::Protanopia: return "protanopia";
    case ColorVisionMode::Tritanopia: return "tritanopia";
    case ColorVisionMode::HighContrastPatterns: return "high_contrast_patterns";
    }
    return "invalid";
}

bool parseColorVision(const std::string& text, ColorVisionMode& value)
{
    if (text == "default") value = ColorVisionMode::Default;
    else if (text == "deuteranopia") value = ColorVisionMode::Deuteranopia;
    else if (text == "protanopia") value = ColorVisionMode::Protanopia;
    else if (text == "tritanopia") value = ColorVisionMode::Tritanopia;
    else if (text == "high_contrast_patterns") value = ColorVisionMode::HighContrastPatterns;
    else return false;
    return true;
}

const char* subtitleName(SubtitleMode value)
{
    switch (value) {
    case SubtitleMode::Off: return "off";
    case SubtitleMode::Essential: return "essential";
    case SubtitleMode::All: return "all";
    }
    return "invalid";
}

bool parseSubtitle(const std::string& text, SubtitleMode& value)
{
    if (text == "off") value = SubtitleMode::Off;
    else if (text == "essential") value = SubtitleMode::Essential;
    else if (text == "all") value = SubtitleMode::All;
    else return false;
    return true;
}

const char* assistName(AssistMode value)
{
    switch (value) {
    case AssistMode::Off: return "off";
    case AssistMode::Basic: return "basic";
    case AssistMode::Guided: return "guided";
    }
    return "invalid";
}

bool parseAssist(const std::string& text, AssistMode& value)
{
    if (text == "off") value = AssistMode::Off;
    else if (text == "basic") value = AssistMode::Basic;
    else if (text == "guided") value = AssistMode::Guided;
    else return false;
    return true;
}

InputBinding binding(const char* action, const char* context,
    const char* device, const char* control, const char* trigger)
{
    return InputBinding{action, context, device, control, trigger};
}

double number(const json::Value& object, const char* key)
{
    return object.at(key).asNumber();
}

bool boolean(const json::Value& object, const char* key)
{
    return object.at(key).asBool();
}

}  // namespace

ProductSettings ProductSettings::defaults()
{
    ProductSettings value;
    value.bindings = {
        binding("toggle_help", "global", "keyboard", "H", "press"),
        binding("toggle_aim", "play", "keyboard", "Tab", "press"),
        binding("shoot", "aim", "mouse", "LeftButton", "release"),
        binding("adjust_aim", "aim", "mouse", "MoveX", "continuous"),
        binding("adjust_tip_offset", "aim", "keyboard", "ArrowKeys", "continuous"),
        binding("power_increase", "aim", "keyboard", "+", "press"),
        binding("power_increase", "aim", "mouse", "WheelUp", "step"),
        binding("power_decrease", "aim", "keyboard", "-", "press"),
        binding("power_decrease", "aim", "mouse", "WheelDown", "step"),
        binding("camera_orbit", "observe", "keyboard", "ArrowKeys", "continuous"),
        binding("camera_orbit", "observe", "mouse", "LeftDrag", "continuous"),
        binding("camera_pan", "observe", "keyboard", "A/D", "continuous"),
        binding("camera_pan", "observe", "mouse", "Shift+LeftDrag", "continuous"),
        binding("zoom_in", "observe", "keyboard", "W", "continuous"),
        binding("zoom_in", "observe", "mouse", "WheelUp", "step"),
        binding("zoom_out", "observe", "keyboard", "S", "continuous"),
        binding("zoom_out", "observe", "mouse", "WheelDown", "step"),
        binding("toggle_camera_anchor", "play", "keyboard", "C", "press"),
        binding("return_to_cue_ball", "play", "keyboard", "Space", "press"),
        binding("quit", "global", "keyboard", "Escape", "press"),
        binding("open_settings", "global", "keyboard", "F10", "press"),
        binding("reset_training_attempt", "training", "keyboard", "R", "press"),
        binding("settings_focus_next", "settings", "keyboard", "ArrowDown", "press"),
        binding("settings_focus_previous", "settings", "keyboard", "ArrowUp", "press"),
        binding("settings_category_next", "settings", "keyboard", "ArrowRight", "press"),
        binding("settings_category_previous", "settings", "keyboard", "ArrowLeft", "press"),
        binding("settings_increase", "settings", "keyboard", "+", "press"),
        binding("settings_decrease", "settings", "keyboard", "-", "press"),
        binding("settings_toggle", "settings", "keyboard", "Space", "press"),
        binding("settings_apply", "settings", "keyboard", "Enter", "press"),
        binding("replay_toggle_pause", "replay", "keyboard", "Space", "press"),
        binding("camera_orbit", "replay", "mouse", "Shift+LeftDrag", "continuous"),
        binding("zoom_in", "replay", "mouse", "WheelUp", "step"),
        binding("zoom_out", "replay", "mouse", "WheelDown", "step"),
        binding("replay_step", "replay", "keyboard", ".", "press"),
        binding("replay_rate_increase", "replay", "keyboard", "+", "press"),
        binding("replay_rate_decrease", "replay", "keyboard", "-", "press"),
        binding("replay_camera_toggle", "replay", "keyboard", "C", "press"),
        binding("replay_seek_previous", "replay", "keyboard", "PageUp", "press"),
        binding("replay_seek_next", "replay", "keyboard", "PageDown", "press")};
    return value;
}

ValidationResult validate(const ProductSettings& value)
{
    if (value.version != kProductSettingsVersion)
        return failure("E_SETTINGS_VERSION", "unsupported product settings version");
    const auto finiteRange = [](double candidate, double minimum, double maximum) {
        return std::isfinite(candidate) && candidate >= minimum && candidate <= maximum;
    };
    if (!finiteRange(value.mouseSensitivity, 0.1, 4.0))
        return failure("E_MOUSE_SENSITIVITY", "mouse sensitivity must be in [0.1, 4.0]");
    if (!finiteRange(value.fieldOfViewDegrees, 35.0, 90.0))
        return failure("E_CAMERA_FOV", "field of view must be in [35, 90]");
    if (!finiteRange(value.cameraDistanceCm, 10.0, 500.0))
        return failure("E_CAMERA_DISTANCE", "camera distance must be in [10, 500] cm");
    if (!finiteRange(value.uiScale, 0.85, 1.35))
        return failure("E_UI_SCALE", "UI scale must be in [0.85, 1.35]");
    if (!finiteRange(value.musicVolume, 0.0, 1.0) ||
        !finiteRange(value.ambienceVolume, 0.0, 1.0) ||
        !finiteRange(value.effectsVolume, 0.0, 1.0) ||
        !finiteRange(value.uiVolume, 0.0, 1.0))
        return failure("E_AUDIO_VOLUME", "audio volumes must be in [0, 1]");

    std::set<std::string> actions;
    for (const InputBinding& item : value.bindings) {
        if (item.action.empty() || item.context.empty() || item.device.empty() ||
            item.control.empty() || item.trigger.empty())
            return failure("E_BINDING_FORMAT", "binding fields must not be empty", item.action);
        actions.insert(item.action);
    }
    for (std::size_t i = 0; i < value.bindings.size(); ++i) {
        const InputBinding& first = value.bindings[i];
        for (std::size_t j = i + 1; j < value.bindings.size(); ++j) {
            const InputBinding& second = value.bindings[j];
            if (first.action == second.action) continue;
            if (normalized(first.device) == normalized(second.device) &&
                normalized(first.control) == normalized(second.control) &&
                normalized(first.trigger) == normalized(second.trigger) &&
                contextsOverlap(first.context, second.context)) {
                return failure("E_BINDING_CONFLICT",
                    "normalized control conflicts in overlapping contexts",
                    first.action, second.action);
            }
        }
    }
    for (const char* required : {"toggle_help", "toggle_aim", "shoot", "adjust_aim",
            "adjust_tip_offset",
            "power_increase", "power_decrease", "camera_orbit", "camera_pan",
            "zoom_in", "zoom_out", "toggle_camera_anchor", "return_to_cue_ball",
            "quit", "open_settings", "reset_training_attempt",
            "settings_focus_next", "settings_focus_previous",
            "settings_category_next", "settings_category_previous",
            "settings_increase", "settings_decrease", "settings_toggle",
            "settings_apply", "replay_toggle_pause", "replay_step",
            "replay_rate_increase", "replay_rate_decrease",
            "replay_camera_toggle", "replay_seek_previous",
            "replay_seek_next"}) {
        if (actions.count(required) == 0)
            return failure("E_REQUIRED_BINDING", std::string("missing action ") + required);
    }
    return success();
}

json::Value serialize(const ProductSettings& value)
{
    json::Value root = json::Value::object();
    root["format"] = json::Value("billiardgl_product_settings_v1");
    root["version"] = json::Value(value.version);
    json::Value gameplay = json::Value::object();
    gameplay["assist"] = json::Value(assistName(value.assist));
    root["gameplay"] = gameplay;
    json::Value input = json::Value::object();
    input["mouse_sensitivity"] = json::Value(value.mouseSensitivity);
    input["invert_mouse_y"] = json::Value(value.invertMouseY);
    json::Value bindings = json::Value::array();
    for (const InputBinding& item : value.bindings) {
        json::Value entry = json::Value::object();
        entry["action"] = json::Value(item.action);
        entry["context"] = json::Value(item.context);
        entry["device"] = json::Value(item.device);
        entry["control"] = json::Value(item.control);
        entry["trigger"] = json::Value(item.trigger);
        bindings.asArray().push_back(entry);
    }
    input["bindings"] = bindings;
    root["input"] = input;
    json::Value accessibility = json::Value::object();
    accessibility["ui_scale"] = json::Value(value.uiScale);
    accessibility["color_vision_mode"] = json::Value(colorVisionName(value.colorVisionMode));
    accessibility["subtitles"] = json::Value(subtitleName(value.subtitles));
    root["accessibility"] = accessibility;
    json::Value audio = json::Value::object();
    audio["music"] = json::Value(value.musicVolume);
    audio["ambience"] = json::Value(value.ambienceVolume);
    audio["effects"] = json::Value(value.effectsVolume);
    audio["ui"] = json::Value(value.uiVolume);
    audio["music_muted"] = json::Value(value.musicMuted);
    audio["ambience_muted"] = json::Value(value.ambienceMuted);
    audio["effects_muted"] = json::Value(value.effectsMuted);
    audio["ui_muted"] = json::Value(value.uiMuted);
    root["audio"] = audio;
    json::Value display = json::Value::object();
    display["field_of_view_degrees"] = json::Value(value.fieldOfViewDegrees);
    display["camera_distance_cm"] = json::Value(value.cameraDistanceCm);
    display["bloom"] = json::Value(value.bloom);
    display["depth_of_field"] = json::Value(value.depthOfField);
    display["dynamic_camera"] = json::Value(value.dynamicCamera);
    display["camera_shake"] = json::Value(value.cameraShake);
    root["display"] = display;
    return root;
}

ValidationResult parse(const json::Value& value, ProductSettings& output)
{
    try {
        if (!value.isObject() || value.at("format").asString() !=
                "billiardgl_product_settings_v1")
            return failure("E_SETTINGS_FORMAT", "invalid product settings format");
        ProductSettings parsed;
        parsed.version = value.at("version").asInt();
        const json::Value& gameplay = value.at("gameplay");
        if (!parseAssist(gameplay.at("assist").asString(), parsed.assist))
            return failure("E_ASSIST_MODE", "unknown assist mode");
        const json::Value& input = value.at("input");
        parsed.mouseSensitivity = number(input, "mouse_sensitivity");
        parsed.invertMouseY = boolean(input, "invert_mouse_y");
        parsed.bindings.clear();
        for (const json::Value& entry : input.at("bindings").asArray()) {
            parsed.bindings.push_back(InputBinding{
                entry.at("action").asString(), entry.at("context").asString(),
                entry.at("device").asString(), entry.at("control").asString(),
                entry.at("trigger").asString()});
        }
        // Early v1 files predate the HUD-promised arrow-key tip control.
        // Preserve every saved binding and add only this missing default.
        if (std::none_of(parsed.bindings.begin(), parsed.bindings.end(),
                [](const InputBinding& item) {
                    return item.action == "adjust_tip_offset";
                })) {
            parsed.bindings.push_back(binding("adjust_tip_offset", "aim",
                "keyboard", "ArrowKeys", "continuous"));
        }
        // Upgrade old saved settings without overwriting a user's replay controls.
        for (const auto& added : ProductSettings::defaults().bindings) {
            if (added.context != "replay" || added.device != "mouse") continue;
            const bool occupied = std::any_of(parsed.bindings.begin(), parsed.bindings.end(),
                [&](const InputBinding& existing) {
                    return existing.context == "replay" && existing.device == added.device &&
                        existing.control == added.control && existing.trigger == added.trigger;
                });
            if (!occupied) parsed.bindings.push_back(added);
        }
        const json::Value& accessibility = value.at("accessibility");
        parsed.uiScale = number(accessibility, "ui_scale");
        if (!parseColorVision(accessibility.at("color_vision_mode").asString(),
                parsed.colorVisionMode))
            return failure("E_COLOR_VISION_MODE", "unknown color vision mode");
        if (!parseSubtitle(accessibility.at("subtitles").asString(), parsed.subtitles))
            return failure("E_SUBTITLE_MODE", "unknown subtitle mode");
        const json::Value& audio = value.at("audio");
        parsed.musicVolume = number(audio, "music");
        parsed.ambienceVolume = number(audio, "ambience");
        parsed.effectsVolume = number(audio, "effects");
        // v1 files written before the shipping audio foundation remain valid.
        // Missing independent UI/mute controls inherit ProductSettings defaults.
        if (audio.has("ui")) parsed.uiVolume = number(audio, "ui");
        if (audio.has("music_muted"))
            parsed.musicMuted = boolean(audio, "music_muted");
        if (audio.has("ambience_muted"))
            parsed.ambienceMuted = boolean(audio, "ambience_muted");
        if (audio.has("effects_muted"))
            parsed.effectsMuted = boolean(audio, "effects_muted");
        if (audio.has("ui_muted"))
            parsed.uiMuted = boolean(audio, "ui_muted");
        const json::Value& display = value.at("display");
        parsed.fieldOfViewDegrees = number(display, "field_of_view_degrees");
        parsed.cameraDistanceCm = number(display, "camera_distance_cm");
        parsed.bloom = boolean(display, "bloom");
        parsed.depthOfField = boolean(display, "depth_of_field");
        parsed.dynamicCamera = boolean(display, "dynamic_camera");
        parsed.cameraShake = boolean(display, "camera_shake");
        const ValidationResult checked = validate(parsed);
        if (!checked.ok) return checked;
        output = parsed;
        return success();
    } catch (const std::exception& error) {
        return failure("E_SETTINGS_FORMAT", error.what());
    }
}

ValidationResult loadFile(const std::string& path, ProductSettings& output)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return failure("E_SETTINGS_READ", "cannot open settings file: " + path);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const json::ParseResult decoded = json::parse(bytes);
    if (!decoded.ok) return failure("E_SETTINGS_FORMAT", decoded.error);
    return parse(decoded.value, output);
}

ValidationResult saveFileAtomic(const std::string& path,
    const ProductSettings& value)
{
    const ValidationResult checked = validate(value);
    if (!checked.ok) return checked;
    const std::filesystem::path destination(path);
    const std::filesystem::path temporary = destination.string() + ".tmp";
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return failure("E_SETTINGS_WRITE",
                "cannot open temporary settings file: " + temporary.string());
            output << json::stringify(serialize(value)) << '\n';
            output.flush();
            if (!output) {
                std::filesystem::remove(temporary);
                return failure("E_SETTINGS_WRITE", "failed to flush temporary settings file");
            }
        }
        std::filesystem::rename(temporary, destination);
        return success();
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return failure("E_SETTINGS_WRITE", error.what());
    }
}

bool requiresTextReflow(const ProductSettings& value) noexcept
{
    return value.uiScale > 1.15;
}

bool assistCanAutoAimOrShoot(const ProductSettings&) noexcept
{
    return false;
}

BindingResolution resolveBinding(const ProductSettings& value,
    const std::vector<std::string>& activeContexts,
    const std::string& device, const std::string& control,
    const std::string& trigger)
{
    const std::string wantedDevice = normalized(device);
    const std::string wantedControl = normalized(control);
    const std::string wantedTrigger = normalized(trigger);
    const auto matchesControl = [&](const InputBinding& item) {
        return normalized(item.device) == wantedDevice &&
            normalized(item.control) == wantedControl &&
            normalized(item.trigger) == wantedTrigger;
    };
    // The caller supplies contexts from most-specific to least-specific.
    // Global is an explicit fallback so a modal screen cannot hide quit/help.
    for (const std::string& context : activeContexts) {
        for (const InputBinding& item : value.bindings) {
            if (normalized(item.context) == normalized(context) &&
                matchesControl(item))
                return {true, item.action, item.context};
        }
    }
    for (const InputBinding& item : value.bindings) {
        if (normalized(item.context) == "global" && matchesControl(item))
            return {true, item.action, item.context};
    }
    return {};
}

Transaction::Transaction(ProductSettings& current)
    : current_(current), original_(current), draft_(current)
{
}

ValidationResult Transaction::apply()
{
    const ValidationResult checked = validate(draft_);
    if (!checked.ok) return checked;
    current_ = draft_;
    original_ = current_;
    return success();
}

ValidationResult Transaction::applyAndSave(const std::string& path)
{
    const ValidationResult persisted = saveFileAtomic(path, draft_);
    if (!persisted.ok) return persisted;
    current_ = draft_;
    original_ = current_;
    return success();
}

void Transaction::cancel()
{
    draft_ = original_;
}

void Transaction::restoreAllDefaults()
{
    draft_ = ProductSettings::defaults();
}

}  // namespace billiardgl::settings
