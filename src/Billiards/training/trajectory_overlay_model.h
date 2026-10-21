#pragma once

#include "replay/replay_transport_model.h"
#include "training/preview_runtime.h"

#include <array>
#include <string>
#include <vector>

namespace billiardgl::training {

enum class TrajectoryLineStyle {
    Dashed,
    SolidWithMarkers
};

struct TrajectoryEncoding {
    std::array<float, 4> color{{1.0f, 1.0f, 1.0f, 1.0f}};
    TrajectoryLineStyle lineStyle = TrajectoryLineStyle::Dashed;
    std::string marker;
    std::string legendLabel;
};

struct OverlaySample {
    std::uint64_t tick = 0;
    Point3 position;
    bool visible = true;
};

struct OverlayTrack {
    TrajectoryEncoding encoding;
    std::vector<OverlaySample> samples;
    bool visible = true;
    bool endpointVisible = false;
    Point3 endpoint;
    int railCollisionEvents = 0;
};

struct OverlayLegendItem {
    TrajectoryEncoding encoding;
    bool trackVisible = true;
    bool endpointVisible = false;
    Point3 endpoint;
    int railCollisionEvents = 0;
    std::string endpointText;
    std::string railText;
};

struct TrajectoryOverlaySnapshot {
    OverlayTrack predicted;
    OverlayTrack actual;
    std::vector<OverlayLegendItem> legend;
    bool advancedExpanded = false;
};

class TrajectoryOverlayModel {
public:
    TrajectoryOverlayModel();

    void setPredicted(const PreviewResult& preview);
    void setActual(const replay::ReplayTransportSnapshot& transport);
    void setPredictedVisible(bool visible) { predicted_.visible = visible; }
    void setActualVisible(bool visible) { actual_.visible = visible; }
    void setAdvancedExpanded(bool expanded) { advancedExpanded_ = expanded; }
    TrajectoryOverlaySnapshot snapshot() const;

private:
    OverlayTrack predicted_;
    OverlayTrack actual_;
    bool advancedExpanded_ = false;
};

}  // namespace billiardgl::training
