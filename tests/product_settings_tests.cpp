#include "product_settings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::string bytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
}

void testDefaultsAndAccessibility()
{
    const auto value = billiardgl::settings::ProductSettings::defaults();
    expect(billiardgl::settings::validate(value).ok,
        "product defaults are complete and conflict-free");
    expect(value.bindings.size() == 40,
        "31 product actions include the three replay camera mouse defaults");
    expect(!billiardgl::settings::assistCanAutoAimOrShoot(value),
        "assist levels never grant automatic aim or shooting");
    expect(!billiardgl::settings::requiresTextReflow(value),
        "default UI scale does not require reflow");
    auto large = value;
    large.uiScale = 1.35;
    expect(billiardgl::settings::requiresTextReflow(large),
        "135 percent UI scale requires responsive reflow");

    auto legacyDocument = billiardgl::settings::serialize(value);
    auto& legacyBindings = legacyDocument["input"]["bindings"].asArray();
    legacyBindings.erase(std::remove_if(legacyBindings.begin(),
        legacyBindings.end(), [](const billiardgl::json::Value& entry) {
            return entry.at("action").asString() == "adjust_tip_offset" ||
                (entry.at("context").asString() == "replay" &&
                 entry.at("device").asString() == "mouse");
        }), legacyBindings.end());
    billiardgl::settings::ProductSettings migrated;
    expect(billiardgl::settings::parse(legacyDocument, migrated).ok &&
        migrated.bindings.size() == 40 &&
        billiardgl::settings::resolveBinding(migrated, {"replay"},
            "mouse", "Shift+LeftDrag", "continuous").action == "camera_orbit" &&
        billiardgl::settings::resolveBinding(migrated, {"aim", "play"},
            "keyboard", "ArrowKeys", "continuous").action ==
                "adjust_tip_offset",
        "early v1 settings gain the promised tip binding without a reset");
}

void testConflictAndTransactionIsolation()
{
    auto current = billiardgl::settings::ProductSettings::defaults();
    billiardgl::settings::Transaction transaction(current);
    transaction.draft().bindings.push_back({"toggle_aim", "play", "keyboard",
        " h ", "PRESS"});
    const auto rejected = transaction.apply();
    expect(!rejected.ok && rejected.errorCode == "E_BINDING_CONFLICT" &&
        !rejected.firstAction.empty() && !rejected.secondAction.empty(),
        "normalized overlapping binding conflict blocks apply and identifies both actions");
    expect(current.bindings.size() == 40,
        "failed apply leaves current settings unchanged");
    transaction.cancel();
    expect(transaction.draft().bindings.size() == current.bindings.size(),
        "cancel discards unapplied settings");

    billiardgl::settings::Transaction disjoint(current);
    disjoint.draft().bindings.push_back({"shoot", "aim", "keyboard", "Q", "press"});
    disjoint.draft().bindings.push_back({"camera_pan", "observe", "keyboard", "Q", "press"});
    expect(disjoint.apply().ok,
        "same normalized control is allowed in disjoint aim and observe contexts");
}

void testBoundsAndRestore()
{
    auto current = billiardgl::settings::ProductSettings::defaults();
    billiardgl::settings::Transaction transaction(current);
    transaction.draft().uiScale = 1.5;
    expect(transaction.apply().errorCode == "E_UI_SCALE",
        "font scale above 135 percent is rejected");
    transaction.restoreAllDefaults();
    transaction.draft().mouseSensitivity =
        std::numeric_limits<double>::infinity();
    expect(transaction.apply().errorCode == "E_MOUSE_SENSITIVITY",
        "non-finite settings values are rejected");
    transaction.restoreAllDefaults();
    expect(transaction.apply().ok && current.uiScale == 1.0,
        "restore all defaults is previewed then atomically applied");
}

void testRuntimeBindingResolution()
{
    auto value = billiardgl::settings::ProductSettings::defaults();
    const auto shoot = billiardgl::settings::resolveBinding(
        value, {"aim", "play"}, " MOUSE ", "leftbutton", "Release");
    expect(shoot.matched && shoot.action == "shoot" && shoot.context == "aim",
        "runtime resolver normalizes controls and prefers the active specific context");
    const auto help = billiardgl::settings::resolveBinding(
        value, {"training", "play"}, "keyboard", "h", "press");
    expect(help.matched && help.action == "toggle_help" && help.context == "global",
        "global accessibility bindings remain available in modal contexts");
    for (auto& item : value.bindings) {
        if (item.action == "toggle_help") item.control = "F1";
    }
    expect(!billiardgl::settings::resolveBinding(
        value, {"play"}, "keyboard", "H", "press").matched &&
        billiardgl::settings::resolveBinding(
            value, {"play"}, "keyboard", "F1", "press").action == "toggle_help",
        "saved rebindings change runtime action lookup without hard-coded key fallback");
}

void testPersistenceAndParseIsolation()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "billiardgl_product_settings_v1.json";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path.string() + ".tmp", ignored);

    auto current = billiardgl::settings::ProductSettings::defaults();
    billiardgl::settings::Transaction transaction(current);
    transaction.draft().uiScale = 1.35;
    transaction.draft().colorVisionMode =
        billiardgl::settings::ColorVisionMode::HighContrastPatterns;
    transaction.draft().subtitles = billiardgl::settings::SubtitleMode::Essential;
    expect(transaction.applyAndSave(path.string()).ok &&
        std::filesystem::exists(path) && !std::filesystem::exists(path.string() + ".tmp"),
        "validated settings commit through same-directory atomic replacement");

    billiardgl::settings::ProductSettings loaded;
    expect(billiardgl::settings::loadFile(path.string(), loaded).ok &&
        loaded.uiScale == 1.35 &&
        loaded.colorVisionMode ==
            billiardgl::settings::ColorVisionMode::HighContrastPatterns &&
        loaded.subtitles == billiardgl::settings::SubtitleMode::Essential,
        "versioned settings round-trip all accessibility selections");

    const std::string preserved = bytes(path);
    auto invalid = current;
    invalid.bindings.push_back({"quit", "global", "keyboard", "H", "press"});
    expect(!billiardgl::settings::saveFileAtomic(path.string(), invalid).ok &&
        bytes(path) == preserved,
        "validation failure cannot corrupt the prior settings file");

    billiardgl::json::Value malformed =
        billiardgl::settings::serialize(current);
    malformed["version"] = billiardgl::json::Value(2);
    const double before = loaded.uiScale;
    expect(billiardgl::settings::parse(malformed, loaded).errorCode ==
            "E_SETTINGS_VERSION" && loaded.uiScale == before,
        "unsupported schema is rejected without partially mutating output");
    std::filesystem::remove(path, ignored);
}

}  // namespace

int main()
{
    testDefaultsAndAccessibility();
    testConflictAndTransactionIsolation();
    testBoundsAndRestore();
    testRuntimeBindingResolution();
    testPersistenceAndParseIsolation();
    if (failures != 0) {
        std::cerr << failures << " product settings checks failed\n";
        return 1;
    }
    std::cout << "Product settings, rebinding, and accessibility checks passed\n";
    return 0;
}
