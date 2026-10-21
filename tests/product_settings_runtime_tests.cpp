#include "product_settings_runtime.h"

#include <filesystem>
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
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            "billiardgl_product_settings_runtime_v1.json";
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(path.string() + ".tmp", ignored);

        billiardgl::settings::ProductSettingsRuntime runtime(path.string());
        const auto startup = runtime.start();
        expect(startup.ok && startup.usedDefaults &&
            startup.warningCode == "E_SETTINGS_MISSING" &&
            billiardgl::settings::validate(runtime.current()).ok,
            "missing startup settings fall back to validated defaults");

        runtime.input().setActiveContexts({"aim", "play"});
        expect(runtime.input().resolve(
            "mouse", "LeftButton", "release").action == "shoot" &&
            runtime.input().resolve(
            "keyboard", "H", "press").action == "toggle_help",
            "runtime routes specific and global bindings through active contexts");

        runtime.openMenu();
        runtime.handleMenu(billiardgl::settings::MenuCommand::CategoryNext);
        runtime.handleMenu(billiardgl::settings::MenuCommand::FocusNext);
        const bool originalInvert = runtime.current().invertMouseY;
        expect(runtime.handleMenu(
            billiardgl::settings::MenuCommand::Toggle).changed &&
            runtime.menu().preview().invertMouseY != originalInvert &&
            runtime.current().invertMouseY == originalInvert,
            "menu commands update preview without mutating live settings");
        expect(runtime.handleMenu(billiardgl::settings::MenuCommand::Apply).closed &&
            runtime.current().invertMouseY != originalInvert &&
            std::filesystem::exists(path) &&
            !std::filesystem::exists(path.string() + ".tmp"),
            "apply atomically persists preview before changing live settings");

        billiardgl::settings::ProductSettingsRuntime reloaded(path.string());
        const auto reload = reloaded.start();
        expect(reload.ok && !reload.usedDefaults &&
            reloaded.current().invertMouseY != originalInvert,
            "startup reloads the atomically saved product settings");

        reloaded.openMenu();
        reloaded.handleMenu(billiardgl::settings::MenuCommand::CategoryNext);
        reloaded.handleMenu(billiardgl::settings::MenuCommand::FocusNext);
        reloaded.handleMenu(billiardgl::settings::MenuCommand::Toggle);
        expect(reloaded.handleMenu(
            billiardgl::settings::MenuCommand::Cancel).closed &&
            reloaded.current().invertMouseY != originalInvert,
            "cancel discards runtime preview without writing");

        reloaded.openMenu();
        expect(reloaded.beginRebind("toggle_aim").ok &&
            reloaded.captureRebind("keyboard", "H", "press").errorCode ==
                "E_BINDING_CONFLICT" &&
            reloaded.captureRebind("keyboard", "F2", "press").ok,
            "rebind conflict rolls back while valid capture updates preview");
        expect(reloaded.handleMenu(
            billiardgl::settings::MenuCommand::Apply).ok,
            "valid rebind persists through the product transaction");
        reloaded.input().setActiveContexts({"play"});
        expect(reloaded.input().resolve(
            "keyboard", "F2", "press").action == "toggle_aim" &&
            !reloaded.input().resolve(
                "keyboard", "Tab", "press").matched,
            "applied rebind immediately changes context-aware runtime routing");

        const std::filesystem::path impossible = path / "child.json";
        billiardgl::settings::ProductSettingsRuntime failed(impossible.string());
        failed.start();
        failed.openMenu();
        const double priorScale = failed.current().uiScale;
        failed.handleMenu(billiardgl::settings::MenuCommand::CategoryNext);
        failed.handleMenu(billiardgl::settings::MenuCommand::CategoryNext);
        failed.handleMenu(billiardgl::settings::MenuCommand::Increase);
        const auto rejected = failed.handleMenu(
            billiardgl::settings::MenuCommand::Apply);
        expect(!rejected.ok && rejected.errorCode == "E_SETTINGS_WRITE" &&
            failed.current().uiScale == priorScale && failed.menu().isOpen(),
            "persistence failure leaves live settings unchanged and menu recoverable");

        std::filesystem::remove(path, ignored);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
