#include "render/ui/product_ui_frame_coordinator.h"

#include "deterministic_hash.h"
#include "render/modern/camera_projection.h"
#include "render/modern/modern_product_session.h"
#include "render/ui/product_shell_draw_list.h"
#include "render/ui/training_control_hud.h"
#include "render/ui/training_goal_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>

namespace billiardgl::render::ui {
namespace {

template <typename Value>
void appendExactBits(std::string& output, const Value& value)
{
    output.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

void appendExactField(std::string& output, const std::string& value)
{
    const std::uint64_t length = value.size();
    appendExactBits(output, length);
    output += value;
}

void appendLedgerField(std::string& output, const std::string& value)
{
    output += std::to_string(value.size()) + ":" + value + "\n";
}

std::string doubleBits(double value)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::hex << std::setfill('0') << std::setw(16) << bits;
    return output.str();
}

std::string insetGeometryDigest(
    const training::TrajectoryInsetDrawPackage& package)
{
    std::string canonical;
    for (const auto& vertex : package.vertices) {
        appendExactBits(canonical, vertex.xPhysicalPx);
        appendExactBits(canonical, vertex.yPhysicalPx);
        for (const float channel : vertex.linearRgba)
            appendExactBits(canonical, channel);
        appendExactBits(canonical, vertex.insetOwnershipToken);
    }
    for (const auto& batch : package.semanticBatches) {
        appendExactField(canonical, batch.parentSourceKey);
        appendExactField(canonical, batch.primitiveId);
        appendExactField(canonical, batch.arcSegmentId);
        appendExactField(canonical, batch.insetMemberKey);
        appendExactBits(canonical, batch.track);
        appendExactBits(canonical, batch.role);
        appendExactBits(canonical, batch.firstVertex);
        appendExactBits(canonical, batch.vertexCount);
        appendExactBits(canonical, batch.insetOwnershipToken);
    }
    return sha256Hex(canonical);
}

std::array<std::string, 5> insetOrderedDigests(
    const training::TrajectoryInsetDrawPackage& package)
{
    std::string parents, probes, events, contributors, intervals;
    for (const auto& key : package.orderedParentSourceKeys)
        appendLedgerField(parents, key);
    for (const auto& probe : package.probes) {
        for (const auto& value : {probe.probeId, probe.parentSourceKey,
                 probe.insetMemberKey, probe.companionInsetMemberKey})
            appendLedgerField(probes, value);
        appendLedgerField(probes, std::to_string(probe.insetOwnershipToken));
        appendLedgerField(probes, std::to_string(probe.strokeXPhysicalPx));
        appendLedgerField(probes, std::to_string(probe.strokeYPhysicalPx));
        appendLedgerField(probes, std::to_string(probe.companionXPhysicalPx));
        appendLedgerField(probes, std::to_string(probe.companionYPhysicalPx));
        appendLedgerField(probes, std::to_string(probe.backgroundXPhysicalPx));
        appendLedgerField(probes, std::to_string(probe.backgroundYPhysicalPx));
        probes += training::trajectoryInsetProbeSourceEvidenceLedger(probe);
    }
    for (const auto& event : package.predictedDashEvents) {
        appendLedgerField(events, event.eventKey);
        appendLedgerField(events,
            std::to_string(event.routeWideDashEventOrdinal));
        appendLedgerField(events, std::to_string(event.routeSpanOrdinal));
        appendLedgerField(events, doubleBits(event.routeDistanceStartPx));
        appendLedgerField(events, doubleBits(event.routeDistanceEndPx));
        for (std::size_t role = 0; role < event.orderedRoleMemberKeys.size();
             ++role) {
            appendLedgerField(events, event.orderedRoleMemberKeys[role]);
            appendLedgerField(events,
                std::to_string(event.orderedRoleTokens[role]));
        }
        for (const auto& contributor : event.orderedContributors) {
            appendLedgerField(contributors, event.eventKey);
            appendLedgerField(contributors, contributor.parentSourceKey);
            appendLedgerField(contributors, contributor.arcSegmentId);
            appendLedgerField(contributors,
                std::to_string(contributor.routeOrderOrdinal));
            appendLedgerField(contributors,
                doubleBits(contributor.sourceLocalTStart));
            appendLedgerField(contributors,
                doubleBits(contributor.sourceLocalTEnd));
            appendLedgerField(contributors,
                doubleBits(contributor.eventRouteDistanceStartPx));
            appendLedgerField(contributors,
                doubleBits(contributor.eventRouteDistanceEndPx));
        }
    }
    for (const auto& row : package.mappingRows) {
        if (row.orderedPredictedRouteIntervals.empty()) continue;
        appendLedgerField(intervals, row.parentSourceKey);
        appendLedgerField(intervals,
            std::to_string(row.orderedPredictedRouteIntervals.size()));
        for (const auto& interval : row.orderedPredictedRouteIntervals) {
            appendLedgerField(intervals, interval.arcSegmentId);
            appendLedgerField(intervals,
                std::to_string(interval.routeOrderOrdinal));
            appendLedgerField(intervals,
                std::to_string(interval.routeSpanOrdinal));
            appendLedgerField(intervals, doubleBits(interval.sourceLocalTStart));
            appendLedgerField(intervals, doubleBits(interval.sourceLocalTEnd));
            appendLedgerField(intervals,
                doubleBits(interval.routeDistanceStartPx));
            appendLedgerField(intervals,
                doubleBits(interval.routeDistanceEndPx));
            appendLedgerField(intervals, interval.dashEventKey);
            appendLedgerField(intervals, interval.nonRenderedReason);
        }
    }
    return {{sha256Hex(parents), sha256Hex(probes), sha256Hex(events),
        sha256Hex(contributors), sha256Hex(intervals)}};
}

std::string insetLayoutDigest(const TrajectoryInsetShellLayout& layout)
{
    std::string canonical;
    const auto rect = [&](const product_hud::HudRect& value) {
        appendExactBits(canonical, value.x);
        appendExactBits(canonical, value.y);
        appendExactBits(canonical, value.width);
        appendExactBits(canonical, value.height);
    };
    appendExactField(canonical, layout.profileId);
    appendExactBits(canonical, layout.insetVisible);
    appendExactBits(canonical, layout.insetControlOn);
    appendExactBits(canonical, layout.worldOverlayVisible);
    appendExactBits(canonical, layout.worldOverlayControlOn);
    rect(layout.authoritativePanelRect); rect(layout.panelRect);
    rect(layout.plotCutoutRect); rect(layout.headerRect); rect(layout.titleRect);
    for (const auto& value : layout.orientationLabelRects) rect(value);
    for (const auto& value : layout.legendRects) rect(value);
    rect(layout.toggleRect); rect(layout.toggleLabelRect); rect(layout.focusRect);
    for (const auto& value : layout.borderRects) rect(value);
    appendExactField(canonical, layout.title);
    appendExactField(canonical, layout.toggleLabel);
    for (const auto& value : layout.orientationLabels)
        appendExactField(canonical, value);
    for (const auto& value : layout.legendEntries)
        appendExactField(canonical, value);
    return sha256Hex(canonical);
}

std::string insetExclusionDigest(const TrajectoryInsetShellRequest& request)
{
    std::string canonical;
    appendExactBits(canonical, request.exclusions.size());
    for (const auto& exclusion : request.exclusions) {
        appendExactField(canonical, exclusion.id);
        appendExactBits(canonical, exclusion.active);
        appendExactBits(canonical, exclusion.rect.x);
        appendExactBits(canonical, exclusion.rect.y);
        appendExactBits(canonical, exclusion.rect.width);
        appendExactBits(canonical, exclusion.rect.height);
        if (!exclusion.additionalRects.empty()) {
            appendExactField(canonical, "additional_occupied_rectangles_v1");
            appendExactBits(canonical, exclusion.additionalRects.size());
        }
        for (const auto& rect : exclusion.additionalRects) {
            appendExactBits(canonical, rect.x);
            appendExactBits(canonical, rect.y);
            appendExactBits(canonical, rect.width);
            appendExactBits(canonical, rect.height);
        }
    }
    return sha256Hex(canonical);
}

constexpr const char* kHudTokenSetId =
    "focused_practice_hud_shipping_tokens_v1";
constexpr const char* kShellTokenSetId =
    "focused_practice_shell_shipping_tokens_v1";

bool isGameplayScreen(ProductScreen screen)
{
    return screen == ProductScreen::Match;
}

bool isShellScreen(ProductScreen screen)
{
    return screen == ProductScreen::MainMenu ||
        screen == ProductScreen::SinglePlayerSetup ||
        screen == ProductScreen::TrainingCatalog ||
        screen == ProductScreen::TrainingSession ||
        screen == ProductScreen::ReplayBrowser ||
        screen == ProductScreen::ReplayPlayback ||
        screen == ProductScreen::Settings || screen == ProductScreen::Help;
}

const char* screenId(ProductScreen screen)
{
    switch (screen) {
    case ProductScreen::MainMenu: return "main_menu";
    case ProductScreen::SinglePlayerSetup: return "single_player_setup";
    case ProductScreen::Match: return "match";
    case ProductScreen::TrainingCatalog: return "training_catalog";
    case ProductScreen::TrainingSession: return "training_session";
    case ProductScreen::ReplayBrowser: return "replay_browser";
    case ProductScreen::ReplayPlayback: return "replay_playback";
    case ProductScreen::Settings: return "settings";
    case ProductScreen::Help: return "help";
    }
    return "unsupported";
}

bool hasTokenSet(const HudShippingTokenRegistry& registry,
    const char* tokenSetId)
{
    return std::find(registry.tokenSetIds.begin(), registry.tokenSetIds.end(),
        tokenSetId) != registry.tokenSetIds.end();
}

void appendDraw(HudDrawList& destination, const HudDrawList& source)
{
    const std::uint32_t textRunBase = static_cast<std::uint32_t>(
        destination.textRuns.size());
    for (const UiPrimitive& sourcePrimitive : source.primitives) {
        UiPrimitive primitive = sourcePrimitive;
        if (primitive.kind == UiPrimitiveKind::GlyphQuad &&
            primitive.textRunIndex != 0xffffffffu)
            primitive.textRunIndex += textRunBase;
        destination.primitives.push_back(std::move(primitive));
    }
    destination.triangles.insert(destination.triangles.end(),
        source.triangles.begin(), source.triangles.end());
    destination.iconLabels.insert(destination.iconLabels.end(),
        source.iconLabels.begin(), source.iconLabels.end());
    destination.textRuns.insert(destination.textRuns.end(),
        source.textRuns.begin(), source.textRuns.end());
    destination.glyphQuadCount += source.glyphQuadCount;
    destination.expandedIconCount += source.expandedIconCount;
    destination.containsDebug = destination.containsDebug ||
        source.containsDebug;
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
    if (!std::isfinite(value))
        throw std::runtime_error("product_ui_evidence_non_finite");
    const double scaled = std::round(static_cast<double>(value) * 1024.0);
    if (scaled < std::numeric_limits<std::int32_t>::min() ||
        scaled > std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("product_ui_evidence_out_of_range");
    return static_cast<std::int32_t>(scaled);
}

void appendFloat(std::vector<std::uint8_t>& bytes, float value)
{
    appendU32(bytes, static_cast<std::uint32_t>(quantized(value)));
}

void appendRect(std::vector<std::uint8_t>& bytes,
    const product_hud::HudRect& rect)
{
    appendFloat(bytes, rect.x);
    appendFloat(bytes, rect.y);
    appendFloat(bytes, rect.width);
    appendFloat(bytes, rect.height);
}

void appendColor(std::vector<std::uint8_t>& bytes, const UiColor& color)
{
    appendFloat(bytes, color.red);
    appendFloat(bytes, color.green);
    appendFloat(bytes, color.blue);
    appendFloat(bytes, color.alpha);
}

std::vector<std::uint8_t> evidence(const ProductUiFrame& frame)
{
    std::vector<std::uint8_t> bytes;
    appendString(bytes, "billiardgl_product_ui_frame_v1");
    appendU32(bytes, static_cast<std::uint32_t>(frame.overlay));
    appendString(bytes, screenId(frame.screen));
    appendU32(bytes, static_cast<std::uint32_t>(frame.palette));
    appendU32(bytes, static_cast<std::uint32_t>(frame.physicalWidth));
    appendU32(bytes, static_cast<std::uint32_t>(frame.physicalHeight));
    appendFloat(bytes, frame.dpi);
    appendU32(bytes, frame.ok ? 1u : 0u);
    appendString(bytes, frame.errorCode);
    appendRect(bytes, frame.draw.safeRect);
    appendU32(bytes, static_cast<std::uint32_t>(frame.draw.primitives.size()));
    for (const UiPrimitive& primitive : frame.draw.primitives) {
        appendU32(bytes, static_cast<std::uint32_t>(primitive.kind));
        appendRect(bytes, primitive.rect);
        appendRect(bytes, primitive.sourceRect);
        appendRect(bytes, primitive.clipRect);
        appendColor(bytes, primitive.color);
        appendFloat(bytes, primitive.u0);
        appendFloat(bytes, primitive.v0);
        appendFloat(bytes, primitive.u1);
        appendFloat(bytes, primitive.v1);
        appendFloat(bytes, primitive.thickness);
        appendU32(bytes, primitive.codepoint);
        appendString(bytes, primitive.token);
    }
    const std::vector<std::uint8_t> triangleBytes =
        canonicalHudTriangleBytes(frame.draw.triangles);
    appendU32(bytes, static_cast<std::uint32_t>(triangleBytes.size()));
    bytes.insert(bytes.end(), triangleBytes.begin(), triangleBytes.end());
    appendU32(bytes, static_cast<std::uint32_t>(frame.draw.iconLabels.size()));
    for (const UiIconLabel& label : frame.draw.iconLabels) {
        appendFloat(bytes, label.center.x);
        appendFloat(bytes, label.center.y);
        appendRect(bytes, label.clipRect);
        appendColor(bytes, label.color);
        appendFloat(bytes, label.requestedPixelSize);
        appendString(bytes, label.text);
        appendString(bytes, label.fontRole);
        appendString(bytes, label.sourceToken);
        appendString(bytes, label.resolvedToken);
    }
    appendU32(bytes, static_cast<std::uint32_t>(frame.draw.textRuns.size()));
    for (const TextRunMetrics& text : frame.draw.textRuns) {
        appendString(bytes, text.text);
        appendRect(bytes, text.clipRect);
        appendFloat(bytes, text.requestedPixelSize);
        appendFloat(bytes, text.measuredWidth);
        appendU32(bytes, text.tabularDigits ? 1u : 0u);
    }
    appendU32(bytes, static_cast<std::uint32_t>(frame.draw.glyphQuadCount));
    appendU32(bytes, frame.rulesDecisionActive ? 1u : 0u);
    appendU32(bytes, static_cast<std::uint32_t>(
        frame.rulesDecisionHitRects.size()));
    for (const RulesDecisionUiHitRect& hit : frame.rulesDecisionHitRects) {
        appendString(bytes, hit.controlId);
        appendString(bytes, hit.actionId);
        appendRect(bytes, hit.physicalRect);
        appendU32(bytes, hit.enabled ? 1u : 0u);
        appendU32(bytes, hit.confirm ? 1u : 0u);
    }
    return bytes;
}

void finish(ProductUiFrame& frame)
{
    try {
        frame.canonicalEvidence = evidence(frame);
        frame.evidenceSha256 = sha256Hex(std::string(
            frame.canonicalEvidence.begin(), frame.canonicalEvidence.end()));
    } catch (const std::exception& error) {
        frame.ok = false;
        frame.errorCode = error.what();
        frame.canonicalEvidence.clear();
        frame.evidenceSha256.clear();
    }
}

ProductUiFrame failure(ProductUiFrame frame, const std::string& error)
{
    frame.ok = false;
    frame.errorCode = error.empty() ? "product_ui_unknown_error" : error;
    frame.draw.ok = false;
    frame.draw.errorCode = frame.errorCode;
    finish(frame);
    return frame;
}

}  // namespace

std::string canonicalTrajectoryInsetGeometryDigest(
    const training::TrajectoryInsetDrawPackage& package)
{
    return insetGeometryDigest(package);
}

std::string canonicalTrajectoryInsetLayoutDigest(
    const TrajectoryInsetShellLayout& layout)
{
    return insetLayoutDigest(layout);
}

void TrajectoryInsetVisibleCurrentFrame::rebind() noexcept
{
    request.package = &package;
    request.shellRequest = &shellRequest;
    request.layout = &layout;
    request.chrome = &chrome;
}

TrajectoryInsetVisibleCurrentFrame::TrajectoryInsetVisibleCurrentFrame(
    const TrajectoryInsetVisibleCurrentFrame& other)
    : ok(other.ok), consumed(other.consumed), executing(other.executing), errorCode(other.errorCode),
      shellRequest(other.shellRequest), layout(other.layout),
      chrome(other.chrome), package(other.package), request(other.request)
{
    rebind();
}

TrajectoryInsetVisibleCurrentFrame&
TrajectoryInsetVisibleCurrentFrame::operator=(
    const TrajectoryInsetVisibleCurrentFrame& other)
{
    if (this == &other) return *this;
    ok = other.ok; consumed = other.consumed; executing = other.executing; errorCode = other.errorCode;
    shellRequest = other.shellRequest; layout = other.layout;
    chrome = other.chrome; package = other.package; request = other.request;
    rebind();
    return *this;
}

TrajectoryInsetVisibleCurrentFrame::TrajectoryInsetVisibleCurrentFrame(
    TrajectoryInsetVisibleCurrentFrame&& other) noexcept
    : ok(other.ok), consumed(other.consumed), executing(other.executing), errorCode(std::move(other.errorCode)),
      shellRequest(std::move(other.shellRequest)), layout(std::move(other.layout)),
      chrome(std::move(other.chrome)), package(std::move(other.package)),
      request(std::move(other.request))
{
    rebind();
}

TrajectoryInsetVisibleCurrentFrame&
TrajectoryInsetVisibleCurrentFrame::operator=(
    TrajectoryInsetVisibleCurrentFrame&& other) noexcept
{
    if (this == &other) return *this;
    ok = other.ok; consumed = other.consumed; executing = other.executing;
    errorCode = std::move(other.errorCode);
    shellRequest = std::move(other.shellRequest); layout = std::move(other.layout);
    chrome = std::move(other.chrome); package = std::move(other.package);
    request = std::move(other.request); rebind();
    return *this;
}

TrajectoryInsetVisibleCurrentFrame makeTrajectoryInsetVisibleCurrentFrame(
    const ModernProductSessionSnapshot& session,
    const PresentationSnapshot&,
    const ProductUiFrameEnvironment& environment,
    const TrajectoryInsetVisibleCurrentContext& context,
    const ProductUiFrame* preparedFrame)
{
    TrajectoryInsetVisibleCurrentFrame current;
    if (!context.sourceContentSha256.empty() &&
            context.sourceContentSha256 != trajectoryInsetSourceContentSha256(session.shell)) {
        current.errorCode = "trajectory_inset_source_content_mismatch";
        return current;
    }
    const ProductScreen screen = session.shell.screen;
    const bool accepted = screen == ProductScreen::TrainingSession ||
        screen == ProductScreen::ReplayPlayback;
    if (!environment.productMode || environment.automation ||
        environment.windowless || !accepted || !context.present ||
        !context.sessionStable || context.transitioning || context.errored ||
        context.modalVisible || !context.insetToggleOn ||
        context.worldOverlayToggleOn || context.sessionSerial == 0 ||
        context.frameSerial == 0 || context.layoutSerial == 0 ||
        context.sessionSerial != context.frameSerial ||
        context.layoutSerial != context.frameSerial ||
        context.cp1GenerationId.empty() ||
        (context.samples != 1 && context.samples != 2 &&
            context.samples != 4)) {
        current.errorCode = "trajectory_inset_visible_current_context_invalid";
        return current;
    }
    const int width = session.shellViewport.physicalWidth;
    const int height = session.shellViewport.physicalHeight;
    const float scale = session.shell.uiScale;
    if (width <= 0 || height <= 0 || !std::isfinite(scale) || scale <= 0.0f) {
        current.errorCode = "trajectory_inset_visible_viewport_invalid";
        return current;
    }
    current.shellRequest.profileId = std::to_string(width) + "x" +
        std::to_string(height) + "_ui" +
        std::to_string(static_cast<int>(std::lround(scale * 100.0f)));
    if (preparedFrame != nullptr)
        current.shellRequest.profileId = "product_" + current.shellRequest.profileId;
    current.shellRequest.physicalWidth = width;
    current.shellRequest.physicalHeight = height;
    current.shellRequest.uiScale = scale;
    current.shellRequest.insetControlOn = context.insetToggleOn;
    current.shellRequest.worldOverlayControlOn =
        context.worldOverlayToggleOn;
    current.shellRequest.insetControlFocused = context.insetControlFocused;
    current.shellRequest.modalVisible = context.modalVisible;
    current.shellRequest.exclusions = {
        {"player status HUD", {32, 24, 420, 108}},
        {"shot power and tip controls",
            {32, static_cast<float>(height) - 96, 600, 72}},
        {"rules decision panel or modal", {500, 160, 420, 360}, false},
        {"caption and accessibility live-region band",
            {360, static_cast<float>(height) - 80, 560, 48}},
        {"safe-area edges and product footer",
            {0, static_cast<float>(height) - 112 * scale,
                static_cast<float>(width), 112 * scale}},
    };
    if (preparedFrame != nullptr) {
        if (!preparedFrame->ok || preparedFrame->screen != screen ||
                preparedFrame->physicalWidth != width ||
                preparedFrame->physicalHeight != height) {
            current.errorCode = "trajectory_inset_prepared_ui_mismatch";
            return current;
        }
        for (auto& exclusion : current.shellRequest.exclusions) {
            exclusion.rect = {};
            exclusion.active = false;
            exclusion.additionalRects.clear();
        }
        const auto include = [&](std::size_t index,
                const product_hud::HudRect& rect) {
            if (rect.width <= 0 || rect.height <= 0) return;
            auto& exclusion = current.shellRequest.exclusions[index];
            if (!exclusion.active) {
                exclusion.rect = rect;
                exclusion.active = true;
            } else {
                exclusion.additionalRects.push_back(rect);
            }
        };
        for (const auto& primitive : preparedFrame->draw.primitives) {
            if (primitive.kind != UiPrimitiveKind::Panel) continue;
            const std::size_t index = primitive.token == "training_control_panel" ? 1 :
                primitive.token == "shell_footer_panel" ? 4 : 0;
            include(index, primitive.rect);
        }
        for (const auto& rect : preparedFrame->rulesDecisionExclusionRects)
            include(2, rect);
    }
    current.layout = buildTrajectoryInsetShellLayout(current.shellRequest);
    if (current.layout.ok && !current.layout.insetVisible) {
        current.errorCode = current.layout.suppressionReason;
        return current;
    }
    current.chrome = buildTrajectoryInsetChromeDrawList(current.layout);
    if (!current.layout.ok || !current.chrome.ok) {
        current.errorCode = !current.layout.ok ? current.layout.errorCode :
                                              current.chrome.errorCode;
        return current;
    }
    training::TrajectoryInsetBuildRequest build;
    build.generationId = context.cp1GenerationId;
    build.snapshot = session.shell.trajectoryOverlay;
    build.palette = context.palette;
    build.layout = {current.layout.profileId, width, height, scale,
        {current.layout.panelRect.x, current.layout.panelRect.y,
            current.layout.panelRect.width, current.layout.panelRect.height},
        {current.layout.plotCutoutRect.x, current.layout.plotCutoutRect.y,
            current.layout.plotCutoutRect.width,
            current.layout.plotCutoutRect.height}};
    current.package = training::buildTrajectoryInsetDrawPackage(build);
    if (!current.package.ok) {
        current.errorCode = current.package.errorCode;
        return current;
    }
    current.request.present = true;
    current.request.sessionSerial = context.sessionSerial;
    current.request.frameSerial = context.frameSerial;
    current.request.layoutSerial = context.layoutSerial;
    current.request.screen = screen;
    current.request.sessionStable = true;
    current.request.insetToggleOn = true;
    current.request.worldOverlayToggleOn = false;
    current.request.samples = context.samples;
    current.request.palette = context.palette;
    current.request.cp1GenerationId = current.package.generationId;
    current.request.cp1Digests = insetOrderedDigests(current.package);
    current.request.cp1GeometryDigest = insetGeometryDigest(current.package);
    current.request.cp2ProfileId = current.layout.profileId;
    current.request.cp2LayoutDigest = insetLayoutDigest(current.layout);
    current.request.cp2ExclusionDigest =
        insetExclusionDigest(current.shellRequest);
    current.rebind();
    current.ok = true;
    return current;
}

modern::TrajectoryInsetVisibleResult executeTrajectoryInsetVisibleCurrentFrame(
    TrajectoryInsetVisibleCurrentFrame& current,
    const TrajectoryInsetVisibleProductCallbacks& callbacks)
{
    modern::TrajectoryInsetVisibleResult result;
    if (!current.ok || current.consumed || current.executing || !callbacks.executeIntegration ||
        !callbacks.submitChrome || !callbacks.commitVisible ||
        !callbacks.invalidate) {
        result.errorCode = current.consumed || current.executing
            ? "trajectory_inset_visible_current_frame_reused"
            : "trajectory_inset_visible_product_callbacks_invalid";
        return result;
    }
    current.executing = true;
    struct FinishAttempt {
        TrajectoryInsetVisibleCurrentFrame& frame;
        ~FinishAttempt() { frame.consumed = true; frame.executing = false; }
    } finishAttempt{current};
    result = callbacks.executeIntegration(current.request);
    if (!result.ok || !result.presented) {
        callbacks.invalidate();
        result.ok = result.presented = false;
        return result;
    }
    std::string chromeError;
    if (!callbacks.submitChrome(current.layout, current.chrome, chromeError)) {
        callbacks.invalidate();
        result.ok = result.presented = false;
        result.invalidated = true;
        result.errorCode = chromeError.empty()
            ? "trajectory_inset_visible_chrome_submit_failed" : chromeError;
        return result;
    }
    callbacks.commitVisible();
    return result;
}

ProductUiFrameRequest makeProductUiFrameRequest(
    const ModernProductSessionSnapshot& session,
    const PresentationSnapshot& presentation,
    const ProductUiFrameEnvironment& environment)
{
    ProductUiFrameRequest result;
    result.environment = environment;
    result.shell = session.shell;
    result.settings = session.effectiveSettings;
    result.viewport = resolveProtectedShellViewport(
        result.shell, presentation, session.shellViewport);
    result.presentation = presentation;
    result.rulesDecisionUiImplementationEnabled =
        session.rulesDecisionUiImplementationEnabled;
    result.rulesDecisionUiProductExposed =
        session.rulesDecisionUiProductExposed;
    result.rulesDecisionGroupChoiceProductEnabled =
        session.rulesDecisionGroupChoiceProductEnabled;
    result.rulesDecisionFullTableBallInHandProductEnabled =
        session.rulesDecisionFullTableBallInHandProductEnabled;
    result.rulesDecisionIllegalBreakProductEnabled =
        session.rulesDecisionIllegalBreakProductEnabled;
    result.rulesDecisionBreakShotFoulProductEnabled =
        session.rulesDecisionBreakShotFoulProductEnabled;
    result.rulesDecisionEightOnBreakProductEnabled =
        session.rulesDecisionEightOnBreakProductEnabled;
    result.rulesDecisionProductActive = session.rulesDecisionProductActive;
    result.rulesDecisionLocale = session.rulesDecisionLocale;
    result.rulesDecision = session.rulesDecision;
    return result;
}

ShellViewport resolveProtectedShellViewport(
    const ProductShellSnapshot& shell,
    const PresentationSnapshot& presentation,
    const ShellViewport& viewport)
{
    ShellViewport result = viewport;
    if (shell.screen != ProductScreen::TrainingSession &&
        shell.screen != ProductScreen::ReplayPlayback)
        return result;
    const SharedCameraProjection projection = buildSharedCameraProjection(
        presentation.camera, viewport.physicalWidth, viewport.physicalHeight);
    if (!projection.valid) return result;
    bool any = false;
    float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
    auto include = [&](Point3 world) {
        world.y = 86.65f;
        const HomogeneousPoint clip = transformWorldToClip(projection, world);
        PhysicalScreenPoint physical;
        if (!finiteHomogeneous(clip) || clip.w <= 1e-5 ||
            !clipToPhysicalScreen(projection, clip, physical)) return;
        const float x = static_cast<float>(physical.x);
        const float y = static_cast<float>(physical.y);
        if (!any) left = right = x, top = bottom = y, any = true;
        else {
            left = std::min(left, x); right = std::max(right, x);
            top = std::min(top, y); bottom = std::max(bottom, y);
        }
    };
    for (const auto& sample : shell.trajectoryOverlay.predicted.samples)
        if (sample.visible) include(sample.position);
    for (const auto& sample : shell.trajectoryOverlay.actual.samples)
        if (sample.visible) include(sample.position);
    if (shell.trajectoryOverlay.predicted.endpointVisible)
        include(shell.trajectoryOverlay.predicted.endpoint);
    if (shell.trajectoryOverlay.actual.endpointVisible)
        include(shell.trajectoryOverlay.actual.endpoint);
    if (!any) return result;
    constexpr float dilation = 48.0f;
    result.hasProtectedGameplayRect = true;
    result.protectedGameplayRect = {left - dilation, top - dilation,
        right - left + dilation * 2.0f,
        bottom - top + dilation * 2.0f};
    return result;
}

ProductUiFrame ProductUiFrameCoordinator::build(
    const ProductUiFrameRequest& request) const
{
    ProductUiFrame frame;
    frame.screen = request.shell.screen;
    frame.palette = hudPaletteModeFor(request.settings.colorVisionMode);
    frame.physicalWidth = request.viewport.physicalWidth;
    frame.physicalHeight = request.viewport.physicalHeight;
    frame.dpi = request.viewport.dpi;
    if (!request.environment.productMode || request.environment.automation ||
        request.environment.windowless) {
        frame.ok = true;
        frame.draw.ok = true;
        finish(frame);
        return frame;
    }
    if (!std::isfinite(request.settings.uiScale) ||
        request.settings.uiScale < 0.85 || request.settings.uiScale > 1.35)
        return failure(std::move(frame), "product_ui_scale_invalid");
    if (!std::isfinite(request.shell.uiScale) ||
        std::fabs(request.shell.uiScale - request.settings.uiScale) > 1e-5)
        return failure(std::move(frame), "product_ui_scale_mismatch");
    if (request.viewport.logicalWidth <= 0 ||
        request.viewport.logicalHeight <= 0 ||
        request.viewport.physicalWidth <= 0 ||
        request.viewport.physicalHeight <= 0 ||
        !std::isfinite(request.viewport.dpi) || request.viewport.dpi <= 0.0f)
        return failure(std::move(frame), "product_ui_viewport_invalid");
    if (!tokenRegistry_.ok)
        return failure(std::move(frame), tokenRegistry_.errorCode.empty() ?
            "product_ui_token_registry_invalid" : tokenRegistry_.errorCode);
    if (request.rulesDecisionUiProductExposed)
        return failure(std::move(frame),
            "rules_decision_ui_product_exposure_forbidden");

    if (isShellScreen(frame.screen)) {
        frame.overlay = ProductUiOverlayKind::Shell;
        if (!hasTokenSet(tokenRegistry_, kShellTokenSetId))
            return failure(std::move(frame),
                "product_ui_shell_token_set_unavailable");
        ProductShellSnapshot shell = request.shell;
        shell.uiScale = static_cast<float>(request.settings.uiScale);
        ShippingScreenDrawListBuilder builder;
        const ShellDrawList shellDraw = builder.build(
            shell, request.viewport, atlas_);
        if (!shellDraw.draw.ok)
            return failure(std::move(frame), shellDraw.draw.errorCode);
        const HudIconExpansionResult expanded = expandShippingHudIcons(
            shellDraw.draw, tokenRegistry_, frame.palette, kShellTokenSetId);
        if (!expanded.ok) return failure(std::move(frame), expanded.errorCode);
        frame.draw = expanded.draw;
        if (frame.screen == ProductScreen::TrainingSession) {
            if (!hasTokenSet(tokenRegistry_, kHudTokenSetId))
                return failure(std::move(frame),
                    "product_ui_training_hud_token_set_unavailable");
            const ShellScreenLayout shellLayout = buildShippingScreenLayout(
                shell, request.viewport);
            if (!shellLayout.ok)
                return failure(std::move(frame), shellLayout.errorCode);
            const product_hud::HudRect available =
                shellLayout.gameplayCorridorRect.width > 0.0f &&
                    shellLayout.gameplayCorridorRect.height > 0.0f ?
                shellLayout.gameplayCorridorRect : shellLayout.safeRect;
            const TrainingControlHudResult trainingHud =
                buildTrainingControlHud(request.presentation,
                    request.shell.training,
                    request.viewport, shellLayout.safeRect, available,
                    static_cast<float>(request.settings.uiScale), atlas_);
            if (!trainingHud.ok)
                return failure(std::move(frame), trainingHud.errorCode);
            const HudIconExpansionResult expandedTraining =
                expandShippingHudIcons(trainingHud.draw, tokenRegistry_,
                    frame.palette, kHudTokenSetId);
            if (!expandedTraining.ok)
                return failure(std::move(frame), expandedTraining.errorCode);
            appendDraw(frame.draw, expandedTraining.draw);
            if (shell.training.teachingHints.available) {
                auto goal = buildTrainingGoalOverlayDrawPackage(
                    makeTrainingGoalOverlayRequest(shell.training.teachingHints,
                        request.presentation.camera, request.viewport.physicalWidth,
                        request.viewport.physicalHeight,
                        static_cast<float>(request.settings.uiScale)));
                // GPU packets emit primitives before triangles. Explicitly
                // exclude chrome footprints so world guide triangles cannot
                // paint across labels or controls regardless of stream order.
                std::vector<product_hud::HudRect> chromeRects;
                for (const auto& primitive : frame.draw.primitives)
                    if (primitive.kind == UiPrimitiveKind::Panel || primitive.kind == UiPrimitiveKind::SolidRect)
                        chromeRects.push_back(primitive.rect);
                // Conservative screen footprints keep the teaching line off
                // visible balls without pretending this UI overlay has depth.
                const auto projection = buildSharedCameraProjection(request.presentation.camera,
                    request.viewport.physicalWidth, request.viewport.physicalHeight);
                for (const auto& ball : request.presentation.balls) {
                    if (!ball.visible) continue;
                    float left = std::numeric_limits<float>::max(), top = left;
                    float right = -left, bottom = -left;
                    bool projected = false;
                    for (int axis = 0; axis < 3; ++axis)
                    for (float sign : {-1.0f, 1.0f}) {
                        Point3 point{ball.position.x, ball.position.y, ball.position.z};
                        if (axis == 0) point.x += sign * kBallRadius;
                        if (axis == 1) point.y += sign * kBallRadius;
                        if (axis == 2) point.z += sign * kBallRadius;
                        const auto clip = transformWorldToClip(projection, point);
                        PhysicalScreenPoint pixel;
                        if (!finiteHomogeneous(clip) || clip.w <= 1e-5 ||
                            !clipToPhysicalScreen(projection, clip, pixel)) continue;
                        left = std::min(left, static_cast<float>(pixel.x));
                        right = std::max(right, static_cast<float>(pixel.x));
                        top = std::min(top, static_cast<float>(pixel.y));
                        bottom = std::max(bottom, static_cast<float>(pixel.y));
                        projected = true;
                    }
                    if (projected) chromeRects.push_back({left - 3.0f, top - 3.0f,
                        right - left + 6.0f, bottom - top + 6.0f});
                }
                auto& triangles = goal.draw.triangles;
                triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
                    [&](const UiTriangle& triangle) {
                        const float left = std::min({triangle.first.x, triangle.second.x, triangle.third.x});
                        const float right = std::max({triangle.first.x, triangle.second.x, triangle.third.x});
                        const float top = std::min({triangle.first.y, triangle.second.y, triangle.third.y});
                        const float bottom = std::max({triangle.first.y, triangle.second.y, triangle.third.y});
                        return std::any_of(chromeRects.begin(), chromeRects.end(),
                            [&](const product_hud::HudRect& rect) {
                                return left < rect.x + rect.width && right > rect.x &&
                                    top < rect.y + rect.height && bottom > rect.y;
                            });
                    }), triangles.end());
                // Offscreen teaching marks are optional presentation. A camera
                // orbit must never turn a hidden target into a fatal UI error.
                if (goal.ok) frame.draw.triangles.insert(frame.draw.triangles.begin(),
                    triangles.begin(), triangles.end());
            }
        }
    } else if (isGameplayScreen(frame.screen)) {
        frame.overlay = ProductUiOverlayKind::GameplayHud;
        if (!hasTokenSet(tokenRegistry_, kHudTokenSetId))
            return failure(std::move(frame),
                "product_ui_hud_token_set_unavailable");
        if (!hudManifest_.ok)
            return failure(std::move(frame), "product_ui_hud_manifest_invalid");
        product_hud::ProductHudViewModel model =
            product_hud::HudStateReducer::reduce(request.presentation);
        model.helpLines = product_hud::buildHelpLines(request.settings);
        product_hud::ResponsiveHudLayoutSolver solver(hudManifest_);
        const product_hud::HudLayoutResult layout = solver.solve({
            request.viewport.physicalWidth, request.viewport.physicalHeight,
            request.viewport.dpi, static_cast<float>(request.settings.uiScale),
            model.state,
            product_hud::longestHelpLineCharacters(model.helpLines)});
        if (!layout.ok) return failure(std::move(frame), layout.errorCode);
        HudDrawListBuilder builder;
        const HudDrawList draw = builder.buildProduct(model, layout, atlas_);
        if (!draw.ok) return failure(std::move(frame), draw.errorCode);
        const HudIconExpansionResult expanded = expandShippingHudIcons(
            draw, tokenRegistry_, frame.palette, kHudTokenSetId);
        if (!expanded.ok) return failure(std::move(frame), expanded.errorCode);
        frame.draw = expanded.draw;
        const bool internalDecision =
            request.environment.rulesDecisionEvidenceMode &&
            request.rulesDecisionUiImplementationEnabled;
        const bool productRulesDecision =
            (request.rulesDecisionGroupChoiceProductEnabled ||
             request.rulesDecisionFullTableBallInHandProductEnabled ||
             request.rulesDecisionIllegalBreakProductEnabled ||
             request.rulesDecisionBreakShotFoulProductEnabled ||
             request.rulesDecisionEightOnBreakProductEnabled) &&
            request.rulesDecisionProductActive;
        if (internalDecision || productRulesDecision) {
            const RulesDecisionProductDraw rulesDraw =
                buildRulesDecisionProductDraw(request.rulesDecision,
                    request.presentation.camera,
                    request.rulesDecisionLocale, frame.palette,
                    request.viewport,
                    static_cast<float>(request.settings.uiScale), atlas_,
                    productRulesDecision && !internalDecision);
            if (!rulesDraw.ok)
                return failure(std::move(frame), rulesDraw.errorCode);
            if (rulesDraw.active) {
                frame.overlay = ProductUiOverlayKind::RulesDecision;
                frame.rulesDecisionActive = true;
                frame.rulesDecisionHitRects = rulesDraw.hitRects;
                frame.rulesDecisionExclusionRects =
                    rulesDraw.exclusionRects;
                // A rules decision owns the interaction surface. Keeping the
                // gameplay/player/camera HUD underneath creates competing
                // labels and false focus affordances around the modal.
                frame.draw = rulesDraw.draw;
            }
        }
    } else {
        return failure(std::move(frame), "product_ui_screen_unsupported");
    }
    frame.ok = true;
    frame.draw.ok = true;
    finish(frame);
    return frame;
}

}  // namespace billiardgl::render::ui
