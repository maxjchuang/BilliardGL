#pragma once

#include "presentation_snapshot.h"
#include "render/ui/hud_draw_list.h"
#include "table_specs.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace billiardgl::training {
struct TrainingTeachingHints;
}

namespace billiardgl::render::ui {

struct TrainingGoalPointCm {
    float x = 0.0f;
    float z = 0.0f;
};

struct TrainingGoalCircle {
    bool visible = false;
    TrainingGoalPointCm center;
    float radiusCm = 0.0f;
};

struct TrainingGoalPocket {
    bool visible = false;
    int pocketId = -1;
    std::string canonicalId;
    TrainingGoalPointCm center;
    float markerRadiusCm = 0.0f;
};

// Presentation-only input. Values are supplied by the validated Training
// scenario; this helper never predicts or evaluates a shot.
struct TrainingGoalOverlayRequest {
    CameraPresentation camera;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float uiScale = 1.0f;
    TableSpec table = defaultTableSpec();
    PocketSpec pockets = defaultPocketSpec();
    TrainingGoalCircle cueBallTargetZone;
    TrainingGoalPocket targetPocket;
    // Two or more authoritative X/Z points. Three points represent the usual
    // cue-ball -> object-ball -> pocket teaching line.
    std::vector<TrainingGoalPointCm> collinearDirectionPoints;
    // A cut shot has a separate cue approach and object-to-pocket line.
    std::vector<TrainingGoalPointCm> cueApproachPoints;
};

struct TrainingGoalOverlayDrawPackage {
    bool ok = false;
    std::string errorCode;
    HudDrawList draw;
    float clothSurfaceYCm = 0.0f;
    bool usedSharedCameraProjection = false;
    bool physicalPixelCoordinates = false;
    bool transparentUnfilledMarkers = false;
    bool playfieldCpuClipApplied = false;
    // This CPU/UI package has no scene depth attachment. It deliberately does
    // not claim ball/cue occlusion; the consumer may composite it below HUD
    // chrome or add an authoritative presentation mask later.
    bool depthOcclusionApplied = false;
    bool stopZoneCenterVisible = false;
    bool targetPocketCenterVisible = false;
    std::array<float, 2> stopZoneCenterPhysicalPx{{0.0f, 0.0f}};
    std::array<float, 2> targetPocketCenterPhysicalPx{{0.0f, 0.0f}};
    std::size_t stopZoneStrokeSegments = 0;
    std::size_t targetPocketStrokeSegments = 0;
    std::size_t directionDashCount = 0;
    std::size_t directionArrowStrokeCount = 0;
};

TrainingGoalOverlayDrawPackage buildTrainingGoalOverlayDrawPackage(
    const TrainingGoalOverlayRequest& request);

// Thin mapping seam for the product coordinator. A zero target radius means
// that the lesson intentionally has no cue-ball stop zone (for example the
// introductory straight-pot lesson); it is not treated as malformed input.
TrainingGoalOverlayRequest makeTrainingGoalOverlayRequest(
    const training::TrainingTeachingHints& hints,
    const CameraPresentation& camera, int physicalWidth, int physicalHeight,
    float uiScale, const TableSpec& table = defaultTableSpec(),
    const PocketSpec& pockets = defaultPocketSpec());

}  // namespace billiardgl::render::ui
