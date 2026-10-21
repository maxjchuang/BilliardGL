#include "automation_json.h"
#include "deterministic_hash.h"
#include "render/ui/hud_shipping_tokens.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render::ui;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("shipping HUD token: " + message);
}

std::string tokenPath()
{
    return std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json";
}

std::string shellTokenPath()
{
    return std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json";
}

std::string readTokenBytes()
{
    std::ifstream input(tokenPath(), std::ios::binary);
    expect(static_cast<bool>(input), "approved token JSON is readable");
    return {std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

json::Value tokenJson()
{
    const json::ParseResult parsed = json::parse(readTokenBytes());
    expect(parsed.ok, "test fixture JSON parses");
    return parsed.value;
}

std::string readShellTokenBytes()
{
    std::ifstream input(shellTokenPath(), std::ios::binary);
    expect(static_cast<bool>(input), "approved Shell token JSON is readable");
    return {std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

std::string readFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    expect(static_cast<bool>(input), "shipping evidence file is readable");
    return {std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

json::Value shellTokenJson()
{
    const json::ParseResult parsed = json::parse(readShellTokenBytes());
    expect(parsed.ok, "Shell test fixture JSON parses");
    return parsed.value;
}

HudDrawList iconList(const std::string& token,
    product_hud::HudRect source = {100.0f, 80.0f, 48.0f, 48.0f},
    product_hud::HudRect clip = {0.0f, 0.0f, 1920.0f, 1080.0f})
{
    HudDrawList draw;
    draw.ok = true;
    draw.safeRect = clip;
    UiPrimitive icon;
    icon.kind = UiPrimitiveKind::Icon;
    icon.sourceRect = source;
    icon.rect = source;
    icon.clipRect = clip;
    icon.token = token;
    draw.primitives.push_back(icon);
    return draw;
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t value = 1469598103934665603ull;
    for (std::uint8_t byte : bytes) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}

void testStrictApprovedLoad()
{
    const HudShippingTokenSet loaded = loadHudShippingTokenSet(tokenPath());
    expect(loaded.ok && loaded.schemaVersion == 1 &&
        loaded.tokenSetId == "focused_practice_hud_shipping_tokens_v1" &&
        loaded.authorship == "original_billiardgl_vector_tokens" &&
        !loaded.externalAssets && loaded.icons.size() == 16 &&
        loaded.aliases.size() == 10 && loaded.stateIconUsage.size() == 5 &&
        loaded.palettes.size() == 2 &&
        loaded.productNamespace == "shipping_hud" &&
        loaded.debugNamespace == "debug_telemetry",
        "schema, identity, authorship, asset, icon, alias, state, palette, and namespace contracts load");

    std::string resolved, error;
    expect(loaded.resolve("tip_target", resolved, error) &&
        resolved == "tip_target" &&
        loaded.resolve("wrong_ball", resolved, error) &&
        resolved == "crossed_circle",
        "direct and alias tokens resolve deterministically");
    expect(!loaded.resolve("item_square", resolved, error) &&
        error == "hud_icon_token_unknown:item_square",
        "unapproved ProductShell icon fails explicitly without fallback");
    expect(!loaded.resolve("debug_telemetry/fps", resolved, error) &&
        error == "hud_shipping_debug_token_forbidden",
        "debug namespace cannot enter shipping tokens");

    const UiColor& standardAccent = loaded.palettes.at(
        HudPaletteMode::Standard).roles.at("accent");
    const UiColor& cvdAccent = loaded.palettes.at(
        HudPaletteMode::ColorVisionSafe).roles.at("accent");
    expect(std::round(standardAccent.red * 255.0f) == 49.0f &&
        std::round(cvdAccent.red * 255.0f) == 93.0f &&
        standardAccent.alpha == 1.0f,
        "sRGB8 standard and CVD palettes parse without premultiplication");
}

void testStrictFailures()
{
    expect(parseHudShippingTokenSet("{bad-json").errorCode ==
        "hud_token_json_invalid", "bad JSON has a stable error");
    expect(loadHudShippingTokenSet("/tmp/no-such-hud-token.json").errorCode ==
        "hud_token_file_unreadable", "missing file has a stable error");

    auto fails = [](json::Value root, const std::string& expectedPrefix,
            const std::string& message) {
        const HudShippingTokenSet result = parseHudShippingTokenSet(
            json::stringify(root));
        expect(!result.ok && result.errorCode.rfind(expectedPrefix, 0) == 0,
            message + " (actual " + result.errorCode + ")");
    };

    json::Value root = tokenJson();
    root["unexpected"] = json::Value(true);
    fails(root, "hud_token_unknown_field", "unknown root field is rejected");

    root = tokenJson();
    root.asObject().erase("authorship");
    fails(root, "hud_token_required_field", "missing identity field is rejected");

    root = tokenJson();
    root["external_assets"] = json::Value(true);
    fails(root, "hud_token_external_assets_forbidden",
        "external assets are forbidden");

    root = tokenJson();
    root["gpu_contract"]["coordinate_space"] = json::Value("bottom_left");
    fails(root, "hud_token_contract_mismatch",
        "GPU coordinate contract mismatch is rejected");

    root = tokenJson();
    root["layout_contract"]["retina_case"]["scale"] = json::Value(2.0);
    fails(root, "hud_token_contract_mismatch",
        "Retina contract mismatch is rejected");

    root = tokenJson();
    root["typography"]["minimum_logical_px"] = json::Value(12);
    fails(root, "hud_token_contract_mismatch",
        "typography contract mismatch is rejected");

    root = tokenJson();
    root["palette_modes"]["standard"].asObject().erase("warning");
    fails(root, "hud_token_required_field", "missing palette role is rejected");

    root = tokenJson();
    root["palette_modes"]["standard"]["accent"] = json::Value("#GG0000");
    fails(root, "hud_token_palette_color", "invalid palette hex is rejected");

    root = tokenJson();
    root["icon_tokens"]["power_bars"]["commands"].asArray()[0]["width"] =
        json::Value(0);
    fails(root, "hud_token_degenerate_geometry",
        "degenerate rectangle is rejected");

    root = tokenJson();
    root["icon_tokens"]["tip_target"]["commands"].asArray()[0]["radius"] =
        json::Value(20);
    fails(root, "hud_token_degenerate_geometry",
        "out-of-bounds circle is rejected");

    root = tokenJson();
    root["icon_tokens"]["camera_frame"]["commands"].asArray()[0]
        ["surprise"] = json::Value(1);
    fails(root, "hud_token_unknown_field",
        "unknown geometry command field is rejected");

    root = tokenJson();
    root["aliases"]["wrong_ball"] = json::Value("no_contact");
    root["aliases"]["no_contact"] = json::Value("wrong_ball");
    fails(root, "hud_token_alias_cycle", "alias cycle is rejected");

    std::string nonFinite = readTokenBytes();
    const std::string needle = "\"schema_version\": 1";
    const std::size_t position = nonFinite.find(needle);
    expect(position != std::string::npos, "non-finite fixture anchor exists");
    nonFinite.replace(position, needle.size(), "\"schema_version\": 1e999");
    expect(parseHudShippingTokenSet(nonFinite).errorCode ==
        "hud_token_json_invalid", "non-finite JSON number is rejected");
}

void testStrictShellLoadAndFailures()
{
    const std::string bytes = readShellTokenBytes();
    const HudShippingTokenSet shell = loadShellShippingTokenSet(
        shellTokenPath());
    expect(shell.ok, "approved Shell token set loads (actual " +
        shell.errorCode + ")");
    expect(shell.schemaVersion == 1 &&
        shell.tokenSetId == "focused_practice_shell_shipping_tokens_v1" &&
        shell.authorship == "original_billiardgl_vector_tokens" &&
        !shell.externalAssets && shell.icons.size() == 43 &&
        shell.aliases.empty() && shell.screenIconUsage.size() == 9 &&
        shell.stateIconUsage.size() == 6 && shell.palettes.size() == 2 &&
        shell.productNamespace == "product_shell" &&
        shell.debugNamespace == "debug_overlay" &&
        sha256Hex(bytes) ==
            "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3",
        "exact Shell schema, identity, SHA, 43-token, usage, palette, and namespace contracts load");
    const std::string shellRoot = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/";
    expect(sha256Hex(readFile(shellRoot +
            "focused_practice_shell_shipping_tokens_v1.manifest.json")) ==
            "cb937f7c590a7ab0a2b7666e1fd6b522eb032c9393909558e31872fc4faf743a" &&
        sha256Hex(readFile(shellRoot +
            "focused_practice_shell_shipping_tokens_flat_proof_v1.svg")) ==
            "7f6b32654f1d20d3323a111024fe08dad47961933dd78ebce9e57892a1185778",
        "Shell manifest and flat-proof file identities remain exact");
    for (const auto palette : {HudPaletteMode::Standard,
            HudPaletteMode::ColorVisionSafe}) {
        for (const auto& icon : shell.icons) {
            const HudIconExpansionResult expanded = expandShippingHudIcons(
                iconList(icon.first), shell, palette);
            expect(expanded.ok && !expanded.draw.triangles.empty() &&
                expanded.draw.primitives.empty(),
                "all 43 Shell tokens expand under both approved palettes");
        }
    }
    expect(loadShippingTokenSet(shellTokenPath()).ok &&
        parseShippingTokenSet(bytes).ok,
        "generic loader dispatches the explicitly identified Shell schema");

    auto fails = [](json::Value root, const std::string& expectedPrefix,
            const std::string& message) {
        const HudShippingTokenSet result = parseShellShippingTokenSet(
            json::stringify(root));
        expect(!result.ok && result.errorCode.rfind(expectedPrefix, 0) == 0,
            message + " (actual " + result.errorCode + ")");
    };
    json::Value root = shellTokenJson();
    root["layout_contract"] = json::Value(json::Value::Object{});
    fails(root, "hud_token_unknown_field",
        "HUD-only root contract is forbidden in the Shell schema");
    root = shellTokenJson();
    root["external_assets"].asArray().push_back(json::Value("icon.svg"));
    fails(root, "hud_token_external_assets_forbidden",
        "Shell external assets must remain an exact empty array");
    root = shellTokenJson();
    root["icon_tokens"]["camera_frame"]["usage"]["screens"] =
        json::Value(json::Value::Array{});
    fails(root, "hud_token_usage_empty",
        "per-icon screen audit cannot be erased");
    root = shellTokenJson();
    root["screen_icon_usage"]["match"].asArray().push_back(
        json::Value("item_square"));
    fails(root, "hud_token_contract_mismatch",
        "nine-screen emitted-token audit is exact");
    root = shellTokenJson();
    root["palette_modes"]["standard"].asObject().erase("disabled");
    fails(root, "hud_token_required_field",
        "Shell-specific palette roles are exact");
    root = shellTokenJson();
    root["gpu_contract"]["unknown_token_policy"] = json::Value("fallback");
    fails(root, "hud_token_contract_mismatch",
        "unknown Shell icons remain hard failures");
    root = shellTokenJson();
    root["token_set_id"] = json::Value("unknown_shipping_set");
    expect(parseShippingTokenSet(json::stringify(root)).errorCode ==
        "hud_token_set_id_unknown:unknown_shipping_set",
        "generic loader hard-fails an unknown token-set identity");
}

void testExpansionAndPalettes()
{
    const HudShippingTokenSet tokens = loadHudShippingTokenSet(tokenPath());
    expect(tokens.ok, "approved token set loads for expansion");
    for (const auto& icon : tokens.icons) {
        const HudIconExpansionResult expanded = expandShippingHudIcons(
            iconList(icon.first), tokens, HudPaletteMode::Standard);
        expect(expanded.ok && expanded.draw.ok &&
            expanded.draw.primitives.empty() &&
            !expanded.draw.triangles.empty(),
            "every approved icon expands to explicit triangles: " + icon.first);
        for (const UiTriangle& triangle : expanded.draw.triangles) {
            expect(triangle.sourceToken == icon.first &&
                triangle.resolvedToken == icon.first,
                "direct expansion preserves stable source and resolved token");
        }
    }

    const HudIconExpansionResult standard = expandShippingHudIcons(
        iconList("wrong_ball"), tokens, HudPaletteMode::Standard);
    const HudIconExpansionResult cvd = expandShippingHudIcons(
        iconList("wrong_ball"), tokens, HudPaletteMode::ColorVisionSafe);
    expect(standard.ok && cvd.ok &&
        standard.draw.triangles.size() == cvd.draw.triangles.size() &&
        standard.draw.triangles.front().sourceToken == "wrong_ball" &&
        standard.draw.triangles.front().resolvedToken == "crossed_circle" &&
        standard.draw.triangles.front().color.red !=
            cvd.draw.triangles.front().color.red,
        "alias source identity survives expansion and palette selection changes color only");

    expect(hudPaletteModeFor(settings::ColorVisionMode::Default) ==
            HudPaletteMode::Standard &&
        hudPaletteModeFor(settings::ColorVisionMode::Deuteranopia) ==
            HudPaletteMode::ColorVisionSafe &&
        hudPaletteModeFor(settings::ColorVisionMode::Protanopia) ==
            HudPaletteMode::ColorVisionSafe &&
        hudPaletteModeFor(settings::ColorVisionMode::Tritanopia) ==
            HudPaletteMode::ColorVisionSafe &&
        hudPaletteModeFor(settings::ColorVisionMode::HighContrastPatterns) ==
            HudPaletteMode::ColorVisionSafe,
        "ProductSettings selects one of the two approved palettes via a pure mapping");

    const HudIconExpansionResult labelled = expandShippingHudIcons(
        iconList("eight_ball_octagon"), tokens, HudPaletteMode::Standard);
    expect(labelled.ok && labelled.draw.iconLabels.size() == 1 &&
        labelled.draw.iconLabels.front().text == "8" &&
        labelled.draw.iconLabels.front().fontRole == "caption_sdf" &&
        labelled.draw.iconLabels.front().sourceToken == "eight_ball_octagon",
        "SDF label slot survives geometry expansion as an explicit request");

    HudDrawList unknown = iconList("item_square");
    const HudIconExpansionResult missing = expandShippingHudIcons(
        unknown, tokens, HudPaletteMode::Standard);
    expect(!missing.ok &&
        missing.errorCode == "hud_icon_token_unknown:item_square",
        "ProductShell token outside the approved set fails explicitly");
    unknown.containsDebug = true;
    expect(expandShippingHudIcons(unknown, tokens,
        HudPaletteMode::Standard).errorCode ==
        "hud_shipping_debug_draw_forbidden",
        "debug draw list cannot mix into shipping expansion");
    unknown.containsDebug = false;
    unknown.ok = false;
    expect(expandShippingHudIcons(unknown, tokens,
        HudPaletteMode::Standard).errorCode == "hud_draw_list_invalid",
        "invalid upstream draw list does not expand");
}

void testExplicitRegistryMerge()
{
    const HudShippingTokenSet hud = loadHudShippingTokenSet(tokenPath());
    const HudShippingTokenRegistry single = mergeHudShippingTokenSets({hud});
    expect(single.ok && single.tokenSetIds.size() == 1 &&
        single.icons.size() == 16 &&
        expandShippingHudIcons(iconList("tip_target"), single,
            HudPaletteMode::Standard).ok,
        "single approved set uses the same explicit registry path");
    expect(mergeHudShippingTokenSets({}).errorCode ==
        "hud_token_registry_empty",
        "empty registry is rejected");
    expect(mergeHudShippingTokenSets({hud, hud}).errorCode ==
        "hud_token_set_id_collision:focused_practice_hud_shipping_tokens_v1",
        "duplicate token-set identity is rejected");

    HudShippingTokenSet shell = hud;
    shell.tokenSetId = "focused_practice_shell_shipping_tokens_v1";
    shell.icons.clear();
    shell.aliases.clear();
    shell.stateIconUsage.clear();
    shell.icons["shell_only"] = hud.icons.at("camera_frame");
    const HudShippingTokenRegistry merged =
        mergeHudShippingTokenSets({hud, shell});
    expect(merged.ok && merged.tokenSetIds.size() == 2 &&
        merged.icons.size() == 17 &&
        expandShippingHudIcons(iconList("shell_only"), merged,
            HudPaletteMode::ColorVisionSafe).ok,
        "distinct future shell token set merges without implicit fallback");

    HudShippingTokenSet collision = shell;
    collision.icons.clear();
    collision.icons["tip_target"] = hud.icons.at("tip_target");
    expect(mergeHudShippingTokenSets({hud, collision}).ok,
        "byte-equivalent shared source token geometry may be deduplicated");
    collision.icons["tip_target"].commands.front().radius += 0.5;
    expect(mergeHudShippingTokenSets({hud, collision}).errorCode ==
        "hud_token_name_collision:tip_target",
        "cross-set token geometry conflict is explicit");

    HudShippingTokenSet paletteMismatch = shell;
    paletteMismatch.palettes[HudPaletteMode::Standard].roles["accent"].red = 0.0f;
    expect(mergeHudShippingTokenSets({hud, paletteMismatch}).errorCode ==
        "hud_token_registry_palette_mismatch",
        "cross-set palette drift is rejected");

    HudShippingTokenSet namespaceMismatch = shell;
    namespaceMismatch.productNamespace = "other_shipping";
    expect(mergeHudShippingTokenSets({hud, namespaceMismatch}).ok,
        "distinct product namespaces coexist without flattening their palettes");

    const HudShippingTokenSet approvedShell =
        loadShellShippingTokenSet(shellTokenPath());
    const HudShippingTokenRegistry approved =
        mergeHudShippingTokenSets({hud, approvedShell});
    expect(approved.ok && approved.tokenSetIds.size() == 2 &&
        approved.tokenSets.size() == 2 &&
        approved.productNamespaces.size() == 2 && approved.icons.size() == 56,
        "approved HUD and 43-token Shell sets merge as two explicit namespaces");
    expect(expandShippingHudIcons(iconList("power_bars"), approved,
            HudPaletteMode::Standard).errorCode ==
            "hud_token_registry_namespace_required" &&
        expandShippingHudIcons(iconList("power_bars"), approved,
            HudPaletteMode::Standard,
            "focused_practice_hud_shipping_tokens_v1").ok &&
        expandShippingHudIcons(iconList("power_bars"), approved,
            HudPaletteMode::Standard,
            "focused_practice_shell_shipping_tokens_v1").ok,
        "multi-namespace expansion requires an explicit token-set identity");

    HudShippingTokenSet cycleA = shell;
    cycleA.tokenSetId = "cycle_a";
    cycleA.icons.clear();
    cycleA.aliases = {{"cycle_first", "cycle_second"}};
    HudShippingTokenSet cycleB = shell;
    cycleB.tokenSetId = "cycle_b";
    cycleB.icons.clear();
    cycleB.aliases = {{"cycle_second", "cycle_first"}};
    expect(mergeHudShippingTokenSets({cycleA, cycleB}).errorCode ==
        "hud_token_alias_cycle",
        "cross-set alias cycles are rejected after explicit merge");
}

void testResponsiveClipAndDeterminism()
{
    const HudShippingTokenSet tokens = loadHudShippingTokenSet(tokenPath());
    struct Case {
        int logicalWidth;
        int logicalHeight;
        int physicalWidth;
        int physicalHeight;
        float userScale;
    } cases[] = {
        {1280, 720, 1280, 720, 0.85f},
        {1280, 720, 1280, 720, 1.35f},
        {1920, 1080, 1920, 1080, 1.0f},
        {1280, 720, 1920, 1080, 1.0f},
    };
    std::size_t previousCount = 0;
    for (const Case& value : cases) {
        const float density = std::min(
            static_cast<float>(value.physicalWidth) / value.logicalWidth,
            static_cast<float>(value.physicalHeight) / value.logicalHeight);
        const float size = 24.0f * density * value.userScale;
        HudDrawList input = iconList("tip_target",
            {43.0f * density, 24.0f * density, size, size},
            {0.0f, 0.0f, static_cast<float>(value.physicalWidth),
                static_cast<float>(value.physicalHeight)});
        const HudIconExpansionResult first = expandShippingHudIcons(
            input, tokens, HudPaletteMode::Standard);
        const HudIconExpansionResult second = expandShippingHudIcons(
            input, tokens, HudPaletteMode::Standard);
        const auto firstBytes = canonicalHudTriangleBytes(first.draw.triangles);
        const auto secondBytes = canonicalHudTriangleBytes(second.draw.triangles);
        expect(first.ok && second.ok && !first.draw.triangles.empty() &&
            first.draw.triangles.size() == second.draw.triangles.size() &&
            firstBytes == secondBytes && fnv1a(firstBytes) == fnv1a(secondBytes),
            "720p/1080p/Retina and 85-135% geometry is byte and vertex deterministic");
        if (previousCount != 0)
            expect(first.draw.triangles.size() == previousCount,
                "responsive transforms preserve token topology");
        previousCount = first.draw.triangles.size();
    }

    HudDrawList clipped = iconList("group_solid_filled_circle",
        {0.0f, 0.0f, 48.0f, 48.0f}, {24.0f, 0.0f, 24.0f, 48.0f});
    clipped.primitives.front().rect = {24.0f, 0.0f, 24.0f, 48.0f};
    const HudIconExpansionResult expanded = expandShippingHudIcons(
        clipped, tokens, HudPaletteMode::Standard);
    expect(expanded.ok && !expanded.draw.triangles.empty(),
        "partially clipped icon still produces visible geometry");
    for (const UiTriangle& triangle : expanded.draw.triangles) {
        for (const UiPoint& point : {triangle.first, triangle.second,
                triangle.third})
            expect(point.x >= 24.0f - 0.001f && point.x <= 48.0f + 0.001f &&
                point.y >= -0.001f && point.y <= 48.0f + 0.001f,
                "every expanded vertex is clipped at the CPU boundary");
    }
}

}  // namespace

int main()
{
    try {
        testStrictApprovedLoad();
        testStrictFailures();
        testStrictShellLoadAndFailures();
        testExpansionAndPalettes();
        testExplicitRegistryMerge();
        testResponsiveClipAndDeterminism();
        std::cout << "Shipping HUD token load/expansion checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
