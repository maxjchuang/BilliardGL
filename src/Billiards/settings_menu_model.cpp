#include "settings_menu_model.h"

#include <algorithm>
#include <array>

namespace billiardgl::settings {
namespace {

constexpr std::array<const char*, 1> kGameplay{{"assist_level"}};
constexpr std::array<const char*, 3> kInput{{
    "mouse_sensitivity", "invert_mouse_y", "rebind_actions"}};
constexpr std::array<const char*, 3> kAccessibility{{
    "ui_scale", "color_vision_mode", "subtitles"}};
constexpr std::array<const char*, 8> kAudio{{
    "music_volume", "ambience_volume", "effects_volume", "ui_volume",
    "music_muted", "ambience_muted", "effects_muted", "ui_muted"}};
constexpr std::array<const char*, 6> kDisplay{{
    "field_of_view", "camera_distance", "bloom", "depth_of_field",
    "dynamic_camera", "camera_shake"}};

int categoryCount(SettingsCategory category)
{
    switch (category) {
    case SettingsCategory::Gameplay: return static_cast<int>(kGameplay.size());
    case SettingsCategory::Input: return static_cast<int>(kInput.size());
    case SettingsCategory::Accessibility: return static_cast<int>(kAccessibility.size());
    case SettingsCategory::Audio: return static_cast<int>(kAudio.size());
    case SettingsCategory::Display: return static_cast<int>(kDisplay.size());
    }
    return 0;
}

const char* categoryItem(SettingsCategory category, int index)
{
    if (index < 0 || index >= categoryCount(category)) return "";
    switch (category) {
    case SettingsCategory::Gameplay: return kGameplay[static_cast<std::size_t>(index)];
    case SettingsCategory::Input: return kInput[static_cast<std::size_t>(index)];
    case SettingsCategory::Accessibility: return kAccessibility[static_cast<std::size_t>(index)];
    case SettingsCategory::Audio: return kAudio[static_cast<std::size_t>(index)];
    case SettingsCategory::Display: return kDisplay[static_cast<std::size_t>(index)];
    }
    return "";
}

template <typename Enum>
Enum cycle(Enum value, int direction, int count)
{
    int index = static_cast<int>(value) + direction;
    if (index < 0) index = count - 1;
    if (index >= count) index = 0;
    return static_cast<Enum>(index);
}

MenuResult failure(const char* code)
{
    MenuResult result;
    result.ok = false;
    result.errorCode = code;
    return result;
}

}  // namespace

SettingsMenuModel::SettingsMenuModel(ProductSettings& current) : current_(current) {}

void SettingsMenuModel::open()
{
    transaction_ = std::make_unique<Transaction>(current_);
    open_ = true;
    restoreConfirmation_ = false;
    category_ = SettingsCategory::Gameplay;
    focusIndex_ = 0;
    rebindAction_.clear();
}

int SettingsMenuModel::focusCount() const noexcept
{
    return categoryCount(category_);
}

const char* SettingsMenuModel::focusedItemId() const noexcept
{
    return categoryItem(category_, focusIndex_);
}

const ProductSettings& SettingsMenuModel::preview() const noexcept
{
    return transaction_ ? transaction_->draft() : current_;
}

MenuResult SettingsMenuModel::focusItem(const std::string& id)
{
    if (!open_ || !transaction_) return failure("E_SETTINGS_MENU_CLOSED");
    for (int index = 0; index < focusCount(); ++index) {
        if (id == categoryItem(category_, index)) {
            focusIndex_ = index;
            return {};
        }
    }
    return failure("E_SETTINGS_ITEM_UNKNOWN");
}

void SettingsMenuModel::changeCategory(int direction)
{
    int value = static_cast<int>(category_) + direction;
    value = std::max(0, std::min(4, value));
    category_ = static_cast<SettingsCategory>(value);
    focusIndex_ = std::min(focusIndex_, focusCount() - 1);
}

MenuResult SettingsMenuModel::adjustFocused(int direction)
{
    if (!transaction_) return failure("E_SETTINGS_MENU_CLOSED");
    ProductSettings& value = transaction_->draft();
    const std::string id = focusedItemId();
    if (id == "assist_level")
        value.assist = cycle(value.assist, direction, 3);
    else if (id == "mouse_sensitivity")
        value.mouseSensitivity = std::clamp(value.mouseSensitivity + direction * 0.1, 0.1, 4.0);
    else if (id == "invert_mouse_y")
        value.invertMouseY = !value.invertMouseY;
    else if (id == "ui_scale")
        value.uiScale = std::clamp(value.uiScale + direction * 0.05, 0.85, 1.35);
    else if (id == "color_vision_mode")
        value.colorVisionMode = cycle(value.colorVisionMode, direction, 5);
    else if (id == "subtitles")
        value.subtitles = cycle(value.subtitles, direction, 3);
    else if (id == "music_volume")
        value.musicVolume = std::clamp(value.musicVolume + direction * 0.05, 0.0, 1.0);
    else if (id == "ambience_volume")
        value.ambienceVolume = std::clamp(value.ambienceVolume + direction * 0.05, 0.0, 1.0);
    else if (id == "effects_volume")
        value.effectsVolume = std::clamp(value.effectsVolume + direction * 0.05, 0.0, 1.0);
    else if (id == "ui_volume")
        value.uiVolume = std::clamp(value.uiVolume + direction * 0.05, 0.0, 1.0);
    else if (id == "music_muted") value.musicMuted = !value.musicMuted;
    else if (id == "ambience_muted") value.ambienceMuted = !value.ambienceMuted;
    else if (id == "effects_muted") value.effectsMuted = !value.effectsMuted;
    else if (id == "ui_muted") value.uiMuted = !value.uiMuted;
    else if (id == "field_of_view")
        value.fieldOfViewDegrees = std::clamp(value.fieldOfViewDegrees + direction, 35.0, 90.0);
    else if (id == "camera_distance")
        value.cameraDistanceCm = std::clamp(value.cameraDistanceCm + direction * 5.0, 10.0, 500.0);
    else if (id == "bloom") value.bloom = !value.bloom;
    else if (id == "depth_of_field") value.depthOfField = !value.depthOfField;
    else if (id == "dynamic_camera") value.dynamicCamera = !value.dynamicCamera;
    else if (id == "camera_shake") value.cameraShake = !value.cameraShake;
    else return failure(id == "rebind_actions" ? "E_REBIND_ACTION_REQUIRED" :
        "E_SETTINGS_ITEM_UNKNOWN");
    return {true, true, false, {}};
}

MenuResult SettingsMenuModel::handle(MenuCommand command)
{
    if (!open_ || !transaction_) return failure("E_SETTINGS_MENU_CLOSED");
    if (restoreConfirmation_ && command != MenuCommand::ConfirmRestoreAll &&
        command != MenuCommand::RejectRestoreAll)
        return failure("E_RESTORE_CONFIRMATION_REQUIRED");
    switch (command) {
    case MenuCommand::FocusNext:
        focusIndex_ = std::min(focusIndex_ + 1, focusCount() - 1);
        return {};
    case MenuCommand::FocusPrevious:
        focusIndex_ = std::max(focusIndex_ - 1, 0);
        return {};
    case MenuCommand::CategoryNext:
        changeCategory(1);
        return {};
    case MenuCommand::CategoryPrevious:
        changeCategory(-1);
        return {};
    case MenuCommand::Increase: return adjustFocused(1);
    case MenuCommand::Decrease: return adjustFocused(-1);
    case MenuCommand::Toggle: return adjustFocused(1);
    case MenuCommand::Apply: {
        const ValidationResult result = transaction_->apply();
        if (!result.ok) return failure(result.errorCode.c_str());
        open_ = false;
        return {true, true, true, {}};
    }
    case MenuCommand::Cancel:
        transaction_->cancel();
        open_ = false;
        rebindAction_.clear();
        return {true, false, true, {}};
    case MenuCommand::RequestRestoreAll:
        restoreConfirmation_ = true;
        return {};
    case MenuCommand::ConfirmRestoreAll:
        transaction_->restoreAllDefaults();
        restoreConfirmation_ = false;
        return {true, true, false, {}};
    case MenuCommand::RejectRestoreAll:
        restoreConfirmation_ = false;
        return {};
    }
    return failure("E_SETTINGS_MENU_COMMAND");
}

MenuResult SettingsMenuModel::applyAndSave(const std::string& path)
{
    if (!open_ || !transaction_) return failure("E_SETTINGS_MENU_CLOSED");
    if (restoreConfirmation_)
        return failure("E_RESTORE_CONFIRMATION_REQUIRED");
    const ValidationResult result = transaction_->applyAndSave(path);
    if (!result.ok) return failure(result.errorCode.c_str());
    open_ = false;
    rebindAction_.clear();
    return {true, true, true, {}};
}

MenuResult SettingsMenuModel::beginRebind(const std::string& action)
{
    if (!open_ || !transaction_) return failure("E_SETTINGS_MENU_CLOSED");
    const auto& bindings = transaction_->draft().bindings;
    if (std::none_of(bindings.begin(), bindings.end(),
            [&](const InputBinding& item) { return item.action == action; }))
        return failure("E_REBIND_ACTION_UNKNOWN");
    rebindAction_ = action;
    return {};
}

MenuResult SettingsMenuModel::captureRebind(const std::string& device,
    const std::string& control, const std::string& trigger)
{
    if (!open_ || !transaction_) return failure("E_SETTINGS_MENU_CLOSED");
    if (rebindAction_.empty()) return failure("E_REBIND_NOT_ACTIVE");
    auto& bindings = transaction_->draft().bindings;
    const auto found = std::find_if(bindings.begin(), bindings.end(),
        [&](const InputBinding& item) { return item.action == rebindAction_; });
    if (found == bindings.end()) return failure("E_REBIND_ACTION_UNKNOWN");
    const InputBinding original = *found;
    found->device = device;
    found->control = control;
    found->trigger = trigger;
    const ValidationResult checked = validate(transaction_->draft());
    if (!checked.ok) {
        *found = original;
        return failure(checked.errorCode.c_str());
    }
    rebindAction_.clear();
    return {true, true, false, {}};
}

}  // namespace billiardgl::settings
