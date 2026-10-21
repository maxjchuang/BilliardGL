#pragma once

#include "product_settings.h"
#include "settings_menu_model.h"

#include <string>
#include <vector>

namespace billiardgl::settings {

struct StartupResult {
    bool ok = true;
    bool usedDefaults = false;
    std::string warningCode;
};

class InputActionRouter {
public:
    explicit InputActionRouter(const ProductSettings& settings)
        : settings_(settings) {}

    void setActiveContexts(std::vector<std::string> contexts);
    const std::vector<std::string>& activeContexts() const noexcept
    {
        return activeContexts_;
    }
    BindingResolution resolve(const std::string& device,
        const std::string& control, const std::string& trigger) const;

private:
    const ProductSettings& settings_;
    std::vector<std::string> activeContexts_{"play"};
};

class ProductSettingsRuntime {
public:
    explicit ProductSettingsRuntime(std::string path);

    StartupResult start();
    const ProductSettings& current() const noexcept { return current_; }
    const std::string& path() const noexcept { return path_; }
    SettingsMenuModel& menu() noexcept { return menu_; }
    const SettingsMenuModel& menu() const noexcept { return menu_; }
    InputActionRouter& input() noexcept { return input_; }
    const InputActionRouter& input() const noexcept { return input_; }

    void openMenu();
    MenuResult handleMenu(MenuCommand command);
    MenuResult beginRebind(const std::string& action);
    MenuResult captureRebind(const std::string& device,
        const std::string& control, const std::string& trigger);

private:
    std::string path_;
    ProductSettings current_ = ProductSettings::defaults();
    SettingsMenuModel menu_;
    InputActionRouter input_;
};

}  // namespace billiardgl::settings
