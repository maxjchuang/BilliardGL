#pragma once

#include "automation_json.h"

#include <string>
#include <vector>

namespace billiardgl::settings {

constexpr int kProductSettingsVersion = 1;

enum class ColorVisionMode {
    Default,
    Deuteranopia,
    Protanopia,
    Tritanopia,
    HighContrastPatterns
};

enum class SubtitleMode { Off, Essential, All };
enum class AssistMode { Off, Basic, Guided };

struct InputBinding {
    std::string action;
    std::string context;
    std::string device;
    std::string control;
    std::string trigger;
};

struct ProductSettings {
    int version = kProductSettingsVersion;
    double mouseSensitivity = 1.0;
    bool invertMouseY = false;
    double fieldOfViewDegrees = 55.0;
    double cameraDistanceCm = 180.0;
    double uiScale = 1.0;
    ColorVisionMode colorVisionMode = ColorVisionMode::Default;
    SubtitleMode subtitles = SubtitleMode::All;
    AssistMode assist = AssistMode::Basic;
    double musicVolume = 0.7;
    double ambienceVolume = 0.7;
    double effectsVolume = 1.0;
    double uiVolume = 1.0;
    bool musicMuted = false;
    bool ambienceMuted = false;
    bool effectsMuted = false;
    bool uiMuted = false;
    bool bloom = true;
    bool depthOfField = false;
    bool dynamicCamera = true;
    bool cameraShake = true;
    std::vector<InputBinding> bindings;

    static ProductSettings defaults();
};

struct ValidationResult {
    bool ok = false;
    std::string errorCode;
    std::string message;
    std::string firstAction;
    std::string secondAction;
};

struct BindingResolution {
    bool matched = false;
    std::string action;
    std::string context;
};

ValidationResult validate(const ProductSettings& value);
json::Value serialize(const ProductSettings& value);
ValidationResult parse(const json::Value& value, ProductSettings& output);
ValidationResult loadFile(const std::string& path, ProductSettings& output);
ValidationResult saveFileAtomic(const std::string& path,
    const ProductSettings& value);

bool requiresTextReflow(const ProductSettings& value) noexcept;
bool assistCanAutoAimOrShoot(const ProductSettings& value) noexcept;
BindingResolution resolveBinding(const ProductSettings& value,
    const std::vector<std::string>& activeContexts,
    const std::string& device, const std::string& control,
    const std::string& trigger);

class Transaction {
public:
    explicit Transaction(ProductSettings& current);

    ProductSettings& draft() noexcept { return draft_; }
    const ProductSettings& draft() const noexcept { return draft_; }
    ValidationResult apply();
    ValidationResult applyAndSave(const std::string& path);
    void cancel();
    void restoreAllDefaults();

private:
    ProductSettings& current_;
    ProductSettings original_;
    ProductSettings draft_;
};

}  // namespace billiardgl::settings
