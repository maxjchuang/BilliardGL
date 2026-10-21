#include "settings_menu_model.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

}  // namespace

int main()
{
    try {
        auto current = billiardgl::settings::ProductSettings::defaults();
        billiardgl::settings::SettingsMenuModel menu(current);
        menu.open();
        expect(menu.isOpen() && menu.focusIndex() == 0 &&
            std::string(menu.focusedItemId()) == "assist_level",
            "settings opens with visible deterministic keyboard focus");
        menu.handle(billiardgl::settings::MenuCommand::FocusPrevious);
        expect(menu.focusIndex() == 0, "focus navigation does not wrap");
        menu.handle(billiardgl::settings::MenuCommand::CategoryNext);
        expect(menu.category() == billiardgl::settings::SettingsCategory::Input &&
            menu.focusCount() == 3, "keyboard reaches the input category");
        menu.handle(billiardgl::settings::MenuCommand::FocusNext);
        const bool beforeInvert = current.invertMouseY;
        expect(menu.handle(billiardgl::settings::MenuCommand::Toggle).changed &&
            menu.preview().invertMouseY != beforeInvert &&
            current.invertMouseY == beforeInvert,
            "setting changes stay in preview until apply");

        expect(menu.beginRebind("toggle_aim").ok,
            "keyboard can enter rebind capture");
        expect(menu.captureRebind("keyboard", "H", "press").errorCode ==
            "E_BINDING_CONFLICT" && menu.waitingForBinding(),
            "conflicting capture rolls back and keeps capture active");
        expect(menu.captureRebind("keyboard", "F2", "press").ok &&
            !menu.waitingForBinding(), "valid input binding is previewed");
        expect(menu.handle(billiardgl::settings::MenuCommand::Apply).closed &&
            current.invertMouseY != beforeInvert &&
            billiardgl::settings::resolveBinding(current, {"play"},
                "keyboard", "F2", "press").action == "toggle_aim",
            "apply atomically commits preview and runtime binding lookup");

        menu.open();
        menu.handle(billiardgl::settings::MenuCommand::CategoryNext);
        menu.handle(billiardgl::settings::MenuCommand::CategoryNext);
        expect(menu.category() == billiardgl::settings::SettingsCategory::Accessibility,
            "keyboard can reach accessibility settings");
        menu.handle(billiardgl::settings::MenuCommand::Increase);
        expect(menu.preview().uiScale > current.uiScale,
            "keyboard can edit UI scale");
        expect(menu.handle(billiardgl::settings::MenuCommand::Cancel).closed &&
            current.uiScale == 1.0, "cancel discards unapplied changes");

        menu.open();
        menu.handle(billiardgl::settings::MenuCommand::RequestRestoreAll);
        expect(menu.restoreConfirmationVisible() &&
            !menu.handle(billiardgl::settings::MenuCommand::FocusNext).ok,
            "restore defaults requires explicit confirmation");
        menu.handle(billiardgl::settings::MenuCommand::ConfirmRestoreAll);
        menu.handle(billiardgl::settings::MenuCommand::Apply);
        expect(current.bindings.front().control == "H" && !current.invertMouseY,
            "confirmed restore all defaults is applied transactionally");
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
