#include "training/trajectory_overlay_model.h"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>

namespace billiardgl::training {
namespace {

std::string endpointText(const OverlayTrack& track)
{
    if (!track.visible) return "Hidden";
    if (!track.endpointVisible) return "Endpoint unavailable";
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::fixed << std::setprecision(1)
         << "X " << track.endpoint.x << " cm / Z "
         << track.endpoint.z << " cm";
    return text.str();
}

OverlayLegendItem legendItem(const OverlayTrack& track)
{
    OverlayLegendItem item;
    item.encoding = track.encoding;
    item.trackVisible = track.visible;
    item.endpointVisible = track.endpointVisible;
    item.endpoint = track.endpoint;
    item.railCollisionEvents = track.railCollisionEvents;
    item.endpointText = endpointText(track);
    item.railText = "Rails " +
        std::to_string(std::max(0, track.railCollisionEvents));
    return item;
}

}  // namespace

TrajectoryOverlayModel::TrajectoryOverlayModel()
{
    // Scene-linear HDR values calibrated through the renderer's ACES pass.
    predicted_.encoding.color = {{8.0f, 6.0f, 0.15f, 1.0f}};
    predicted_.encoding.lineStyle = TrajectoryLineStyle::Dashed;
    predicted_.encoding.marker = "endpoint_ring";
    predicted_.encoding.legendLabel = "training.trajectory.predicted";

    actual_.encoding.color = {{0.002f, 0.02f, 0.03f, 1.0f}};
    actual_.encoding.lineStyle = TrajectoryLineStyle::SolidWithMarkers;
    actual_.encoding.marker = "tick_dot";
    actual_.encoding.legendLabel = "training.trajectory.replay_actual";
}

void TrajectoryOverlayModel::setPredicted(const PreviewResult& preview)
{
    predicted_.samples.clear();
    predicted_.endpointVisible = false;
    predicted_.railCollisionEvents = preview.railCollisionEvents;
    if (!preview.ok) return;
    for (const PreviewSample& sample : preview.samples) {
        predicted_.samples.push_back(
            {sample.tick, sample.cueBallPosition, sample.cueBallVisible});
    }
    for (auto sample = predicted_.samples.rbegin();
         sample != predicted_.samples.rend(); ++sample) {
        if (!sample->visible) continue;
        predicted_.endpointVisible = true;
        predicted_.endpoint = sample->position;
        break;
    }
}

void TrajectoryOverlayModel::setActual(
    const replay::ReplayTransportSnapshot& transport)
{
    actual_.samples.clear();
    actual_.samples.reserve(transport.cueTrajectory.size());
    for (const replay::TrajectorySample& sample : transport.cueTrajectory)
        actual_.samples.push_back({sample.tick, sample.position, true});
    actual_.endpointVisible = transport.cueEndpointVisible;
    actual_.endpoint = transport.cueEndpoint;
    actual_.railCollisionEvents = transport.railCollisionEvents;
}

TrajectoryOverlaySnapshot TrajectoryOverlayModel::snapshot() const
{
    TrajectoryOverlaySnapshot result;
    result.predicted = predicted_;
    result.actual = actual_;
    result.legend = {legendItem(predicted_), legendItem(actual_)};
    result.advancedExpanded = advancedExpanded_;
    return result;
}

}  // namespace billiardgl::training
