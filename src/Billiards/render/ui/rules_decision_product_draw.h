#pragma once

#include "render/ui/hud_draw_list.h"
#include "render/ui/hud_shipping_tokens.h"
#include "render/ui/product_shell_layout.h"
#include "render/ui/rules_decision_ui_draw_list.h"
#include "rules/rules_decision_ui_model.h"

#include <string>
#include <vector>

namespace billiardgl::render::ui {

struct RulesDecisionUiHitRect {
    std::string controlId;
    std::string actionId;
    product_hud::HudRect physicalRect;
    bool enabled = false;
    bool confirm = false;
};

struct RulesDecisionProductDraw {
    bool ok = false;
    bool active = false;
    std::string errorCode;
    HudDrawList draw;
    std::vector<RulesDecisionUiHitRect> hitRects;
    std::vector<product_hud::HudRect> exclusionRects;
};

RulesDecisionProductDraw buildRulesDecisionProductDraw(
    const rules::ui::DecisionUiSnapshot& snapshot,
    const CameraPresentation& camera,
    RulesDecisionLocale locale,
    HudPaletteMode palette,
    const ShellViewport& viewport,
    float uiScale,
    const SdfFontAtlas& atlas,
    bool englishOnlyFont = false);

}  // namespace billiardgl::render::ui
