#include "render/ui/rules_decision_ui_draw_list.h"
#include "rules/rules_decision_ui_model.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <set>
#include <utility>

namespace billiardgl::render::ui {
namespace {

using Localized = std::pair<std::string, std::string>;

const std::vector<std::string> kCanonicalStates{
    "group_choice_open_unselected",
    "group_choice_armed_confirm",
    "full_table_preview_valid",
    "full_table_preview_invalid",
    "behind_head_string_preview_valid",
    "behind_head_string_preview_invalid",
    "behind_head_string_respot_open_unselected",
    "behind_head_string_respot_armed_confirm",
    "eight_legal_open_unselected",
    "eight_foul_open_unselected",
    "break_illegal_open_unselected",
    "break_shot_foul_open_unselected",
    "submitting_progress",
    "authority_error",
    "state_drift_refreshing",
    "ai_or_remote_pending",
    "section_6g_classification_pending",
};

const std::map<std::string, Localized> kActionLabels{
    {"rules.group_choice.choose_solids", {"Choose solids", "选择全色球"}},
    {"rules.group_choice.choose_stripes", {"Choose stripes", "选择花色球"}},
    {"rules.ball_in_hand.confirm_full_table_placement",
        {"Confirm placement", "确认放置"}},
    {"rules.ball_in_hand.confirm_behind_head_string",
        {"Confirm behind head string", "确认放置在开球线后"}},
    {"rules.ball_in_hand.request_nearest_legal_respot",
        {"Request nearest legal ball respot", "请求重置最近的合法目标球"}},
    {"rules.eight_on_break.spot_continue",
        {"Spot 8-ball and continue", "重置8号球并继续"}},
    {"rules.eight_on_break.spot_ball_in_hand_head_string",
        {"Spot 8-ball; ball in hand behind head string",
         "重置8号球；开球线后自由球"}},
    {"rules.eight_on_break.rerack_original_breaker",
        {"Rerack - original breaker", "重摆 - 原开球方开球"}},
    {"rules.eight_on_break.rerack_opponent_breaks",
        {"Rerack - opponent breaks", "重摆 - 对手开球"}},
    {"rules.break_foul.accept_table", {"Accept table", "接受台面"}},
    {"rules.break_foul.ball_in_hand_behind_head_string",
        {"Ball in hand behind head string", "开球线后自由球"}},
    {"rules.break_foul.rerack_opponent_breaks",
        {"Rerack - I break", "重摆 - 由我开球"}},
    {"rules.break_foul.rerack_original_breaker",
        {"Rerack - original breaker", "重摆 - 原开球方开球"}},
};

std::optional<Localized> actionLabel(const std::string& actionId)
{
    const auto found = kActionLabels.find(actionId);
    if (found != kActionLabels.end()) return found->second;
    const auto ball = rules::ui::productRespotChoiceBallId(actionId);
    if (!ball) return std::nullopt;
    return Localized{"Choose ball " + std::to_string(*ball) + " to respot",
        "选择重置" + std::to_string(*ball) + "号球"};
}

const std::set<std::string> kFoundationActions{
    "rules.foundation.rerack", "rules.foundation.respot"};

bool zh(RulesDecisionLocale locale)
{
    return locale == RulesDecisionLocale::ZhCn;
}

std::string localized(const Localized& value, RulesDecisionLocale locale)
{
    return zh(locale) ? value.second : value.first;
}

std::string actorDisplay(int actorPlayerId, RulesDecisionLocale locale)
{
    if (actorPlayerId < 0) return {};
    return zh(locale) ? "玩家" + std::to_string(actorPlayerId + 1) :
        "Player " + std::to_string(actorPlayerId + 1);
}

std::string armedCopy(const std::string& actionResult, int actorPlayerId,
    RulesDecisionLocale locale)
{
    const std::string actor = actorDisplay(actorPlayerId, locale);
    return zh(locale) ? "已选 — " + actor + "：" + actionResult :
        "Armed — " + actor + ": " + actionResult;
}

std::string confirmCopy(const std::string& actionResult, int actorPlayerId,
    RulesDecisionLocale locale)
{
    const std::string actor = actorDisplay(actorPlayerId, locale);
    return zh(locale) ? "确认 — " + actor + "：" + actionResult :
        "Confirm — " + actor + ": " + actionResult;
}

std::string placementConfirmCopy(int actorPlayerId,
    RulesDecisionLocale locale)
{
    const std::string actor = actorDisplay(actorPlayerId, locale);
    return zh(locale) ? "确认放置 — " + actor :
        "Confirm placement — " + actor;
}

std::string coordinateValue(double value)
{
    std::ostringstream output;
    // Pointer projection naturally produces long binary-float tails.  The
    // typed placement request retains the full value; the product copy is a
    // bounded centimetre readout that must fit for every legal table point.
    if (std::fabs(value) < 0.005) value = 0.0;
    output << std::fixed << std::setprecision(2) << value;
    std::string result = output.str();
    while (result.size() > 2 && result.back() == '0' &&
            result[result.size() - 2] != '.')
        result.pop_back();
    return result;
}

std::string previewCopy(const RulesDecisionCanonicalFixture& fixture,
    RulesDecisionLocale locale, bool behindHeadString)
{
    const std::string x = coordinateValue(fixture.previewWorldX);
    const std::string z = coordinateValue(fixture.previewWorldZ);
    if (behindHeadString)
        return zh(locale) ? "合法区域—开球线后 • 预览 X " + x +
                " 厘米，Z " + z + " 厘米" :
            "Legal region — behind head string • Preview X " + x +
                " cm, Z " + z + " cm";
    return zh(locale) ? "预览 X " + x + " 厘米，Z " + z + " 厘米" :
        "Preview X " + x + " cm, Z " + z + " cm";
}

Localized placementErrorLabel(const std::string& code)
{
    if (code == "rules_placement_nonfinite")
        return {"Position is not finite.", "位置不是有限数值。"};
    if (code == "rules_placement_out_of_bounds")
        return {"Cue ball is outside table bounds.", "母球超出台面边界。"};
    if (code == "rules_placement_outside_head_string_area")
        return {"Cue ball must be behind the head string.",
            "母球必须放在开球线后。"};
    if (code == "rules_placement_forbidden_geometry")
        return {"Cue ball intersects blocked table geometry.",
            "母球与禁止放置的台面区域相交。"};
    if (code == "rules_placement_authority_geometry_invalid")
        return {"Table placement geometry is unavailable.",
            "台面放置区域暂不可用。"};
    if (code == "rules_placement_overlap")
        return {"Cue ball overlaps another ball.", "母球与其他球重叠。"};
    return {"Placement is not legal.", "该位置不合法。"};
}

struct FixtureCopy {
    Localized title;
    Localized body;
};

FixtureCopy fixtureCopy(const std::string& state)
{
    if (state.rfind("group_choice", 0) == 0)
        return {{"Choose your group", "选择球组"},
            {"Both groups qualified. Choose only from this authority offer.",
             "两组均已满足条件；请仅从裁判系统当前选项中选择。"}};
    if (state.rfind("full_table", 0) == 0)
        return {{"Ball in hand - full table", "自由球 - 全台"},
            {"Preview is local. Authority validation is required before confirm.",
             "预览仅保存在本地；确认前必须通过裁判系统验证。"}};
    if (state.rfind("behind_head_string", 0) == 0) {
        if (state.rfind("behind_head_string_respot", 0) == 0) {
            return {{"Choose a ball to respot", "选择要重置的目标球"},
                {"The nearest legal balls are tied. Choose one exact ball.",
                 "最近的合法目标球距离相同；请选择一个具体球。"}};
        }
        return {{"Behind head string placement", "开球线后自由球"},
            {"Place the cue-ball center behind the head string.",
             "请将母球中心放在开球线后。"}};
    }
    if (state.rfind("eight_legal_", 0) == 0 ||
        state.rfind("eight_foul_", 0) == 0)
        return {{"8-ball on the break", "开球时8号球入袋"},
            {"This is not a win. Choose only from the authority offer.",
             "这不算获胜；只能选择裁判系统当前提供的处理方式。"}};
    if (state.rfind("break_illegal_", 0) == 0 ||
        state.rfind("break_shot_foul_", 0) == 0)
        return {{"Break decision", "开球犯规选择"},
            {"Choose an action offered for this exact break ruling.",
             "请选择裁判系统为本次开球判定提供的处理方式。"}};
    if (state == "submitting_progress")
        return {{"Submitting ruling", "正在提交判定"},
            {"Waiting for an exact authority acknowledgement.",
             "正在等待裁判系统返回完全匹配的确认。"}};
    if (state == "authority_error")
        return {{"Authority error", "裁判系统错误"},
            {"The ruling remains blocked. Refresh authority state before acting.",
             "判定仍处于阻塞状态；请先刷新裁判系统状态。"}};
    if (state == "state_drift_refreshing")
        return {{"Ruling changed", "判定已变化"},
            {"The ruling changed. Refreshing the authority offer.",
             "判定状态已变化，正在刷新裁判系统提供的选项。"}};
    if (state == "ai_or_remote_pending")
        return {{"Decision pending", "等待判定"},
            {"Waiting for the acting player authority.",
             "正在等待当前行动方的裁判系统处理。"}};
    return {{"Break ruling unavailable", "开球判定暂不可用"},
        {"The required miscue classification is not available. No foul choice has been created.",
         "当前缺少所需的滑杆分类信息，尚未生成任何犯规处理选项。"}};
}

std::vector<std::string> utf8Tokens(const std::string& text)
{
    std::vector<std::string> tokens;
    for (std::size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first < 0x80) {
            const bool whitespace = first == ' ' || first == '\t';
            std::size_t end = index + 1;
            while (end < text.size()) {
                const unsigned char next = static_cast<unsigned char>(text[end]);
                if (next >= 0x80) break;
                const bool nextWhitespace = next == ' ' || next == '\t';
                if (nextWhitespace != whitespace) break;
                ++end;
            }
            tokens.push_back(text.substr(index, end - index));
            index = end;
            continue;
        }
        std::size_t count = (first & 0xe0u) == 0xc0u ? 2 :
            (first & 0xf0u) == 0xe0u ? 3 : 4;
        if (index + count > text.size()) count = text.size() - index;
        tokens.push_back(text.substr(index, count));
        index += count;
    }
    return tokens;
}

struct WrappedText {
    bool ok = false;
    std::vector<std::string> lines;
    float maximumWidth = 0.0f;
    float lineHeight = 0.0f;
};

std::string joinedLines(const WrappedText& wrapped)
{
    std::string result;
    for (std::size_t index = 0; index < wrapped.lines.size(); ++index) {
        if (index) result += '\n';
        result += wrapped.lines[index];
    }
    return result;
}

WrappedText wrap(const std::string& text, float fontPx, float maximumWidth,
    int maximumLines, const RulesDecisionTextMeasure& measure)
{
    WrappedText result;
    const auto lineMetric = measure("Ag国", fontPx);
    if (!lineMetric.ok) return result;
    result.lineHeight = std::max(fontPx * 1.25f, lineMetric.height);
    std::string current;
    for (const std::string& token : utf8Tokens(text)) {
        std::string candidate = current + token;
        const auto metric = measure(candidate, fontPx);
        if (!metric.ok) return result;
        if (metric.width <= maximumWidth || current.empty()) {
            if (metric.width > maximumWidth) return result;
            current = std::move(candidate);
            continue;
        }
        while (!current.empty() && current.back() == ' ') current.pop_back();
        result.lines.push_back(current);
        current = token;
        while (!current.empty() && current.front() == ' ') current.erase(0, 1);
        if (static_cast<int>(result.lines.size()) >= maximumLines)
            return result;
    }
    if (!current.empty()) result.lines.push_back(current);
    if (result.lines.empty() || static_cast<int>(result.lines.size()) > maximumLines)
        return result;
    for (const auto& line : result.lines) {
        const auto metric = measure(line, fontPx);
        if (!metric.ok || metric.width > maximumWidth) return result;
        result.maximumWidth = std::max(result.maximumWidth, metric.width);
    }
    result.ok = true;
    return result;
}

bool intersects(const RulesDecisionRect& a, const RulesDecisionRect& b)
{
    return a.x < b.x + b.width && b.x < a.x + a.width &&
        a.y < b.y + b.height && b.y < a.y + a.height;
}

float polygonArea(const std::vector<RulesDecisionPoint>& polygon)
{
    if (polygon.size() < 3) return 0.0f;
    double twiceArea = 0.0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const RulesDecisionPoint& first = polygon[index];
        const RulesDecisionPoint& second = polygon[(index + 1) % polygon.size()];
        twiceArea += static_cast<double>(first.x) * second.y -
            static_cast<double>(first.y) * second.x;
    }
    return static_cast<float>(std::fabs(twiceArea) * 0.5);
}

template <typename Inside, typename Intersection>
std::vector<RulesDecisionPoint> clipPolygon(
    const std::vector<RulesDecisionPoint>& input, Inside inside,
    Intersection intersection)
{
    std::vector<RulesDecisionPoint> result;
    if (input.empty()) return result;
    RulesDecisionPoint previous = input.back();
    bool previousInside = inside(previous);
    for (const RulesDecisionPoint& current : input) {
        const bool currentInside = inside(current);
        if (currentInside != previousInside)
            result.push_back(intersection(previous, current));
        if (currentInside) result.push_back(current);
        previous = current;
        previousInside = currentInside;
    }
    return result;
}

std::vector<RulesDecisionPoint> clipToRect(
    std::vector<RulesDecisionPoint> polygon, const RulesDecisionRect& rect)
{
    const auto vertical = [](float x, RulesDecisionPoint first,
            RulesDecisionPoint second) {
        const float dx = second.x - first.x;
        const float t = std::fabs(dx) <= 1e-6f ? 0.0f :
            (x - first.x) / dx;
        return RulesDecisionPoint{x, first.y + (second.y - first.y) * t};
    };
    const auto horizontal = [](float y, RulesDecisionPoint first,
            RulesDecisionPoint second) {
        const float dy = second.y - first.y;
        const float t = std::fabs(dy) <= 1e-6f ? 0.0f :
            (y - first.y) / dy;
        return RulesDecisionPoint{first.x + (second.x - first.x) * t, y};
    };
    polygon = clipPolygon(polygon,
        [&](RulesDecisionPoint point) { return point.x >= rect.x; },
        [&](RulesDecisionPoint a, RulesDecisionPoint b) {
            return vertical(rect.x, a, b);
        });
    polygon = clipPolygon(polygon,
        [&](RulesDecisionPoint point) {
            return point.x <= rect.x + rect.width;
        }, [&](RulesDecisionPoint a, RulesDecisionPoint b) {
            return vertical(rect.x + rect.width, a, b);
        });
    polygon = clipPolygon(polygon,
        [&](RulesDecisionPoint point) { return point.y >= rect.y; },
        [&](RulesDecisionPoint a, RulesDecisionPoint b) {
            return horizontal(rect.y, a, b);
        });
    return clipPolygon(polygon,
        [&](RulesDecisionPoint point) {
            return point.y <= rect.y + rect.height;
        }, [&](RulesDecisionPoint a, RulesDecisionPoint b) {
            return horizontal(rect.y + rect.height, a, b);
        });
}

float projectedTableOcclusionFraction(const RulesDecisionDisplayProfile& profile,
    const RulesDecisionRect& panel)
{
    if (!profile.projectedTableAvailable) return -1.0f;
    std::vector<RulesDecisionPoint> visibleTable(
        profile.projectedTableCorners.begin(),
        profile.projectedTableCorners.end());
    visibleTable = clipToRect(std::move(visibleTable),
        {0.0f, 0.0f, static_cast<float>(profile.logicalWidth),
            static_cast<float>(profile.logicalHeight)});
    const float visibleArea = polygonArea(visibleTable);
    if (!(visibleArea > 1.0f) || !std::isfinite(visibleArea)) return -1.0f;
    const float obscuredArea = polygonArea(clipToRect(visibleTable, panel));
    if (!std::isfinite(obscuredArea)) return -1.0f;
    return obscuredArea / visibleArea;
}

bool isOpenPendingOrStale(const std::string& state)
{
    return state.find("open_unselected") != std::string::npos ||
        state == "ai_or_remote_pending" ||
        state == "state_drift_refreshing" ||
        state == "section_6g_classification_pending";
}

bool isArmedConfirm(const std::string& state)
{
    constexpr const char suffix[] = "_armed_confirm";
    constexpr std::size_t suffixLength = sizeof(suffix) - 1;
    return state.size() >= suffixLength &&
        state.compare(state.size() - suffixLength, suffixLength, suffix) == 0;
}

std::string unwrapped(std::string text)
{
    text.erase(std::remove_if(text.begin(), text.end(), [](char value) {
        return value == '\n' || value == ' ' || value == '\t';
    }), text.end());
    return text;
}

const RulesDecisionDrawItem* itemWithId(const RulesDecisionDrawList& list,
    const std::string& id)
{
    const auto found = std::find_if(list.items.begin(), list.items.end(),
        [&id](const RulesDecisionDrawItem& item) { return item.id == id; });
    return found == list.items.end() ? nullptr : &*found;
}

const RulesDecisionDrawItem* actionItem(
    const RulesDecisionDrawList& list, const std::string& actionId)
{
    const auto found = std::find_if(list.items.begin(), list.items.end(),
        [&](const RulesDecisionDrawItem& item) {
            return item.kind == RulesDecisionDrawKind::Action &&
                item.actionId == actionId;
        });
    return found == list.items.end() ? nullptr : &*found;
}

int kindCount(const RulesDecisionDrawList& list, RulesDecisionDrawKind kind)
{
    return static_cast<int>(std::count_if(list.items.begin(), list.items.end(),
        [kind](const RulesDecisionDrawItem& item) { return item.kind == kind; }));
}

void addText(RulesDecisionDrawList& result, const std::string& id,
    const std::string& text, float x, float y, float width, float font,
    int maximumLines, const RulesDecisionTextMeasure& measure,
    float& consumedHeight)
{
    const WrappedText wrapped = wrap(text, font, width, maximumLines, measure);
    if (!wrapped.ok) {
        result.layoutError = "text_wrap_failed:" + id;
        return;
    }
    consumedHeight = wrapped.lineHeight * wrapped.lines.size();
    std::string rendered;
    for (std::size_t i = 0; i < wrapped.lines.size(); ++i) {
        if (i) rendered += '\n';
        rendered += wrapped.lines[i];
    }
    result.items.push_back({id, RulesDecisionDrawKind::Text,
        {x, y, wrapped.maximumWidth, consumedHeight}, rendered, "", font,
        static_cast<int>(wrapped.lines.size())});
}

}  // namespace

const std::vector<std::string>& rulesDecisionCanonicalStateIds()
{
    return kCanonicalStates;
}

RulesDecisionCanonicalFixture rulesDecisionCanonicalFixture(
    const std::string& state)
{
    RulesDecisionCanonicalFixture result;
    result.stateId = state;
    if (state.rfind("group_choice", 0) == 0) {
        result.flowId = "simultaneous_group_choice";
        result.branchId = "two_group_simultaneous_first_contact_and_pocket";
        result.offeredActionIds = {"rules.group_choice.choose_solids",
            "rules.group_choice.choose_stripes"};
        if (state == "group_choice_armed_confirm")
            result.armedActionId = result.offeredActionIds.front();
        if (state == "group_choice_armed_confirm")
            result.focusedActionId = result.armedActionId;
    } else if (state.rfind("full_table", 0) == 0) {
        result.flowId = "ball_in_hand_full_table";
        result.branchId = "full_table";
        result.placement = true;
        result.placementValid = state == "full_table_preview_valid";
        result.offeredActionIds = {
            "rules.ball_in_hand.confirm_full_table_placement"};
    } else if (state.rfind("behind_head_string", 0) == 0) {
        result.sourceClause = "WPA-II-08";
        if (state.rfind("behind_head_string_respot", 0) == 0) {
            result.flowId = rules::kBehindHeadStringRespotFlowId;
            result.branchId = "behind_head_string_respot_tie";
            result.offeredActionIds = {
                rules::ui::productRespotChoiceActionId(1),
                rules::ui::productRespotChoiceActionId(2)};
        } else {
            result.flowId = "ball_in_hand_behind_head_string";
            result.branchId = "behind_head_string";
            result.placement = true;
            result.placementValid = state ==
                "behind_head_string_preview_valid";
            result.offeredActionIds = {
                "rules.ball_in_hand.confirm_behind_head_string"};
        }
    } else if (state.rfind("eight_legal_", 0) == 0) {
        result.flowId = "eight_on_break_spot_or_rerack";
        result.branchId = "legal_break";
        result.offeredActionIds = {"rules.eight_on_break.spot_continue",
            "rules.eight_on_break.rerack_original_breaker"};
    } else if (state.rfind("eight_foul_", 0) == 0) {
        result.flowId = "eight_on_break_spot_or_rerack";
        result.branchId = "foul_break";
        result.offeredActionIds = {
            "rules.eight_on_break.spot_ball_in_hand_head_string",
            "rules.eight_on_break.rerack_opponent_breaks",
            "rules.eight_on_break.rerack_original_breaker"};
    } else if (state.rfind("break_illegal_", 0) == 0) {
        result.flowId = "break_foul_opponent_choice";
        result.branchId = "illegal_break_6c";
        result.sourceClause = "WPA-II-06-C";
        result.offeredActionIds = {"rules.break_foul.accept_table",
            "rules.break_foul.rerack_opponent_breaks",
            "rules.break_foul.rerack_original_breaker"};
    } else if (state.rfind("break_shot_foul_", 0) == 0) {
        result.flowId = "break_foul_opponent_choice";
        result.branchId = "break_shot_foul_6e";
        result.sourceClause = "WPA-II-06-E";
        result.offeredActionIds = {
            "rules.break_foul.ball_in_hand_behind_head_string",
            "rules.break_foul.rerack_opponent_breaks",
            "rules.break_foul.rerack_original_breaker"};
    }
    if (isArmedConfirm(state) && !result.offeredActionIds.empty()) {
        result.armedActionId = result.offeredActionIds.front();
        result.focusedActionId = result.armedActionId;
    }
    return result;
}

std::vector<std::string> allRulesDecisionUiStrings()
{
    std::vector<std::string> result{"Ag国", "Actor: Player 1",
        "Revision: 42", "Preview: X 0.0 cm, Z -36.0 cm",
        "Valid placement", "Invalid placement",
        "Armed — Player 1: Choose solids",
        "Confirm — Player 1: Choose solids",
        "Legal region — behind head string • Preview X 0.0 cm, Z -36.0 cm",
        "Waiting for authority", "WPA-II-06-G",
        "行动方：玩家1 · 修订：42 · 条款：WPA-II-06-C",
        "预览：X 0.0 厘米，Z -36.0 厘米",
        "已选 — 玩家1：选择全色球", "确认 — 玩家1：选择全色球",
        "合法区域—开球线后 • 预览 X 0.0 厘米，Z -36.0 厘米"};
    for (const auto& pair : kActionLabels) {
        result.emplace_back(pair.second.first);
        result.emplace_back(pair.second.second);
    }
    for (const char* code : {"rules_placement_nonfinite",
            "rules_placement_out_of_bounds",
            "rules_placement_outside_head_string_area",
            "rules_placement_forbidden_geometry",
            "rules_placement_authority_geometry_invalid",
            "rules_placement_overlap", "unknown"}) {
        const auto copy = placementErrorLabel(code);
        result.emplace_back(copy.first);
        result.emplace_back(copy.second);
    }
    for (int ball = 1; ball < 16; ++ball) {
        const auto label = actionLabel(
            rules::ui::productRespotChoiceActionId(ball));
        result.emplace_back(label->first);
        result.emplace_back(label->second);
    }
    for (const auto& state : kCanonicalStates) {
        const auto copy = fixtureCopy(state);
        result.emplace_back(copy.title.first);
        result.emplace_back(copy.title.second);
        result.emplace_back(copy.body.first);
        result.emplace_back(copy.body.second);
    }
    return result;
}

RulesDecisionDrawList buildRulesDecisionDrawList(
    const RulesDecisionCanonicalFixture& fixture,
    RulesDecisionLocale locale, RulesDecisionPalette palette,
    const RulesDecisionDisplayProfile& profile,
    const RulesDecisionTextMeasure& measure)
{
    RulesDecisionDrawList result;
    result.stateId = fixture.stateId;
    result.locale = locale;
    result.palette = palette;
    result.profile = profile;
    result.offeredActionIds = fixture.offeredActionIds;
    result.actorPlayerId = fixture.actorPlayerId;
    result.previewWorldX = fixture.previewWorldX;
    result.previewWorldZ = fixture.previewWorldZ;
    result.armedActionId = fixture.armedActionId;
    result.shotInputBlocked = fixture.shotInputBlocked;
    if (profile.logicalWidth <= 0 || profile.logicalHeight <= 0 ||
        profile.physicalWidth != std::lround(profile.logicalWidth * profile.density) ||
        profile.physicalHeight != std::lround(profile.logicalHeight * profile.density) ||
        profile.uiScale < 0.85f || profile.uiScale > 1.35f || !measure) {
        result.layoutError = "invalid_profile";
        return result;
    }

    const float scale = profile.uiScale;
    const float safe = 24.0f / profile.density;
    const float preferredWidth = fixture.placement ? 540.0f : 720.0f;
    const float modalWidth = std::min(preferredWidth,
        static_cast<float>(profile.logicalWidth) - safe * 2.0f);
    if (modalWidth < 520.0f) {
        result.layoutError = "modal_too_narrow";
        return result;
    }
    // Placement is a direct-manipulation mode: keep its controls at the safe
    // edge so the projected table remains visible beneath the world-space
    // placement guide.  Choice-only modals stay centered.
    const float x = fixture.placement ? safe :
        (profile.logicalWidth - modalWidth) * 0.5f;
    const float padding = (fixture.placement ? 16.0f : 24.0f) * scale;
    const float contentWidth = modalWidth - 2.0f * padding;
    const float titleFont = 24.0f * scale;
    const float bodyFont = 16.0f * scale;
    const float metaFont = 14.0f * scale;
    const float actionFont = 16.0f * scale;
    const float criticalShape = std::max(18.0f * scale,
        18.0f / profile.density);
    const float exactShapeV4 = std::max(24.0f * scale,
        24.0f / profile.density);
    float y = safe + 22.0f;
    const float panelY = y;
    const std::size_t panelIndex = result.items.size();
    result.items.push_back({fixture.placement ? "placement_ribbon" : "modal",
        fixture.placement ? RulesDecisionDrawKind::PlacementRibbon :
            RulesDecisionDrawKind::ModalPanel,
        {x, panelY, modalWidth, 0.0f}, "", "", 0.0f, 0, true, false,
        false});
    y += padding;
    float height = 0.0f;
    const FixtureCopy copy = fixtureCopy(fixture.stateId);
    addText(result, "title", localized(copy.title, locale), x + padding, y,
        contentWidth, titleFont, 2, measure, height);
    if (!result.layoutError.empty()) return result;
    y += height + 8.0f * scale;
    addText(result, "body", localized(copy.body, locale), x + padding, y,
        contentWidth, bodyFont, 3, measure, height);
    if (!result.layoutError.empty()) return result;
    y += height + 8.0f * scale;

    const std::string actor = actorDisplay(fixture.actorPlayerId, locale);
    if (actor.empty()) {
        result.layoutError = "invalid_actor_player_id";
        return result;
    }
    std::string metadata = fixture.sourceClause.empty() ?
        (zh(locale) ? "行动方：" + actor + " · 修订：42" :
            "Actor: " + actor + " · Revision: 42") :
        (zh(locale) ? "行动方：" + actor + " · 修订：42 · 条款：" + fixture.sourceClause :
            "Actor: " + actor + " · Revision: 42 · Clause: " + fixture.sourceClause);
    addText(result, "metadata", metadata, x + padding, y, contentWidth,
        metaFont, 2, measure, height);
    if (!result.layoutError.empty()) return result;
    y += height + 12.0f * scale;

    if (fixture.placement) {
        const std::string coordinate = previewCopy(fixture, locale, false);
        float coordinateHeight = 0.0f;
        addText(result, "coordinate", coordinate, x + padding, y,
            contentWidth * 0.45f, metaFont, 2, measure,
            coordinateHeight);
        if (!result.layoutError.empty()) return result;
        float coordinateLaneHeight = coordinateHeight;
        if (fixture.offeredActionIds.empty() ||
            fixture.offeredActionIds.size() > 2 ||
            (fixture.offeredActionIds.front() !=
                    rules::kConfirmBehindHeadStringPlacementActionId &&
             fixture.offeredActionIds.front() !=
                    rules::kConfirmFullTablePlacementActionId) ||
            (fixture.offeredActionIds.size() == 2 &&
             fixture.offeredActionIds[1] !=
                    rules::kRequestNearestLegalRespotActionId)) {
            result.layoutError = "placement_action_inventory_invalid";
            return result;
        }
        const float actionX = x + padding + contentWidth * 0.50f;
        const float actionWidth = contentWidth * 0.50f;
        float actionY = y;
        float singleActionConfirmBottom = 0.0f;
        for (std::size_t index = 0;
                index < fixture.offeredActionIds.size(); ++index) {
            const std::string& actionId = fixture.offeredActionIds[index];
            const auto actionText = actionLabel(actionId);
            if (!actionText) {
                result.layoutError = "unknown_action:" + actionId;
                return result;
            }
            const bool placementConfirm = index == 0;
            const bool armed = actionId == fixture.armedActionId;
            const std::string exactResult = localized(*actionText, locale);
            // The separate diamond and Confirm row carry the armed state.
            // Repeating the actor/action prefix in this half-width lane can
            // make a valid placement modal exceed its occlusion budget.
            const std::string actionCopy = exactResult;
            const WrappedText label = wrap(actionCopy, actionFont,
                actionWidth - 28.0f * scale, 2, measure);
            if (!label.ok) {
                result.layoutError = "action_wrap_failed:" + actionId;
                return result;
            }
            const float actionHeight = std::max(48.0f * scale,
                label.lineHeight * label.lines.size() + 12.0f * scale);
            result.items.push_back({"action_" + std::to_string(index),
                RulesDecisionDrawKind::Action,
                {actionX, actionY, actionWidth, actionHeight},
                joinedLines(label), actionId, actionFont,
                static_cast<int>(label.lines.size()),
                !placementConfirm || fixture.placementValid, false, armed});
            if (armed) {
                result.items.push_back({"armed_diamond",
                    RulesDecisionDrawKind::ArmedDiamond,
                    {actionX + actionWidth - criticalShape - 4.0f * scale,
                     actionY + (actionHeight - criticalShape) * 0.5f,
                     criticalShape, criticalShape}, "", actionId,
                    0.0f, 0, true, false, true});
                // A single FullTable action can keep Confirm in its right
                // lane.  Putting it under the two-line coordinate instead
                // needlessly raises the modal over the projected table.  A
                // two-action 06E ribbon still uses the coordinate lane: its
                // action lane is already taller and would otherwise exceed
                // the same real table-occlusion budget.
                const bool singleAction =
                    fixture.offeredActionIds.size() == 1;
                const float confirmX = singleAction ?
                    actionX : x + padding;
                const float confirmY = singleAction ?
                    actionY + actionHeight + 8.0f * scale :
                    y + coordinateHeight + 8.0f * scale;
                const float confirmWidth = singleAction ?
                    actionWidth : contentWidth * 0.45f;
                const WrappedText confirmLabel = wrap(placementConfirmCopy(
                    fixture.actorPlayerId, locale), actionFont,
                    confirmWidth - 32.0f * scale, 2, measure);
                if (!confirmLabel.ok) {
                    result.layoutError = "confirm_wrap_failed:" + actionId;
                    return result;
                }
                const float confirmHeight = std::max(48.0f * scale,
                    confirmLabel.lineHeight * confirmLabel.lines.size() +
                        12.0f * scale);
                result.items.push_back({"confirm",
                    RulesDecisionDrawKind::Confirm,
                    {confirmX, confirmY, confirmWidth, confirmHeight},
                    joinedLines(confirmLabel), actionId, actionFont,
                    static_cast<int>(confirmLabel.lines.size()), true});
                result.items.push_back({
                    "confirm_double_frame_enter_arrow",
                    RulesDecisionDrawKind::ConfirmDoubleFrameEnterArrow,
                    {confirmX, confirmY, confirmWidth, confirmHeight}, "",
                    actionId, 0.0f, 0, true, false, false});
                if (singleAction)
                    singleActionConfirmBottom = confirmY + confirmHeight;
                else
                    coordinateLaneHeight = coordinateHeight + 8.0f * scale +
                        confirmHeight;
            }
            actionY += actionHeight + 8.0f * scale;
        }
        if (singleActionConfirmBottom > 0.0f)
            actionY = std::max(actionY,
                singleActionConfirmBottom + 8.0f * scale);
        height = std::max(coordinateLaneHeight, actionY - y);
        if (!fixture.placementErrorCode.empty()) {
            float errorHeight = 0.0f;
            addText(result, "placement_error",
                (zh(locale) ? "无效位置：" : "Invalid placement: ") +
                    localized(placementErrorLabel(
                        fixture.placementErrorCode), locale),
                x + padding, y + coordinateLaneHeight + 6.0f * scale,
                contentWidth * 0.45f, metaFont, 3, measure, errorHeight);
            if (!result.layoutError.empty()) return result;
            height = std::max(height,
                coordinateLaneHeight + 6.0f * scale + errorHeight);
        }
        result.items.push_back({"preview_ghost", RulesDecisionDrawKind::PreviewGhostBall,
            {profile.logicalWidth * 0.49f, profile.logicalHeight * 0.56f,
             28.0f * scale, 28.0f * scale}, "", "", 0.0f, 0, true,
             false, false});
        if (fixture.placementValid)
            result.items.push_back({"preview_crosshair",
                RulesDecisionDrawKind::PreviewCrosshair,
                {profile.logicalWidth * 0.49f - 8.0f * scale,
                 profile.logicalHeight * 0.56f - 8.0f * scale,
                 44.0f * scale, 44.0f * scale}, "", "", 0.0f, 0, true,
                 false, false});
        if (fixture.flowId == "ball_in_hand_behind_head_string") {
            result.items.push_back({"head_string", RulesDecisionDrawKind::HeadStringBoundary,
                {profile.logicalWidth * 0.22f, profile.logicalHeight * 0.68f,
                 profile.logicalWidth * 0.56f, 2.0f}, "", "", 0.0f, 0,
                 true, false, false});
            result.items.push_back({"legal_hatch", RulesDecisionDrawKind::LegalRegionHatch,
                {profile.logicalWidth * 0.22f, profile.logicalHeight * 0.68f,
                 profile.logicalWidth * 0.56f, profile.logicalHeight * 0.16f},
                 "", "", 0.0f, 0, true, false, false});
            float worldCopyHeight = 0.0f;
            addText(result, "world_legal_region_copy",
                previewCopy(fixture, locale, true),
                profile.logicalWidth * 0.22f,
                profile.logicalHeight * 0.85f,
                profile.logicalWidth * 0.56f, metaFont, 2, measure,
                worldCopyHeight);
            if (!result.layoutError.empty()) return result;
        } else {
            float worldCopyHeight = 0.0f;
            addText(result, "world_preview_coordinate",
                previewCopy(fixture, locale, false),
                profile.logicalWidth * 0.22f,
                profile.logicalHeight * 0.82f,
                profile.logicalWidth * 0.56f, metaFont, 2, measure,
                worldCopyHeight);
            if (!result.layoutError.empty()) return result;
        }
        if (!fixture.placementValid) {
            result.items.push_back({"invalid_octagon", RulesDecisionDrawKind::InvalidOctagon,
                {profile.logicalWidth * 0.49f - 9.0f * scale,
                 profile.logicalHeight * 0.56f - 9.0f * scale,
                 46.0f * scale, 46.0f * scale}, "", "", 0.0f, 0, true,
                 false, false});
            result.items.push_back({"collision_ring", RulesDecisionDrawKind::InvalidCollisionRing,
                {profile.logicalWidth * 0.49f - 14.0f * scale,
                 profile.logicalHeight * 0.56f - 14.0f * scale,
                 56.0f * scale, 56.0f * scale}, "", "", 0.0f, 0, true,
                 false, false});
        }
        y += height + padding;
    } else {
        const bool compactThreeActionGrid =
            profile.logicalHeight < 480 &&
            fixture.offeredActionIds.size() == 3;
        if (compactThreeActionGrid) {
            // A 1280x720 physical drawable on a Retina screen is a genuine
            // 640x360 logical product window.  Stacking the three 06C/06E/06F
            // actions vertically cannot satisfy the unchanged 82% panel
            // height bound.  Keep every action, label and hit target, but use
            // one responsive three-column row at this bounded small-window
            // profile.  Armed state still receives its separate full-width
            // Confirm row below the complete action inventory.
            const float gap = 8.0f * scale;
            const float actionWidth =
                (contentWidth - gap * 2.0f) / 3.0f;
            struct CompactAction {
                std::string id;
                WrappedText label;
                bool armed = false;
            };
            std::vector<CompactAction> compact;
            float rowHeight = 48.0f * scale;
            compact.reserve(fixture.offeredActionIds.size());
            for (const std::string& actionId : fixture.offeredActionIds) {
                const auto found = actionLabel(actionId);
                if (!found) {
                    result.layoutError = "unknown_action:" + actionId;
                    return result;
                }
                const bool armed = actionId == fixture.armedActionId;
                const std::string exactResult = localized(*found, locale);
                const std::string actionCopy = armed ? armedCopy(exactResult,
                    fixture.actorPlayerId, locale) : exactResult;
                const WrappedText label = wrap(actionCopy, actionFont,
                    actionWidth - 24.0f * scale, 2, measure);
                if (!label.ok) {
                    result.layoutError = "action_wrap_failed:" + actionId;
                    return result;
                }
                rowHeight = std::max(rowHeight,
                    label.lineHeight * label.lines.size() + 16.0f * scale);
                compact.push_back({actionId, label, armed});
            }
            for (std::size_t index = 0; index < compact.size(); ++index) {
                const CompactAction& action = compact[index];
                const float actionX = x + padding +
                    index * (actionWidth + gap);
                result.items.push_back({"action_" + std::to_string(index),
                    RulesDecisionDrawKind::Action,
                    {actionX, y, actionWidth, rowHeight},
                    joinedLines(action.label), action.id, actionFont,
                    static_cast<int>(action.label.lines.size()), true, false,
                    action.armed});
                if (action.armed)
                    result.items.push_back({"armed_diamond_v4",
                        RulesDecisionDrawKind::ArmedDiamond,
                        {actionX + actionWidth - exactShapeV4 - 4.0f * scale,
                         y + (rowHeight - exactShapeV4) * 0.5f,
                         exactShapeV4, exactShapeV4}, "", action.id,
                        0.0f, 0, true, false, true});
                if (action.id == fixture.focusedActionId &&
                        fixture.armedActionId.empty())
                    result.items.push_back({"focus_chevron",
                        RulesDecisionDrawKind::FocusChevron,
                        {actionX + 6.0f * scale,
                         y + (rowHeight - criticalShape) * 0.5f,
                         criticalShape, criticalShape}, "", action.id,
                        0.0f, 0, true, true, false});
            }
            y += rowHeight + gap;
        } else {
        for (std::size_t index = 0; index < fixture.offeredActionIds.size(); ++index) {
            const std::string& actionId = fixture.offeredActionIds[index];
            const auto found = actionLabel(actionId);
            if (!found) {
                result.layoutError = "unknown_action:" + actionId;
                return result;
            }
            const bool armed = actionId == fixture.armedActionId;
            const std::string exactResult = localized(*found, locale);
            const std::string actionCopy = armed ? armedCopy(exactResult,
                fixture.actorPlayerId, locale) : exactResult;
            const WrappedText label = wrap(actionCopy,
                actionFont, contentWidth - 40.0f * scale, 2, measure);
            if (!label.ok) {
                result.layoutError = "action_wrap_failed:" + actionId;
                return result;
            }
            const float actionHeight = std::max(48.0f * scale,
                label.lineHeight * label.lines.size() + 16.0f * scale);
            result.items.push_back({"action_" + std::to_string(index),
                RulesDecisionDrawKind::Action,
                {x + padding, y, contentWidth, actionHeight},
                joinedLines(label), actionId, actionFont,
                static_cast<int>(label.lines.size()), true, false, armed});
            if (armed)
                result.items.push_back({"armed_diamond_v4",
                    RulesDecisionDrawKind::ArmedDiamond,
                    {x + modalWidth - padding - exactShapeV4 - 6.0f * scale,
                     y + (actionHeight - exactShapeV4) * 0.5f,
                     exactShapeV4, exactShapeV4}, "", actionId,
                    0.0f, 0, true, false, true});
            // Once an action is armed, keyboard/pointer focus belongs to the
            // separate Confirm row.  Keeping the action-row chevron as well
            // would create two simultaneous focus targets for every armed
            // non-group product branch (including IllegalBreak 06C).
            if (actionId == fixture.focusedActionId &&
                    fixture.armedActionId.empty())
                result.items.push_back({"focus_chevron",
                    RulesDecisionDrawKind::FocusChevron,
                    {x + padding + 8.0f * scale,
                     y + (actionHeight - criticalShape) * 0.5f,
                     criticalShape, criticalShape}, "", actionId,
                    0.0f, 0, true, true, false});
            y += actionHeight + 8.0f * scale;
        }
        }
        if (!fixture.armedActionId.empty()) {
            const auto found = actionLabel(fixture.armedActionId);
            if (!found) {
                result.layoutError = "unknown_armed_action:" + fixture.armedActionId;
                return result;
            }
            const WrappedText confirmLabel = wrap(confirmCopy(localized(
                *found, locale), fixture.actorPlayerId, locale),
                actionFont, contentWidth - 40.0f * scale, 2, measure);
            if (!confirmLabel.ok) {
                result.layoutError = "confirm_wrap_failed:" + fixture.armedActionId;
                return result;
            }
            const float confirmHeight = std::max(48.0f * scale,
                confirmLabel.lineHeight * confirmLabel.lines.size() +
                    16.0f * scale);
            // Keep the full confirm rectangle as the interaction target while
            // moving only its unchanged glyph run into a dedicated content
            // lane.  The leading lane is exclusively owned by the focus
            // chevron and the trailing lane by the confirm shape.
            const float confirmLeadingInset = exactShapeV4 + 16.0f * scale;
            const float confirmTrailingInset = exactShapeV4 + 12.0f * scale;
            result.items.push_back({"confirm", RulesDecisionDrawKind::Confirm,
                {x + padding, y, contentWidth, confirmHeight},
                "", fixture.armedActionId, actionFont,
                static_cast<int>(confirmLabel.lines.size()), true});
            result.items.push_back({"confirm_copy_inset",
                RulesDecisionDrawKind::Text,
                {x + padding + confirmLeadingInset, y,
                 contentWidth - confirmLeadingInset - confirmTrailingInset,
                 confirmHeight}, joinedLines(confirmLabel),
                fixture.armedActionId, actionFont,
                static_cast<int>(confirmLabel.lines.size()), true});
            result.items.push_back({"focus_chevron_v4",
                RulesDecisionDrawKind::FocusChevron,
                {x + padding + 8.0f * scale,
                 y + (confirmHeight - exactShapeV4) * 0.5f,
                 exactShapeV4, exactShapeV4}, "", fixture.armedActionId,
                0.0f, 0, true, true, false});
            result.items.push_back({"confirm_double_frame_enter_arrow_v4",
                RulesDecisionDrawKind::ConfirmDoubleFrameEnterArrow,
                {x + modalWidth - padding - exactShapeV4 - 6.0f * scale,
                 y + (confirmHeight - exactShapeV4) * 0.5f,
                 exactShapeV4, exactShapeV4}, "",
                fixture.armedActionId, 0.0f, 0, true, false, false});
            y += confirmHeight + 8.0f * scale;
        }
        if (fixture.offeredActionIds.empty())
            result.items.push_back({"blocking_shape", RulesDecisionDrawKind::BlockingShape,
                {x + modalWidth - padding - criticalShape - 6.0f * scale,
                 panelY + padding, criticalShape, criticalShape}, "", "",
                0.0f, 0, true,
                 false, false});
        y += padding - 8.0f * scale;
    }

    RulesDecisionRect& panel = result.items[panelIndex].bounds;
    panel.height = y - panelY;
    if (panel.y + panel.height > profile.logicalHeight - safe ||
        panel.height > profile.logicalHeight * 0.82f) {
        result.layoutError = "panel_height_exceeded";
        return result;
    }
    if (fixture.placement) {
        result.tableOcclusionFraction = projectedTableOcclusionFraction(
            profile, panel);
        if (result.tableOcclusionFraction < 0.0f) {
            result.layoutError = "table_projection_required";
            return result;
        }
    }
    result.layoutBuildOk = true;
    return result;
}

RulesDecisionDrawValidation validateRulesDecisionDrawList(
    const RulesDecisionDrawList& drawList)
{
    RulesDecisionDrawValidation result;
    if (!drawList.layoutBuildOk)
        result.errors.push_back(drawList.layoutError.empty() ?
            "layout_build_failed" : drawList.layoutError);
    const float density = drawList.profile.density;
    const float safe = 24.0f / density;
    int focusCount = 0;
    std::vector<RulesDecisionRect> actionBounds;
    for (const auto& item : drawList.items) {
        if (item.bounds.x < safe - 0.01f || item.bounds.y < safe - 0.01f ||
            item.bounds.x + item.bounds.width >
                drawList.profile.logicalWidth - safe + 0.01f ||
            item.bounds.y + item.bounds.height >
                drawList.profile.logicalHeight - safe + 0.01f)
            result.errors.push_back("safe_area_violation:" + item.id);
        if (item.kind == RulesDecisionDrawKind::Text &&
            item.fontLogicalPx * density + 0.01f <
                14.0f * drawList.profile.uiScale * density)
            result.errors.push_back("glyph_below_minimum:" + item.id);
        const bool critical =
            item.kind == RulesDecisionDrawKind::FocusChevron ||
            item.kind == RulesDecisionDrawKind::ArmedDiamond ||
            item.kind == RulesDecisionDrawKind::ConfirmDoubleFrameEnterArrow ||
            item.kind == RulesDecisionDrawKind::PreviewGhostBall ||
            item.kind == RulesDecisionDrawKind::PreviewCrosshair ||
            item.kind == RulesDecisionDrawKind::InvalidOctagon ||
            item.kind == RulesDecisionDrawKind::InvalidCollisionRing ||
            item.kind == RulesDecisionDrawKind::BlockingShape;
        if (critical && std::min(item.bounds.width, item.bounds.height) *
                density + 0.01f < 18.0f)
            result.errors.push_back("critical_shape_below_18_physical_px:" +
                item.id);
        if (item.focus) ++focusCount;
        if (item.kind == RulesDecisionDrawKind::Action ||
            item.kind == RulesDecisionDrawKind::Confirm) {
            for (const auto& previous : actionBounds)
                if (intersects(previous, item.bounds))
                    result.errors.push_back("action_overlap:" + item.id);
            actionBounds.push_back(item.bounds);
        }
    }
    if (focusCount > 1)
        result.errors.push_back("multiple_focus_targets");
    if (isOpenPendingOrStale(drawList.stateId) &&
        (!drawList.armedActionId.empty() || !drawList.selectedActionId.empty()))
        result.errors.push_back("default_selection_or_arm");
    std::set<std::string> uniqueActions;
    for (const auto& action : drawList.offeredActionIds) {
        if (!uniqueActions.insert(action).second)
            result.errors.push_back("duplicate_action:" + action);
        if (kFoundationActions.count(action))
            result.errors.push_back("foundation_action_visible:" + action);
        if (!actionLabel(action))
            result.errors.push_back("unoffered_action_visible:" + action);
    }
    if (!drawList.shotInputBlocked)
        result.errors.push_back("shot_input_not_blocked");
    if (!drawList.localPreviewAuthorityInvariant ||
        drawList.localAuthorityMutationCount != 0)
        result.errors.push_back("local_preview_authority_mutation");
    if (drawList.tableOcclusionFraction > 0.18f + 0.0001f)
        result.errors.push_back("placement_occlusion_exceeded");
    if (isArmedConfirm(drawList.stateId)) {
        const auto label = actionLabel(drawList.armedActionId);
        const RulesDecisionDrawItem* armed = actionItem(
            drawList, drawList.armedActionId);
        const RulesDecisionDrawItem* confirm = itemWithId(drawList, "confirm");
        const RulesDecisionDrawItem* focus = itemWithId(
            drawList, "focus_chevron_v4");
        const RulesDecisionDrawItem* diamond = itemWithId(
            drawList, "armed_diamond_v4");
        const RulesDecisionDrawItem* confirmShape = itemWithId(
            drawList, "confirm_double_frame_enter_arrow_v4");
        const std::string exactResult = !label ? "" :
            localized(*label, drawList.locale);
        if (armed == nullptr || unwrapped(armed->text) != unwrapped(armedCopy(
                exactResult, drawList.actorPlayerId, drawList.locale)))
            result.errors.push_back("armed_copy_action_actor_mismatch");
        const RulesDecisionDrawItem* confirmCopyItem = itemWithId(
            drawList, "confirm_copy_inset");
        if (confirm == nullptr || !confirm->text.empty() ||
                confirmCopyItem == nullptr ||
                unwrapped(confirmCopyItem->text) != unwrapped(confirmCopy(
                exactResult, drawList.actorPlayerId, drawList.locale)))
            result.errors.push_back("confirm_copy_action_actor_mismatch");
        if (kindCount(drawList, RulesDecisionDrawKind::FocusChevron) != 1 ||
            kindCount(drawList, RulesDecisionDrawKind::ArmedDiamond) != 1 ||
            kindCount(drawList,
                RulesDecisionDrawKind::ConfirmDoubleFrameEnterArrow) != 1)
            result.errors.push_back("armed_focus_confirm_shape_inventory");
        const auto contained = [](const RulesDecisionRect& child,
                                  const RulesDecisionRect& parent) {
            return child.x >= parent.x && child.y >= parent.y &&
                child.x + child.width <= parent.x + parent.width &&
                child.y + child.height <= parent.y + parent.height;
        };
        if (armed == nullptr || confirm == nullptr ||
                confirmCopyItem == nullptr || focus == nullptr ||
                diamond == nullptr || confirmShape == nullptr ||
                !contained(focus->bounds, confirm->bounds) ||
                !contained(confirmShape->bounds, confirm->bounds) ||
                !contained(confirmCopyItem->bounds, confirm->bounds) ||
                intersects(confirmCopyItem->bounds, focus->bounds) ||
                intersects(confirmCopyItem->bounds, confirmShape->bounds) ||
                !contained(diamond->bounds, armed->bounds) ||
                focus->actionId != drawList.armedActionId ||
                diamond->actionId != drawList.armedActionId ||
                confirmShape->actionId != drawList.armedActionId)
            result.errors.push_back(
                "armed_focus_confirm_row_local_role_mismatch");
    }
    const bool placement = drawList.stateId.find("preview_") !=
        std::string::npos;
    if (placement) {
        const bool valid = drawList.stateId.find("preview_valid") !=
            std::string::npos;
        if (kindCount(drawList, RulesDecisionDrawKind::PreviewGhostBall) != 1)
            result.errors.push_back("placement_ghost_cue_ball_missing");
        if (valid &&
                (kindCount(drawList, RulesDecisionDrawKind::PreviewCrosshair) != 1 ||
                 kindCount(drawList, RulesDecisionDrawKind::InvalidOctagon) != 0 ||
                 kindCount(drawList,
                     RulesDecisionDrawKind::InvalidCollisionRing) != 0))
            result.errors.push_back("valid_placement_shape_inventory");
        if (!valid &&
                (kindCount(drawList, RulesDecisionDrawKind::PreviewCrosshair) != 0 ||
                 kindCount(drawList, RulesDecisionDrawKind::InvalidOctagon) != 1 ||
                 kindCount(drawList,
                     RulesDecisionDrawKind::InvalidCollisionRing) != 1))
            result.errors.push_back("invalid_placement_shape_inventory");
        for (const auto& item : drawList.items) {
            if (item.kind != RulesDecisionDrawKind::Action) continue;
            const bool placementConfirm =
                item.actionId == rules::kConfirmFullTablePlacementActionId ||
                item.actionId ==
                    rules::kConfirmBehindHeadStringPlacementActionId;
            const bool expectedEnabled = !placementConfirm || valid;
            if (item.enabled != expectedEnabled)
                result.errors.push_back(
                    "placement_action_enablement_mismatch:" + item.actionId);
        }
        const bool behind = drawList.stateId.rfind("behind_head_string", 0) == 0;
        RulesDecisionCanonicalFixture fixture =
            rulesDecisionCanonicalFixture(drawList.stateId);
        fixture.actorPlayerId = drawList.actorPlayerId;
        fixture.previewWorldX = drawList.previewWorldX;
        fixture.previewWorldZ = drawList.previewWorldZ;
        const RulesDecisionDrawItem* world = itemWithId(drawList,
            behind ? "world_legal_region_copy" : "world_preview_coordinate");
        if (world == nullptr || unwrapped(world->text) != unwrapped(previewCopy(
                fixture, drawList.locale, behind)))
            result.errors.push_back("placement_world_coordinate_copy");
        if (behind &&
                (kindCount(drawList, RulesDecisionDrawKind::HeadStringBoundary) != 1 ||
                 kindCount(drawList, RulesDecisionDrawKind::LegalRegionHatch) != 1))
            result.errors.push_back("behind_head_string_region_pattern");
    }
    result.ok = result.errors.empty();
    return result;
}

}  // namespace billiardgl::render::ui
