#include "render/ui/product_shell_draw_list.h"
#include "deterministic_hash.h"
#include "automation_json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace ui = billiardgl::render::ui;
namespace hud = billiardgl::product_hud;

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}

bool close(float a, float b, float epsilon = 1e-3f)
{
    return std::fabs(a - b) <= epsilon;
}

bool sameRect(const hud::HudRect& a, const hud::HudRect& b)
{
    return close(a.x, b.x) && close(a.y, b.y) &&
        close(a.width, b.width) && close(a.height, b.height);
}

std::string bytes(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "read frozen CP1 binding: " + path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

float overlapArea(const hud::HudRect& a, const hud::HudRect& b)
{
    const float x = std::max(0.0f,
        std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x));
    const float y = std::max(0.0f,
        std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y));
    return x * y;
}

struct Profile {
    const char* id;
    int width;
    int height;
    float uiScale;
    hud::HudRect panel;
    hud::HudRect plot;
};

const std::array<Profile, 4> kProfiles{{
    {"1920x1080_ui100", 1920, 1080, 1.0f,
        {1440, 680, 448, 288}, {1456, 728, 416, 208}},
    {"1920x1080_ui135", 1920, 1080, 1.35f,
        {1272.0f, 540.0f, 604.8f, 388.8f},
        {1293.6f, 604.8f, 561.6f, 280.8f}},
    {"1280x720_ui100", 1280, 720, 1.0f,
        {912, 392, 336, 216}, {936, 440, 288, 144}},
    {"1280x720_ui135", 1280, 720, 1.35f,
        {783.2f, 277.2f, 453.6f, 291.6f},
        {804.8f, 342.0f, 410.4f, 205.2f}},
}};

ui::TrajectoryInsetShellRequest requestFor(const Profile& profile)
{
    ui::TrajectoryInsetShellRequest request;
    request.profileId = profile.id;
    request.physicalWidth = profile.width;
    request.physicalHeight = profile.height;
    request.uiScale = profile.uiScale;
    request.exclusions = {
        {"player status HUD", {32, 24, 420, 108}},
        {"shot power and tip controls", {32,
            static_cast<float>(profile.height) - 96, 600, 72}},
        {"rules decision panel or modal", {500, 160, 420, 360}, false},
        {"caption and accessibility live-region band", {360,
            static_cast<float>(profile.height) - 80, 560, 48}},
        {"safe-area edges and product footer", {0,
            static_cast<float>(profile.height) - 112 * profile.uiScale,
            static_cast<float>(profile.width), 112 * profile.uiScale}},
    };
    return request;
}

void testAuthoritativeLayoutsAndTransparentDrawList()
{
    for (const Profile& profile : kProfiles) {
        const auto request = requestFor(profile);
        const auto layout = ui::buildTrajectoryInsetShellLayout(request);
        require(layout.ok && layout.insetVisible,
            std::string(profile.id) + ": visible authoritative layout");
        require(layout.profileId == profile.id &&
            sameRect(layout.panelRect, profile.panel) &&
            sameRect(layout.plotCutoutRect, profile.plot),
            std::string(profile.id) + ": exact panel and plot rectangles");
        require(layout.panelRect.width >= 336.0f &&
            layout.panelRect.height >= 216.0f &&
            close(layout.plotCutoutRect.width /
                layout.plotCutoutRect.height, 2.0f, 1e-5f),
            std::string(profile.id) + ": minimum panel and 2:1 plot");
        require(layout.insetControlOn && !layout.worldOverlayControlOn &&
            layout.worldOverlayVisible == false,
            std::string(profile.id) + ": independent default controls");
        require(layout.title == "TRAJECTORY / 轨迹" &&
            layout.toggleLabel == "Trajectory inset / 轨迹小窗" &&
            layout.orientationLabels == std::array<std::string, 2>{
                "HEAD / 开球端", "FOOT / 底端"} &&
            layout.legendEntries == std::array<std::string, 3>{
                "ACTUAL / 实际 — paired continuous rails",
                "PREDICTED / 预测 — centerline dashes",
                "END / 终点 — endpoint marker"},
            std::string(profile.id) + ": ordered bilingual contract");
        require(layout.legendTextPx >= 16.0f * profile.uiScale - 1e-4f &&
            layout.toggleRect.width >= 44.0f * profile.uiScale - 1e-4f &&
            layout.toggleRect.height >= 44.0f * profile.uiScale - 1e-4f,
            std::string(profile.id) + ": legend and toggle minimums");
        const std::array<hud::HudRect, 8> uiRects{{layout.titleRect,
            layout.orientationLabelRects[0], layout.orientationLabelRects[1],
            layout.legendRects[0], layout.legendRects[1],
            layout.legendRects[2], layout.toggleLabelRect, layout.toggleRect}};
        for (const auto& rect : uiRects) {
            require(rect.width > 0.0f && rect.height > 0.0f &&
                close(overlapArea(rect, layout.panelRect),
                    rect.width * rect.height, 0.05f) &&
                overlapArea(rect, layout.plotCutoutRect) == 0.0f,
                std::string(profile.id) +
                    ": title/labels/legends/toggle contained outside plot");
        }
        for (std::size_t first = 0; first < uiRects.size(); ++first)
            for (std::size_t second = first + 1;
                    second < uiRects.size(); ++second)
                require(overlapArea(uiRects[first], uiRects[second]) == 0.0f,
                    std::string(profile.id) +
                        ": title/labels/legends/toggle pairwise disjoint " +
                        std::to_string(first) + "/" +
                        std::to_string(second));
        for (const auto& border : layout.borderRects)
            require(close(overlapArea(border, layout.panelRect),
                    border.width * border.height, 0.05f) &&
                overlapArea(border, layout.plotCutoutRect) == 0.0f,
                std::string(profile.id) + ": border outside plot");
        for (const auto& exclusion : request.exclusions)
            if (exclusion.active)
                require(overlapArea(layout.panelRect, exclusion.rect) == 0.0f,
                    std::string(profile.id) + ": exclusion " + exclusion.id);

        const std::vector<ui::TrajectoryInsetChromePrimitive> surrounding{{
            ui::TrajectoryInsetChromeKind::OpaquePanel, layout.panelRect,
            "existing_shipping_panel", 0.94f, true}, {
            ui::TrajectoryInsetChromeKind::OpaquePanel,
            {layout.panelRect.x, layout.panelRect.y + layout.panelRect.height -
                20.0f * profile.uiScale, layout.panelRect.width,
                20.0f * profile.uiScale}, "existing_shipping_footer",
            0.94f, true}};
        const auto draw = ui::buildTrajectoryInsetChromeDrawList(
            layout, surrounding);
        require(draw.ok && draw.visible && draw.cutoutExplicit &&
            sameRect(draw.transparentCutoutRect, layout.plotCutoutRect),
            std::string(profile.id) + ": explicit transparent cutout");
        require(draw.trajectorySemanticMemberCount == 0 &&
            draw.trajectoryProbeCount == 0 &&
            draw.trajectoryDenominatorContribution == 0,
            std::string(profile.id) + ": UI excluded from semantics");
        require(draw.orderedText == std::vector<std::string>{
                "TRAJECTORY / 轨迹", "HEAD / 开球端", "FOOT / 底端",
                "ACTUAL / 实际 — paired continuous rails",
                "PREDICTED / 预测 — centerline dashes",
                "END / 终点 — endpoint marker",
                "Trajectory inset / 轨迹小窗"},
            std::string(profile.id) + ": draw-list text order");
        const std::array<hud::HudRect, 8> expectedTextAndToggle{{
            layout.titleRect, layout.orientationLabelRects[0],
            layout.orientationLabelRects[1], layout.legendRects[0],
            layout.legendRects[1], layout.legendRects[2],
            layout.toggleLabelRect, layout.toggleRect}};
        for (std::size_t index = 0; index < expectedTextAndToggle.size(); ++index) {
            const auto found = std::find_if(draw.primitives.begin(),
                draw.primitives.end(), [&](const auto& primitive) {
                    return primitive.token == (index == 7 ?
                        "trajectory_inset_toggle" :
                        "trajectory_inset_text_" + std::to_string(index));
                });
            require(found != draw.primitives.end() &&
                sameRect(found->rect, expectedTextAndToggle[index]),
                std::string(profile.id) + ": draw UI rect exact from layout");
        }
        for (const auto& input : surrounding) {
            float emittedArea = 0.0f;
            for (const auto& primitive : draw.primitives)
                if (primitive.sourceToken == input.token)
                    emittedArea += primitive.rect.width * primitive.rect.height;
            const float expectedArea = input.rect.width * input.rect.height -
                overlapArea(input.rect, layout.plotCutoutRect);
            require(close(emittedArea, expectedArea, 0.05f),
                std::string(profile.id) +
                    ": surrounding primitive area preserved minus cutout");
        }
        for (const auto& primitive : draw.primitives) {
            require(overlapArea(primitive.rect, draw.transparentCutoutRect) == 0.0f,
                std::string(profile.id) + ": chrome never covers cutout");
            require(primitive.opacity > 0.0f,
                std::string(profile.id) + ": only explicit opaque chrome primitives");
            require(primitive.excludedFromTrajectoryDenominator,
                std::string(profile.id) + ": every UI primitive excluded");
        }
        const auto layoutAudit = ui::auditTrajectoryInsetShellLayout(
            request, layout);
        const auto drawAudit = ui::auditTrajectoryInsetChromeDrawList(
            layout, draw, surrounding);
        require(layoutAudit.ok && drawAudit.ok,
            std::string(profile.id) + ": independent audits pass layout=" +
                layoutAudit.errorCode + ":draw=" + drawAudit.errorCode);
    }
}

void testEveryNamedExclusionSuppressesWithoutRelocation()
{
    const Profile& profile = kProfiles.front();
    for (std::size_t index = 0; index < 5; ++index) {
        auto request = requestFor(profile);
        request.exclusions[index].rect = profile.panel;
        if (index == 2) request.exclusions[index].active = true;
        const auto layout = ui::buildTrajectoryInsetShellLayout(request);
        const std::string expected = index == 2 ?
            "inset_suppressed_by_modal" : "inset_layout_unavailable";
        require(layout.ok && !layout.insetVisible &&
            layout.suppressionReason == expected &&
            sameRect(layout.authoritativePanelRect, profile.panel) &&
            !layout.worldOverlayVisible,
            "each named exclusion suppresses exactly without relocation: " +
                request.exclusions[index].id);
    }
}

void testCp1InventoryAndSourceBindingsRemainExact()
{
    const std::array<std::pair<const char*, const char*>, 5> bindings{{
        {"src/Billiards/training/trajectory_inset_model.h",
            "3a6e0550d1f85b9168528454b74c5768c5d6c9b9ba6db0dd8fd4fdac1c66a8c6"},
        {"src/Billiards/training/trajectory_inset_model.cpp",
            "58a9a3dfe922c235314884d36bd8cea47df8cfe400adcf5a4a716e64e093e2b4"},
        {"src/Billiards/training/trajectory_inset_renderer.h",
            "2e7191c1b260dc257a07f5868d0dc1a9dc0012e69696229f3fd0118c02af3947"},
        {"src/Billiards/training/trajectory_inset_renderer.cpp",
            "9969c1736fb89329e25dfa1d0252f18e5475d18a9027ddebf740cab4e8804201"},
        {"tests/phase4_v19_trajectory_inset_cpu_tests.cpp",
            "56c1c8e21bcd87b00cc11cdb5e21012a2acb0b4c3dcd4e08b98b9dfb254eef6b"},
    }};
    for (const auto& binding : bindings)
        require(billiardgl::sha256Hex(bytes(binding.first)) == binding.second,
            std::string("CP1 source byte-exact: ") + binding.first);
    const std::string receipt = bytes(
        "docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json");
    require(billiardgl::sha256Hex(receipt) ==
        "3208c819bd81ddc2143921a3b3ef2136127b275317a6ae0f5549de18188de758",
        "CP1 receipt byte-exact");
    struct InventoryRow {
        const char* profile; int sources; int members; int probes; int events;
        const char* parent; const char* probe; const char* event;
        const char* contributor; const char* intervals;
    };
    const std::array<InventoryRow, 4> expected{{
        {"1920x1080_ui100",8,72,29,17,"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","0fe2720d3e580ef5c720984011a3836b15493b019a892e6feb1458863c5efc17","ae1b95a3ca21f3986441721b8378a3d4460d45de700da1fd73fcbd2da6452254","ab91990a175d41ce9db37a0aac24f7b3406e122f58f4a471d47ae1be751eee4b","4a95ae275292661a2ae4b16037cea1da20b9839e818fc2f0fd1986ace4e0f3aa"},
        {"1920x1080_ui135",8,90,35,23,"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","00c2c0055e3aef5e7d85f5eddd2620abf808410b3b049792b26cfc8658d949ec","16d190923fee2896a9a68690ba1b7d94ff293c9c45b3823b251901f55213a65c","4372f48b8cd588625e4f6a545fcbf07ba86043eb68a1b58ace21b94556a2c49e","26d619f7bc4f2e35884508273a8a76895cf50db1fef97d67e457d88c76a112e7"},
        {"1280x720_ui100",8,54,23,11,"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","78dcf2aa7f4cf259dbbd6ce6517fb0ed50d42cf3e7060a809d3f8943a3b327b0","852636e235f01a60bdf148efe89e7eabc3ad7609af76323d4a82b982508c479c","9e38595b53554dc12837b67f0e7cd7d201e95448c1bd4d21b437827e7fd280cf","610365e6deeacb0dd83d1c928d698ccf0dce50856cbe8413a54c233f588e64ce"},
        {"1280x720_ui135",8,69,28,16,"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","86bbf305a7f3b170e76445443c13c117d0c65ad0125f04eb9903af99ba492707","3ba15c995c63ff46b44df044e197bdd0b194fa86c3b273d687ada1775c4f305b","d028e45f97a9b1c888e09f750ab6e6d8dc7e9bd9fcde22448cf46d2de053ab3e","064302def27dbfc76ea763f10965c5defacc358d67e5cb63052af5d4bdffa70b"}
    }};
    const auto parsed = billiardgl::json::parse(receipt);
    require(parsed.ok && parsed.value.has("canonical_layout_inventory"),
        "CP1 receipt inventory parses structurally");
    const auto& rows = parsed.value.at("canonical_layout_inventory").asArray();
    require(rows.size() == expected.size(), "CP1 exact four inventory rows");
    const auto rowMatches = [](const billiardgl::json::Value& value,
            const InventoryRow& row) {
        return value.at("profileId").asString() == row.profile &&
            value.at("sourceGroupDenominator").asInt() == row.sources &&
            value.at("semanticMemberDenominator").asInt() == row.members &&
            value.at("probeDenominator").asInt() == row.probes &&
            value.at("dashEventCount").asInt() == row.events &&
            value.at("routeSpanCount").asInt() == 1 &&
            value.at("orderedParentSourceDigest").asString() == row.parent &&
            value.at("orderedProbeDigest").asString() == row.probe &&
            value.at("orderedDashEventDigest").asString() == row.event &&
            value.at("orderedDashContributorDigest").asString() == row.contributor &&
            value.at("orderedSourceRouteIntervalDigest").asString() == row.intervals;
    };
    for (std::size_t index = 0; index < rows.size(); ++index)
        require(rowMatches(rows[index], expected[index]),
            std::string("CP1 structured inventory exact: ") +
                expected[index].profile);
    std::string mutatedReceipt = receipt;
    const auto countAt = mutatedReceipt.find("\"semanticMemberDenominator\":72");
    require(countAt != std::string::npos, "CP1 count mutation locates field");
    mutatedReceipt.replace(countAt, std::string("\"semanticMemberDenominator\":72").size(),
        "\"semanticMemberDenominator\":73");
    require(billiardgl::sha256Hex(mutatedReceipt) !=
        "3208c819bd81ddc2143921a3b3ef2136127b275317a6ae0f5549de18188de758",
        "CP1 inventory count mutation changes receipt binding");
    const auto mutatedParsed = billiardgl::json::parse(mutatedReceipt);
    require(mutatedParsed.ok && !rowMatches(
        mutatedParsed.value.at("canonical_layout_inventory").asArray()[0],
        expected[0]), "CP1 structured count mutation is rejected");
}

void testSuppressionAndFailClosedMutations()
{
    const Profile& profile = kProfiles.front();
    auto request = requestFor(profile);
    request.modalVisible = true;
    request.exclusions[2].active = true;
    auto layout = ui::buildTrajectoryInsetShellLayout(request);
    require(layout.ok && !layout.insetVisible &&
        layout.suppressionReason == "inset_suppressed_by_modal" &&
        !layout.worldOverlayVisible,
        "modal suppresses inset without world fallback");
    auto draw = ui::buildTrajectoryInsetChromeDrawList(layout);
    require(draw.ok && !draw.visible && draw.primitives.empty(),
        "suppressed inset emits no chrome or cutout writes");

    request = requestFor(profile);
    request.exclusions[0].rect = profile.panel;
    layout = ui::buildTrajectoryInsetShellLayout(request);
    require(layout.ok && !layout.insetVisible &&
        layout.suppressionReason == "inset_layout_unavailable" &&
        !layout.worldOverlayVisible,
        "collision suppresses without relocation or world fallback");
    require(sameRect(layout.authoritativePanelRect, profile.panel),
        "collision retains authoritative panel instead of relocating");

    request = requestFor(profile);
    request.physicalWidth = 800;
    request.physicalHeight = 600;
    request.profileId = "unsupported";
    layout = ui::buildTrajectoryInsetShellLayout(request);
    require(layout.ok && !layout.insetVisible &&
        layout.suppressionReason == "inset_layout_unavailable",
        "unsupported or below-minimum layout suppresses exactly");

    request = requestFor(profile);
    request.exclusions.push_back({"foreign exclusion", {0, 0, 1, 1}});
    layout = ui::buildTrajectoryInsetShellLayout(request);
    require(!layout.ok &&
        layout.errorCode == "trajectory_inset_exclusion_inventory_invalid",
        "unknown exclusion inventory fails closed");

    request = requestFor(profile);
    request.worldOverlayControlOn = true;
    layout = ui::buildTrajectoryInsetShellLayout(request);
    require(!layout.ok &&
        layout.errorCode == "trajectory_inset_world_overlay_forbidden_in_cp2",
        "world overlay cannot be enabled by CP2 visible/suppressed states");

    request = requestFor(profile);
    const auto valid = ui::buildTrajectoryInsetShellLayout(request);
    const auto validDraw = ui::buildTrajectoryInsetChromeDrawList(valid);

    auto mutated = valid;
    mutated.plotCutoutRect.width -= 1.0f;
    require(!ui::auditTrajectoryInsetShellLayout(request, mutated).ok,
        "reference rectangle mutation fails closed");
    mutated = valid;
    std::swap(mutated.legendEntries[0], mutated.legendEntries[1]);
    require(!ui::auditTrajectoryInsetShellLayout(request, mutated).ok,
        "legend reorder fails closed");
    mutated = valid;
    mutated.trajectoryDenominatorContribution = 1;
    require(!ui::auditTrajectoryInsetShellLayout(request, mutated).ok,
        "UI denominator growth fails closed");
    mutated = valid;
    mutated.titleRect.width = 0.0f;
    require(!ui::auditTrajectoryInsetShellLayout(request, mutated).ok,
        "zero-sized UI rect fails closed");
    mutated = valid;
    mutated.legendRects[1] = mutated.legendRects[0];
    require(!ui::auditTrajectoryInsetShellLayout(request, mutated).ok,
        "label/legend collision fails closed");

    auto mutatedDraw = validDraw;
    mutatedDraw.primitives.push_back({ui::TrajectoryInsetChromeKind::OpaquePanel,
        {valid.plotCutoutRect.x, valid.plotCutoutRect.y, 1, 1}, {}, 1.0f, true});
    require(!ui::auditTrajectoryInsetChromeDrawList(valid, mutatedDraw).ok,
        "one opaque cutout pixel fails closed");
    mutatedDraw = validDraw;
    mutatedDraw.transparentCutoutRect.width -= 1.0f;
    require(!ui::auditTrajectoryInsetChromeDrawList(valid, mutatedDraw).ok,
        "cutout drift fails closed");
    mutatedDraw = validDraw;
    mutatedDraw.trajectoryProbeCount = 1;
    require(!ui::auditTrajectoryInsetChromeDrawList(valid, mutatedDraw).ok,
        "UI probe growth fails closed");
    mutatedDraw = ui::buildTrajectoryInsetChromeDrawList(valid, {{
        ui::TrajectoryInsetChromeKind::OpaquePanel, valid.panelRect,
        "coverage_source", 1.0f, true}});
    const std::vector<ui::TrajectoryInsetChromePrimitive> coverageSource{{
        ui::TrajectoryInsetChromeKind::OpaquePanel, valid.panelRect,
        "coverage_source", 1.0f, true}};
    const auto sourcePiece = std::find_if(mutatedDraw.primitives.begin(),
        mutatedDraw.primitives.end(), [](const auto& primitive) {
            return primitive.sourceToken == "coverage_source";
        });
    require(sourcePiece != mutatedDraw.primitives.end(),
        "coverage mutation locates emitted split piece");
    mutatedDraw.primitives.erase(sourcePiece);
    require(!ui::auditTrajectoryInsetChromeDrawList(
        valid, mutatedDraw, coverageSource).ok,
        "dropped surrounding split piece fails closed");
    mutatedDraw = ui::buildTrajectoryInsetChromeDrawList(valid, {{
        ui::TrajectoryInsetChromeKind::OpaquePanel, valid.panelRect,
        "coverage_source", 1.0f, true}});
    mutatedDraw.primitives.push_back(*std::find_if(mutatedDraw.primitives.begin(),
        mutatedDraw.primitives.end(), [](const auto& primitive) {
            return primitive.sourceToken == "coverage_source";
        }));
    require(!ui::auditTrajectoryInsetChromeDrawList(
        valid, mutatedDraw, coverageSource).ok,
        "duplicated surrounding split piece fails closed");
    mutatedDraw = ui::buildTrajectoryInsetChromeDrawList(valid, coverageSource);
    mutatedDraw.coverageClaims[0].expectedVisibleArea += 1.0f;
    require(!ui::auditTrajectoryInsetChromeDrawList(
        valid, mutatedDraw, coverageSource).ok,
        "self-claimed coverage area mutation fails closed");
    mutatedDraw = ui::buildTrajectoryInsetChromeDrawList(valid, coverageSource);
    mutatedDraw.coverageClaims[0].orderedExpectedVisibleRects[0].width += 1.0f;
    require(!ui::auditTrajectoryInsetChromeDrawList(
        valid, mutatedDraw, coverageSource).ok,
        "self-claimed coverage shape mutation fails closed");
    mutatedDraw = ui::buildTrajectoryInsetChromeDrawList(valid, coverageSource);
    mutatedDraw.coverageClaims[0].sourceRect.x += 1.0f;
    require(!ui::auditTrajectoryInsetChromeDrawList(
        valid, mutatedDraw, coverageSource).ok,
        "coverage source rect mutation fails closed");
    mutatedDraw = validDraw;
    const auto text = std::find_if(mutatedDraw.primitives.begin(),
        mutatedDraw.primitives.end(), [](const auto& primitive) {
            return primitive.token == "trajectory_inset_text_0";
        });
    require(text != mutatedDraw.primitives.end(),
        "generated chrome mutation locates title");
    mutatedDraw.primitives.push_back(*text);
    require(!ui::auditTrajectoryInsetChromeDrawList(valid, mutatedDraw).ok,
        "duplicate generated title fails closed");
    mutatedDraw = validDraw;
    const auto border = std::find_if(mutatedDraw.primitives.begin(),
        mutatedDraw.primitives.end(), [](const auto& primitive) {
            return primitive.token == "trajectory_inset_border_0";
        });
    require(border != mutatedDraw.primitives.end(),
        "generated chrome mutation locates border");
    mutatedDraw.primitives.erase(border);
    require(!ui::auditTrajectoryInsetChromeDrawList(valid, mutatedDraw).ok,
        "missing generated border fails closed");
    mutatedDraw = validDraw;
    mutatedDraw.primitives.push_back({ui::TrajectoryInsetChromeKind::OpaquePanel,
        {valid.panelRect.x, valid.panelRect.y + 2, 1, 1},
        "foreign_opaque_chrome", 1.0f, true});
    require(!ui::auditTrajectoryInsetChromeDrawList(valid, mutatedDraw).ok,
        "unrecognized generated opaque chrome fails closed");

    request = requestFor(profile);
    request.insetControlFocused = true;
    const auto focused = ui::buildTrajectoryInsetShellLayout(request);
    const auto focusedDraw = ui::buildTrajectoryInsetChromeDrawList(focused);
    require(focused.ok && focused.insetControlFocused &&
        std::count_if(focusedDraw.primitives.begin(),
            focusedDraw.primitives.end(), [](const auto& primitive) {
                return primitive.kind == ui::TrajectoryInsetChromeKind::Focus &&
                    primitive.token == "trajectory_inset_focus";
            }) == 1 &&
        ui::auditTrajectoryInsetChromeDrawList(focused, focusedDraw).ok,
        "focused inset emits one exact excluded focus primitive");
}

}  // namespace

int main()
{
    testAuthoritativeLayoutsAndTransparentDrawList();
    testEveryNamedExclusionSuppressesWithoutRelocation();
    testCp1InventoryAndSourceBindingsRemainExact();
    testSuppressionAndFailClosedMutations();
    std::cout << "PASS phase4_v19_cp2_layout_draw_list profiles=4 "
                 "transparent_cutout=4 named_exclusions=5 "
                 "fail_closed_mutations=27 cp1_inventory_rows=4\n";
    return 0;
}
