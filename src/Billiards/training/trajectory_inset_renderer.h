#pragma once

#include "training/trajectory_inset_model.h"

#include <array>
#include <cstdint>
#include <vector>

namespace billiardgl::training {

struct TrajectoryInsetSampleProfile {
    std::string id;
    std::vector<std::array<float, 2>> positions;
};

struct TrajectoryInsetSemanticRaster {
    bool ok = false;
    std::string errorCode;
    int width = 0;
    int height = 0;
    std::size_t sampleCount = 0;
    std::vector<std::uint32_t> perSampleTokens;
    std::string tokenDigest;
};

TrajectoryInsetDrawPackage buildTrajectoryInsetDrawPackage(
    const TrajectoryInsetBuildRequest& request);

TrajectoryInsetSemanticRaster rasterTrajectoryInsetSemantics(
    const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSampleProfile& sampleProfile);

TrajectoryInsetSampleProfile trajectoryInsetSampleProfile1x();
TrajectoryInsetSampleProfile trajectoryInsetSampleProfile2x();
TrajectoryInsetSampleProfile trajectoryInsetSampleProfile4x();
bool validateTrajectoryInsetVisitEvidence(const TrajectoryInsetDrawPackage& package,
    std::string& error);
// Approved terminal-only successor: no geometry/color/token change. Requires
// the original contiguous same-parent core/core/capsule/capsule block; a
// second application is rejected without modifying the packet.
bool applyActualTerminalOwnershipOrder(TrajectoryInsetDrawPackage& package,
    const std::string& terminalParent, std::string& error);

}  // namespace billiardgl::training
