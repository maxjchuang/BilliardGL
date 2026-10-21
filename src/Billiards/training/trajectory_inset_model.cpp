#include "training/trajectory_inset_model.h"
#include <cstring>

namespace billiardgl::training {

const char* trajectoryInsetRoleName(TrajectoryInsetRole role)
{
    switch (role) {
    case TrajectoryInsetRole::ActualLeftHalo: return "actual_left_halo";
    case TrajectoryInsetRole::ActualLeftCore: return "actual_left_core";
    case TrajectoryInsetRole::ActualRightHalo: return "actual_right_halo";
    case TrajectoryInsetRole::ActualRightCore: return "actual_right_core";
    case TrajectoryInsetRole::ActualEndpointCapsule: return "actual_endpoint_capsule";
    case TrajectoryInsetRole::ActualDirectionNotch: return "actual_direction_notch";
    case TrajectoryInsetRole::PredictedOuterRim: return "predicted_outer_rim";
    case TrajectoryInsetRole::PredictedInnerHalo: return "predicted_inner_halo";
    case TrajectoryInsetRole::PredictedDashCore: return "predicted_dash_core";
    case TrajectoryInsetRole::PredictedEndpointDiamond: return "predicted_endpoint_diamond";
    case TrajectoryInsetRole::PredictedEventVisitMarker: return "predicted_event_visit_marker";
    case TrajectoryInsetRole::PredictedEventVisitMarkerHalo: return "predicted_event_visit_marker_halo";
    case TrajectoryInsetRole::PredictedEventVisitLeader: return "predicted_event_visit_leader";
    }
    return "unknown";
}

std::string trajectoryInsetProbeSourceEvidenceLedger(const InsetTrajectoryProbe& p)
{
    if(p.sourceCoreMemberKey.empty()) return {};
    std::string out;
    const auto field=[&](const std::string& s){out+=std::to_string(s.size())+":"+s+"\n";};
    const auto scalar=[&](float f){std::uint32_t bits;std::memcpy(&bits,&f,sizeof(bits));field(std::to_string(bits));};
    field("event_visit_evidence_v1");field(p.sourceCoreMemberKey);
    field(p.sourceCoreVisible?"visible":"hidden");field(p.evidenceRole);field(p.visitLabel);
    field(std::to_string(p.sourceFirstTick));field(std::to_string(p.sourceLastTick));
    scalar(p.sourceAnchor[0]);scalar(p.sourceAnchor[1]);
    scalar(p.visitMarkerRect.x);scalar(p.visitMarkerRect.y);
    scalar(p.visitMarkerRect.width);scalar(p.visitMarkerRect.height);
    return out;
}

const char* trajectoryInsetTrackName(TrajectoryInsetTrack track)
{
    return track == TrajectoryInsetTrack::Actual ? "actual" : "predicted";
}

}  // namespace billiardgl::training
