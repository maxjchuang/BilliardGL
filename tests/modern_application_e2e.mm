#include "render/modern/modern_application_mac.h"
#include "render/modern/modern_product_session.h"
#include "platform_audio.h"
#include "automation_json.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>
#include <sstream>
#include <stdexcept>

namespace {

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("product lifecycle E2E: " + message);
}

void verifyNativeInsetMarkerCapture(const std::filesystem::path& directory,
    const std::string& fixtureName, bool cvd)
{
    std::ifstream receiptFile(directory / "cp1-marker-receipt.json");
    const std::string receiptText((std::istreambuf_iterator<char>(receiptFile)), {});
    const auto receipt = billiardgl::json::parse(receiptText);
    expect(receipt.ok, "CP1 marker receipt is valid JSON");
    const int width = static_cast<int>(receipt.value.at("physical_width").asNumber());
    const int height = static_cast<int>(receipt.value.at("physical_height").asNumber());
    std::vector<std::uint32_t> owners(static_cast<std::size_t>(width) * height);
    std::ifstream ownerFile(directory / "final-semantic-owner.u32", std::ios::binary);
    ownerFile.read(reinterpret_cast<char*>(owners.data()),
        static_cast<std::streamsize>(owners.size() * sizeof(std::uint32_t)));
    expect(ownerFile.good(), "native R32UI ownership frame is complete");
    std::ifstream colorFile(directory / "inset-stage-1.ppm", std::ios::binary);
    std::string magic;
    int ppmWidth = 0, ppmHeight = 0, maximum = 0;
    colorFile >> magic >> ppmWidth >> ppmHeight >> maximum;
    colorFile.get();
    expect(magic == "P6" && ppmWidth == width && ppmHeight == height && maximum == 255,
        "GL_BACK color frame matches native semantic dimensions");
    std::vector<unsigned char> color(owners.size() * 3);
    colorFile.read(reinterpret_cast<char*>(color.data()), static_cast<std::streamsize>(color.size()));
    expect(colorFile.good(), "GL_BACK RGB frame is complete");
    const auto encode = [](double linear) {
        return static_cast<int>(std::lround(255.0 * (linear <= .0031308 ?
            12.92 * linear : 1.055 * std::pow(linear, 1.0 / 2.4) - .055)));
    };
    const std::array<int, 3> predicted{{255, encode(cvd ? .4 : .78), encode(cvd ? .02 : .08)}};
    std::vector<std::string> labels;
    std::size_t checkedCells = 0;
    for (const auto& probe : receipt.value.at("probes").asArray()) {
        const auto& label = probe.at("visitLabel").asString();
        if (label.empty()) continue;
        labels.push_back(label);
        expect(!probe.at("sourceCoreMemberKey").asString().empty() &&
            !probe.at("parent").asString().empty(), "marker keeps original source/event identity");
        const char* expectedBits = label == "1" ? "010110010010111" :
            label == "2" ? "111001111100111" : label == "T" ? "111010010010010" : nullptr;
        expect(expectedBits != nullptr, "only independently specified 1/2/T glyphs are accepted");
        expect(probe.at("sourceCoreVisible").asBool() == (label == "2"),
            "first covered visit and subpixel tail remain truthfully hidden");
        expect(probe.at("sourceFirstTick").asNumber() <= probe.at("sourceLastTick").asNumber(),
            "marker keeps ordered original source tick interval");
        const auto token = static_cast<std::uint32_t>(probe.at("token").asNumber());
        const auto& cells = probe.at("glyph_samples").asArray();
        const float uiScale = static_cast<float>(receipt.value.at("ui_scale").asNumber());
        const auto& markerRect = probe.at("markerRect").asArray();
        const auto& plotRect = receipt.value.at("plot_rect").asArray();
        const float plotX = static_cast<float>(plotRect[0].asNumber());
        const float plotY = static_cast<float>(plotRect[1].asNumber());
        const float markerX = static_cast<float>(markerRect[0].asNumber());
        const float markerY = static_cast<float>(markerRect[1].asNumber());
        const float markerWidth = static_cast<float>(markerRect[2].asNumber());
        expect(cells.size() == 15, "full 3x5 glyph matrix has unchanged sample denominator");
        std::size_t cellOrdinal = 0;
        for (const auto& cell : cells) {
            const int x = static_cast<int>(cell.at("x").asNumber());
            const int y = static_cast<int>(cell.at("y").asNumber());
            const int row = static_cast<int>(cellOrdinal / 3);
            const int col = static_cast<int>(cellOrdinal % 3);
            const float centerX = markerX + markerWidth * .5f - 4.8f * uiScale +
                (col * 3.2f + 1.6f) * uiScale;
            const float centerY = markerY + (2.0f + row * 3.2f + 1.6f) * uiScale;
            const int canonicalX = static_cast<int>(std::floor(centerX - plotX)) +
                static_cast<int>(std::lround(plotX));
            const int canonicalY = static_cast<int>(std::floor(centerY - plotY)) +
                static_cast<int>(std::lround(plotY));
            expect(x == canonicalX && y == canonicalY,
                "fixed glyph centers use independent CP1 local-floor plus canonical rounded plot origin");
            expect(x >= 0 && x < width && y >= 0 && y < height, "glyph cell is inside native drawable");
            const bool ink = cell.at("ink").asBool();
            expect(ink == (expectedBits[cellOrdinal++] == '1'), "receipt agrees with independent glyph bitmap");
            const auto ownerIndex = static_cast<std::size_t>(height - 1 - y) * width + x;
            expect((owners[ownerIndex] == token) == ink, "actual final GPU owner matches 1/2/T glyph shape");
            // PPM is top-down; the raw GL R32UI frame is bottom-up.
            const auto colorIndex = (static_cast<std::size_t>(y) * width + x) * 3;
            bool predictedColor = true;
            for (int c = 0; c < 3; ++c)
                predictedColor &= std::abs(static_cast<int>(color[colorIndex + c]) - predicted[c]) <= 2;
            expect(predictedColor == ink, "GL_BACK ink and negative cells retain readable 1/2/T bitmap");
            ++checkedCells;
        }
    }
    expect(labels == (fixtureName == "retrace" ? std::vector<std::string>{"1", "2"} :
        std::vector<std::string>{"T"}), "native fixture retains exact expected marker inventory");
    std::ofstream(directory / "native-marker-verification.json")
        << "{\"origin\":\"explicit_recorded_preview_replay_fixture\",\"fixture\":\""
        << fixtureName << "\",\"palette\":\"" << (cvd ? "color_vision_safe" : "standard")
        << "\",\"marker_count\":" << labels.size() << ",\"glyph_cells_checked\":"
        << checkedCells << ",\"semantic_owner\":\"PASS\",\"GL_BACK_glyph\":\"PASS\"}\n";
}

billiardgl::render::PlatformInputEvent key(const std::string& control)
{
    billiardgl::render::PlatformInputEvent event;
    event.kind = billiardgl::render::PlatformInputKind::KeyDown;
    event.control = control;
    return event;
}

void clickShellItem(billiardgl::render::ModernProductSession& session,
    const std::string& id)
{
    using namespace billiardgl::render;
    const ui::ShellScreenLayout layout = session.shellLayout();
    expect(layout.ok, "CPU shell layout is available to macOS product session");
    const auto found = std::find_if(layout.elements.begin(),
        layout.elements.end(), [&](const ui::ShellElementLayout& item) {
            return item.id == id;
        });
    expect(found != layout.elements.end(), "macOS shell click id exists");
    const double x = (found->rect.x + found->rect.width * 0.5) *
        layout.viewport.logicalWidth / layout.viewport.physicalWidth;
    const double y = (found->rect.y + found->rect.height * 0.5) *
        layout.viewport.logicalHeight / layout.viewport.physicalHeight;
    PlatformInputEvent event;
    event.logicalX = x;
    event.logicalY = y;
    event.backingScale = static_cast<double>(layout.viewport.physicalWidth) /
        layout.viewport.logicalWidth;
    event.kind = PlatformInputKind::PointerMove;
    expect(session.handlePlatformInput(event).handled,
        "macOS product session updates shell hover");
    event.kind = PlatformInputKind::PointerButtonDown;
    event.control = "LeftButton";
    expect(session.handlePlatformInput(event).handled,
        "macOS product session captures shell press");
    event.kind = PlatformInputKind::PointerButtonUp;
    expect(session.handlePlatformInput(event).ok,
        "macOS product session activates shell release");
}

void productLifecycleEvidence(const std::filesystem::path& outputDirectory)
{
    using namespace billiardgl;
    using namespace billiardgl::render;
    const std::filesystem::path settingsPath =
        outputDirectory / "invalid-product-settings.json";
    {
        std::ofstream invalid(settingsPath, std::ios::binary);
        invalid << "{not-json";
    }

    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions product;
    product.settingsPath = settingsPath.string();
    product.trainingManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/training/phase4/training_pack_v1.json";
    ModernProductSession session(runtime, ai, product);
    session.setShellViewport({1280, 720, 2560, 1440, 192.0f});
    auto snapshot = session.snapshot();
    expect(snapshot.settingsStartup.usedDefaults &&
        !snapshot.settingsStartup.warningCode.empty(),
        "invalid settings visibly fall back to defaults");
    expect(snapshot.shell.screen == ProductScreen::MainMenu &&
        snapshot.shell.screenId == "main_menu" &&
        snapshot.trainingLoadError.empty() &&
        snapshot.shell.training.scenarioCount == 12,
        "app-facing snapshot starts at Main with the validated training pack");

    clickShellItem(session, "help");
    expect(session.snapshot().shell.screen == ProductScreen::Help,
        "macOS product session enters Help by Retina pointer hit");
    clickShellItem(session, "help_back");
    expect(session.snapshot().shell.screen == ProductScreen::MainMenu,
        "macOS product session returns from Help by the shared draw-list row");

    expect(session.handlePlatformInput(key("Enter")).ok &&
        session.snapshot().shell.screen == ProductScreen::SinglePlayerSetup,
        "Main Enter opens single-player setup through shell navigation");
    expect(session.handlePlatformInput(key("ArrowDown")).ok &&
        session.handlePlatformInput(key("Enter")).ok,
        "Club difficulty is selected through normalized input");
    expect(session.handlePlatformInput(key("ArrowDown")).ok &&
        session.handlePlatformInput(key("ArrowDown")).ok &&
        session.handlePlatformInput(key("Enter")).ok,
        "setup activation starts a match through the shared controller graph");
    snapshot = session.snapshot();
    expect(snapshot.shell.screen == ProductScreen::Match &&
        snapshot.shell.aiDifficulty == ai::Difficulty::Club &&
        runtime.rulesMode() == RulesMode::ChineseEightBallV1 &&
        snapshot.aiPlayer.configuration.enabled &&
        snapshot.aiPlayer.configuration.player == 1,
        "match snapshot, approved rules, runtime, and AI share one authority");

    const settings::AssistMode savedAssist = session.settings().current().assist;
    expect(session.handlePlatformInput(key("F10")).ok &&
        session.snapshot().shell.screen == ProductScreen::Settings,
        "F10 opens product settings through the input controller");
    expect(session.handlePlatformInput(key("+")).ok &&
        session.snapshot().shell.settingsPreviewActive &&
        session.settings().menu().preview().assist != savedAssist &&
        session.settings().current().assist == savedAssist,
        "settings adjustments remain preview-only");
    expect(session.handlePlatformInput(key("Escape")).ok &&
        session.snapshot().shell.screen == ProductScreen::Match &&
        session.settings().current().assist == savedAssist,
        "Escape cancels preview and returns to the match");

    expect(session.handlePlatformInput(key("F10")).ok &&
        session.beginRebind("toggle_aim").ok &&
        session.snapshot().shell.rebindCaptureActive,
        "app-facing rebind enters capture state");
    expect(session.handlePlatformInput(key("F2")).ok &&
        !session.snapshot().shell.rebindCaptureActive &&
        session.handlePlatformInput(key("Enter")).ok &&
        session.snapshot().shell.screen == ProductScreen::Match,
        "captured F2 binding applies atomically and closes Settings");
    expect(session.handlePlatformInput(key("F2")).ok &&
        runtime.state().aim.mode == AimMode::Aim,
        "applied binding drives the same game runtime");

    PlatformInputEvent pointer;
    pointer.kind = PlatformInputKind::PointerMove;
    pointer.logicalX = 100.5;
    pointer.logicalY = 50.25;
    pointer.deltaX = 2.0;
    pointer.backingScale = 2.0;
    expect(session.handlePlatformInput(pointer).ok &&
        session.snapshot().pointer.backingX == 201.0 &&
        session.snapshot().pointer.backingY == 100.5,
        "app snapshot exposes logical-to-backing pointer evidence");

    PlatformInputEvent shoot;
    shoot.kind = PlatformInputKind::PointerButtonUp;
    shoot.control = "LeftButton";
    shoot.backingScale = 2.0;
    expect(session.handlePlatformInput(shoot).ok && runtime.state().ballsMoving,
        "mouse release shoots through ProductInputController");
    const ModernProductEventResult moving = session.handlePlatformInput(key("F2"));
    expect(!moving.ok && moving.errorCode == "input_balls_moving" &&
        session.snapshot().lastError == "input_balls_moving",
        "moving-ball shot authority rejection is visible at app boundary");
    expect(session.handlePlatformInput(key("Escape")).ok &&
        session.snapshot().shell.screen == ProductScreen::MainMenu,
        "Match Escape returns to Main even while physics is moving");
    const ModernProductEventResult quit = session.handlePlatformInput(key("Escape"));
    expect(quit.ok && quit.appRequest == ProductShellAppRequest::Quit &&
        session.snapshot().quitRequested,
        "Main Escape produces an effective application quit request");

    GameRuntime aiRuntime;
    ai::AiPlayerController aiAuthority;
    ModernProductSessionOptions aiOptions;
    aiOptions.settingsPath = (outputDirectory / "missing-ai-settings.json").string();
    ModernProductSession aiSession(aiRuntime, aiAuthority, aiOptions);
    expect(aiSession.shell().startSinglePlayer().ok,
        "AI authority fixture starts through ProductShellController");
    rules::ShotFacts dryBreak;
    dryBreak.shooter = 0;
    dryBreak.firstContactBall = 1;
    dryBreak.anyRailAfterFirstContact = true;
    dryBreak.breakShot = true;
    dryBreak.distinctObjectBallsToRail = 4;
    expect(aiRuntime.adjudicateRulesShot(dryBreak).ok &&
        aiRuntime.matchState().currentPlayer == 1,
        "authoritative dry break transfers the turn to AI");
    const ModernProductEventResult rejected =
        aiSession.handlePlatformInput(key("Tab"));
    expect(!rejected.ok && rejected.errorCode == "input_ai_turn_active" &&
        aiSession.snapshot().aiPlayer.configuration.enabled,
        "human shot-authority input is rejected on the shared AI turn");
}

}  // namespace

int main(int argc, const char* argv[])
{
    try {
        if (argc != 2 && argc != 3 && argc != 4)
            throw std::runtime_error("output directory required");
        const std::filesystem::path outputDirectory(argv[1]);
        std::filesystem::create_directories(outputDirectory);
        if (argc == 4 && std::string(argv[2]) == "--pixel-matrix") {
            using namespace billiardgl;
            using namespace billiardgl::render;
            const std::string profile = argv[3];
            const bool physical1280 = profile == "physical-1280x720";
            const bool physical1920 = profile == "physical-1920x1080";
            const bool retina = profile == "retina-1280x720";
            const bool cvd = profile == "cvd-1920x1080";
            expect(physical1280 || physical1920 || retina || cvd,
                "pixel matrix profile is recognized");
            settings::ProductSettings setting =
                settings::ProductSettings::defaults();
            if (cvd)
                setting.colorVisionMode =
                    settings::ColorVisionMode::Deuteranopia;
            const auto settingPath = outputDirectory /
                "native-pixel-matrix-settings.json";
            const auto saved = settings::saveFileAtomic(
                settingPath.string(), setting);
            expect(saved.ok, "pixel matrix settings are written atomically");

            ModernApplicationOptions options;
            options.width = physical1280 || retina ? 1280 : 1920;
            options.height = physical1280 || retina ? 720 : 1080;
            options.msaaSamples = 4;
            options.quality = RenderQuality::Medium;
            options.productMode = true;
            options.showWindow = true;
            options.title = "BilliardGL Native Product Pixel Matrix";
            options.productSettingsPath = settingPath.string();
            options.trainingManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/training/phase4/training_pack_v2.json";
            options.executableVersion = "billiardgl-product-source-v1";
            options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
            const std::string phase2 = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/phase2/";
            options.pbrParametersPath = phase2 +
                "focused_practice_hall_pbr_v1.params.json";
            options.pbrAssetManifestPath = phase2 +
                "focused_practice_hall_modular_v1.asset.json";
            options.lightingRigPath = phase2 +
                "focused_practice_hall_lighting_rig_v1.json";
            options.gltfScenePath = phase2 +
                "focused_practice_hall_modular_v1.gltf";
            options.venueScenePath = phase2 +
                "focused_practice_hall_venue_v1.gltf";
            setenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_SELF_TEST", "1", 1);
            setenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PROFILE",
                profile.c_str(), 1);
            setenv("BILLIARDGL_VISIBLE_CAPTURE_DIR",
                outputDirectory.string().c_str(), 1);
            if (!retina)
                setenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PHYSICAL_SIZE",
                    "1", 1);
            else
                unsetenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PHYSICAL_SIZE");
            std::istringstream input;
            std::ostringstream output, errors;
            const int result = runModernApplication(
                options, input, output, errors);
            unsetenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_SELF_TEST");
            unsetenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PROFILE");
            unsetenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PHYSICAL_SIZE");
            unsetenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
            expect(result == 0, "pixel matrix native run failed: " +
                errors.str());
            expect(output.str().find(
                    "visible_pixel_matrix_complete profile=" + profile +
                    " states=7") != std::string::npos,
                "pixel matrix native run completes all seven states");
            const std::array<const char*, 7> states{{
                "default", "aim", "post_shot", "foul_modal", "help",
                "training", "replay"}};
            for (const char* state : states) {
                const auto image = outputDirectory /
                    (std::string(state) + ".ppm");
                const auto noGlyphImage = outputDirectory /
                    (std::string(state) + "-no-glyphs.ppm");
                expect(std::filesystem::exists(image) &&
                    std::filesystem::file_size(image) > 64,
                    std::string("pixel matrix image exists for ") + state);
                expect(std::filesystem::exists(noGlyphImage) &&
                    std::filesystem::file_size(noGlyphImage) > 64,
                    std::string("paired no-glyph image exists for ") +
                        state);
            }
            const auto receipt = outputDirectory /
                "native-pixel-matrix-receipt.jsonl";
            std::ifstream receiptStream(receipt);
            std::string line;
            int rows = 0;
            bool replayTickReceiptVerified = false;
            while (std::getline(receiptStream, line)) {
                const auto parsed = json::parse(line);
                expect(parsed.ok && parsed.value.at("profile").asString() ==
                    profile, "pixel matrix receipt row is valid and bound");
                expect(parsed.value.at("glyph_atlas_binding_verified").asBool() &&
                    parsed.value.at("font_atlas_cache_key").asString().size() > 0 &&
                    parsed.value.at("paired_frozen_state_sha256").asString().size() == 64 &&
                    parsed.value.at("paired_frozen_state_before_sha256").asString() ==
                        parsed.value.at("paired_frozen_state_after_sha256").asString() &&
                    parsed.value.at("paired_normal_rgba_sha256").asString().size() == 64 &&
                    parsed.value.at("paired_no_glyph_rgba_sha256").asString().size() == 64 &&
                    parsed.value.at("paired_restored_rgba_sha256").asString() ==
                        parsed.value.at("paired_normal_rgba_sha256").asString() &&
                    parsed.value.at("paired_normal_ppm_sha256").asString().size() == 64 &&
                    parsed.value.at("paired_no_glyph_ppm_sha256").asString().size() == 64 &&
                    parsed.value.at("paired_normal_rgba_sha256").asString() !=
                        parsed.value.at("paired_no_glyph_rgba_sha256").asString() &&
                    parsed.value.at("paired_no_glyph_canonical_sha256").asString() ==
                        parsed.value.at("product_ui_canonical_sha256").asString() &&
                    parsed.value.at("paired_restored_canonical_sha256").asString() ==
                        parsed.value.at("product_ui_canonical_sha256").asString() &&
                    parsed.value.at("paired_no_glyph_suppressed_count").asNumber() > 0 &&
                    parsed.value.at("paired_no_glyph_suppressed_icon_labels").asNumber() == 0 &&
                    parsed.value.at("paired_restored_user_visible").asBool() &&
                    parsed.value.at("final_ownership_ppm_color_bound").asBool() &&
                    parsed.value.at("final_ownership_capture_id").asString().size() > 0 &&
                    parsed.value.at("final_ownership_color_sha256").asString().size() == 64 &&
                    parsed.value.at("final_ownership_semantic_sha256").asString().size() == 64 &&
                    static_cast<int>(parsed.value.at(
                        "final_ownership_msaa_samples").asNumber()) == 4,
                    "pixel matrix receipt binds same-state atlas-authenticated glyph counterfactual");
                const auto& glyphs = parsed.value.at(
                    "product_ui_glyph_quads").asArray();
                expect(!glyphs.empty(),
                    "pixel matrix receipt retains glyph identity inventory");
                for (const auto& glyph : glyphs)
                    expect(glyph.at("atlas_record_sha256").asString().size() == 64,
                        "each submitted glyph binds exact frozen atlas record");
                if (parsed.value.at("state").asString() == "replay") {
                    const int expectedTicks = static_cast<int>(
                        parsed.value.at("actual_tick_count").asNumber());
                    int receiptTicks = 0;
                    for (const auto& probe : parsed.value.at(
                            "trajectory_probes").asArray()) {
                        if (static_cast<int>(probe.at(
                                "component_role").asNumber()) != 3)
                            continue;
                        ++receiptTicks;
                        expect(probe.at("halo_valid").asBool(),
                            "replay Tick probe marks its authored halo coordinate valid");
                    }
                    expect(expectedTicks > 0 && receiptTicks == expectedTicks,
                        "replay receipt binds every visible tick to one Tick probe");
                    replayTickReceiptVerified = true;
                }
                ++rows;
            }
            expect(rows == 7,
                "pixel matrix receipt has exactly seven state rows");
            expect(replayTickReceiptVerified,
                "pixel matrix receipt verifies replay Tick probe inventory");
            std::cout << "modern-native-product-pixel-matrix: pass profile="
                << profile << " states=7\n";
            return 0;
        }
        if (argc == 3 && (std::string(argv[2]) == "--inset-product" ||
                std::string(argv[2]) == "--inset-marker-fixtures")) {
            using namespace billiardgl::render;
            const bool markerFixtures = std::string(argv[2]) == "--inset-marker-fixtures";
            ModernApplicationOptions options;
            options.width = 1280; options.height = 720; options.msaaSamples = 4;
            options.productMode = true; options.showWindow = true;
            options.title = "BilliardGL Trajectory Inset Product E2E";
            options.executableVersion = "billiardgl-product-source-v1";
            options.productSettingsPath = (outputDirectory / "inset-settings.json").string();
            options.trainingManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/training/phase4/training_pack_v2.json";
            options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
            const std::string phase2 = std::string(BILLIARDGL_SOURCE_ROOT) + "/assets/modern/phase2/";
            options.pbrParametersPath = phase2 + "focused_practice_hall_pbr_v1.params.json";
            options.pbrAssetManifestPath = phase2 + "focused_practice_hall_modular_v1.asset.json";
            options.lightingRigPath = phase2 + "focused_practice_hall_lighting_rig_v1.json";
            options.gltfScenePath = phase2 + "focused_practice_hall_modular_v1.gltf";
            options.venueScenePath = phase2 + "focused_practice_hall_venue_v1.gltf";
            {
                billiardgl::GameRuntime runtime;
                billiardgl::ai::AiPlayerController ai;
                ModernProductSessionOptions sessionOptions;
                sessionOptions.manualInspectors = true;
                sessionOptions.settingsPath = options.productSettingsPath;
                sessionOptions.trainingManifestPath = options.trainingManifestPath;
                sessionOptions.executableVersion = options.executableVersion;
                ModernProductSession session(runtime, ai, sessionOptions);
                session.setShellViewport({1280,720,2560,1440,192.0f});
                const auto started = session.shell().startTrainingScenario(
                    "launch_straight_draw_intro_v2", options.executableVersion, 0);
                expect(started.ok, "inset CPU catalog precondition: " + started.errorCode);
                const auto activate = [&](const char* id) {
                    session.shell().setInspectorExpanded(true);
                    std::optional<billiardgl::product_hud::HudRect> target;
                    for (std::size_t row=0;row<32 && !target;++row) {
                        session.shell().setInspectorScrollRow(row);
                        const auto layout=session.shellLayout();
                        for (const auto& element:layout.elements)
                            if(element.id==id && element.enabled) target=element.rect;
                    }
                    expect(target.has_value(),std::string("inset CPU button present: ")+id);
                    PlatformInputEvent event;
                    event.control="LeftButton";
                    event.logicalX=(target->x+target->width*.5)*.5;
                    event.logicalY=(target->y+target->height*.5)*.5;
                    event.kind=PlatformInputKind::PointerButtonDown;
                    expect(session.handlePlatformInput(event).ok,"inset CPU button down");
                    event.kind=PlatformInputKind::PointerButtonUp;
                    const auto result = session.handlePlatformInput(event);
                    expect(result.ok, std::string("inset CPU activation: ") + id + ":" + result.errorCode);
                };
                expect(session.handlePlatformInput(key("Tab")).ok, "inset CPU aim entry");
                activate("preview_training");
                expect(session.snapshot().shell.training.previewReady &&
                    session.snapshot().shell.training.previewSampleCount > 0,
                    "inset CPU real physics preview ready");
                expect(session.shell().watchTrainingReferenceReplay().ok,
                    "inset CPU trusted replay load");
                for (int i=0;i<900 && session.snapshot().shell.replay.cueTrajectory.size()<24;++i)
                    activate("replay_step_tick");
                expect(session.snapshot().shell.replay.cueTrajectory.size()>=24,
                    "inset CPU real replay trajectory ready");
                std::cout << "inset_product_cpu_preconditions=PASS\n";
            }
            if (markerFixtures) {
                // These are declared recorded-source regressions. They do not
                // replace the ordinary session/toggle/seek acceptance above.
                for (const std::string name : {"retrace", "subpixel_tail"}) {
                    for (const bool cvd : {false, true}) {
                        const auto directory = outputDirectory / (name + (cvd ? "-cvd" : "-standard"));
                        std::filesystem::create_directories(directory);
                        const auto fixture = std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
                            "tests/fixtures" / ("trajectory_inset_" + name + "_20260930.txt");
                        options.width = name == "retrace" ? 2560 : 1280;
                        options.height = name == "retrace" ? 1440 : 720;
                        options.productSettingsPath = (directory / "settings.json").string();
                        setenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PHYSICAL_SIZE", "1", 1);
                        setenv("BILLIARDGL_VISIBLE_INSET_SELF_TEST", "1", 1);
                        setenv("BILLIARDGL_VISIBLE_CAPTURE_DIR", directory.string().c_str(), 1);
                        setenv("BILLIARDGL_INSET_MARKER_FIXTURE", fixture.string().c_str(), 1);
                        setenv("BILLIARDGL_INSET_MARKER_RECEIPT_PATH", (directory / "cp1-marker-receipt.json").string().c_str(), 1);
                        setenv("BILLIARDGL_INSET_FINAL_PROBE_SELF_TEST", "1", 1);
                        if (cvd) setenv("BILLIARDGL_INSET_MARKER_CVD", "1", 1);
                        else unsetenv("BILLIARDGL_INSET_MARKER_CVD");
                        std::istringstream input;
                        std::ostringstream output, errors;
                        const int result = runModernApplication(options, input, output, errors);
                        unsetenv("BILLIARDGL_VISIBLE_INSET_SELF_TEST");
                        unsetenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
                        unsetenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PHYSICAL_SIZE");
                        unsetenv("BILLIARDGL_INSET_MARKER_FIXTURE");
                        unsetenv("BILLIARDGL_INSET_MARKER_RECEIPT_PATH");
                        unsetenv("BILLIARDGL_INSET_FINAL_PROBE_SELF_TEST");
                        unsetenv("BILLIARDGL_INSET_MARKER_CVD");
                        std::ofstream(directory / "native-output.log") << output.str();
                        std::ofstream(directory / "native-errors.log") << errors.str();
                        std::cout << output.str();
                        expect(result == 0, "marker fixture native run failed: " + errors.str());
                        expect(output.str().find("native_owner_readback=PASS") != std::string::npos,
                            "recorded marker fixture traverses native final semantic capture");
                        expect(std::filesystem::file_size(directory / "final-semantic-owner.u32") ==
                            static_cast<std::uintmax_t>(options.width) * options.height * 4,
                            "final semantic capture matches actual native drawable dimensions");
                        verifyNativeInsetMarkerCapture(directory, name, cvd);
                    }
                }
                return 0;
            }
            setenv("BILLIARDGL_VISIBLE_INSET_SELF_TEST", "1", 1);
            setenv("BILLIARDGL_VISIBLE_CAPTURE_DIR", outputDirectory.string().c_str(), 1);
            std::istringstream input;
            std::ostringstream output, errors;
            const int result = runModernApplication(options, input, output, errors);
            unsetenv("BILLIARDGL_VISIBLE_INSET_SELF_TEST");
            unsetenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
            std::cout << output.str();
            expect(result == 0, "inset product run failed: " + errors.str());
            expect(output.str().find("visible_inset stage=7 visible=1") != std::string::npos,
                "normal inset Training/Replay/toggle/resize/seek chain completed");
            return 0;
        }
        if (argc == 3 && std::string(argv[2]) == "--visible-drawable") {
            using namespace billiardgl::render;
            ModernApplicationOptions options;
            options.width = 1280;
            options.height = 720;
            options.msaaSamples = 4;
            options.productMode = true;
            options.showWindow = true;
            options.title = "BilliardGL Visible Drawable E2E";
            options.productSettingsPath =
                (outputDirectory / "visible-drawable-settings.json").string();
            options.trainingManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/training/phase4/training_pack_v1.json";
            options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
            const std::string phase2 = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/phase2/";
            options.pbrParametersPath = phase2 +
                "focused_practice_hall_pbr_v1.params.json";
            options.pbrAssetManifestPath = phase2 +
                "focused_practice_hall_modular_v1.asset.json";
            options.lightingRigPath = phase2 +
                "focused_practice_hall_lighting_rig_v1.json";
            options.gltfScenePath = phase2 +
                "focused_practice_hall_modular_v1.gltf";
            options.venueScenePath = phase2 +
                "focused_practice_hall_venue_v1.gltf";
            setenv("BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST", "1", 1);
            setenv("BILLIARDGL_VISIBLE_CAPTURE_DIR", outputDirectory.string().c_str(), 1);
            const std::string diagnosticDirectory =
                (outputDirectory / "playtest-diagnostics").string();
            setenv("BILLIARDGL_PLAYTEST_LOG_DIR", diagnosticDirectory.c_str(), 1);
            std::istringstream input;
            std::ostringstream output, errors;
            const int result = runModernApplication(options, input, output, errors);
            unsetenv("BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST");
            unsetenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
            unsetenv("BILLIARDGL_PLAYTEST_LOG_DIR");
            expect(result == 0, "visible drawable run failed: " + errors.str());
            const auto rulesOffer = outputDirectory / "rules-06c-offer.ppm";
            const auto rulesArmed = outputDirectory / "rules-06c-armed.ppm";
            const auto rulesCommitted = outputDirectory / "rules-06c-committed.ppm";
            const auto rules06EParentOffer =
                outputDirectory / "rules-06e-parent-offer.ppm";
            const auto rules06EParentArmed =
                outputDirectory / "rules-06e-parent-armed.ppm";
            const auto rules06EPreview =
                outputDirectory / "rules-06e-bih-preview.ppm";
            const auto rules06EArmed =
                outputDirectory / "rules-06e-bih-armed.ppm";
            const auto rules06ECommitted =
                outputDirectory / "rules-06e-committed.ppm";
            const auto rules06FParentOffer =
                outputDirectory / "rules-06f-parent-offer.ppm";
            const auto rules06FParentArmed =
                outputDirectory / "rules-06f-parent-armed.ppm";
            const auto rules06FPreview =
                outputDirectory / "rules-06f-bih-preview.ppm";
            const auto rules06FArmed =
                outputDirectory / "rules-06f-bih-armed.ppm";
            const auto rules06FCommitted =
                outputDirectory / "rules-06f-committed.ppm";
            const auto rulesGroupOffer =
                outputDirectory / "rules-group-choice-offer.ppm";
            const auto rulesGroupArmed =
                outputDirectory / "rules-group-choice-armed.ppm";
            const auto rulesGroupCommitted =
                outputDirectory / "rules-group-choice-committed.ppm";
            const auto rulesFullTableOffer =
                outputDirectory / "rules-full-table-bih-offer.ppm";
            const auto rulesFullTablePreview =
                outputDirectory / "rules-full-table-bih-preview.ppm";
            const auto rulesFullTableArmed =
                outputDirectory / "rules-full-table-bih-armed.ppm";
            const auto rulesFullTableCommitted =
                outputDirectory / "rules-full-table-bih-committed.ppm";
            expect(output.str().find("visible_rules_06c actions=3") != std::string::npos &&
                output.str().find("focus_cancel=gesture,armed") != std::string::npos &&
                output.str().find(
                    "local_actor_fixture=ai_disabled_after_product_start") !=
                    std::string::npos &&
                output.str().find("gpu_submitted=offer,armed,committed") !=
                    std::string::npos &&
                std::filesystem::exists(rulesOffer) &&
                std::filesystem::file_size(rulesOffer) > 64 &&
                std::filesystem::exists(rulesArmed) &&
                std::filesystem::file_size(rulesArmed) > 64 &&
                std::filesystem::exists(rulesCommitted) &&
                std::filesystem::file_size(rulesCommitted) > 64,
                "visible product run must capture real 06C offer, arm, and committed frames");
            const auto verify06CRerack = [&](const std::string& actionId,
                                              const std::string& suffix,
                                              int expectedBreaker,
                                              int expectedActor) {
                const auto rerackDirectory = outputDirectory /
                    ("rules-06c-" + suffix);
                std::filesystem::create_directories(rerackDirectory);
                auto rerackOptions = options;
                rerackOptions.productSettingsPath =
                    (rerackDirectory / "settings.json").string();
                const std::string rerackDiagnostics =
                    (rerackDirectory / "playtest-diagnostics").string();
                setenv("BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST", "1", 1);
                setenv("BILLIARDGL_VISIBLE_CAPTURE_DIR",
                    rerackDirectory.string().c_str(), 1);
                setenv("BILLIARDGL_PLAYTEST_LOG_DIR",
                    rerackDiagnostics.c_str(), 1);
                setenv("BILLIARDGL_VISIBLE_RULES_06C_ACTION",
                    actionId.c_str(), 1);
                std::istringstream rerackInput;
                std::ostringstream rerackOutput, rerackErrors;
                const int rerackResult = runModernApplication(rerackOptions,
                    rerackInput, rerackOutput, rerackErrors);
                unsetenv("BILLIARDGL_VISIBLE_RULES_06C_ACTION");
                unsetenv("BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST");
                unsetenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
                unsetenv("BILLIARDGL_PLAYTEST_LOG_DIR");
                const auto offer = rerackDirectory /
                    "rules-06c-offer.ppm";
                const auto armed = rerackDirectory /
                    "rules-06c-armed.ppm";
                const auto committed = rerackDirectory /
                    "rules-06c-committed.ppm";
                bool hasRawJournal = false;
                bool hasExactAckJournal = false;
                if (std::filesystem::exists(rerackDiagnostics)) {
                    for (const auto& item : std::filesystem::
                            recursive_directory_iterator(rerackDiagnostics)) {
                        if (item.is_regular_file() &&
                            item.path().extension() == ".jsonl" &&
                            item.file_size() > 0) {
                            hasRawJournal = true;
                            std::ifstream journal(item.path());
                            std::string line;
                            while (std::getline(journal, line)) {
                                const auto parsed = billiardgl::json::parse(line);
                                if (!parsed.ok ||
                                    parsed.value.at("kind").asString() !=
                                        "rules_decision_ack")
                                    continue;
                                const auto& ack = parsed.value.at("details");
                                const std::uint64_t before = std::stoull(
                                    ack.at("revision_before").asString());
                                const std::uint64_t after = std::stoull(
                                    ack.at("revision_after").asString());
                                hasExactAckJournal =
                                    ack.at("action_id").asString() == actionId &&
                                    ack.at("actor_player_id").asInt() ==
                                        expectedActor &&
                                    after == before + 1 &&
                                    ack.at("authority_state_hash").asString().size() ==
                                        64 &&
                                    ack.at("physical_collector").asString() ==
                                        "weak_break" &&
                                    ack.at("rack_rebuilt").asBool() &&
                                    ack.at("breaker_player_id").asInt() ==
                                        expectedBreaker &&
                                    ack.at("stale_confirm_error").asString() ==
                                        "input_unbound" &&
                                    ack.at("gpu_submitted").asString() ==
                                        "offer,armed,committed";
                            }
                        }
                    }
                }
                const std::string receipt = rerackOutput.str();
                const auto receiptPath = rerackDirectory / "receipt.log";
                {
                    std::ofstream retainedReceipt(receiptPath,
                        std::ios::binary | std::ios::trunc);
                    retainedReceipt << receipt;
                    retainedReceipt.flush();
                    expect(retainedReceipt.good(),
                        "visible 06C rerack stdout receipt is retained (" +
                            suffix + ")");
                }
                expect(rerackResult == 0 &&
                    receipt.find("visible_rules_06c actions=3") !=
                        std::string::npos &&
                    receipt.find("action=" + actionId) !=
                        std::string::npos &&
                    receipt.find("physical_collector=weak_break") !=
                        std::string::npos &&
                    receipt.find("rack_rebuilt=1") != std::string::npos &&
                    receipt.find("breaker=" +
                        std::to_string(expectedBreaker)) != std::string::npos &&
                    receipt.find("focus_cancel=gesture,armed") !=
                        std::string::npos &&
                    receipt.find("gpu_submitted=offer,armed,committed") !=
                        std::string::npos &&
                    std::filesystem::exists(offer) &&
                    std::filesystem::file_size(offer) > 64 &&
                    std::filesystem::exists(armed) &&
                    std::filesystem::file_size(armed) > 64 &&
                    std::filesystem::exists(committed) &&
                    std::filesystem::file_size(committed) > 64 &&
                    std::filesystem::exists(receiptPath) &&
                    std::filesystem::file_size(receiptPath) == receipt.size() &&
                    hasRawJournal && hasExactAckJournal,
                    "visible real-shot 06C rerack failed (" + suffix +
                        "): " + rerackErrors.str());
            };
            verify06CRerack(
                billiardgl::rules::kBreakFoulRerackOpponentActionId,
                "rerack-opponent", 1, 1);
            verify06CRerack(
                billiardgl::rules::kBreakFoulRerackOriginalActionId,
                "rerack-original", 0, 1);
            expect(output.str().find("visible_rules_06e actions=3") !=
                    std::string::npos &&
                output.str().find("physical_collector=cue_scratch") !=
                    std::string::npos &&
                output.str().find(
                    "fixture_setup=cue_inside_side_mouth") !=
                    std::string::npos &&
                output.str().find("projection_pan=nonzero") !=
                    std::string::npos &&
                output.str().find(
                    "modal_blocked=1 preview_cancelled=1") !=
                    std::string::npos &&
                output.str().find("authority_commits=2") !=
                    std::string::npos &&
                std::filesystem::exists(rules06EParentOffer) &&
                std::filesystem::file_size(rules06EParentOffer) > 64 &&
                std::filesystem::exists(rules06EParentArmed) &&
                std::filesystem::file_size(rules06EParentArmed) > 64 &&
                std::filesystem::exists(rules06EPreview) &&
                std::filesystem::file_size(rules06EPreview) > 64 &&
                std::filesystem::exists(rules06EArmed) &&
                std::filesystem::file_size(rules06EArmed) > 64 &&
                std::filesystem::exists(rules06ECommitted) &&
                std::filesystem::file_size(rules06ECommitted) > 64,
                "visible product run must capture physical 06E parent and BIH pointer/confirm frames");
            expect(output.str().find("visible_rules_06f actions=3") !=
                    std::string::npos &&
                output.str().find(
                    "physical_collector=cue_scratch+eight_capture") !=
                    std::string::npos &&
                output.str().find(
                    "fixture_setup=opposite_side_mouths") !=
                    std::string::npos &&
                output.str().find("authority_commits=2") !=
                    std::string::npos &&
                std::filesystem::exists(rules06FParentOffer) &&
                std::filesystem::file_size(rules06FParentOffer) > 64 &&
                std::filesystem::exists(rules06FParentArmed) &&
                std::filesystem::file_size(rules06FParentArmed) > 64 &&
                std::filesystem::exists(rules06FPreview) &&
                std::filesystem::file_size(rules06FPreview) > 64 &&
                std::filesystem::exists(rules06FArmed) &&
                std::filesystem::file_size(rules06FArmed) > 64 &&
                std::filesystem::exists(rules06FCommitted) &&
                std::filesystem::file_size(rules06FCommitted) > 64,
                "visible product run must capture physical 06F parent and its own BIH pointer/confirm frames");
            expect(output.str().find(
                    "visible_rules_group_choice actions=2") !=
                    std::string::npos &&
                output.str().find(
                    "fixture=simultaneous-contact-rule-facts") !=
                    std::string::npos &&
                output.str().find("cocoa_arm_confirm=1") !=
                    std::string::npos &&
                std::filesystem::exists(rulesGroupOffer) &&
                std::filesystem::file_size(rulesGroupOffer) > 64 &&
                std::filesystem::exists(rulesGroupArmed) &&
                std::filesystem::file_size(rulesGroupArmed) > 64 &&
                std::filesystem::exists(rulesGroupCommitted) &&
                std::filesystem::file_size(rulesGroupCommitted) > 64,
                "visible product run must arm and confirm the exact group-choice offer");
            expect(output.str().find(
                    "visible_rules_full_table_bih decision=") !=
                    std::string::npos &&
                output.str().find(
                    "physical_collector=ordinary_cue_scratch") !=
                    std::string::npos &&
                output.str().find(
                    "pointer_preview=actual_camera") !=
                    std::string::npos &&
                std::filesystem::exists(rulesFullTableOffer) &&
                std::filesystem::file_size(rulesFullTableOffer) > 64 &&
                std::filesystem::exists(rulesFullTablePreview) &&
                std::filesystem::file_size(rulesFullTablePreview) > 64 &&
                std::filesystem::exists(rulesFullTableArmed) &&
                std::filesystem::file_size(rulesFullTableArmed) > 64 &&
                std::filesystem::exists(rulesFullTableCommitted) &&
                std::filesystem::file_size(rulesFullTableCommitted) > 64,
                "visible product run must use a physical ordinary foul and actual-camera FullTable pointer/confirm");
            expect(errors.str().find("Playtest diagnostics:") != std::string::npos &&
                errors.str().find("WARNING: playtest") == std::string::npos,
                "visible product run must enable writable diagnostic logging");
            {
                const std::string prefix = "Playtest diagnostics: ";
                const std::string emitted = errors.str();
                const auto begin = emitted.find(prefix) + prefix.size();
                const auto end = emitted.find('\n', begin);
                const auto sessionPath = emitted.substr(begin, end - begin);
                int frames = 0, cadenceFrames = 0;
                for (const auto& entry : std::filesystem::directory_iterator(sessionPath)) {
                    if (entry.path().extension() != ".jsonl") continue;
                    std::ifstream stream(entry.path());
                    std::string line;
                    while (std::getline(stream, line)) {
                        const auto parsed = billiardgl::json::parse(line);
                        expect(parsed.ok, "native timing evidence must be valid JSON");
                        if (parsed.value.at("kind").asString() != "swap_submitted") continue;
                        const auto& timing = parsed.value.at("details");
                        expect(timing.at("frame_timing_valid").asBool() &&
                            timing.at("timing_scope").asString() ==
                                "event_loop_through_swap_completion" &&
                            timing.at("includes_simulation").asBool() &&
                            timing.at("includes_display_swap").asBool() &&
                            !timing.at("measures_scanout").asBool(),
                            "native timing must cover the loop and swap without claiming scanout");
                        const double loop = timing.at("loop_through_swap_ms").asNumber();
                        const double swap = timing.at("swap_ms").asNumber();
                        expect(timing.at("drawable_width_px").asInt() > 0 &&
                            timing.at("drawable_height_px").asInt() > 0 &&
                            timing.at("render_quality_enum").asInt() ==
                                static_cast<int>(options.quality) &&
                            timing.at("requested_msaa_samples").asInt() == options.msaaSamples,
                            "native timing binds actual drawable dimensions and requested renderer settings");
                        expect(swap >= 0 && loop >= swap,
                            "full loop duration includes the nonnegative swap duration");
                        if (timing.has("swap_to_swap_ms")) {
                            expect(timing.at("swap_to_swap_ms").asNumber() >= loop,
                                "cadence includes current loop and previous post-swap work");
                            ++cadenceFrames;
                        }
                        ++frames;
                    }
                }
                expect(frames > 1 && cadenceFrames > 0,
                    "current native session produces real full-loop and cadence samples");
            }
            {
                billiardgl::audio::SdlMixerAudioCueConsumer consumer;
                const auto prepared = consumer.prepare();
                expect(prepared.result.ok && consumer.status().deviceInitialized &&
                    consumer.status().cachedAssetCount == 4 && consumer.status().queuedCueCount == 0,
                    "audio warmup must cache four distinct assets without playing sounds");
                const auto repeated = consumer.prepare();
                expect(repeated.result.ok && consumer.status().cachedAssetCount == 4,
                    "audio warmup is idempotent");
                consumer.updateBus(billiardgl::audio::AudioBus::Effects, {0.0, true});
                billiardgl::audio::MixedAudioCue impact;
                impact.cue.kind = billiardgl::audio::AudioCueKind::BallBall;
                impact.cue.bus = billiardgl::audio::AudioBus::Effects;
                impact.outputGain = 0.0; // Exercise playback silently.
                const auto start = std::chrono::steady_clock::now();
                const auto played = consumer.consume({impact});
                const double collisionMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
                expect(played.ok && played.queuedCount == 1 &&
                    consumer.status().cachedAssetCount == 4,
                    "first collision uses the prepared cache");
                std::cout << "audio prepare device_ms=" << prepared.deviceMilliseconds
                    << " assets_ms=" << prepared.assetMilliseconds
                    << " cached_first_collision_ms=" << collisionMs << '\n';
            }
            {
                const char* previousDriver = std::getenv("SDL_AUDIODRIVER");
                const std::string savedDriver = previousDriver ? previousDriver : "";
                const bool hadDriver = previousDriver != nullptr;
                setenv("SDL_AUDIODRIVER", "billiardgl-nonexistent-test-driver", 1);
                billiardgl::audio::SdlMixerAudioCueConsumer unavailable;
                const auto firstFailure = unavailable.prepare();
                const auto repeatedFailure = unavailable.prepare();
                if (hadDriver) setenv("SDL_AUDIODRIVER", savedDriver.c_str(), 1);
                else unsetenv("SDL_AUDIODRIVER");
                expect(!firstFailure.result.ok && !repeatedFailure.result.ok &&
                    !unavailable.status().deviceInitialized &&
                    firstFailure.result.code == repeatedFailure.result.code,
                    "unavailable audio device fails stably without throwing or retrying on every cue");
            }
            std::cout << "modern-visible-drawable-e2e: pass\n";
            return 0;
        }
        productLifecycleEvidence(outputDirectory);
        expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0,
            "fake-consumer product E2E does not initialize SDL audio");
        {
            billiardgl::render::ModernApplicationOptions windowlessProduct;
            windowlessProduct.productMode = true;
            windowlessProduct.showWindow = false;
            std::istringstream noInput;
            std::ostringstream noOutput, rejectedErrors;
            expect(billiardgl::render::runModernApplication(windowlessProduct,
                noInput, noOutput, rejectedErrors) == 2 &&
                rejectedErrors.str().find("interactive visible window") !=
                    std::string::npos,
                "windowless product mode is rejected before Cocoa input or renderer setup");
            expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0,
                "rejected windowless product mode never initializes audio");
        }
        const auto aim = (outputDirectory / "modern-aim.ppm").string();
        const auto stopped = (outputDirectory / "modern-stopped.ppm").string();
        std::ostringstream script;
        script << "{\"id\":1,\"version\":1,\"command\":\"toggle_aim\",\"params\":{}}\n"
               << "{\"id\":2,\"version\":1,\"command\":\"set_aim_yaw\",\"params\":{\"yaw\":1.5707963}}\n"
               << "{\"id\":3,\"version\":1,\"command\":\"set_shot_power\",\"params\":{\"power\":120}}\n"
               << "{\"id\":4,\"version\":1,\"command\":\"screenshot\",\"params\":{\"path\":\"" << aim << "\"}}\n"
               << "{\"id\":5,\"version\":1,\"command\":\"shoot\",\"params\":{}}\n"
               << "{\"id\":6,\"version\":1,\"command\":\"run_until\",\"params\":{\"condition\":\"balls_stopped\",\"max_steps\":100000}}\n"
               << "{\"id\":7,\"version\":1,\"command\":\"screenshot\",\"params\":{\"path\":\"" << stopped << "\"}}\n"
               << "{\"id\":8,\"version\":1,\"command\":\"get_state\",\"params\":{}}\n"
               << "{\"id\":9,\"version\":1,\"command\":\"collect_render_performance_v1\",\"params\":{\"warmup_frames\":120,\"sample_frames\":300}}\n"
               << "{\"id\":10,\"version\":1,\"command\":\"quit\",\"params\":{}}\n";
        std::istringstream input(script.str()); std::ostringstream output, errors;
        billiardgl::render::ModernApplicationOptions options;
        options.width=1920;options.height=1080;options.msaaSamples=4;
        options.automation=true;options.showWindow=false;
        options.lookdevManifestPath=std::string(BILLIARDGL_SOURCE_ROOT)+"/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
        const std::string phase2=std::string(BILLIARDGL_SOURCE_ROOT)+"/assets/modern/phase2/";
        options.pbrParametersPath=phase2+"focused_practice_hall_pbr_v1.params.json";
        options.pbrAssetManifestPath=phase2+"focused_practice_hall_modular_v1.asset.json";
        options.lightingRigPath=phase2+"focused_practice_hall_lighting_rig_v1.json";
        options.gltfScenePath=phase2+"focused_practice_hall_modular_v1.gltf";
        options.venueScenePath=phase2+"focused_practice_hall_venue_v1.gltf";
        if (billiardgl::render::runModernApplication(options,input,output,errors)!=0) throw std::runtime_error(errors.str());
        expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0,
            "rendered automation never initializes the product audio device");
        const std::string protocol=output.str();
        expect(protocol.find("\"measurement_scope\":\"render_submission_and_flush_only\"") != std::string::npos &&
            protocol.find("\"includes_simulation\":false") != std::string::npos &&
            protocol.find("\"includes_display_swap\":false") != std::string::npos &&
            protocol.find("\"qualifies_as_product_frame_budget\":false") != std::string::npos,
            "automation render timings cannot be mistaken for a full product-frame budget");
        if(protocol.find("\"renderer_backend\":\"modern\"")==std::string::npos
            || protocol.find("\"balls_moving\":false")==std::string::npos
            || !std::filesystem::exists(aim)||!std::filesystem::exists(stopped))
            throw std::runtime_error("modern automation loop did not complete aim/shot/stop/capture:\n"+protocol);

        options.automation=false;options.showWindow=false;
        options.msaaSamples=4;
        options.quality=billiardgl::render::RenderQuality::High;
        const auto benchmark=billiardgl::render::runModernRendererBenchmark(options,120,600);
        if(benchmark.cpuPassMilliseconds.size()!=5||benchmark.gpuPassMilliseconds.size()!=5)
            throw std::runtime_error("600-frame async CPU/GPU pass benchmark incomplete");
        double highGpuP95=0.0;
        for(const auto& entry:benchmark.gpuPassMilliseconds)highGpuP95+=entry.second.p95;
        if(highGpuP95>=12.0)throw std::runtime_error("High quality GPU P95 exceeds 12ms budget");
        std::ostringstream reportText;
        reportText<<"warmup="<<benchmark.warmupFrames<<" measured="<<benchmark.measuredFrames<<'\n';
        reportText<<"high_gpu_total_p95="<<highGpuP95<<'\n';
        for(const auto& entry:benchmark.cpuPassMilliseconds)reportText<<"cpu "<<entry.first<<" p50="<<entry.second.p50<<" p95="<<entry.second.p95<<" p99="<<entry.second.p99<<'\n';
        for(const auto& entry:benchmark.gpuPassMilliseconds)reportText<<"gpu "<<entry.first<<" p50="<<entry.second.p50<<" p95="<<entry.second.p95<<" p99="<<entry.second.p99<<'\n';
        std::ofstream report(outputDirectory/"benchmark.txt",std::ios::binary);
        report<<reportText.str();
        std::cout<<reportText.str();
        std::cout<<"modern-application-e2e: pass\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
