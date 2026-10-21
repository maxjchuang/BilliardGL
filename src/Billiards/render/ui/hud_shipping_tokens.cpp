#include "render/ui/hud_shipping_tokens.h"

#include "automation_json.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace billiardgl::render::ui {
namespace {

constexpr double kTokenExtent = 24.0;
constexpr double kEpsilon = 1e-9;
constexpr int kCircleSegments = 32;
constexpr int kRoundSegments = 16;

class TokenError final : public std::runtime_error {
public:
    explicit TokenError(const std::string& code) : std::runtime_error(code) {}
};

using KeySet = std::set<std::string>;

const json::Value::Object& object(const json::Value& value,
    const std::string& context)
{
    if (!value.isObject()) throw TokenError("hud_token_type:" + context);
    return value.asObject();
}

const json::Value::Array& array(const json::Value& value,
    const std::string& context)
{
    if (!value.isArray()) throw TokenError("hud_token_type:" + context);
    return value.asArray();
}

void exactKeys(const json::Value& value, const KeySet& allowed,
    const KeySet& required, const std::string& context)
{
    const auto& values = object(value, context);
    for (const auto& entry : values) {
        if (allowed.count(entry.first) == 0)
            throw TokenError("hud_token_unknown_field:" + context + ":" +
                entry.first);
    }
    for (const std::string& key : required) {
        if (values.count(key) == 0)
            throw TokenError("hud_token_required_field:" + context + ":" +
                key);
    }
}

const json::Value& field(const json::Value& value, const char* key,
    const std::string& context)
{
    const auto& values = object(value, context);
    const auto found = values.find(key);
    if (found == values.end())
        throw TokenError("hud_token_required_field:" + context + ":" + key);
    return found->second;
}

std::string stringValue(const json::Value& value,
    const std::string& context)
{
    if (!value.isString() || value.asString().empty())
        throw TokenError("hud_token_string:" + context);
    return value.asString();
}

bool boolValue(const json::Value& value, const std::string& context)
{
    if (!value.isBool()) throw TokenError("hud_token_bool:" + context);
    return value.asBool();
}

double numberValue(const json::Value& value, const std::string& context)
{
    if (!value.isNumber() || !std::isfinite(value.asNumber()))
        throw TokenError("hud_token_non_finite:" + context);
    return value.asNumber();
}

int integerValue(const json::Value& value, const std::string& context)
{
    const double number = numberValue(value, context);
    if (std::floor(number) != number ||
        number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max())
        throw TokenError("hud_token_integer:" + context);
    return static_cast<int>(number);
}

std::vector<std::string> stringArray(const json::Value& value,
    const std::string& context)
{
    std::vector<std::string> result;
    const auto& values = array(value, context);
    result.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index)
        result.push_back(stringValue(values[index], context + "[]"));
    return result;
}

void expectStrings(const json::Value& value,
    const std::vector<std::string>& expected, const std::string& context)
{
    if (stringArray(value, context) != expected)
        throw TokenError("hud_token_contract_mismatch:" + context);
}

void expectString(const json::Value& value, const char* expected,
    const std::string& context)
{
    if (stringValue(value, context) != expected)
        throw TokenError("hud_token_contract_mismatch:" + context);
}

void expectBool(const json::Value& value, bool expected,
    const std::string& context)
{
    if (boolValue(value, context) != expected)
        throw TokenError("hud_token_contract_mismatch:" + context);
}

void expectNumber(const json::Value& value, double expected,
    const std::string& context)
{
    if (std::fabs(numberValue(value, context) - expected) > kEpsilon)
        throw TokenError("hud_token_contract_mismatch:" + context);
}

bool inTokenBounds(double value)
{
    return value >= 0.0 && value <= kTokenExtent;
}

HudTokenPoint point(const json::Value& value, const std::string& context)
{
    const auto& parts = array(value, context);
    if (parts.size() != 2) throw TokenError("hud_token_point:" + context);
    HudTokenPoint result{numberValue(parts[0], context + ".x"),
        numberValue(parts[1], context + ".y")};
    if (!inTokenBounds(result.x) || !inTokenBounds(result.y))
        throw TokenError("hud_token_geometry_bounds:" + context);
    return result;
}

double cross(const HudTokenPoint& first, const HudTokenPoint& second,
    const HudTokenPoint& third)
{
    return (second.x - first.x) * (third.y - first.y) -
        (second.y - first.y) * (third.x - first.x);
}

void validatePath(const std::vector<HudTokenPoint>& points, bool polygon,
    const std::string& context, bool allowConcave)
{
    const std::size_t minimum = polygon ? 3 : 2;
    if (points.size() < minimum)
        throw TokenError("hud_token_degenerate_geometry:" + context);
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (std::fabs(points[index].x - points[index - 1].x) <= kEpsilon &&
            std::fabs(points[index].y - points[index - 1].y) <= kEpsilon)
            throw TokenError("hud_token_degenerate_geometry:" + context);
    }
    if (!polygon) return;
    double area = 0.0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const HudTokenPoint& next = points[(index + 1) % points.size()];
        area += points[index].x * next.y - next.x * points[index].y;
    }
    if (std::fabs(area) <= kEpsilon)
        throw TokenError("hud_token_degenerate_geometry:" + context);
    if (allowConcave) return;
    double orientation = 0.0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const double turn = cross(points[index],
            points[(index + 1) % points.size()],
            points[(index + 2) % points.size()]);
        if (std::fabs(turn) <= kEpsilon) continue;
        if (orientation == 0.0) orientation = turn;
        else if ((orientation > 0.0) != (turn > 0.0))
            throw TokenError("hud_token_non_convex_polygon:" + context);
    }
}

UiColor parseColor(const std::string& text, const std::string& context)
{
    if (text.size() != 7 && text.size() != 9)
        throw TokenError("hud_token_palette_color:" + context);
    if (text[0] != '#') throw TokenError("hud_token_palette_color:" + context);
    auto nibble = [&](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        throw TokenError("hud_token_palette_color:" + context);
    };
    auto channel = [&](std::size_t offset) {
        return static_cast<float>((nibble(text[offset]) * 16 +
            nibble(text[offset + 1])) / 255.0);
    };
    return {channel(1), channel(3), channel(5),
        text.size() == 9 ? channel(7) : 1.0f};
}

const KeySet kPaletteRoles{"accent", "destructive", "focus",
    "icon_primary", "surface", "surface_strong", "text_primary",
    "text_secondary", "warning"};
const KeySet kShellPaletteRoles{"accent", "disabled", "error", "focus",
    "icon_primary", "surface", "text_primary", "text_secondary"};
const KeySet kAllPaletteRoles{"accent", "destructive", "disabled", "error",
    "focus", "icon_primary", "surface", "surface_strong", "text_primary",
    "text_secondary", "warning"};

std::string parseRole(const json::Value& command, const char* key,
    const std::string& context, bool optional)
{
    if (!command.has(key)) return optional ? "none" :
        throw TokenError("hud_token_required_field:" + context + ":" + key);
    const std::string role = stringValue(field(command, key, context),
        context + "." + key);
    if (role != "none" && kAllPaletteRoles.count(role) == 0)
        throw TokenError("hud_token_palette_role_unknown:" + context);
    return role;
}

HudTokenCommand parseCommand(const json::Value& value,
    const std::string& context, bool allowConcave = false)
{
    exactKeys(value, {"primitive", "fill_role", "stroke_role",
        "stroke_width", "x", "y", "width", "height", "cx", "cy",
        "radius", "points", "line_cap", "line_join"}, {"primitive"},
        context);
    const std::string primitive = stringValue(field(value, "primitive", context),
        context + ".primitive");
    HudTokenCommand result;
    if (primitive == "rect") result.kind = HudTokenCommandKind::Rect;
    else if (primitive == "circle") result.kind = HudTokenCommandKind::Circle;
    else if (primitive == "polyline") result.kind = HudTokenCommandKind::Polyline;
    else if (primitive == "polygon") result.kind = HudTokenCommandKind::Polygon;
    else throw TokenError("hud_token_primitive_unknown:" + context);
    result.fillRole = parseRole(value, "fill_role", context, true);
    result.strokeRole = parseRole(value, "stroke_role", context, true);
    if (value.has("stroke_width")) {
        result.strokeWidth = numberValue(field(value, "stroke_width", context),
            context + ".stroke_width");
        if (result.strokeWidth <= 0.0 || result.strokeWidth > kTokenExtent)
            throw TokenError("hud_token_stroke_width:" + context);
    }
    if ((result.strokeRole == "none") != (result.strokeWidth == 0.0))
        throw TokenError("hud_token_stroke_contract:" + context);
    if (result.fillRole == "none" && result.strokeRole == "none")
        throw TokenError("hud_token_invisible_geometry:" + context);

    if (result.kind == HudTokenCommandKind::Rect) {
        exactKeys(value, {"primitive", "fill_role", "stroke_role",
            "stroke_width", "x", "y", "width", "height"},
            {"primitive", "x", "y", "width", "height"}, context);
        result.x = numberValue(field(value, "x", context), context + ".x");
        result.y = numberValue(field(value, "y", context), context + ".y");
        result.width = numberValue(field(value, "width", context),
            context + ".width");
        result.height = numberValue(field(value, "height", context),
            context + ".height");
        if (result.width <= 0.0 || result.height <= 0.0 || result.x < 0.0 ||
            result.y < 0.0 || result.x + result.width > kTokenExtent ||
            result.y + result.height > kTokenExtent)
            throw TokenError("hud_token_degenerate_geometry:" + context);
    } else if (result.kind == HudTokenCommandKind::Circle) {
        exactKeys(value, {"primitive", "fill_role", "stroke_role",
            "stroke_width", "cx", "cy", "radius"},
            {"primitive", "cx", "cy", "radius"}, context);
        result.centerX = numberValue(field(value, "cx", context),
            context + ".cx");
        result.centerY = numberValue(field(value, "cy", context),
            context + ".cy");
        result.radius = numberValue(field(value, "radius", context),
            context + ".radius");
        if (result.radius <= 0.0 ||
            result.centerX - result.radius < 0.0 ||
            result.centerY - result.radius < 0.0 ||
            result.centerX + result.radius > kTokenExtent ||
            result.centerY + result.radius > kTokenExtent)
            throw TokenError("hud_token_degenerate_geometry:" + context);
    } else {
        exactKeys(value, {"primitive", "fill_role", "stroke_role",
            "stroke_width", "points", "line_cap", "line_join"},
            {"primitive", "points"}, context);
        const auto& points = array(field(value, "points", context),
            context + ".points");
        for (std::size_t index = 0; index < points.size(); ++index)
            result.points.push_back(point(points[index], context + ".points"));
        validatePath(result.points,
            result.kind == HudTokenCommandKind::Polygon, context,
            allowConcave);
        if (value.has("line_cap")) {
            expectString(field(value, "line_cap", context), "round",
                context + ".line_cap");
            result.roundCap = true;
        }
        if (value.has("line_join")) {
            expectString(field(value, "line_join", context), "round",
                context + ".line_join");
            result.roundJoin = true;
        }
        if (result.kind == HudTokenCommandKind::Polyline &&
            result.fillRole != "none")
            throw TokenError("hud_token_polyline_fill_forbidden:" + context);
    }
    return result;
}

HudIconToken parseIcon(const json::Value& value, const std::string& context)
{
    exactKeys(value, {"commands", "label_slot"}, {"commands"}, context);
    HudIconToken result;
    const auto& commands = array(field(value, "commands", context),
        context + ".commands");
    if (commands.empty()) throw TokenError("hud_token_icon_empty:" + context);
    for (std::size_t index = 0; index < commands.size(); ++index)
        result.commands.push_back(parseCommand(commands[index],
            context + ".commands[" + std::to_string(index) + "]"));
    if (value.has("label_slot")) {
        const json::Value& slot = field(value, "label_slot", context);
        exactKeys(slot, {"center", "font_role", "text"},
            {"center", "font_role", "text"}, context + ".label_slot");
        result.labelSlot.present = true;
        result.labelSlot.center = point(field(slot, "center", context),
            context + ".label_slot.center");
        result.labelSlot.fontRole = stringValue(field(slot, "font_role", context),
            context + ".label_slot.font_role");
        result.labelSlot.text = stringValue(field(slot, "text", context),
            context + ".label_slot.text");
        if (result.labelSlot.fontRole != "caption_sdf")
            throw TokenError("hud_token_label_font_contract:" + context);
    }
    return result;
}

HudIconToken parseShellIcon(const json::Value& value,
    const std::string& context)
{
    exactKeys(value, {"commands", "usage"}, {"commands", "usage"}, context);
    HudIconToken result;
    const auto& commands = array(field(value, "commands", context),
        context + ".commands");
    if (commands.empty()) throw TokenError("hud_token_icon_empty:" + context);
    for (std::size_t index = 0; index < commands.size(); ++index)
        result.commands.push_back(parseCommand(commands[index],
            context + ".commands[" + std::to_string(index) + "]", true));
    const json::Value& usage = field(value, "usage", context);
    exactKeys(usage, {"screens", "states"}, {"screens", "states"},
        context + ".usage");
    result.screens = stringArray(field(usage, "screens", context),
        context + ".usage.screens");
    result.states = stringArray(field(usage, "states", context),
        context + ".usage.states");
    if (result.screens.empty() || result.states.empty())
        throw TokenError("hud_token_usage_empty:" + context);
    return result;
}

HudTokenPalette parsePalette(const json::Value& value,
    const std::string& context)
{
    exactKeys(value, kPaletteRoles, kPaletteRoles, context);
    HudTokenPalette result;
    for (const std::string& role : kPaletteRoles)
        result.roles[role] = parseColor(stringValue(field(value, role.c_str(),
            context), context + "." + role), context + "." + role);
    return result;
}

HudTokenPalette parseShellPalette(const json::Value& value,
    const std::string& context)
{
    exactKeys(value, kShellPaletteRoles, kShellPaletteRoles, context);
    HudTokenPalette result;
    for (const std::string& role : kShellPaletteRoles)
        result.roles[role] = parseColor(stringValue(field(value, role.c_str(),
            context), context + "." + role), context + "." + role);
    return result;
}

bool sameStringSet(const std::vector<std::string>& first,
    const std::vector<std::string>& second)
{
    return std::set<std::string>(first.begin(), first.end()) ==
        std::set<std::string>(second.begin(), second.end()) &&
        first.size() == second.size();
}

void expectPair(const json::Value& value, int first, int second,
    const std::string& context);

void validateShellDebugContract(const json::Value& value,
    HudShippingTokenSet& result)
{
    const std::string context = "debug_isolation";
    exactKeys(value, {"debug_allowed_in_release_capture",
        "debug_default_visible", "debug_namespace",
        "shares_runtime_icon_nodes", "shipping_namespace"},
        {"debug_allowed_in_release_capture", "debug_default_visible",
        "debug_namespace", "shares_runtime_icon_nodes",
        "shipping_namespace"}, context);
    expectBool(field(value, "debug_allowed_in_release_capture", context),
        false, context + ".debug_allowed_in_release_capture");
    expectBool(field(value, "debug_default_visible", context), false,
        context + ".debug_default_visible");
    expectBool(field(value, "shares_runtime_icon_nodes", context), false,
        context + ".shares_runtime_icon_nodes");
    result.debugNamespace = stringValue(field(value, "debug_namespace",
        context), context + ".debug_namespace");
    const std::string shippingNamespace = stringValue(field(value,
        "shipping_namespace", context), context + ".shipping_namespace");
    if (result.debugNamespace != "debug_overlay" ||
        shippingNamespace != "product_shell" ||
        result.productNamespace != shippingNamespace)
        throw TokenError("hud_token_namespace_contract");
}

void validateShellGpuContract(const json::Value& value)
{
    const std::string context = "gpu_contract";
    exactKeys(value, {"allowed_primitives", "color_input",
        "coordinate_space", "curve_policy", "font_backend",
        "icon_expansion", "registry_policy", "stroke_units",
        "svg_runtime_dependency", "unknown_token_policy"},
        {"allowed_primitives", "color_input", "coordinate_space",
        "curve_policy", "font_backend", "icon_expansion",
        "registry_policy", "stroke_units", "svg_runtime_dependency",
        "unknown_token_policy"}, context);
    expectStrings(field(value, "allowed_primitives", context),
        {"rect", "circle", "polyline", "polygon"},
        context + ".allowed_primitives");
    expectString(field(value, "color_input", context),
        "srgb8_unpremultiplied_role_token", context + ".color_input");
    expectString(field(value, "coordinate_space", context),
        "normalized_24x24_top_left_y_down", context + ".coordinate_space");
    expectString(field(value, "curve_policy", context),
        "straight_segments_and_analytic_circles_only",
        context + ".curve_policy");
    expectString(field(value, "font_backend", context),
        "existing_freetype_sdf_atlas", context + ".font_backend");
    expectString(field(value, "icon_expansion", context),
        "cpu_expand_to_solid_and_line_triangles_before_gpu_submit",
        context + ".icon_expansion");
    expectString(field(value, "registry_policy", context),
        "standalone_product_shell_set_no_runtime_merge_required",
        context + ".registry_policy");
    expectString(field(value, "stroke_units", context),
        "logical_px_scaled_then_pixel_snapped", context + ".stroke_units");
    expectBool(field(value, "svg_runtime_dependency", context), false,
        context + ".svg_runtime_dependency");
    expectString(field(value, "unknown_token_policy", context),
        "hard_error_no_fallback", context + ".unknown_token_policy");
}

void validateShellAccessibility(const json::Value& value)
{
    const std::string context = "accessibility_contract";
    exactKeys(value, {"disabled_state_preserves_shape_and_text",
        "error_state_uses_octagon_cross_plus_error_text",
        "focus_state_uses_outline_plus_chevron_plus_text",
        "minimum_essential_graphic_contrast_ratio",
        "minimum_text_contrast_ratio", "never_color_only",
        "required_redundant_channels",
        "selected_state_uses_diamond_check_plus_text"},
        {"disabled_state_preserves_shape_and_text",
        "error_state_uses_octagon_cross_plus_error_text",
        "focus_state_uses_outline_plus_chevron_plus_text",
        "minimum_essential_graphic_contrast_ratio",
        "minimum_text_contrast_ratio", "never_color_only",
        "required_redundant_channels",
        "selected_state_uses_diamond_check_plus_text"}, context);
    expectBool(field(value, "disabled_state_preserves_shape_and_text",
        context), true, context + ".disabled_state_preserves_shape_and_text");
    expectBool(field(value, "error_state_uses_octagon_cross_plus_error_text",
        context), true,
        context + ".error_state_uses_octagon_cross_plus_error_text");
    expectBool(field(value, "focus_state_uses_outline_plus_chevron_plus_text",
        context), true,
        context + ".focus_state_uses_outline_plus_chevron_plus_text");
    expectNumber(field(value, "minimum_essential_graphic_contrast_ratio",
        context), 3.0, context + ".minimum_essential_graphic_contrast_ratio");
    expectNumber(field(value, "minimum_text_contrast_ratio", context), 4.5,
        context + ".minimum_text_contrast_ratio");
    expectBool(field(value, "never_color_only", context), true,
        context + ".never_color_only");
    expectStrings(field(value, "required_redundant_channels", context),
        {"shape", "adjacent_text_label", "state_or_value_text"},
        context + ".required_redundant_channels");
    expectBool(field(value, "selected_state_uses_diamond_check_plus_text",
        context), true,
        context + ".selected_state_uses_diamond_check_plus_text");
}

void validateShellTypography(const json::Value& value)
{
    const std::string context = "typography_and_scale";
    exactKeys(value, {"error_icon_minimum_logical_px",
        "focus_chevron_minimum_logical_px", "font_backend",
        "item_icon_recommended_logical_px", "minimum_text_logical_px",
        "minimum_text_retina_physical_px",
        "selected_icon_minimum_logical_px", "supported_viewports"},
        {"error_icon_minimum_logical_px",
        "focus_chevron_minimum_logical_px", "font_backend",
        "item_icon_recommended_logical_px", "minimum_text_logical_px",
        "minimum_text_retina_physical_px",
        "selected_icon_minimum_logical_px", "supported_viewports"}, context);
    expectNumber(field(value, "error_icon_minimum_logical_px", context), 22,
        context + ".error_icon_minimum_logical_px");
    expectNumber(field(value, "focus_chevron_minimum_logical_px", context), 10,
        context + ".focus_chevron_minimum_logical_px");
    expectString(field(value, "font_backend", context),
        "existing_freetype_sdf_atlas", context + ".font_backend");
    expectNumber(field(value, "item_icon_recommended_logical_px", context), 24,
        context + ".item_icon_recommended_logical_px");
    expectNumber(field(value, "minimum_text_logical_px", context), 14,
        context + ".minimum_text_logical_px");
    expectNumber(field(value, "minimum_text_retina_physical_px", context), 21,
        context + ".minimum_text_retina_physical_px");
    expectNumber(field(value, "selected_icon_minimum_logical_px", context), 14,
        context + ".selected_icon_minimum_logical_px");
    const auto& viewports = array(field(value, "supported_viewports", context),
        context + ".supported_viewports");
    if (viewports.size() != 3)
        throw TokenError("hud_token_contract_mismatch:" + context +
            ".supported_viewports");
    const int expected[3][5] = {{96, 1280, 720, 1280, 720},
        {96, 1920, 1080, 1920, 1080}, {144, 1280, 720, 1920, 1080}};
    for (std::size_t index = 0; index < viewports.size(); ++index) {
        const std::string name = context + ".supported_viewports[" +
            std::to_string(index) + "]";
        exactKeys(viewports[index], {"dpi", "logical", "physical"},
            {"dpi", "logical", "physical"}, name);
        expectNumber(field(viewports[index], "dpi", name), expected[index][0],
            name + ".dpi");
        expectPair(field(viewports[index], "logical", name),
            expected[index][1], expected[index][2], name + ".logical");
        expectPair(field(viewports[index], "physical", name),
            expected[index][3], expected[index][4], name + ".physical");
    }
}

void validateDebugContract(const json::Value& value,
    HudShippingTokenSet& result)
{
    const std::string context = "debug_isolation";
    exactKeys(value, {"debug_default_visible",
        "debug_forbidden_in_release_capture", "debug_token_namespace",
        "product_token_namespace", "shared_layout_nodes"},
        {"debug_default_visible", "debug_forbidden_in_release_capture",
        "debug_token_namespace", "product_token_namespace",
        "shared_layout_nodes"}, context);
    expectBool(field(value, "debug_default_visible", context), false,
        context + ".debug_default_visible");
    expectBool(field(value, "debug_forbidden_in_release_capture", context), true,
        context + ".debug_forbidden_in_release_capture");
    expectBool(field(value, "shared_layout_nodes", context), false,
        context + ".shared_layout_nodes");
    result.debugNamespace = stringValue(field(value, "debug_token_namespace",
        context), context + ".debug_token_namespace");
    result.productNamespace = stringValue(field(value,
        "product_token_namespace", context),
        context + ".product_token_namespace");
    if (result.debugNamespace != "debug_telemetry" ||
        result.productNamespace != "shipping_hud" ||
        result.debugNamespace == result.productNamespace)
        throw TokenError("hud_token_namespace_contract");
}

void validateGpuContract(const json::Value& value)
{
    const std::string context = "gpu_contract";
    exactKeys(value, {"allowed_primitives", "color_input",
        "coordinate_space", "curve_policy", "font_backend",
        "icon_expansion", "output_primitives", "stroke_units",
        "svg_runtime_dependency"}, {"allowed_primitives", "color_input",
        "coordinate_space", "curve_policy", "font_backend",
        "icon_expansion", "output_primitives", "stroke_units",
        "svg_runtime_dependency"}, context);
    expectStrings(field(value, "allowed_primitives", context),
        {"rect", "circle", "polyline", "polygon"},
        context + ".allowed_primitives");
    expectString(field(value, "color_input", context),
        "srgb8_unpremultiplied_role_token", context + ".color_input");
    expectString(field(value, "coordinate_space", context),
        "normalized_24x24_top_left_y_down", context + ".coordinate_space");
    expectString(field(value, "curve_policy", context),
        "straight_segments_only_no_runtime_tessellation",
        context + ".curve_policy");
    expectString(field(value, "font_backend", context),
        "existing_freetype_sdf_atlas", context + ".font_backend");
    expectString(field(value, "icon_expansion", context),
        "cpu_expand_to_solid_and_line_triangles_before_gpu_submit",
        context + ".icon_expansion");
    expectStrings(field(value, "output_primitives", context),
        {"Panel", "SolidRect", "Line", "Icon", "GlyphQuad"},
        context + ".output_primitives");
    expectString(field(value, "stroke_units", context),
        "logical_px_scaled_then_pixel_snapped", context + ".stroke_units");
    expectBool(field(value, "svg_runtime_dependency", context), false,
        context + ".svg_runtime_dependency");
}

void validateMargin(const json::Value& value, const std::string& context,
    double left, double top, double right, double bottom)
{
    exactKeys(value, {"bottom", "left", "right", "top"},
        {"bottom", "left", "right", "top"}, context);
    expectNumber(field(value, "left", context), left, context + ".left");
    expectNumber(field(value, "top", context), top, context + ".top");
    expectNumber(field(value, "right", context), right, context + ".right");
    expectNumber(field(value, "bottom", context), bottom, context + ".bottom");
}

void expectPair(const json::Value& value, int first, int second,
    const std::string& context)
{
    const auto& parts = array(value, context);
    if (parts.size() != 2 || integerValue(parts[0], context) != first ||
        integerValue(parts[1], context) != second)
        throw TokenError("hud_token_contract_mismatch:" + context);
}

void validateLayoutContract(const json::Value& value)
{
    const std::string context = "layout_contract";
    exactKeys(value, {"minimum_720p_safe_margin_px", "required_viewports",
        "retina_case", "wide_safe_margin_px"},
        {"minimum_720p_safe_margin_px", "required_viewports", "retina_case",
        "wide_safe_margin_px"}, context);
    validateMargin(field(value, "minimum_720p_safe_margin_px", context),
        context + ".minimum_720p_safe_margin_px", 43, 24, 43, 36);
    validateMargin(field(value, "wide_safe_margin_px", context),
        context + ".wide_safe_margin_px", 64, 36, 64, 54);
    const auto& viewports = array(field(value, "required_viewports", context),
        context + ".required_viewports");
    if (viewports.size() != 2)
        throw TokenError("hud_token_contract_mismatch:required_viewports");
    expectPair(viewports[0], 1280, 720, "required_viewports[0]");
    expectPair(viewports[1], 1920, 1080, "required_viewports[1]");
    const json::Value& retina = field(value, "retina_case", context);
    exactKeys(retina, {"logical_px", "physical_px", "scale"},
        {"logical_px", "physical_px", "scale"}, "retina_case");
    expectPair(field(retina, "logical_px", "retina_case"), 1280, 720,
        "retina_case.logical_px");
    expectPair(field(retina, "physical_px", "retina_case"), 1920, 1080,
        "retina_case.physical_px");
    expectNumber(field(retina, "scale", "retina_case"), 1.5,
        "retina_case.scale");
}

void validateAccessibility(const json::Value& value)
{
    const std::string context = "palette_accessibility";
    exactKeys(value, {"foul_encoding", "group_encoding", "never_color_only",
        "power_encoding", "required_redundant_channels", "supported_modes"},
        {"foul_encoding", "group_encoding", "never_color_only",
        "power_encoding", "required_redundant_channels", "supported_modes"},
        context);
    expectStrings(field(value, "foul_encoding", context),
        {"warning_shape", "FOUL_text", "reason_text"},
        context + ".foul_encoding");
    expectStrings(field(value, "group_encoding", context),
        {"solid_or_band_pattern", "shape", "text_label", "ball_number"},
        context + ".group_encoding");
    expectBool(field(value, "never_color_only", context), true,
        context + ".never_color_only");
    expectStrings(field(value, "power_encoding", context),
        {"bar_length", "segment_count", "numeric_percent"},
        context + ".power_encoding");
    expectStrings(field(value, "required_redundant_channels", context),
        {"shape", "text", "value_or_pattern"},
        context + ".required_redundant_channels");
    expectStrings(field(value, "supported_modes", context),
        {"standard", "color_vision_safe"}, context + ".supported_modes");
}

void validateTypography(const json::Value& value)
{
    const std::string context = "typography";
    exactKeys(value, {"backend", "compact_logical_px", "hierarchy",
        "minimum_logical_px", "retina_minimum_physical_px",
        "tabular_numeric_fields", "wide_logical_px"},
        {"backend", "compact_logical_px", "hierarchy",
        "minimum_logical_px", "retina_minimum_physical_px",
        "tabular_numeric_fields", "wide_logical_px"}, context);
    expectString(field(value, "backend", context),
        "existing_freetype_sdf_atlas", context + ".backend");
    expectStrings(field(value, "hierarchy", context),
        {"display", "primary", "body", "caption"}, context + ".hierarchy");
    expectNumber(field(value, "minimum_logical_px", context), 13,
        context + ".minimum_logical_px");
    expectNumber(field(value, "retina_minimum_physical_px", context), 21,
        context + ".retina_minimum_physical_px");
    expectStrings(field(value, "tabular_numeric_fields", context),
        {"power_percent", "cue_speed", "tip_offset", "ball_number"},
        context + ".tabular_numeric_fields");
    const auto validateSizes = [&](const json::Value& sizes,
            const std::string& name, int body, int caption, int display,
            int primary) {
        exactKeys(sizes, {"body", "caption", "display", "primary"},
            {"body", "caption", "display", "primary"}, name);
        expectNumber(field(sizes, "body", name), body, name + ".body");
        expectNumber(field(sizes, "caption", name), caption, name + ".caption");
        expectNumber(field(sizes, "display", name), display, name + ".display");
        expectNumber(field(sizes, "primary", name), primary, name + ".primary");
    };
    validateSizes(field(value, "compact_logical_px", context),
        context + ".compact_logical_px", 16, 13, 24, 20);
    validateSizes(field(value, "wide_logical_px", context),
        context + ".wide_logical_px", 18, 14, 32, 24);
}

product_hud::HudRect intersection(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    const float x0 = std::max(first.x, second.x);
    const float y0 = std::max(first.y, second.y);
    const float x1 = std::min(first.x + first.width, second.x + second.width);
    const float y1 = std::min(first.y + first.height, second.y + second.height);
    return {x0, y0, std::max(0.0f, x1 - x0),
        std::max(0.0f, y1 - y0)};
}

bool empty(const product_hud::HudRect& rect)
{
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

enum class ClipEdge { Left, Right, Top, Bottom };

bool inside(const UiPoint& point, const product_hud::HudRect& clip,
    ClipEdge edge)
{
    if (edge == ClipEdge::Left) return point.x >= clip.x;
    if (edge == ClipEdge::Right) return point.x <= clip.x + clip.width;
    if (edge == ClipEdge::Top) return point.y >= clip.y;
    return point.y <= clip.y + clip.height;
}

UiPoint edgeIntersection(const UiPoint& first, const UiPoint& second,
    const product_hud::HudRect& clip, ClipEdge edge)
{
    const bool vertical = edge == ClipEdge::Left || edge == ClipEdge::Right;
    const float boundary = edge == ClipEdge::Left ? clip.x :
        edge == ClipEdge::Right ? clip.x + clip.width :
        edge == ClipEdge::Top ? clip.y : clip.y + clip.height;
    const float denominator = vertical ? second.x - first.x :
        second.y - first.y;
    const float amount = std::fabs(denominator) <= 1e-12f ? 0.0f :
        (boundary - (vertical ? first.x : first.y)) / denominator;
    UiPoint result{first.x + (second.x - first.x) * amount,
        first.y + (second.y - first.y) * amount};
    if (vertical) result.x = boundary;
    else result.y = boundary;
    return result;
}

std::vector<UiPoint> clipPolygon(std::vector<UiPoint> polygon,
    const product_hud::HudRect& clip)
{
    for (ClipEdge edge : {ClipEdge::Left, ClipEdge::Right,
            ClipEdge::Top, ClipEdge::Bottom}) {
        if (polygon.empty()) break;
        std::vector<UiPoint> output;
        UiPoint previous = polygon.back();
        bool previousInside = inside(previous, clip, edge);
        for (const UiPoint& current : polygon) {
            const bool currentInside = inside(current, clip, edge);
            if (currentInside != previousInside)
                output.push_back(edgeIntersection(previous, current, clip, edge));
            if (currentInside) output.push_back(current);
            previous = current;
            previousInside = currentInside;
        }
        polygon = std::move(output);
    }
    return polygon;
}

void emitPolygon(HudDrawList& draw, const std::vector<UiPoint>& polygon,
    const product_hud::HudRect& clip, const UiColor& color,
    const std::string& sourceToken, const std::string& resolvedToken)
{
    auto turn = [](const UiPoint& first, const UiPoint& second,
            const UiPoint& third) {
        return (second.x - first.x) * (third.y - first.y) -
            (second.y - first.y) * (third.x - first.x);
    };
    bool convex = true;
    float orientation = 0.0f;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const float value = turn(polygon[index],
            polygon[(index + 1) % polygon.size()],
            polygon[(index + 2) % polygon.size()]);
        if (std::fabs(value) <= 1e-7f) continue;
        if (orientation == 0.0f) orientation = value;
        else if ((orientation > 0.0f) != (value > 0.0f)) {
            convex = false;
            break;
        }
    }
    if (!convex) {
        float signedArea = 0.0f;
        for (std::size_t index = 0; index < polygon.size(); ++index) {
            const UiPoint& next = polygon[(index + 1) % polygon.size()];
            signedArea += polygon[index].x * next.y - next.x * polygon[index].y;
        }
        const bool counterClockwise = signedArea > 0.0f;
        std::vector<std::size_t> indices(polygon.size());
        for (std::size_t index = 0; index < indices.size(); ++index)
            indices[index] = index;
        auto inTriangle = [&](const UiPoint& point, const UiPoint& first,
                const UiPoint& second, const UiPoint& third) {
            const float a = turn(first, second, point);
            const float b = turn(second, third, point);
            const float c = turn(third, first, point);
            return counterClockwise ?
                a >= -1e-7f && b >= -1e-7f && c >= -1e-7f :
                a <= 1e-7f && b <= 1e-7f && c <= 1e-7f;
        };
        while (indices.size() > 3) {
            bool emitted = false;
            for (std::size_t index = 0; index < indices.size(); ++index) {
                const std::size_t previous = indices[
                    (index + indices.size() - 1) % indices.size()];
                const std::size_t current = indices[index];
                const std::size_t next = indices[(index + 1) % indices.size()];
                const float corner = turn(polygon[previous], polygon[current],
                    polygon[next]);
                if ((counterClockwise && corner <= 1e-7f) ||
                    (!counterClockwise && corner >= -1e-7f))
                    continue;
                bool contains = false;
                for (const std::size_t candidate : indices) {
                    if (candidate == previous || candidate == current ||
                        candidate == next)
                        continue;
                    if (inTriangle(polygon[candidate], polygon[previous],
                            polygon[current], polygon[next])) {
                        contains = true;
                        break;
                    }
                }
                if (contains) continue;
                emitPolygon(draw, {polygon[previous], polygon[current],
                    polygon[next]}, clip, color, sourceToken, resolvedToken);
                indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(index));
                emitted = true;
                break;
            }
            if (!emitted)
                throw TokenError("hud_token_polygon_triangulation_failed:" +
                    sourceToken);
        }
        emitPolygon(draw, {polygon[indices[0]], polygon[indices[1]],
            polygon[indices[2]]}, clip, color, sourceToken, resolvedToken);
        return;
    }
    const std::vector<UiPoint> clipped = clipPolygon(polygon, clip);
    if (clipped.size() < 3) return;
    for (std::size_t index = 1; index + 1 < clipped.size(); ++index) {
        const float area = (clipped[index].x - clipped[0].x) *
                (clipped[index + 1].y - clipped[0].y) -
            (clipped[index].y - clipped[0].y) *
                (clipped[index + 1].x - clipped[0].x);
        if (std::fabs(area) <= 1e-7f) continue;
        draw.triangles.push_back({clipped[0], clipped[index],
            clipped[index + 1], color, clip, sourceToken, resolvedToken});
    }
}

UiPoint transform(const HudTokenPoint& point,
    const product_hud::HudRect& rect)
{
    return {rect.x + static_cast<float>(point.x / kTokenExtent) * rect.width,
        rect.y + static_cast<float>(point.y / kTokenExtent) * rect.height};
}

void emitDisk(HudDrawList& draw, const UiPoint& center, float radius,
    int segments, const product_hud::HudRect& clip, const UiColor& color,
    const std::string& sourceToken, const std::string& resolvedToken)
{
    constexpr double pi = 3.1415926535897932384626433832795;
    for (int index = 0; index < segments; ++index) {
        const double firstAngle = 2.0 * pi * index / segments;
        const double secondAngle = 2.0 * pi * (index + 1) / segments;
        emitPolygon(draw, {center,
            {center.x + radius * static_cast<float>(std::cos(firstAngle)),
                center.y + radius * static_cast<float>(std::sin(firstAngle))},
            {center.x + radius * static_cast<float>(std::cos(secondAngle)),
                center.y + radius * static_cast<float>(std::sin(secondAngle))}},
            clip, color, sourceToken, resolvedToken);
    }
}

void emitSegment(HudDrawList& draw, const UiPoint& first,
    const UiPoint& second, float thickness,
    const product_hud::HudRect& clip, const UiColor& color,
    const std::string& sourceToken, const std::string& resolvedToken)
{
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 1e-7f) return;
    const float nx = -dy / length * thickness * 0.5f;
    const float ny = dx / length * thickness * 0.5f;
    emitPolygon(draw, {{first.x + nx, first.y + ny},
        {second.x + nx, second.y + ny},
        {second.x - nx, second.y - ny},
        {first.x - nx, first.y - ny}}, clip, color, sourceToken,
        resolvedToken);
}

const UiColor& roleColor(const HudTokenPalette& palette,
    const std::string& role)
{
    const auto found = palette.roles.find(role);
    if (found == palette.roles.end())
        throw TokenError("hud_token_palette_role_unknown:" + role);
    return found->second;
}

void emitStrokePath(HudDrawList& draw, const std::vector<UiPoint>& points,
    bool closed, bool roundCap, bool roundJoin, float thickness,
    const product_hud::HudRect& clip, const UiColor& color,
    const std::string& sourceToken, const std::string& resolvedToken)
{
    const std::size_t segmentCount = closed ? points.size() : points.size() - 1;
    for (std::size_t index = 0; index < segmentCount; ++index)
        emitSegment(draw, points[index], points[(index + 1) % points.size()],
            thickness, clip, color, sourceToken, resolvedToken);
    if (roundJoin) {
        const std::size_t begin = closed ? 0 : 1;
        const std::size_t end = closed ? points.size() : points.size() - 1;
        for (std::size_t index = begin; index < end; ++index)
            emitDisk(draw, points[index], thickness * 0.5f, kRoundSegments,
                clip, color, sourceToken, resolvedToken);
    }
    if (!closed && roundCap) {
        emitDisk(draw, points.front(), thickness * 0.5f, kRoundSegments,
            clip, color, sourceToken, resolvedToken);
        emitDisk(draw, points.back(), thickness * 0.5f, kRoundSegments,
            clip, color, sourceToken, resolvedToken);
    }
}

void expandCommand(HudDrawList& draw, const HudTokenCommand& command,
    const product_hud::HudRect& rect, const product_hud::HudRect& clip,
    const HudTokenPalette& palette, const std::string& sourceToken,
    const std::string& resolvedToken)
{
    std::vector<UiPoint> points;
    if (command.kind == HudTokenCommandKind::Rect) {
        points = {transform({command.x, command.y}, rect),
            transform({command.x + command.width, command.y}, rect),
            transform({command.x + command.width,
                command.y + command.height}, rect),
            transform({command.x, command.y + command.height}, rect)};
    } else if (command.kind == HudTokenCommandKind::Polyline ||
            command.kind == HudTokenCommandKind::Polygon) {
        for (const HudTokenPoint& value : command.points)
            points.push_back(transform(value, rect));
    }
    if (command.kind == HudTokenCommandKind::Circle) {
        const UiPoint center = transform(
            {command.centerX, command.centerY}, rect);
        const float radius = static_cast<float>(command.radius / kTokenExtent) *
            std::min(rect.width, rect.height);
        if (command.fillRole != "none")
            emitDisk(draw, center, radius, kCircleSegments, clip,
                roleColor(palette, command.fillRole), sourceToken, resolvedToken);
        if (command.strokeRole != "none") {
            std::vector<UiPoint> ring;
            constexpr double pi = 3.1415926535897932384626433832795;
            for (int index = 0; index < kCircleSegments; ++index) {
                const double angle = 2.0 * pi * index / kCircleSegments;
                ring.push_back({center.x + radius *
                        static_cast<float>(std::cos(angle)),
                    center.y + radius * static_cast<float>(std::sin(angle))});
            }
            const float thickness = std::max(1.0f, std::round(
                static_cast<float>(command.strokeWidth / kTokenExtent) *
                std::min(rect.width, rect.height)));
            emitStrokePath(draw, ring, true, false, true, thickness, clip,
                roleColor(palette, command.strokeRole), sourceToken,
                resolvedToken);
        }
        return;
    }
    const bool closed = command.kind == HudTokenCommandKind::Rect ||
        command.kind == HudTokenCommandKind::Polygon;
    if (closed && command.fillRole != "none")
        emitPolygon(draw, points, clip, roleColor(palette, command.fillRole),
            sourceToken, resolvedToken);
    if (command.strokeRole != "none") {
        const float thickness = std::max(1.0f, std::round(
            static_cast<float>(command.strokeWidth / kTokenExtent) *
            std::min(rect.width, rect.height)));
        emitStrokePath(draw, points, closed, command.roundCap,
            command.roundJoin, thickness, clip,
            roleColor(palette, command.strokeRole), sourceToken, resolvedToken);
    }
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

void appendString(std::vector<std::uint8_t>& bytes, const std::string& value)
{
    appendU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

std::int32_t quantized(float value)
{
    if (!std::isfinite(value)) throw TokenError("hud_triangle_non_finite");
    const double scaled = std::round(static_cast<double>(value) * 1024.0);
    if (scaled < std::numeric_limits<std::int32_t>::min() ||
        scaled > std::numeric_limits<std::int32_t>::max())
        throw TokenError("hud_triangle_out_of_range");
    return static_cast<std::int32_t>(scaled);
}

std::uint8_t colorByte(float value)
{
    if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
        throw TokenError("hud_triangle_color_invalid");
    return static_cast<std::uint8_t>(std::round(value * 255.0f));
}

bool samePoint(const HudTokenPoint& first, const HudTokenPoint& second)
{
    return first.x == second.x && first.y == second.y;
}

bool sameCommand(const HudTokenCommand& first, const HudTokenCommand& second)
{
    if (first.kind != second.kind || first.fillRole != second.fillRole ||
        first.strokeRole != second.strokeRole ||
        first.strokeWidth != second.strokeWidth || first.x != second.x ||
        first.y != second.y || first.width != second.width ||
        first.height != second.height || first.centerX != second.centerX ||
        first.centerY != second.centerY || first.radius != second.radius ||
        first.roundCap != second.roundCap ||
        first.roundJoin != second.roundJoin ||
        first.points.size() != second.points.size())
        return false;
    for (std::size_t index = 0; index < first.points.size(); ++index)
        if (!samePoint(first.points[index], second.points[index])) return false;
    return true;
}

bool sameIcon(const HudIconToken& first, const HudIconToken& second)
{
    if (first.commands.size() != second.commands.size() ||
        first.labelSlot.present != second.labelSlot.present)
        return false;
    for (std::size_t index = 0; index < first.commands.size(); ++index)
        if (!sameCommand(first.commands[index], second.commands[index]))
            return false;
    return !first.labelSlot.present ||
        (samePoint(first.labelSlot.center, second.labelSlot.center) &&
            first.labelSlot.fontRole == second.labelSlot.fontRole &&
            first.labelSlot.text == second.labelSlot.text);
}

}  // namespace

bool HudShippingTokenSet::resolve(const std::string& sourceToken,
    std::string& resolvedToken, std::string& error) const
{
    resolvedToken.clear();
    error.clear();
    if (!ok) {
        error = errorCode.empty() ? "hud_token_set_invalid" : errorCode;
        return false;
    }
    if (sourceToken.empty() ||
        sourceToken.rfind(debugNamespace, 0) == 0 ||
        sourceToken.rfind("debug_", 0) == 0) {
        error = "hud_shipping_debug_token_forbidden";
        return false;
    }
    std::set<std::string> visited;
    std::string current = sourceToken;
    while (true) {
        if (!visited.insert(current).second) {
            error = "hud_token_alias_cycle";
            return false;
        }
        const auto icon = icons.find(current);
        if (icon != icons.end()) {
            resolvedToken = current;
            return true;
        }
        const auto alias = aliases.find(current);
        if (alias == aliases.end()) {
            error = "hud_icon_token_unknown:" + sourceToken;
            return false;
        }
        current = alias->second;
    }
}

bool HudShippingTokenRegistry::resolve(const std::string& sourceToken,
    std::string& resolvedToken, std::string& error) const
{
    resolvedToken.clear();
    error.clear();
    if (!ok) {
        error = errorCode.empty() ? "hud_token_registry_invalid" : errorCode;
        return false;
    }
    const bool debugPrefixed = std::any_of(debugNamespaces.begin(),
        debugNamespaces.end(), [&](const std::string& prefix) {
            return !prefix.empty() && sourceToken.rfind(prefix, 0) == 0;
        });
    if (sourceToken.empty() || debugPrefixed ||
        sourceToken.rfind(debugNamespace, 0) == 0 ||
        sourceToken.rfind("debug_", 0) == 0) {
        error = "hud_shipping_debug_token_forbidden";
        return false;
    }
    std::set<std::string> visited;
    std::string current = sourceToken;
    while (true) {
        if (!visited.insert(current).second) {
            error = "hud_token_alias_cycle";
            return false;
        }
        if (icons.count(current) != 0) {
            resolvedToken = current;
            return true;
        }
        const auto alias = aliases.find(current);
        if (alias == aliases.end()) {
            error = "hud_icon_token_unknown:" + sourceToken;
            return false;
        }
        current = alias->second;
    }
}

HudShippingTokenSet parseHudShippingTokenSet(const std::string& jsonBytes)
{
    HudShippingTokenSet result;
    const json::ParseResult parsed = json::parse(jsonBytes);
    if (!parsed.ok) {
        result.errorCode = "hud_token_json_invalid";
        return result;
    }
    try {
        exactKeys(parsed.value, {"aliases", "authorship", "debug_isolation",
            "external_assets", "gpu_contract", "icon_tokens",
            "layout_contract", "palette_accessibility", "palette_modes",
            "schema_version", "state_icon_usage", "token_set_id",
            "typography"}, {"aliases", "authorship", "debug_isolation",
            "external_assets", "gpu_contract", "icon_tokens",
            "layout_contract", "palette_accessibility", "palette_modes",
            "schema_version", "state_icon_usage", "token_set_id",
            "typography"}, "root");
        result.schemaVersion = integerValue(
            field(parsed.value, "schema_version", "root"), "schema_version");
        if (result.schemaVersion != 1)
            throw TokenError("hud_token_schema_version_unsupported");
        result.tokenSetId = stringValue(
            field(parsed.value, "token_set_id", "root"), "token_set_id");
        if (result.tokenSetId != "focused_practice_hud_shipping_tokens_v1")
            throw TokenError("hud_token_set_id_mismatch");
        result.authorship = stringValue(
            field(parsed.value, "authorship", "root"), "authorship");
        if (result.authorship != "original_billiardgl_vector_tokens")
            throw TokenError("hud_token_authorship_unapproved");
        result.externalAssets = boolValue(
            field(parsed.value, "external_assets", "root"), "external_assets");
        if (result.externalAssets)
            throw TokenError("hud_token_external_assets_forbidden");

        validateDebugContract(field(parsed.value, "debug_isolation", "root"),
            result);
        validateGpuContract(field(parsed.value, "gpu_contract", "root"));
        validateLayoutContract(field(parsed.value, "layout_contract", "root"));
        validateAccessibility(field(parsed.value, "palette_accessibility", "root"));
        validateTypography(field(parsed.value, "typography", "root"));

        const KeySet expectedIcons{"ball_outside_rectangle", "camera_frame",
            "crossed_circle", "cue_ball_down_arrow", "eight_ball_octagon",
            "group_open_diamond", "group_solid_filled_circle",
            "group_stripe_banded_ring", "help_circle", "power_bars",
            "question_diamond", "rail_with_slash", "tip_ball_outline",
            "tip_position_marker", "tip_target", "warning_triangle"};
        const json::Value& icons = field(parsed.value, "icon_tokens", "root");
        exactKeys(icons, expectedIcons, expectedIcons, "icon_tokens");
        for (const auto& entry : icons.asObject()) {
            if (entry.first.rfind(result.debugNamespace, 0) == 0 ||
                entry.first.rfind("debug_", 0) == 0)
                throw TokenError("hud_shipping_debug_token_forbidden");
            result.icons[entry.first] = parseIcon(entry.second,
                "icon_tokens." + entry.first);
        }

        const json::Value& aliases = field(parsed.value, "aliases", "root");
        object(aliases, "aliases");
        for (const auto& entry : aliases.asObject()) {
            if (entry.first.empty() || result.icons.count(entry.first) != 0 ||
                entry.first.rfind(result.debugNamespace, 0) == 0 ||
                entry.first.rfind("debug_", 0) == 0)
                throw TokenError("hud_token_alias_invalid:" + entry.first);
            result.aliases[entry.first] = stringValue(entry.second,
                "aliases." + entry.first);
        }
        if (result.aliases.size() != 10)
            throw TokenError("hud_token_alias_count_mismatch");

        const json::Value& paletteModes =
            field(parsed.value, "palette_modes", "root");
        exactKeys(paletteModes, {"standard", "color_vision_safe"},
            {"standard", "color_vision_safe"}, "palette_modes");
        result.palettes[HudPaletteMode::Standard] = parsePalette(
            field(paletteModes, "standard", "palette_modes"),
            "palette_modes.standard");
        result.palettes[HudPaletteMode::ColorVisionSafe] = parsePalette(
            field(paletteModes, "color_vision_safe", "palette_modes"),
            "palette_modes.color_vision_safe");

        const std::map<std::string, std::vector<std::string>> expectedUsage{
            {"aiming", {"power_bars", "tip_target", "camera_frame"}},
            {"default", {"group_solid_filled_circle",
                "group_stripe_banded_ring", "camera_frame"}},
            {"foul", {"warning_triangle"}},
            {"help", {"help_circle", "power_bars", "tip_target",
                "camera_frame"}},
            {"post_shot", {"camera_frame"}}};
        const json::Value& usage = field(parsed.value, "state_icon_usage", "root");
        KeySet stateKeys;
        for (const auto& entry : expectedUsage) stateKeys.insert(entry.first);
        exactKeys(usage, stateKeys, stateKeys, "state_icon_usage");
        for (const auto& expected : expectedUsage) {
            const std::vector<std::string> values = stringArray(
                field(usage, expected.first.c_str(), "state_icon_usage"),
                "state_icon_usage." + expected.first);
            if (values != expected.second)
                throw TokenError("hud_token_state_usage_mismatch:" +
                    expected.first);
            result.stateIconUsage[expected.first] = values;
        }

        // Mark valid before resolving aliases because resolve deliberately
        // refuses to operate on a partially validated token set.
        result.ok = true;
        for (const auto& alias : result.aliases) {
            std::string resolved;
            std::string error;
            if (!result.resolve(alias.first, resolved, error))
                throw TokenError(error);
        }
        for (const auto& state : result.stateIconUsage) {
            for (const std::string& token : state.second) {
                std::string resolved;
                std::string error;
                if (!result.resolve(token, resolved, error))
                    throw TokenError(error);
            }
        }
    } catch (const TokenError& error) {
        result.ok = false;
        result.errorCode = error.what();
    } catch (const std::exception&) {
        result.ok = false;
        result.errorCode = "hud_token_schema_type_invalid";
    }
    return result;
}

HudShippingTokenSet parseShellShippingTokenSet(const std::string& jsonBytes)
{
    HudShippingTokenSet result;
    const json::ParseResult parsed = json::parse(jsonBytes);
    if (!parsed.ok) {
        result.errorCode = "hud_token_json_invalid";
        return result;
    }
    try {
        exactKeys(parsed.value, {"accessibility_contract", "aliases",
            "authorship", "debug_isolation", "external_assets",
            "global_state_icon_usage", "gpu_contract", "icon_tokens",
            "namespace", "palette_modes", "schema_version",
            "screen_icon_usage", "source_audit", "token_set_id",
            "typography_and_scale"}, {"accessibility_contract", "aliases",
            "authorship", "debug_isolation", "external_assets",
            "global_state_icon_usage", "gpu_contract", "icon_tokens",
            "namespace", "palette_modes", "schema_version",
            "screen_icon_usage", "source_audit", "token_set_id",
            "typography_and_scale"}, "root");
        result.schemaVersion = integerValue(field(parsed.value,
            "schema_version", "root"), "schema_version");
        if (result.schemaVersion != 1)
            throw TokenError("hud_token_schema_version_unsupported");
        result.tokenSetId = stringValue(field(parsed.value, "token_set_id",
            "root"), "token_set_id");
        if (result.tokenSetId != "focused_practice_shell_shipping_tokens_v1")
            throw TokenError("hud_token_set_id_mismatch");
        result.authorship = stringValue(field(parsed.value, "authorship",
            "root"), "authorship");
        if (result.authorship != "original_billiardgl_vector_tokens")
            throw TokenError("hud_token_authorship_unapproved");
        result.productNamespace = stringValue(field(parsed.value, "namespace",
            "root"), "namespace");
        if (result.productNamespace != "product_shell")
            throw TokenError("hud_token_namespace_contract");
        const auto& externalAssets = array(field(parsed.value,
            "external_assets", "root"), "external_assets");
        if (!externalAssets.empty())
            throw TokenError("hud_token_external_assets_forbidden");
        result.externalAssets = false;

        validateShellDebugContract(field(parsed.value, "debug_isolation",
            "root"), result);
        validateShellGpuContract(field(parsed.value, "gpu_contract", "root"));
        validateShellAccessibility(field(parsed.value,
            "accessibility_contract", "root"));
        validateShellTypography(field(parsed.value, "typography_and_scale",
            "root"));

        const KeySet expectedIcons{"ai_diamond", "aim_crosshair",
            "assist_rings", "back_arrow", "camera_frame", "camera_motion",
            "caption_box", "catalog_grid", "cue_ball", "distance_arrow",
            "empty_box", "error_cross_octagon", "exit_door",
            "focus_brackets", "focus_chevron", "glow_circle", "help_circle",
            "impact_burst", "item_square", "keyboard_outline", "mouse_axis",
            "music_note", "pattern_circle", "pause_bars", "play_triangle",
            "power_bars", "replay_file", "replay_triangle", "reset_arrow",
            "rules_shield", "seek_next", "seek_previous",
            "selected_check_diamond", "settings_sliders", "shake_lines",
            "single_player_badge", "speed_down", "speed_up", "text_scale",
            "training_target", "ui_click", "venue_waves",
            "vertical_arrows"};
        const json::Value& icons = field(parsed.value, "icon_tokens", "root");
        exactKeys(icons, expectedIcons, expectedIcons, "icon_tokens");
        for (const auto& entry : icons.asObject()) {
            if (entry.first.rfind(result.debugNamespace, 0) == 0 ||
                entry.first.rfind("debug_", 0) == 0)
                throw TokenError("hud_shipping_debug_token_forbidden");
            result.icons[entry.first] = parseShellIcon(entry.second,
                "icon_tokens." + entry.first);
        }

        const json::Value& aliases = field(parsed.value, "aliases", "root");
        exactKeys(aliases, {}, {}, "aliases");

        const json::Value& paletteModes = field(parsed.value,
            "palette_modes", "root");
        exactKeys(paletteModes, {"standard", "color_vision_safe"},
            {"standard", "color_vision_safe"}, "palette_modes");
        result.palettes[HudPaletteMode::Standard] = parseShellPalette(
            field(paletteModes, "standard", "palette_modes"),
            "palette_modes.standard");
        result.palettes[HudPaletteMode::ColorVisionSafe] = parseShellPalette(
            field(paletteModes, "color_vision_safe", "palette_modes"),
            "palette_modes.color_vision_safe");

        const std::map<std::string, std::vector<std::string>> expectedScreens{
            {"help", {"aim_crosshair", "power_bars", "cue_ball",
                "camera_frame", "rules_shield", "back_arrow"}},
            {"main_menu", {"single_player_badge", "training_target",
                "replay_triangle", "settings_sliders", "help_circle",
                "exit_door"}},
            {"match", {"rules_shield", "settings_sliders", "help_circle"}},
            {"replay_browser", {"replay_file", "empty_box"}},
            {"replay_playback", {"play_triangle", "pause_bars", "speed_down",
                "speed_up", "camera_frame", "seek_previous", "seek_next",
                "replay_file"}},
            {"settings", {"assist_rings", "mouse_axis", "vertical_arrows",
                "keyboard_outline", "text_scale", "pattern_circle",
                "caption_box", "music_note", "venue_waves", "impact_burst",
                "ui_click", "camera_frame", "distance_arrow", "glow_circle",
                "focus_brackets", "camera_motion", "shake_lines"}},
            {"single_player_setup", {"rules_shield", "ai_diamond",
                "play_triangle"}},
            {"training_catalog", {"training_target", "empty_box"}},
            {"training_session", {"training_target", "reset_arrow",
                "catalog_grid"}}};
        const std::map<std::string, std::vector<std::string>> expectedStates{
            {"disabled", {"ai_diamond", "aim_crosshair", "assist_rings",
                "back_arrow", "camera_frame", "camera_motion", "caption_box",
                "catalog_grid", "cue_ball", "distance_arrow", "empty_box",
                "exit_door", "focus_brackets", "glow_circle", "help_circle",
                "impact_burst", "keyboard_outline", "mouse_axis", "music_note",
                "pattern_circle", "pause_bars", "play_triangle", "power_bars",
                "replay_file", "replay_triangle", "reset_arrow", "rules_shield",
                "seek_next", "seek_previous", "settings_sliders", "shake_lines",
                "single_player_badge", "speed_down", "speed_up", "text_scale",
                "training_target", "ui_click", "venue_waves",
                "vertical_arrows"}},
            {"error", {"error_cross_octagon"}},
            {"fallback_item", {"empty_box", "item_square"}},
            {"focus", {"focus_chevron"}},
            {"normal", {"ai_diamond", "aim_crosshair", "assist_rings",
                "back_arrow", "camera_frame", "camera_motion", "caption_box",
                "catalog_grid", "cue_ball", "distance_arrow", "empty_box",
                "exit_door", "focus_brackets", "glow_circle", "help_circle",
                "impact_burst", "keyboard_outline", "mouse_axis", "music_note",
                "pattern_circle", "pause_bars", "play_triangle", "power_bars",
                "replay_file", "replay_triangle", "reset_arrow", "rules_shield",
                "seek_next", "seek_previous", "settings_sliders", "shake_lines",
                "single_player_badge", "speed_down", "speed_up", "text_scale",
                "training_target", "ui_click", "venue_waves",
                "vertical_arrows"}},
            {"selected", {"selected_check_diamond"}}};
        auto parseUsage = [&](const char* key,
                const std::map<std::string, std::vector<std::string>>& expected,
                std::map<std::string, std::vector<std::string>>& output) {
            const json::Value& usage = field(parsed.value, key, "root");
            KeySet keys;
            for (const auto& entry : expected) keys.insert(entry.first);
            exactKeys(usage, keys, keys, key);
            for (const auto& entry : expected) {
                const std::vector<std::string> values = stringArray(field(usage,
                    entry.first.c_str(), key), std::string(key) + "." +
                    entry.first);
                if (values != entry.second)
                    throw TokenError("hud_token_contract_mismatch:" +
                        std::string(key) + "." + entry.first);
                output[entry.first] = values;
            }
        };
        parseUsage("screen_icon_usage", expectedScreens,
            result.screenIconUsage);
        parseUsage("global_state_icon_usage", expectedStates,
            result.stateIconUsage);

        for (const auto& icon : result.icons) {
            std::vector<std::string> screens;
            std::vector<std::string> states;
            for (const auto& usage : result.screenIconUsage)
                if (std::find(usage.second.begin(), usage.second.end(),
                        icon.first) != usage.second.end())
                    screens.push_back(usage.first);
            for (const auto& usage : result.stateIconUsage)
                if (std::find(usage.second.begin(), usage.second.end(),
                        icon.first) != usage.second.end())
                    states.push_back(usage.first);
            if (screens.empty()) screens.push_back("all_screens");
            if (!sameStringSet(icon.second.screens, screens) ||
                !sameStringSet(icon.second.states, states))
                throw TokenError("hud_token_usage_mismatch:" + icon.first);
        }

        const json::Value& audit = field(parsed.value, "source_audit", "root");
        exactKeys(audit, {"exact_emitted_icon_tokens", "screen_count",
            "screen_enum", "source_files"}, {"exact_emitted_icon_tokens",
            "screen_count", "screen_enum", "source_files"}, "source_audit");
        const std::vector<std::string> audited = stringArray(field(audit,
            "exact_emitted_icon_tokens", "source_audit"),
            "source_audit.exact_emitted_icon_tokens");
        if (std::set<std::string>(audited.begin(), audited.end()) !=
                expectedIcons || audited.size() != expectedIcons.size())
            throw TokenError("hud_token_contract_mismatch:source_audit.exact_emitted_icon_tokens");
        expectNumber(field(audit, "screen_count", "source_audit"), 9,
            "source_audit.screen_count");
        expectStrings(field(audit, "screen_enum", "source_audit"),
            {"MainMenu", "SinglePlayerSetup", "Match", "TrainingCatalog",
                "TrainingSession", "ReplayBrowser", "ReplayPlayback",
                "Settings", "Help"}, "source_audit.screen_enum");
        expectStrings(field(audit, "source_files", "source_audit"),
            {"src/Billiards/render/ui/product_shell_draw_list.cpp",
                "src/Billiards/product_shell_controller.cpp",
                "src/Billiards/product_shell_controller.h"},
            "source_audit.source_files");

        result.ok = true;
    } catch (const TokenError& error) {
        result.ok = false;
        result.errorCode = error.what();
    } catch (const std::exception&) {
        result.ok = false;
        result.errorCode = "hud_token_schema_type_invalid";
    }
    return result;
}

HudShippingTokenSet loadHudShippingTokenSet(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        HudShippingTokenSet result;
        result.errorCode = "hud_token_file_unreadable";
        return result;
    }
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    return parseHudShippingTokenSet(bytes);
}

HudShippingTokenSet loadShellShippingTokenSet(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        HudShippingTokenSet result;
        result.errorCode = "hud_token_file_unreadable";
        return result;
    }
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    return parseShellShippingTokenSet(bytes);
}

HudShippingTokenSet parseShippingTokenSet(const std::string& jsonBytes)
{
    const json::ParseResult parsed = json::parse(jsonBytes);
    if (!parsed.ok) {
        HudShippingTokenSet result;
        result.errorCode = "hud_token_json_invalid";
        return result;
    }
    try {
        const std::string id = stringValue(field(parsed.value, "token_set_id",
            "root"), "token_set_id");
        if (id == "focused_practice_hud_shipping_tokens_v1")
            return parseHudShippingTokenSet(jsonBytes);
        if (id == "focused_practice_shell_shipping_tokens_v1")
            return parseShellShippingTokenSet(jsonBytes);
        HudShippingTokenSet result;
        result.errorCode = "hud_token_set_id_unknown:" + id;
        return result;
    } catch (const TokenError& error) {
        HudShippingTokenSet result;
        result.errorCode = error.what();
        return result;
    }
}

HudShippingTokenSet loadShippingTokenSet(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        HudShippingTokenSet result;
        result.errorCode = "hud_token_file_unreadable";
        return result;
    }
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    return parseShippingTokenSet(bytes);
}

HudShippingTokenRegistry mergeHudShippingTokenSets(
    const std::vector<HudShippingTokenSet>& tokenSets)
{
    HudShippingTokenRegistry result;
    if (tokenSets.empty()) {
        result.errorCode = "hud_token_registry_empty";
        return result;
    }
    auto sameColor = [](const UiColor& first, const UiColor& second) {
        return first.red == second.red && first.green == second.green &&
            first.blue == second.blue && first.alpha == second.alpha;
    };
    std::map<std::string, std::string> iconNamespaces;
    std::map<std::string, std::string> aliasNamespaces;
    for (const HudShippingTokenSet& set : tokenSets) {
        if (!set.ok) {
            result.errorCode = set.errorCode.empty() ?
                "hud_token_set_invalid" : set.errorCode;
            return result;
        }
        if (std::find(result.tokenSetIds.begin(), result.tokenSetIds.end(),
                set.tokenSetId) != result.tokenSetIds.end()) {
            result.errorCode = "hud_token_set_id_collision:" + set.tokenSetId;
            return result;
        }
        result.tokenSetIds.push_back(set.tokenSetId);
        result.tokenSets.emplace(set.tokenSetId, set);
        if (std::find(result.productNamespaces.begin(),
                result.productNamespaces.end(), set.productNamespace) ==
                result.productNamespaces.end())
            result.productNamespaces.push_back(set.productNamespace);
        if (std::find(result.debugNamespaces.begin(),
                result.debugNamespaces.end(), set.debugNamespace) ==
                result.debugNamespaces.end())
            result.debugNamespaces.push_back(set.debugNamespace);
        if (result.productNamespace.empty()) {
            result.productNamespace = set.productNamespace;
            result.debugNamespace = set.debugNamespace;
            result.palettes = set.palettes;
        } else {
            const bool sameNamespace =
                result.productNamespace == set.productNamespace;
            if (sameNamespace && result.palettes.size() != set.palettes.size()) {
                result.errorCode = "hud_token_registry_palette_mismatch";
                return result;
            }
            for (const auto& palette : sameNamespace ? result.palettes :
                    std::map<HudPaletteMode, HudTokenPalette>{}) {
                const auto candidate = set.palettes.find(palette.first);
                if (candidate == set.palettes.end() ||
                    candidate->second.roles.size() != palette.second.roles.size()) {
                    result.errorCode = "hud_token_registry_palette_mismatch";
                    return result;
                }
                for (const auto& role : palette.second.roles) {
                    const auto other = candidate->second.roles.find(role.first);
                    if (other == candidate->second.roles.end() ||
                        !sameColor(role.second, other->second)) {
                        result.errorCode = "hud_token_registry_palette_mismatch";
                        return result;
                    }
                }
            }
        }
        for (const auto& icon : set.icons) {
            const auto existing = result.icons.find(icon.first);
            if (existing != result.icons.end()) {
                if (iconNamespaces.at(icon.first) != set.productNamespace)
                    continue;
                if (sameIcon(existing->second, icon.second)) continue;
                result.errorCode = "hud_token_name_collision:" + icon.first;
                return result;
            }
            if (result.aliases.count(icon.first) != 0 &&
                aliasNamespaces.at(icon.first) == set.productNamespace) {
                result.errorCode = "hud_token_name_collision:" + icon.first;
                return result;
            }
            result.icons.insert(icon);
            iconNamespaces[icon.first] = set.productNamespace;
        }
        for (const auto& alias : set.aliases) {
            const auto existing = result.aliases.find(alias.first);
            if (existing != result.aliases.end()) {
                if (aliasNamespaces.at(alias.first) != set.productNamespace)
                    continue;
                if (existing->second == alias.second) continue;
                result.errorCode = "hud_token_name_collision:" + alias.first;
                return result;
            }
            if (result.icons.count(alias.first) != 0 &&
                iconNamespaces.at(alias.first) == set.productNamespace) {
                result.errorCode = "hud_token_name_collision:" + alias.first;
                return result;
            }
            result.aliases.insert(alias);
            aliasNamespaces[alias.first] = set.productNamespace;
        }
    }
    result.ok = true;
    for (const auto& alias : result.aliases) {
        std::string resolved;
        std::string error;
        if (!result.resolve(alias.first, resolved, error)) {
            result.ok = false;
            result.errorCode = error;
            return result;
        }
    }
    return result;
}

HudPaletteMode hudPaletteModeFor(
    settings::ColorVisionMode colorVisionMode) noexcept
{
    return colorVisionMode == settings::ColorVisionMode::Default ?
        HudPaletteMode::Standard : HudPaletteMode::ColorVisionSafe;
}

HudIconExpansionResult expandShippingHudIcons(const HudDrawList& input,
    const HudShippingTokenRegistry& tokens, HudPaletteMode paletteMode)
{
    HudIconExpansionResult result;
    result.draw = input;
    result.draw.ok = false;
    result.draw.errorCode.clear();
    if (!input.ok) {
        result.errorCode = "hud_draw_list_invalid";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    if (!tokens.ok) {
        result.errorCode = tokens.errorCode.empty() ?
            "hud_token_set_invalid" : tokens.errorCode;
        result.draw.errorCode = result.errorCode;
        return result;
    }
    if (tokens.productNamespaces.size() > 1) {
        result.errorCode = "hud_token_registry_namespace_required";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    if (input.containsDebug) {
        result.errorCode = "hud_shipping_debug_draw_forbidden";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    const auto palette = tokens.palettes.find(paletteMode);
    if (palette == tokens.palettes.end()) {
        result.errorCode = "hud_token_palette_unavailable";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    result.draw.primitives.clear();
    result.draw.expandedIconCount = 0;
    for (const UiPrimitive& primitive : input.primitives) {
        if (primitive.kind != UiPrimitiveKind::Icon) {
            result.draw.primitives.push_back(primitive);
            continue;
        }
        std::string resolved;
        std::string error;
        if (!tokens.resolve(primitive.token, resolved, error)) {
            result.errorCode = error;
            result.draw.errorCode = error;
            return result;
        }
        const auto icon = tokens.icons.find(resolved);
        if (icon == tokens.icons.end()) {
            result.errorCode = "hud_icon_token_unknown:" + primitive.token;
            result.draw.errorCode = result.errorCode;
            return result;
        }
        const product_hud::HudRect source =
            primitive.sourceRect.width > 0.0f &&
                primitive.sourceRect.height > 0.0f ?
                primitive.sourceRect : primitive.rect;
        const product_hud::HudRect clip = intersection(
            primitive.rect, primitive.clipRect);
        if (source.width <= 0.0f || source.height <= 0.0f || empty(clip))
            continue;
        try {
            for (const HudTokenCommand& command : icon->second.commands)
                expandCommand(result.draw, command, source, clip,
                    palette->second, primitive.token, resolved);
            if (icon->second.labelSlot.present) {
                const UiPoint center = transform(icon->second.labelSlot.center,
                    source);
                if (inside(center, clip, ClipEdge::Left) &&
                    inside(center, clip, ClipEdge::Right) &&
                    inside(center, clip, ClipEdge::Top) &&
                    inside(center, clip, ClipEdge::Bottom)) {
                    result.draw.iconLabels.push_back({center, clip,
                        roleColor(palette->second, "text_primary"),
                        std::max(1.0f, std::min(source.width, source.height) *
                            (13.0f / 24.0f)),
                        icon->second.labelSlot.text,
                        icon->second.labelSlot.fontRole,
                        primitive.token, resolved});
                }
            }
            ++result.draw.expandedIconCount;
        } catch (const TokenError& failure) {
            result.errorCode = failure.what();
            result.draw.errorCode = result.errorCode;
            return result;
        }
    }
    result.draw.ok = true;
    result.ok = true;
    return result;
}

HudIconExpansionResult expandShippingHudIcons(const HudDrawList& input,
    const HudShippingTokenSet& tokens, HudPaletteMode paletteMode)
{
    return expandShippingHudIcons(input, mergeHudShippingTokenSets({tokens}),
        paletteMode);
}

HudIconExpansionResult expandShippingHudIcons(const HudDrawList& input,
    const HudShippingTokenRegistry& registry, HudPaletteMode paletteMode,
    const std::string& tokenSetId)
{
    if (!registry.ok) {
        HudIconExpansionResult result;
        result.draw = input;
        result.errorCode = registry.errorCode.empty() ?
            "hud_token_registry_invalid" : registry.errorCode;
        result.draw.ok = false;
        result.draw.errorCode = result.errorCode;
        return result;
    }
    const auto found = registry.tokenSets.find(tokenSetId);
    if (found == registry.tokenSets.end()) {
        HudIconExpansionResult result;
        result.draw = input;
        result.errorCode = "hud_token_set_unavailable:" + tokenSetId;
        result.draw.ok = false;
        result.draw.errorCode = result.errorCode;
        return result;
    }
    return expandShippingHudIcons(input, found->second, paletteMode);
}

std::vector<std::uint8_t> canonicalHudTriangleBytes(
    const std::vector<UiTriangle>& triangles)
{
    std::vector<std::uint8_t> bytes;
    appendU32(bytes, static_cast<std::uint32_t>(triangles.size()));
    for (const UiTriangle& triangle : triangles) {
        for (const UiPoint& point : {triangle.first, triangle.second,
                triangle.third}) {
            appendU32(bytes, static_cast<std::uint32_t>(quantized(point.x)));
            appendU32(bytes, static_cast<std::uint32_t>(quantized(point.y)));
        }
        for (float channel : {triangle.color.red, triangle.color.green,
                triangle.color.blue, triangle.color.alpha})
            bytes.push_back(colorByte(channel));
        for (float value : {triangle.clipRect.x, triangle.clipRect.y,
                triangle.clipRect.width, triangle.clipRect.height})
            appendU32(bytes, static_cast<std::uint32_t>(quantized(value)));
        appendString(bytes, triangle.sourceToken);
        appendString(bytes, triangle.resolvedToken);
        if (triangle.worldAnnotation)
            appendString(bytes, "world_annotation");
    }
    return bytes;
}

}  // namespace billiardgl::render::ui
