#pragma once

#include "presentation_snapshot.h"
#include "deterministic_hash.h"
#include "product_hud.h"
#include "product_settings.h"
#include "product_shell_controller.h"
#include "render/ui/hud_shipping_tokens.h"
#include "render/ui/product_shell_draw_list.h"
#include "render/ui/product_shell_layout.h"
#include "render/ui/rules_decision_product_draw.h"
#include "render/modern/trajectory_inset_visible_integration.h"
#include "training/trajectory_inset_renderer.h"

#include <cstdint>
#include <functional>
#include <sstream>
#include <locale>
#include <string>
#include <utility>
#include <vector>

namespace billiardgl::render {
struct ModernProductSessionSnapshot;

namespace ui {

enum class ProductUiOverlayKind { None, Shell, GameplayHud, RulesDecision };

struct ProductUiFrameEnvironment {
    bool productMode = false;
    bool automation = false;
    bool windowless = false;
    // Internal evidence route only. The shipping application leaves this
    // false; it does not promote or product-expose the capability.
    bool rulesDecisionEvidenceMode = false;
};

struct ProductUiFrameRequest {
    ProductUiFrameEnvironment environment;
    ProductShellSnapshot shell;
    settings::ProductSettings settings = settings::ProductSettings::defaults();
    ShellViewport viewport;
    PresentationSnapshot presentation;
    bool rulesDecisionUiImplementationEnabled = false;
    bool rulesDecisionUiProductExposed = false;
    bool rulesDecisionGroupChoiceProductEnabled = false;
    bool rulesDecisionFullTableBallInHandProductEnabled = false;
    bool rulesDecisionIllegalBreakProductEnabled = false;
    bool rulesDecisionBreakShotFoulProductEnabled = false;
    bool rulesDecisionEightOnBreakProductEnabled = false;
    bool rulesDecisionProductActive = false;
    RulesDecisionLocale rulesDecisionLocale = RulesDecisionLocale::En;
    rules::ui::DecisionUiSnapshot rulesDecision;
};

struct ProductUiFrame {
    bool ok = false;
    std::string errorCode;
    ProductUiOverlayKind overlay = ProductUiOverlayKind::None;
    ProductScreen screen = ProductScreen::MainMenu;
    HudPaletteMode palette = HudPaletteMode::Standard;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float dpi = 96.0f;
    HudDrawList draw;
    bool rulesDecisionActive = false;
    std::vector<RulesDecisionUiHitRect> rulesDecisionHitRects;
    std::vector<product_hud::HudRect> rulesDecisionExclusionRects;
    std::vector<std::uint8_t> canonicalEvidence;
    std::string evidenceSha256;
};

struct TrajectoryInsetVisibleCurrentContext {
    bool present = false;
    std::uint64_t sessionSerial = 0;
    std::uint64_t frameSerial = 0;
    std::uint64_t layoutSerial = 0;
    bool sessionStable = false;
    bool transitioning = false;
    bool errored = false;
    bool modalVisible = false;
    bool insetToggleOn = false;
    bool worldOverlayToggleOn = false;
    bool insetControlFocused = false;
    int samples = 1;
    training::TrajectoryInsetPalette palette =
        training::TrajectoryInsetPalette::Standard;
    std::string cp1GenerationId;
    std::string sourceContentSha256;
};

inline std::string trajectoryInsetSourceContentSha256(const ProductShellSnapshot& shell)
{
    std::ostringstream identity;
    identity.imbue(std::locale::classic());
    identity << std::hexfloat << "product_inset_source_v1|"
        << static_cast<int>(shell.screen) << '|' << shell.training.scenarioFileSha256
        << '|' << shell.replay.tick << '|' << shell.replay.rackIdentity;
    const auto appendTrack = [&](const training::OverlayTrack& track) {
        identity << '|' << track.visible << '|' << track.endpointVisible << '|'
            << track.endpoint.x << '|' << track.endpoint.y << '|' << track.endpoint.z
            << '|' << track.railCollisionEvents << '|' << track.samples.size();
        for (const auto& sample : track.samples)
            identity << '|' << sample.tick << '|' << sample.visible << '|'
                << sample.position.x << '|' << sample.position.y << '|'
                << sample.position.z;
    };
    appendTrack(shell.trajectoryOverlay.predicted);
    appendTrack(shell.trajectoryOverlay.actual);
    return sha256Hex(identity.str());
}

struct TrajectoryInsetVisibleCurrentFrame {
    bool ok = false;
    bool consumed = false;
    bool executing = false;
    std::string errorCode;
    TrajectoryInsetShellRequest shellRequest;
    TrajectoryInsetShellLayout layout;
    TrajectoryInsetChromeDrawList chrome;
    training::TrajectoryInsetDrawPackage package;
    modern::TrajectoryInsetVisibleFrameRequest request;

    TrajectoryInsetVisibleCurrentFrame() = default;
    TrajectoryInsetVisibleCurrentFrame(
        const TrajectoryInsetVisibleCurrentFrame& other);
    TrajectoryInsetVisibleCurrentFrame& operator=(
        const TrajectoryInsetVisibleCurrentFrame& other);
    TrajectoryInsetVisibleCurrentFrame(
        TrajectoryInsetVisibleCurrentFrame&& other) noexcept;
    TrajectoryInsetVisibleCurrentFrame& operator=(
        TrajectoryInsetVisibleCurrentFrame&& other) noexcept;
    void rebind() noexcept;
};

struct TrajectoryInsetVisibleProductCallbacks {
    std::function<modern::TrajectoryInsetVisibleResult(
        const modern::TrajectoryInsetVisibleFrameRequest&)>
        executeIntegration;
    std::function<bool(const TrajectoryInsetShellLayout&,
        const TrajectoryInsetChromeDrawList&, std::string&)>
        submitChrome;
    std::function<void()> commitVisible;
    std::function<void()> invalidate;
};

TrajectoryInsetVisibleCurrentFrame makeTrajectoryInsetVisibleCurrentFrame(
    const ModernProductSessionSnapshot& session,
    const PresentationSnapshot& presentation,
    const ProductUiFrameEnvironment& environment,
    const TrajectoryInsetVisibleCurrentContext& context,
    const ProductUiFrame* preparedFrame = nullptr);

// Canonical binary-field geometry/layout identities shared by the product
// coordinator and the native CP3 consumer. Ordered CP1 ledgers remain separate.
std::string canonicalTrajectoryInsetGeometryDigest(
    const training::TrajectoryInsetDrawPackage& package);
std::string canonicalTrajectoryInsetLayoutDigest(
    const TrajectoryInsetShellLayout& layout);

modern::TrajectoryInsetVisibleResult executeTrajectoryInsetVisibleCurrentFrame(
    TrajectoryInsetVisibleCurrentFrame& current,
    const TrajectoryInsetVisibleProductCallbacks& callbacks);

ProductUiFrameRequest makeProductUiFrameRequest(
    const ModernProductSessionSnapshot& session,
    const PresentationSnapshot& presentation,
    const ProductUiFrameEnvironment& environment);

// Adds the projected/dilated trajectory protection used by the bounded
// Training/Replay inspector resolver. A failed or empty projection leaves the
// viewport unprotected so the layout can fail independently on its other hard
// bounds rather than inventing screen geometry.
ShellViewport resolveProtectedShellViewport(
    const ProductShellSnapshot& shell,
    const PresentationSnapshot& presentation,
    const ShellViewport& viewport);

class ProductUiFrameCoordinator {
public:
    ProductUiFrameCoordinator(product_hud::ProductHudManifest manifest,
        const SdfFontAtlas& atlas,
        const HudShippingTokenRegistry& tokenRegistry)
        : hudManifest_(std::move(manifest)), atlas_(atlas),
          tokenRegistry_(tokenRegistry) {}

    ProductUiFrame build(const ProductUiFrameRequest& request) const;

private:
    product_hud::ProductHudManifest hudManifest_;
    const SdfFontAtlas& atlas_;
    const HudShippingTokenRegistry& tokenRegistry_;
};

}  // namespace ui
}  // namespace billiardgl::render
