#pragma once

#include "presentation_snapshot.h"
#include "product_settings.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace billiardgl::product_hud {

enum class HudRuntimeState {
    Default,
    Aiming,
    PostShot,
    Foul,
    Help
};

struct DisplayField {
    bool available = false;
    std::string text;
};

struct RuleNotice {
    std::string reasonCode;
    std::string playerText;
    std::string iconToken;
    std::string shapeToken;
};

struct HudProductCapabilities {
    bool cueElevationUi = false;
    bool cueElevationPhysicsPromoted = false;
    bool chalkUi = false;
    bool chalkPhysicsPromoted = false;
};

struct ProductHudViewModel {
    std::string manifestId = "focused_practice_hud_v1";
    HudRuntimeState state = HudRuntimeState::Default;
    DisplayField player;
    DisplayField group;
    std::string groupShapeToken = "group_open_diamond";
    DisplayField targetBall;
    DisplayField targetPocket;
    DisplayField powerPercent;
    DisplayField estimatedCueSpeed;
    DisplayField tipOffset;
    // Normalized cue-ball offsets from the same snapshot as tipOffset.text.
    float tipOffsetX = 0.0f;
    float tipOffsetY = 0.0f;
    DisplayField camera;
    DisplayField assist;
    DisplayField foulReason;
    DisplayField foulResolution;
    DisplayField result;
    std::vector<RuleNotice> ruleNotices;
    std::vector<std::string> helpLines;
    bool showPlayerStatus = false;
    bool showContextHint = false;
    bool showPower = false;
    bool showTipOffset = false;
    bool showShotResult = false;
    bool showFoul = false;
    bool showHelp = false;
    bool showCueElevation = false;
    bool showChalk = false;
};

// The Help overlay is product UI, so it must describe the effective bindings
// that the input controller will resolve rather than a second set of labels.
std::vector<std::string> buildHelpLines(
    const settings::ProductSettings& settings);
std::size_t longestHelpLineCharacters(
    const std::vector<std::string>& lines) noexcept;

class HudStateReducer {
public:
    static ProductHudViewModel reduce(const PresentationSnapshot& snapshot,
        const HudProductCapabilities& capabilities = {});
};

struct HudRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct HudSafeArea {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct HudProfileSpec {
    std::string id;
    int referenceWidth = 0;
    int referenceHeight = 0;
    HudSafeArea safeArea;
    float minimumTextPx = 13.0f;
    std::map<std::string, HudRect> referenceRects;
};

struct FirstShotProxyContract {
    int deadlineMs = 30000;
    int statusVisibleByMs = 500;
    int aimEnteredByMs = 11000;
    int aimControlsLatencyMaxMs = 100;
    int minimumPowerSamples = 3;
    int postShotLatencyMaxMs = 250;
    int maximumPrompts = 3;
};

struct ProductHudManifest {
    bool ok = false;
    std::string errorCode;
    std::string manifestId;
    float minimumUserScale = 0.85f;
    float maximumUserScale = 1.35f;
    HudProfileSpec compact;
    HudProfileSpec wide;
    FirstShotProxyContract firstShot;
    std::string debugOverlayId;
};

ProductHudManifest loadProductHudManifest(const std::string& path);

struct HudLayoutRequest {
    int physicalWidth = 0;
    int physicalHeight = 0;
    float dpi = 96.0f;
    float userScale = 1.0f;
    HudRuntimeState state = HudRuntimeState::Default;
    std::size_t helpLongestLineCharacters = 0;
};

struct HudLayoutResult {
    bool ok = false;
    std::string errorCode;
    std::string profileId;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float resolvedScale = 1.0f;
    float minimumTextPhysicalPx = 13.0f;
    HudSafeArea safeAreaPhysical;
    std::map<std::string, HudRect> elements;
};

class ResponsiveHudLayoutSolver {
public:
    explicit ResponsiveHudLayoutSolver(ProductHudManifest manifest)
        : manifest_(std::move(manifest)) {}

    HudLayoutResult solve(const HudLayoutRequest& request) const;

private:
    ProductHudManifest manifest_;
};

struct DebugOverlayViewModel {
    std::string overlayId = "billiardgl_debug_telemetry_v1";
    bool visible = false;
    bool sharesProductLayoutNodes = false;
    bool forbiddenInReleaseCapture = true;
    std::vector<std::string> fields;
};

DebugOverlayViewModel buildDebugOverlayViewModel(bool requestedVisible,
    bool releaseCapture);

enum class FirstShotEventType {
    HudDefaultVisible,
    AimToggled,
    PowerHoldStarted,
    PowerSampled,
    ShotReleased,
    HudPostShotVisible
};

enum class EventSource {
    UserObserved,
    PresentationObserved,
    Automatic
};

struct FirstShotEvent {
    FirstShotEventType type = FirstShotEventType::HudDefaultVisible;
    int elapsedMs = 0;
    EventSource source = EventSource::PresentationObserved;
    float powerPercent = 0.0f;
    int presentationLatencyMs = 0;
    int visiblePromptCount = 0;
};

struct FirstShotProxyResult {
    bool accepted = false;
    bool complete = false;
    std::string errorCode;
};

// Engineering automation proxy only. It observes UI/reducer events and never
// emits aim, power, or shoot commands.
class FirstShotAutomationProxy {
public:
    explicit FirstShotAutomationProxy(FirstShotProxyContract contract)
        : contract_(contract) {}

    FirstShotProxyResult observe(const FirstShotEvent& event,
        const PresentationSnapshot& snapshot);
    const std::vector<FirstShotEvent>& events() const { return events_; }

private:
    FirstShotProxyContract contract_;
    std::vector<FirstShotEvent> events_;
    int stage_ = 0;
    int powerSamples_ = 0;
    int shotReleases_ = 0;
    int releaseTimeMs_ = -1;
    float lastPower_ = -1.0f;
};

}  // namespace billiardgl::product_hud
