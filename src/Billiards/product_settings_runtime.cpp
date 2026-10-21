#include "product_settings_runtime.h"

#include <filesystem>
#include <utility>

namespace billiardgl::settings {

void InputActionRouter::setActiveContexts(std::vector<std::string> contexts)
{
    activeContexts_ = std::move(contexts);
}

BindingResolution InputActionRouter::resolve(const std::string& device,
    const std::string& control, const std::string& trigger) const
{
    return resolveBinding(
        settings_, activeContexts_, device, control, trigger);
}

ProductSettingsRuntime::ProductSettingsRuntime(std::string path)
    : path_(std::move(path)), menu_(current_), input_(current_)
{
}

StartupResult ProductSettingsRuntime::start()
{
    ProductSettings loaded;
    const ValidationResult result = loadFile(path_, loaded);
    if (result.ok) {
        current_ = std::move(loaded);
        return {};
    }
    current_ = ProductSettings::defaults();
    StartupResult startup;
    startup.usedDefaults = true;
    startup.warningCode = std::filesystem::exists(path_) ?
        result.errorCode : "E_SETTINGS_MISSING";
    return startup;
}

void ProductSettingsRuntime::openMenu()
{
    menu_.open();
}

MenuResult ProductSettingsRuntime::handleMenu(MenuCommand command)
{
    if (command == MenuCommand::Apply)
        return menu_.applyAndSave(path_);
    return menu_.handle(command);
}

MenuResult ProductSettingsRuntime::beginRebind(const std::string& action)
{
    return menu_.beginRebind(action);
}

MenuResult ProductSettingsRuntime::captureRebind(const std::string& device,
    const std::string& control, const std::string& trigger)
{
    return menu_.captureRebind(device, control, trigger);
}

}  // namespace billiardgl::settings
