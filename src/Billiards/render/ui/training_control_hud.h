#pragma once

#include "presentation_snapshot.h"
#include "render/ui/hud_draw_list.h"
#include "render/ui/product_shell_layout.h"

#include <string>

namespace billiardgl::render::ui {

enum class TrainingControlMode {
    Observe,
    Aim,
    Motion,
};

struct TrainingControlHudResult {
    bool ok = false;
    std::string errorCode;
    HudDrawList draw;
    TrainingControlMode mode = TrainingControlMode::Observe;
    bool powerAdjustmentLocked = true;
    bool tipAdjustmentLocked = true;
    product_hud::HudRect panelRect;
    float displayedPowerPercent = 0.0f;
    float displayedTipX = 0.0f;
    float displayedTipY = 0.0f;
};

// Builds the persistent, non-interactive Training control readout. The
// available rectangle is the gameplay corridor produced by the authoritative
// shell layout, so this strip never competes with the inspector toggle or its
// result/retry content.
TrainingControlHudResult buildTrainingControlHud(
    const PresentationSnapshot& presentation,
    const training::TrainingProductSnapshot& training,
    const ShellViewport& viewport,
    const product_hud::HudRect& safeRect,
    const product_hud::HudRect& availableRect,
    float userScale,
    const SdfFontAtlas& atlas);

const char* trainingControlModeName(TrainingControlMode mode) noexcept;

}  // namespace billiardgl::render::ui
