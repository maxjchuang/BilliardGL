#pragma once

#include "presentation_snapshot.h"

#include <string>
#include <vector>

namespace billiardgl::render {

struct PointerProjectionViewport {
    int logicalWidth = 0;
    int logicalHeight = 0;
    int physicalWidth = 0;
    int physicalHeight = 0;
};

struct PointerProjectionRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct TablePlanePointerProjectionRequest {
    CameraPresentation camera;
    PointerProjectionViewport viewport;
    double logicalX = 0.0;
    double logicalY = 0.0;
    double tablePlaneY = 0.0;
    // Physical-pixel rectangles owned by modal UI. A pointer in one of these
    // rectangles must never be interpreted as a table placement preview.
    std::vector<PointerProjectionRect> excludedPhysicalRects;
};

struct TablePlanePointerProjectionResult {
    bool ok = false;
    std::string errorCode;
    double physicalX = 0.0;
    double physicalY = 0.0;
    PresentationPoint rayOrigin;
    PresentationPoint rayDirection;
    PresentationPoint tablePoint;
};

// Pure presentation geometry. This function does not clamp to the playable
// surface, validate a rules placement, confirm a choice, or mutate authority.
// Its successful x/z result is intended for the runtime placement preview.
TablePlanePointerProjectionResult projectPointerToTablePlane(
    const TablePlanePointerProjectionRequest& request);

}  // namespace billiardgl::render
