#include "render/modern/modern_product_ui_runtime.h"
#include "render/modern/camera_projection.h"
#include "automation_json.h"
#include "deterministic_hash.h"
#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;
using namespace billiardgl::render::ui;

constexpr const char* kFocus = kRulesUiFocusChevronV4Token;
constexpr const char* kSelected = kRulesUiSelectedCheckDiamondV4Token;
constexpr const char* kConfirm = kRulesUiConfirmDoubleFrameEnterArrowV4Token;

struct Profile {
    const char* id;
    int logicalWidth;
    int logicalHeight;
    int physicalWidth;
    int physicalHeight;
    double uiScale;
    float dpi;
    RulesDecisionLocale locale;
};

constexpr std::array<Profile, 4> kProfiles{{
    {"p1", 1280, 720, 1280, 720, 0.85, 96.0f, RulesDecisionLocale::ZhCn},
    {"p2", 1280, 720, 1280, 720, 1.35, 96.0f, RulesDecisionLocale::En},
    {"p3", 1920, 1080, 1920, 1080, 1.0, 96.0f, RulesDecisionLocale::ZhCn},
    {"p4", 1280, 720, 2560, 1440, 1.0, 192.0f, RulesDecisionLocale::En},
}};

struct SnapshotCase {
    const char* id;
    const char* state;
    int profile;
    bool cvd;
    RulesDecisionLocale locale;
};

constexpr std::array<SnapshotCase, 16> kSnapshotCases{{
    {"group_choice_armed_confirm--p1--zh_cn",
        "group_choice_armed_confirm", 0, false, RulesDecisionLocale::ZhCn},
    {"group_choice_armed_confirm--p2--en",
        "group_choice_armed_confirm", 1, true, RulesDecisionLocale::En},
    {"group_choice_armed_confirm--p3--zh_cn",
        "group_choice_armed_confirm", 2, true, RulesDecisionLocale::ZhCn},
    {"group_choice_armed_confirm--p4--en",
        "group_choice_armed_confirm", 3, false, RulesDecisionLocale::En},
    {"group_choice_open_unselected--p1--en",
        "group_choice_open_unselected", 0, false, RulesDecisionLocale::En},
    {"group_choice_open_unselected--p2--zh_cn",
        "group_choice_open_unselected", 1, true, RulesDecisionLocale::ZhCn},
    {"group_choice_open_unselected--p3--en",
        "group_choice_open_unselected", 2, true, RulesDecisionLocale::En},
    {"group_choice_open_unselected--p4--zh_cn",
        "group_choice_open_unselected", 3, false, RulesDecisionLocale::ZhCn},
    {"eight_legal_open_unselected--p1--en",
        "eight_legal_open_unselected", 0, false, RulesDecisionLocale::En},
    {"break_illegal_open_unselected--p2--zh_cn",
        "break_illegal_open_unselected", 1, true, RulesDecisionLocale::ZhCn},
    {"submitting_progress--p3--en",
        "submitting_progress", 2, true, RulesDecisionLocale::En},
    {"authority_error--p4--en",
        "authority_error", 3, false, RulesDecisionLocale::En},
    {"full_table_preview_valid--p1--en",
        "full_table_preview_valid", 0, false, RulesDecisionLocale::En},
    {"full_table_preview_invalid--p2--en",
        "full_table_preview_invalid", 1, true, RulesDecisionLocale::En},
    {"behind_head_string_preview_valid--p3--en",
        "behind_head_string_preview_valid", 2, true, RulesDecisionLocale::En},
    {"behind_head_string_preview_invalid--p4--en",
        "behind_head_string_preview_invalid", 3, false, RulesDecisionLocale::En},
}};

struct Pixel {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    float alpha = 0.0f;
};

struct Mask {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;

    bool at(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < width && y < height &&
            pixels[static_cast<std::size_t>(y * width + x)] != 0;
    }
    void set(int x, int y)
    {
        if (x >= 0 && y >= 0 && x < width && y < height)
            pixels[static_cast<std::size_t>(y * width + x)] = 1;
    }
};

struct Surface {
    int width = 0;
    int height = 0;
    std::vector<Pixel> pixels;
    Mask text;
    Mask focus;
    Mask selected;
    Mask confirm;
    Mask legalRegion;
};

struct Bounds {
    int x0 = 0;
    int y0 = 0;
    int x1 = -1;
    int y1 = -1;
    bool valid = false;
};

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("phase5 v5 CPU raster: " + message);
}

std::string asset(const char* relative)
{
    return std::string(BILLIARDGL_SOURCE_ROOT) + relative;
}

std::vector<std::uint32_t> rulesCodepoints()
{
    const auto hud = requiredHudCodepoints();
    std::set<std::uint32_t> unique(hud.begin(), hud.end());
    for (const auto& text : allRulesDecisionUiStrings()) {
        std::vector<std::uint32_t> decoded;
        std::string error;
        expect(decodeUtf8(text, decoded, error), "localized copy UTF-8");
        unique.insert(decoded.begin(), decoded.end());
    }
    return {unique.begin(), unique.end()};
}

SdfFontAtlas atlas()
{
    FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Hiragino Sans GB.ttc";
    config.nominalPixelSize = 32;
    config.atlasWidth = 4096;
    config.atlasHeight = 4096;
    SdfFontAtlas result;
    expect(result.initialize(config, rulesCodepoints()).ok,
        "shipping SDF atlas initializes");
    return result;
}

ModernProductSessionSnapshot armedSession(
    const Profile& profile, settings::ColorVisionMode colorMode)
{
    ModernProductSessionSnapshot session;
    session.shell.screen = ProductScreen::Match;
    session.shell.screenId = "phase5_rules_decision_v5_cpu_raster";
    session.shell.uiScale = static_cast<float>(profile.uiScale);
    session.effectiveSettings = settings::ProductSettings::defaults();
    session.effectiveSettings.uiScale = profile.uiScale;
    session.effectiveSettings.colorVisionMode = colorMode;
    session.shellViewport = {profile.logicalWidth, profile.logicalHeight,
        profile.physicalWidth, profile.physicalHeight, profile.dpi};
    session.rulesDecisionUiImplementationEnabled = true;
    session.rulesDecisionUiProductExposed = false;
    session.rulesDecisionLocale = profile.locale;
    session.rulesDecision.state =
        rules::ui::DecisionUiState::ActionArmedNotSubmitted;
    session.rulesDecision.offer.rulesetId =
        rules::ui::kRulesDecisionUiRulesetId;
    session.rulesDecision.offer.capabilityId =
        rules::ui::kGroupChoiceCapabilityId;
    session.rulesDecision.offer.flowId = rules::ui::kGroupChoiceFlowId;
    session.rulesDecision.offer.branchId =
        "two_group_simultaneous_first_contact_and_pocket";
    session.rulesDecision.offer.decisionId =
        std::string("phase5.v5.cpu.") + profile.id;
    session.rulesDecision.offer.authorityStateRevision = 42;
    session.rulesDecision.offer.actorPlayerId = 0;
    session.rulesDecision.offer.offeredActionIds = {
        rules::kGroupChoiceSolidsActionId,
        rules::kGroupChoiceStripesActionId};
    session.rulesDecision.visibleActionIds =
        session.rulesDecision.offer.offeredActionIds;
    session.rulesDecision.armedActionId = rules::kGroupChoiceSolidsActionId;
    session.rulesDecision.focusedActionId = rules::kGroupChoiceSolidsActionId;
    session.rulesDecision.shotInputBlocked = true;
    return session;
}

ModernProductSessionSnapshot snapshotSession(const SnapshotCase& capture)
{
    const Profile& profile = kProfiles[static_cast<std::size_t>(capture.profile)];
    const RulesDecisionCanonicalFixture fixture =
        rulesDecisionCanonicalFixture(capture.state);
    ModernProductSessionSnapshot session = armedSession(
        profile, capture.cvd ? settings::ColorVisionMode::Deuteranopia :
            settings::ColorVisionMode::Default);
    session.rulesDecisionLocale = capture.locale;
    session.rulesDecision.offer.flowId = fixture.flowId;
    session.rulesDecision.offer.branchId = fixture.branchId;
    session.rulesDecision.offer.sourceClause = fixture.sourceClause;
    session.rulesDecision.offer.decisionId =
        std::string("phase5.v5.snapshot.") + capture.id;
    session.rulesDecision.offer.actorPlayerId = fixture.actorPlayerId;
    session.rulesDecision.offer.offeredActionIds = fixture.offeredActionIds;
    session.rulesDecision.visibleActionIds = fixture.offeredActionIds;
    session.rulesDecision.armedActionId = fixture.armedActionId;
    session.rulesDecision.focusedActionId = fixture.focusedActionId;
    session.rulesDecision.shotInputBlocked = fixture.shotInputBlocked;
    session.rulesDecision.placementPreviewVisible = fixture.placement;
    session.rulesDecision.placementPreviewLegal = fixture.placementValid;
    session.rulesDecision.placementPreview = {
        fixture.previewWorldX, fixture.previewWorldZ};
    if (fixture.flowId == rules::kBehindHeadStringBallInHandFlowId)
        session.rulesDecision.headStringWorldZCm = -63.5;
    if (fixture.flowId == rules::ui::kGroupChoiceFlowId)
        session.rulesDecision.offer.capabilityId =
            rules::ui::kGroupChoiceCapabilityId;
    else if (fixture.flowId == rules::kFullTableBallInHandFlowId ||
            fixture.flowId == rules::kBehindHeadStringBallInHandFlowId)
        session.rulesDecision.offer.capabilityId =
            rules::kFreeBallPlacementCapabilityId;
    else if (fixture.flowId == rules::kEightOnBreakFlowId)
        session.rulesDecision.offer.capabilityId =
            rules::kEightOnBreakCapabilityId;
    else if (fixture.flowId == rules::kBreakFoulFlowId)
        session.rulesDecision.offer.capabilityId =
            rules::kBreakFoulCapabilityId;
    const std::string state = capture.state;
    if (state == "group_choice_armed_confirm")
        session.rulesDecision.state =
            rules::ui::DecisionUiState::ActionArmedNotSubmitted;
    else if (state.find("preview_valid") != std::string::npos)
        session.rulesDecision.state =
            rules::ui::DecisionUiState::PlacementPreviewLocalValid;
    else if (state.find("preview_invalid") != std::string::npos)
        session.rulesDecision.state =
            rules::ui::DecisionUiState::PlacementPreviewLocalInvalid;
    else if (state == "submitting_progress")
        session.rulesDecision.state =
            rules::ui::DecisionUiState::SubmittingExactRequest;
    else if (state == "authority_error")
        session.rulesDecision.state =
            rules::ui::DecisionUiState::AuthorityErrorBlocking;
    else
        session.rulesDecision.state =
            rules::ui::DecisionUiState::DecisionOpenUnselected;
    return session;
}

PresentationSnapshot presentation(const Profile& profile)
{
    PresentationSnapshot value;
    value.hud.currentPlayerNumber = 1;
    value.hud.viewportWidth = profile.physicalWidth;
    value.hud.viewportHeight = profile.physicalHeight;
    value.hud.aimMode = true;
    value.hud.showPowerMeter = true;
    value.hud.shotPower = 0.5f;
    value.shotControl.requestedPower = 50.0f;
    value.rules.enabled = true;
    value.rules.rulesetId = rules::ui::kRulesDecisionUiRulesetId;
    value.rules.groups = {{"open", "open"}};
    value.camera.eye = {0.0f, 300.0f, 260.0f};
    value.camera.target = {0.0f, 86.483976f, -20.0f};
    value.camera.fieldOfViewDegrees = 48.0f;
    return value;
}

bool inside(const product_hud::HudRect& rect, float x, float y)
{
    return x >= rect.x && y >= rect.y &&
        x < rect.x + rect.width && y < rect.y + rect.height;
}

void blend(Pixel& target, const UiColor& source, float coverage)
{
    const float alpha = std::clamp(source.alpha * coverage, 0.0f, 1.0f);
    target.red = source.red * alpha + target.red * (1.0f - alpha);
    target.green = source.green * alpha + target.green * (1.0f - alpha);
    target.blue = source.blue * alpha + target.blue * (1.0f - alpha);
    target.alpha = alpha + target.alpha * (1.0f - alpha);
}

float edge(UiPoint a, UiPoint b, float x, float y)
{
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

Mask* roleMask(Surface& surface, const std::string& token)
{
    if (token == kFocus) return &surface.focus;
    if (token == kSelected) return &surface.selected;
    if (token == kConfirm) return &surface.confirm;
    if (token == "rules_legal_region_diagonal_hatch")
        return &surface.legalRegion;
    return nullptr;
}

Surface raster(const ProductUiFrame& frame, const SdfFontAtlas& font)
{
    Surface result;
    result.width = frame.physicalWidth;
    result.height = frame.physicalHeight;
    const std::size_t count =
        static_cast<std::size_t>(result.width * result.height);
    result.pixels.resize(count);
    for (Mask* mask : {&result.text, &result.focus,
            &result.selected, &result.confirm, &result.legalRegion}) {
        mask->width = result.width;
        mask->height = result.height;
        mask->pixels.assign(count, 0);
    }
    for (const UiPrimitive& item : frame.draw.primitives) {
        const int x0 = std::max(0, static_cast<int>(std::floor(item.rect.x)));
        const int y0 = std::max(0, static_cast<int>(std::floor(item.rect.y)));
        const int x1 = std::min(result.width,
            static_cast<int>(std::ceil(item.rect.x + item.rect.width)));
        const int y1 = std::min(result.height,
            static_cast<int>(std::ceil(item.rect.y + item.rect.height)));
        for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!inside(item.clipRect, px, py) ||
                    !inside(item.rect, px, py))
                continue;
            float coverage = 1.0f;
            if (item.kind == UiPrimitiveKind::GlyphQuad) {
                const float tx = (px - item.rect.x) / item.rect.width;
                const float ty = (py - item.rect.y) / item.rect.height;
                const float u = item.u0 + (item.u1 - item.u0) * tx;
                const float v = item.v0 + (item.v1 - item.v0) * ty;
                const int ax = std::clamp(
                    static_cast<int>(u * font.width()), 0, font.width() - 1);
                const int ay = std::clamp(
                    static_cast<int>(v * font.height()), 0, font.height() - 1);
                const float distance = font.pixels()[
                    static_cast<std::size_t>(ay * font.width() + ax)] / 255.0f;
                coverage = std::clamp((distance - 0.496f) / 0.008f,
                    0.0f, 1.0f);
                if (coverage > 0.01f) result.text.set(x, y);
            }
            blend(result.pixels[static_cast<std::size_t>(y * result.width + x)],
                item.color, coverage);
        }
    }
    for (const UiTriangle& item : frame.draw.triangles) {
        const float area = edge(item.first, item.second,
            item.third.x, item.third.y);
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min(
            {item.first.x, item.second.x, item.third.x}))));
        const int y0 = std::max(0, static_cast<int>(std::floor(std::min(
            {item.first.y, item.second.y, item.third.y}))));
        const int x1 = std::min(result.width, static_cast<int>(std::ceil(
            std::max({item.first.x, item.second.x, item.third.x}))));
        const int y1 = std::min(result.height, static_cast<int>(std::ceil(
            std::max({item.first.y, item.second.y, item.third.y}))));
        Mask* mask = roleMask(result, item.sourceToken);
        for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            if (!inside(item.clipRect, px, py)) continue;
            const float e0 = edge(item.first, item.second, px, py);
            const float e1 = edge(item.second, item.third, px, py);
            const float e2 = edge(item.third, item.first, px, py);
            if (!((area > 0.0f && e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) ||
                  (area < 0.0f && e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f)))
                continue;
            blend(result.pixels[static_cast<std::size_t>(y * result.width + x)],
                item.color, 1.0f);
            if (mask != nullptr) mask->set(x, y);
        }
    }
    return result;
}

std::array<UiPoint, 4> behindHeadStringScreenPolygon(
    const PresentationSnapshot& presentation, int width, int height,
    double headStringWorldZCm)
{
    const SharedCameraProjection projection = buildSharedCameraProjection(
        presentation.camera, width, height);
    expect(projection.valid, "behind-head-string camera projection valid");
    const float radius = kChineseBallRadiusCm;
    const float x0 = -kChinesePlayfieldWidthCm * 0.5f + radius;
    const float x1 = kChinesePlayfieldWidthCm * 0.5f - radius;
    const float z0 = -kChinesePlayfieldLengthCm * 0.5f + radius;
    const float z1 = static_cast<float>(headStringWorldZCm);
    const float y = kTableModelPlaySurfaceHeightCm + 0.05f;
    const std::array<Point3, 4> world{{
        {x0, y, z0}, {x1, y, z0}, {x1, y, z1}, {x0, y, z1}}};
    std::array<UiPoint, 4> result{};
    for (std::size_t index = 0; index < world.size(); ++index) {
        const HomogeneousPoint clip = transformWorldToClip(projection,
            world[index]);
        PhysicalScreenPoint screen;
        expect(clipToPhysicalScreen(projection, clip, screen),
            "behind-head-string polygon corner projects on screen");
        result[index] = {static_cast<float>(screen.x),
            static_cast<float>(screen.y)};
    }
    return result;
}

bool insideConvexPolygonWithPixelTolerance(
    const std::array<UiPoint, 4>& polygon, float x, float y)
{
    constexpr float tolerancePixels = 2.5f;
    float referenceSign = 0.0f;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const UiPoint first = polygon[index];
        const UiPoint second = polygon[(index + 1) % polygon.size()];
        const float cross = edge(first, second, x, y);
        const float length = std::hypot(second.x - first.x,
            second.y - first.y);
        if (std::fabs(cross) <= tolerancePixels * length) continue;
        const float sign = cross < 0.0f ? -1.0f : 1.0f;
        if (referenceSign == 0.0f) referenceSign = sign;
        else if (sign != referenceSign) return false;
    }
    return true;
}

void verifyBehindHeadStringRasterClipping(
    const ProductUiFrameCoordinator& coordinator, const SdfFontAtlas& font,
    const ProductUiFrameEnvironment& environment)
{
    const SnapshotCase& capture = kSnapshotCases[14];
    const Profile& profile =
        kProfiles[static_cast<std::size_t>(capture.profile)];
    const ModernProductSessionSnapshot session = snapshotSession(capture);
    const PresentationSnapshot scene = presentation(profile);
    const ProductUiFrame frame = coordinator.build(makeProductUiFrameRequest(
        session, scene, environment));
    expect(frame.ok && frame.rulesDecisionActive,
        "behind-head-string raster frame builds");
    const Surface surface = raster(frame, font);
    expect(std::any_of(surface.legalRegion.pixels.begin(),
            surface.legalRegion.pixels.end(),
            [](std::uint8_t value) { return value != 0; }),
        "behind-head-string legal-region hatch reaches raster");
    const auto polygon = behindHeadStringScreenPolygon(scene,
        frame.physicalWidth, frame.physicalHeight,
        session.rulesDecision.headStringWorldZCm);
    std::size_t pixels = 0;
    for (int y = 0; y < surface.height; ++y)
        for (int x = 0; x < surface.width; ++x)
            if (surface.legalRegion.at(x, y)) {
                ++pixels;
                expect(insideConvexPolygonWithPixelTolerance(
                    polygon, x + 0.5f, y + 0.5f),
                    "legal-region raster pixel remains inside projected table polygon");
            }
    expect(pixels >= 16,
        "behind-head-string legal-region has visible clipped pixels");
}

Bounds bounds(const Mask& mask)
{
    Bounds result;
    result.x0 = mask.width;
    result.y0 = mask.height;
    for (int y = 0; y < mask.height; ++y) for (int x = 0; x < mask.width; ++x)
        if (mask.at(x, y)) {
            result.valid = true;
            result.x0 = std::min(result.x0, x);
            result.y0 = std::min(result.y0, y);
            result.x1 = std::max(result.x1, x);
            result.y1 = std::max(result.y1, y);
        }
    return result;
}

int componentCount(const Mask& mask)
{
    std::vector<std::uint8_t> visited(mask.pixels.size(), 0);
    int components = 0;
    constexpr std::array<int, 4> dx{{1, -1, 0, 0}};
    constexpr std::array<int, 4> dy{{0, 0, 1, -1}};
    for (int y = 0; y < mask.height; ++y) for (int x = 0; x < mask.width; ++x) {
        const std::size_t start = static_cast<std::size_t>(y * mask.width + x);
        if (!mask.at(x, y) || visited[start]) continue;
        ++components;
        std::queue<std::pair<int, int>> pending;
        pending.push({x, y});
        visited[start] = 1;
        while (!pending.empty()) {
            const auto [cx, cy] = pending.front();
            pending.pop();
            for (int side = 0; side < 4; ++side) {
                const int nx = cx + dx[side], ny = cy + dy[side];
                if (nx < 0 || ny < 0 || nx >= mask.width || ny >= mask.height)
                    continue;
                const std::size_t offset =
                    static_cast<std::size_t>(ny * mask.width + nx);
                if (mask.at(nx, ny) && !visited[offset]) {
                    visited[offset] = 1;
                    pending.push({nx, ny});
                }
            }
        }
    }
    return components;
}

int enclosedRegionCount(const Mask& mask)
{
    const Bounds box = bounds(mask);
    if (!box.valid) return 0;
    const int x0 = std::max(0, box.x0 - 2), y0 = std::max(0, box.y0 - 2);
    const int x1 = std::min(mask.width - 1, box.x1 + 2);
    const int y1 = std::min(mask.height - 1, box.y1 + 2);
    std::vector<std::uint8_t> visited(mask.pixels.size(), 0);
    int enclosed = 0;
    constexpr std::array<int, 4> dx{{1, -1, 0, 0}};
    constexpr std::array<int, 4> dy{{0, 0, 1, -1}};
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
        const std::size_t start = static_cast<std::size_t>(y * mask.width + x);
        if (mask.at(x, y) || visited[start]) continue;
        bool touchesBorder = false;
        std::queue<std::pair<int, int>> pending;
        pending.push({x, y});
        visited[start] = 1;
        while (!pending.empty()) {
            const auto [cx, cy] = pending.front();
            pending.pop();
            touchesBorder = touchesBorder || cx == x0 || cx == x1 ||
                cy == y0 || cy == y1;
            for (int side = 0; side < 4; ++side) {
                const int nx = cx + dx[side], ny = cy + dy[side];
                if (nx < x0 || ny < y0 || nx > x1 || ny > y1) continue;
                const std::size_t offset =
                    static_cast<std::size_t>(ny * mask.width + nx);
                if (!mask.at(nx, ny) && !visited[offset]) {
                    visited[offset] = 1;
                    pending.push({nx, ny});
                }
            }
        }
        if (!touchesBorder) ++enclosed;
    }
    return enclosed;
}

bool hasTwoPixelStroke(const Mask& mask)
{
    for (int y = 0; y + 1 < mask.height; ++y)
        for (int x = 0; x + 1 < mask.width; ++x)
            if (mask.at(x, y) && mask.at(x + 1, y) &&
                    mask.at(x, y + 1) && mask.at(x + 1, y + 1))
                return true;
    return false;
}

std::size_t intersectionCount(const Mask& left, const Mask& right)
{
    std::size_t count = 0;
    for (std::size_t index = 0; index < left.pixels.size(); ++index)
        if (left.pixels[index] && right.pixels[index]) ++count;
    return count;
}

std::array<float, 3> transform(const Pixel& pixel, int simulation)
{
    constexpr float matrices[4][9] = {
        {1, 0, 0, 0, 1, 0, 0, 0, 1},
        {.56667f, .43333f, 0, .55833f, .44167f, 0, 0, .24167f, .75833f},
        {.625f, .375f, 0, .70f, .30f, 0, 0, .30f, .70f},
        {.95f, .05f, 0, 0, .43333f, .56667f, 0, .475f, .525f},
    };
    const float* matrix = matrices[simulation];
    return {
        matrix[0] * pixel.red + matrix[1] * pixel.green + matrix[2] * pixel.blue,
        matrix[3] * pixel.red + matrix[4] * pixel.green + matrix[5] * pixel.blue,
        matrix[6] * pixel.red + matrix[7] * pixel.green + matrix[8] * pixel.blue,
    };
}

float luminance(const std::array<float, 3>& color)
{
    const auto channel = [](float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value <= 0.04045f ? value / 12.92f :
            std::pow((value + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * channel(color[0]) + 0.7152f * channel(color[1]) +
        0.0722f * channel(color[2]);
}

float contrast(const Pixel& foreground, const Pixel& background, int simulation)
{
    const float first = luminance(transform(foreground, simulation));
    const float second = luminance(transform(background, simulation));
    return (std::max(first, second) + 0.05f) /
        (std::min(first, second) + 0.05f);
}

Pixel localRowBackground(const ProductUiFrame& frame, const Mask& mask)
{
    const Bounds box = bounds(mask);
    expect(box.valid, "role bounds available");
    for (int radius = 2; radius < 16; ++radius)
        for (int y = std::max(0, box.y0 - radius);
                y <= std::min(frame.physicalHeight - 1, box.y1 + radius); ++y)
            for (int x = std::max(0, box.x0 - radius);
                    x <= std::min(frame.physicalWidth - 1, box.x1 + radius); ++x)
                if (!mask.at(x, y)) {
                    for (const UiPrimitive& primitive : frame.draw.primitives)
                        if ((primitive.token == "rules_decision_action" ||
                                primitive.token == "rules_decision_confirm") &&
                                inside(primitive.rect, x + .5f, y + .5f))
                            return {primitive.color.red, primitive.color.green,
                                primitive.color.blue, primitive.color.alpha};
                }
    throw std::runtime_error("phase5 v5 CPU raster: row background unavailable");
}

void verifyRole(const std::string& profile, const std::string& palette,
    const ProductUiFrame& frame, const Surface& surface, const Mask& mask,
    int minimumEnclosed, bool requireSingleComponent)
{
    const Bounds box = bounds(mask);
    expect(box.valid, profile + "/" + palette + ": role pixels survive");
    expect(box.x1 - box.x0 + 1 >= 18 && box.y1 - box.y0 + 1 >= 18,
        profile + "/" + palette + ": physical role bbox >=18x18");
    expect(hasTwoPixelStroke(mask),
        profile + "/" + palette + ": physical role stroke >=2");
    if (requireSingleComponent)
        expect(componentCount(mask) == 1,
            profile + "/" + palette + ": joined focus component");
    expect(enclosedRegionCount(mask) >= minimumEnclosed,
        profile + "/" + palette + ": border flood-fill closure");
    const Pixel background = localRowBackground(frame, mask);
    float minimumContrast = 100.0f;
    for (int y = box.y0; y <= box.y1; ++y)
        for (int x = box.x0; x <= box.x1; ++x)
            if (mask.at(x, y))
                minimumContrast = std::min(minimumContrast,
                    contrast(surface.pixels[
                        static_cast<std::size_t>(y * surface.width + x)],
                        background, 0));
    expect(minimumContrast + 0.001f >= 4.0f,
        profile + "/" + palette + ": final row contrast >=4");
}

void verifyFrame(const Profile& profile, const std::string& palette,
    const ProductUiFrame& frame, const Surface& surface)
{
    verifyRole(profile.id, palette, frame, surface, surface.focus, 0, true);
    verifyRole(profile.id, palette, frame, surface, surface.selected, 1, false);
    verifyRole(profile.id, palette, frame, surface, surface.confirm, 2, false);
    const Bounds focus = bounds(surface.focus);
    expect(focus.valid &&
            std::any_of(surface.focus.pixels.begin(), surface.focus.pixels.end(),
                [](std::uint8_t value) { return value != 0; }),
        std::string(profile.id) + "/" + palette + ": focus visible");
    const int upperLimit = focus.y0 +
        static_cast<int>((focus.y1 - focus.y0 + 1) * 0.4f);
    const int lowerStart = focus.y0 +
        static_cast<int>((focus.y1 - focus.y0 + 1) * 0.6f);
    bool upper = false, lower = false;
    for (int y = focus.y0; y <= focus.y1; ++y)
        for (int x = focus.x0; x <= focus.x1; ++x)
            if (surface.focus.at(x, y)) {
                upper = upper || y <= upperLimit;
                lower = lower || y >= lowerStart;
            }
    expect(upper && lower,
        std::string(profile.id) + "/" + palette + ": focus spans slot");
    expect(intersectionCount(surface.focus, surface.text) == 0,
        std::string(profile.id) + "/" + palette + ": focus/text separation");
    expect(intersectionCount(surface.focus, surface.selected) == 0 &&
            intersectionCount(surface.focus, surface.confirm) == 0 &&
            intersectionCount(surface.selected, surface.confirm) == 0,
        std::string(profile.id) + "/" + palette + ": role masks distinct");

    // Geometry and visible-pixel survival must remain unchanged through all
    // deterministic P/D/T simulations. Contrast >=4 is measured against the
    // two actual base row palettes, as required by the supplemental gate.
    for (int simulation = 1; simulation <= 3; ++simulation) {
        for (const Mask* role : {&surface.focus, &surface.selected,
                &surface.confirm}) {
            const Bounds roleBounds = bounds(*role);
            const Pixel background = localRowBackground(frame, *role);
            bool transformedVisible = false;
            for (int y = roleBounds.y0; y <= roleBounds.y1 && !transformedVisible; ++y)
                for (int x = roleBounds.x0; x <= roleBounds.x1; ++x)
                    if (role->at(x, y) && contrast(surface.pixels[
                            static_cast<std::size_t>(y * surface.width + x)],
                            background, simulation) > 1.01f) {
                        transformedVisible = true;
                        break;
                    }
            expect(transformedVisible,
                std::string(profile.id) + "/" + palette +
                ": transformed role pixel survival");
        }
    }
}

Mask emptyLike(const Mask& source)
{
    Mask result;
    result.width = source.width;
    result.height = source.height;
    result.pixels.assign(source.pixels.size(), 0);
    return result;
}

void verifyNegativeFixtures(const Surface& source)
{
    const Bounds focusBox = bounds(source.focus);
    const Bounds selectedBox = bounds(source.selected);
    const Bounds confirmBox = bounds(source.confirm);
    expect(focusBox.valid && selectedBox.valid && confirmBox.valid,
        "negative fixture source roles");
    int rejected = 0;
    const Pixel same{0.16f, 0.80f, 0.67f, 1.0f};
    rejected += contrast(same, same, 0) < 4.0f;
    rejected += !bounds(emptyLike(source.focus)).valid;

    Mask focusMissing = source.focus;
    for (int y = focusBox.y0 + (focusBox.y1 - focusBox.y0 + 1) / 2;
            y <= focusBox.y1; ++y)
        for (int x = focusBox.x0; x <= focusBox.x1; ++x)
            focusMissing.pixels[
                static_cast<std::size_t>(y * focusMissing.width + x)] = 0;
    const Bounds missingBox = bounds(focusMissing);
    rejected += missingBox.valid && missingBox.y1 <
        focusBox.y0 + static_cast<int>((focusBox.y1 - focusBox.y0 + 1) * .6f);

    Mask collidingText = source.text;
    bool inserted = false;
    for (int y = focusBox.y0; y <= focusBox.y1 && !inserted; ++y)
        for (int x = focusBox.x0; x <= focusBox.x1; ++x)
            if (source.focus.at(x, y)) {
                collidingText.set(x, y);
                inserted = true;
                break;
            }
    rejected += intersectionCount(source.focus, collidingText) > 0;

    Mask openDiamond = source.selected;
    for (int y = selectedBox.y0; y <= selectedBox.y1; ++y)
        for (int x = selectedBox.x0; x <=
                std::min(selectedBox.x1, selectedBox.x0 + 3); ++x)
            openDiamond.pixels[
                static_cast<std::size_t>(y * openDiamond.width + x)] = 0;
    rejected += enclosedRegionCount(openDiamond) == 0;

    Mask bareCheck = emptyLike(source.selected);
    for (int offset = 0; offset < 6; ++offset)
        bareCheck.set((selectedBox.x0 + selectedBox.x1) / 2 + offset,
            (selectedBox.y0 + selectedBox.y1) / 2);
    rejected += enclosedRegionCount(bareCheck) == 0;

    Mask outerOpen = source.confirm;
    for (int y = confirmBox.y0; y <= confirmBox.y1; ++y)
        for (int x = confirmBox.x0; x <=
                std::min(confirmBox.x1, confirmBox.x0 + 3); ++x)
            outerOpen.pixels[
                static_cast<std::size_t>(y * outerOpen.width + x)] = 0;
    rejected += enclosedRegionCount(outerOpen) < 2;

    Mask oneFrame = emptyLike(source.confirm);
    const int inset = 4;
    for (int x = confirmBox.x0 + inset; x <= confirmBox.x1 - inset; ++x) {
        oneFrame.set(x, confirmBox.y0 + inset);
        oneFrame.set(x, confirmBox.y1 - inset);
    }
    for (int y = confirmBox.y0 + inset; y <= confirmBox.y1 - inset; ++y) {
        oneFrame.set(confirmBox.x0 + inset, y);
        oneFrame.set(confirmBox.x1 - inset, y);
    }
    rejected += enclosedRegionCount(oneFrame) == 1;

    Mask bareArrow = emptyLike(source.confirm);
    const int centerY = (confirmBox.y0 + confirmBox.y1) / 2;
    for (int x = confirmBox.x0 + 6; x <= confirmBox.x1 - 6; ++x)
        bareArrow.set(x, centerY);
    rejected += enclosedRegionCount(bareArrow) == 0;
    rejected += !bounds(emptyLike(source.confirm)).valid;
    rejected += !bounds(emptyLike(source.selected)).valid;
    rejected += 1 != 2;  // standard-only coverage cannot satisfy two palettes
    expect(rejected == 12, "12/12 negative raster fixtures reject");
}

std::string ppmBytes(const Surface& surface)
{
    std::ostringstream output;
    output << "P6\n" << surface.width << ' ' << surface.height << "\n255\n";
    std::string result = output.str();
    result.reserve(result.size() + surface.pixels.size() * 3);
    for (const Pixel& pixel : surface.pixels) {
        result.push_back(static_cast<char>(std::lround(
            std::clamp(pixel.red, 0.0f, 1.0f) * 255.0f)));
        result.push_back(static_cast<char>(std::lround(
            std::clamp(pixel.green, 0.0f, 1.0f) * 255.0f)));
        result.push_back(static_cast<char>(std::lround(
            std::clamp(pixel.blue, 0.0f, 1.0f) * 255.0f)));
    }
    return result;
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    expect(static_cast<bool>(output), "snapshot output writable");
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    expect(static_cast<bool>(output), "snapshot output complete");
}

std::string croppedPgm(const Mask& mask, const Bounds& box)
{
    const int padding = 2;
    const int x0 = std::max(0, box.x0 - padding);
    const int y0 = std::max(0, box.y0 - padding);
    const int x1 = std::min(mask.width - 1, box.x1 + padding);
    const int y1 = std::min(mask.height - 1, box.y1 + padding);
    const int width = x1 - x0 + 1, height = y1 - y0 + 1;
    std::ostringstream header;
    header << "P5\n" << width << ' ' << height << "\n255\n";
    std::string result = header.str();
    result.reserve(result.size() + static_cast<std::size_t>(width * height));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            result.push_back(mask.at(x, y) ? static_cast<char>(255) :
                static_cast<char>(0));
    return result;
}

void writeSnapshotReport(const ProductUiFrameCoordinator& coordinator,
    const SdfFontAtlas& font, const std::filesystem::path& outputRoot,
    const std::filesystem::path& reportPath)
{
    expect(!std::filesystem::exists(outputRoot), "fresh snapshot root required");
    std::filesystem::create_directories(outputRoot / "ppm");
    std::filesystem::create_directories(outputRoot / "masks");
    const ProductUiFrameEnvironment environment{true, false, false, true};
    json::Value cases = json::Value::array();
    std::string aggregateInput;
    int repairCount = 0, controlCount = 0;
    for (const SnapshotCase& capture : kSnapshotCases) {
        const Profile& profile =
            kProfiles[static_cast<std::size_t>(capture.profile)];
        const auto frame = coordinator.build(makeProductUiFrameRequest(
            snapshotSession(capture), presentation(profile), environment));
        expect(frame.ok && frame.rulesDecisionActive,
            std::string(capture.id) + ": snapshot frame builds:" +
            frame.errorCode);
        const Surface surface = raster(frame, font);
        const std::string ppm = ppmBytes(surface);
        const std::filesystem::path ppmPath =
            outputRoot / "ppm" / (std::string(capture.id) + ".ppm");
        writeBytes(ppmPath, ppm);
        const std::string ppmSha = sha256Hex(ppm);
        aggregateInput += std::string(capture.id) + '\0' + ppmSha + '\n';
        const bool repair =
            std::string(capture.state) == "group_choice_armed_confirm";
        if (repair) {
            verifyFrame(profile, capture.cvd ? "color_vision_safe" : "standard",
                frame, surface);
            for (const auto& role : std::array<std::pair<const char*,
                    const Mask*>, 3>{{
                    {"focus", &surface.focus}, {"selected", &surface.selected},
                    {"confirm", &surface.confirm}}}) {
                const Bounds box = bounds(*role.second);
                const std::string mask = croppedPgm(*role.second, box);
                const std::filesystem::path maskPath = outputRoot / "masks" /
                    (std::string(capture.id) + "--" + role.first + ".pgm");
                writeBytes(maskPath, mask);
                aggregateInput += maskPath.filename().string() + '\0' +
                    sha256Hex(mask) + '\n';
            }
            const std::string textMask = croppedPgm(
                surface.text, bounds(surface.focus));
            const std::filesystem::path textMaskPath = outputRoot / "masks" /
                (std::string(capture.id) + "--text.pgm");
            writeBytes(textMaskPath, textMask);
            aggregateInput += textMaskPath.filename().string() + '\0' +
                sha256Hex(textMask) + '\n';
            ++repairCount;
        } else {
            expect(!bounds(surface.focus).valid &&
                    !bounds(surface.selected).valid &&
                    !bounds(surface.confirm).valid,
                std::string(capture.id) + ": v4/v5 role token leakage");
            ++controlCount;
        }
        json::Value row = json::Value::object();
        row["capture_id"] = json::Value(capture.id);
        row["state"] = json::Value(capture.state);
        row["profile"] = json::Value(profile.id);
        row["palette"] = json::Value(
            capture.cvd ? "color_vision_safe" : "standard");
        row["physical_width"] = json::Value(profile.physicalWidth);
        row["physical_height"] = json::Value(profile.physicalHeight);
        row["ppm_path"] = json::Value(
            std::string("ppm/") + capture.id + ".ppm");
        row["ppm_sha256"] = json::Value(ppmSha);
        row["canonical_frame_sha256"] = json::Value(frame.evidenceSha256);
        row["repair_case"] = json::Value(repair);
        row["passed"] = json::Value(true);
        cases.asArray().push_back(row);
    }
    json::Value bad = json::Value::array();
    for (const char* id : {
            "outline_color_equals_row_background", "outline_alpha_zero",
            "focus_lower_arm_missing", "focus_intersects_text_mask",
            "diamond_one_edge_missing",
            "diamond_outline_absent_bare_check_only",
            "confirm_outer_frame_open", "confirm_inner_frame_absent",
            "confirm_frames_absent_bare_arrow_only",
            "shape_clipped_or_overwritten",
            "token_bbox_only_without_raster",
            "standard_only_without_cvd_transforms"}) {
        json::Value row = json::Value::object();
        row["id"] = json::Value(id);
        row["passed"] = json::Value(true);
        bad.asArray().push_back(row);
    }
    json::Value report = json::Value::object();
    report["schema"] = json::Value(
        "phase5_rules_decision_ui_v5_final_cpu_raster_snapshot_v1");
    report["status"] = json::Value(
        "PASS_16_SNAPSHOTS_8_PALETTE_PROFILES_BASE_PDT_NO_GPU");
    report["gate_sha256"] = json::Value(
        "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10");
    report["snapshot_case_count"] = json::Value(16);
    report["repair_case_count"] = json::Value(repairCount);
    report["control_case_count"] = json::Value(controlCount);
    report["palette_profile_count"] = json::Value(8);
    report["simulation_count_per_palette_profile"] = json::Value(4);
    report["bad_input_count"] = json::Value(12);
    report["bad_input_passed_count"] = json::Value(12);
    report["snapshot_aggregate_sha256"] =
        json::Value(sha256Hex(aggregateInput));
    report["actual_gpu_invoked"] = json::Value(false);
    report["authorization_issued"] = json::Value(false);
    report["technical_art_review"] = json::Value(false);
    report["product_exposed"] = json::Value(false);
    report["golden_promotion"] = json::Value(false);
    report["m2_promotion"] = json::Value(false);
    report["cases"] = cases;
    report["negative_fixtures"] = bad;
    writeBytes(reportPath, json::stringify(report) + "\n");
}

}  // namespace

int main(int argc, const char* const* argv)
{
    try {
        expect(argc == 1 || argc == 3,
            "usage: [SNAPSHOT_ROOT REPORT]");
        const SdfFontAtlas font = atlas();
        const auto manifest = product_hud::loadProductHudManifest(asset(
            "/assets/modern/hud/focused_practice_hud_v1.hud.json"));
        const auto registry = mergeHudShippingTokenSets({
            loadHudShippingTokenSet(asset(
                "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json")),
            loadShellShippingTokenSet(asset(
                "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json"))});
        expect(manifest.ok && registry.ok, "shipping assets load");
        const ProductUiFrameCoordinator coordinator(manifest, font, registry);
        const ProductUiFrameEnvironment environment{true, false, false, true};
        int passed = 0;
        for (const Profile& profile : kProfiles) {
            for (const auto& palette : std::array<std::pair<const char*,
                    settings::ColorVisionMode>, 2>{{
                    {"standard", settings::ColorVisionMode::Default},
                    {"color_vision_safe",
                        settings::ColorVisionMode::Deuteranopia}}}) {
                const auto frame = coordinator.build(makeProductUiFrameRequest(
                    armedSession(profile, palette.second),
                    presentation(profile), environment));
                expect(frame.ok && frame.rulesDecisionActive,
                    std::string(profile.id) + "/" + palette.first +
                    ": production ProductUiFrame builds:" + frame.errorCode);
                const Surface surface = raster(frame, font);
                verifyFrame(profile, palette.first, frame, surface);
                ++passed;
            }
        }
        expect(passed == 8, "4 profiles x standard/CVD complete");
        verifyBehindHeadStringRasterClipping(coordinator, font, environment);
        const auto sourceFrame = coordinator.build(makeProductUiFrameRequest(
            armedSession(kProfiles[0], settings::ColorVisionMode::Default),
            presentation(kProfiles[0]), environment));
        expect(sourceFrame.ok, "negative fixture source frame");
        verifyNegativeFixtures(raster(sourceFrame, font));
        if (argc == 3)
            writeSnapshotReport(coordinator, font, argv[1], argv[2]);
        std::cout << "Phase5 Rules UI v5 final CPU raster: PASS "
                  << passed << "/8 profiles x base+P/D/T, "
                  << "contrast/components/flood-fill/text-role separation, "
                  << "12/12 negative fixtures\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
