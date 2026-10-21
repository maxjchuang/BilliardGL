#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace billiardgl::render::ui {

inline constexpr const char* kRulesUiFocusChevronV4Token =
    "rules_ui.focus_chevron_v4";
inline constexpr const char* kRulesUiSelectedCheckDiamondV4Token =
    "rules_ui.selected_check_diamond_v4";
inline constexpr const char* kRulesUiConfirmDoubleFrameEnterArrowV4Token =
    "rules_ui.confirm_double_frame_plus_enter_arrow_v4";

enum class RulesDecisionLocale { En, ZhCn };
enum class RulesDecisionPalette { Standard, ColorVisionSafe };

struct RulesDecisionPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct RulesDecisionDisplayProfile {
    int logicalWidth = 1280;
    int logicalHeight = 720;
    int physicalWidth = 1280;
    int physicalHeight = 720;
    float density = 1.0f;
    float uiScale = 1.0f;
    std::array<RulesDecisionPoint, 4> projectedTableCorners{};
    bool projectedTableAvailable = false;
};

struct RulesDecisionTextMetrics {
    bool ok = false;
    float width = 0.0f;
    float height = 0.0f;
};

using RulesDecisionTextMeasure = std::function<RulesDecisionTextMetrics(
    const std::string&, float)>;

struct RulesDecisionRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class RulesDecisionDrawKind {
    ModalPanel,
    PlacementRibbon,
    Text,
    Action,
    Confirm,
    FocusChevron,
    ArmedDiamond,
    ConfirmDoubleFrameEnterArrow,
    HeadStringBoundary,
    LegalRegionHatch,
    PreviewGhostBall,
    PreviewCrosshair,
    InvalidOctagon,
    InvalidCollisionRing,
    BlockingShape,
};

struct RulesDecisionDrawItem {
    std::string id;
    RulesDecisionDrawKind kind = RulesDecisionDrawKind::Text;
    RulesDecisionRect bounds;
    std::string text;
    std::string actionId;
    float fontLogicalPx = 0.0f;
    int lineCount = 0;
    bool enabled = true;
    bool focus = false;
    bool armed = false;
};

struct RulesDecisionCanonicalFixture {
    std::string stateId;
    std::vector<std::string> offeredActionIds;
    std::string armedActionId;
    std::string focusedActionId;
    std::string flowId;
    std::string branchId;
    std::string sourceClause;
    int actorPlayerId = 0;
    double previewWorldX = 0.0;
    double previewWorldZ = -36.0;
    bool placement = false;
    bool placementValid = false;
    std::string placementErrorCode;
    bool shotInputBlocked = true;
};

struct RulesDecisionDrawList {
    std::string stateId;
    RulesDecisionLocale locale = RulesDecisionLocale::En;
    RulesDecisionPalette palette = RulesDecisionPalette::Standard;
    RulesDecisionDisplayProfile profile;
    std::vector<RulesDecisionDrawItem> items;
    std::vector<std::string> offeredActionIds;
    int actorPlayerId = -1;
    double previewWorldX = 0.0;
    double previewWorldZ = 0.0;
    std::string armedActionId;
    std::string selectedActionId;
    bool shotInputBlocked = true;
    bool localPreviewAuthorityInvariant = true;
    unsigned localAuthorityMutationCount = 0;
    float tableOcclusionFraction = 0.0f;
    bool layoutBuildOk = false;
    std::string layoutError;
};

struct RulesDecisionDrawValidation {
    bool ok = false;
    std::vector<std::string> errors;
};

const std::vector<std::string>& rulesDecisionCanonicalStateIds();
RulesDecisionCanonicalFixture rulesDecisionCanonicalFixture(
    const std::string& stateId);
std::vector<std::string> allRulesDecisionUiStrings();

RulesDecisionDrawList buildRulesDecisionDrawList(
    const RulesDecisionCanonicalFixture& fixture,
    RulesDecisionLocale locale,
    RulesDecisionPalette palette,
    const RulesDecisionDisplayProfile& profile,
    const RulesDecisionTextMeasure& measure);

RulesDecisionDrawValidation validateRulesDecisionDrawList(
    const RulesDecisionDrawList& drawList);

}  // namespace billiardgl::render::ui
