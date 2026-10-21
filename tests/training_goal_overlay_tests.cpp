#include "render/modern/camera_projection.h"
#include "render/ui/training_goal_overlay.h"
#include "training/training_product_controller.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;
using namespace billiardgl::render::ui;

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

CameraPresentation overhead(float panX = 0.0f)
{
    CameraPresentation camera;
    camera.eye = {0.0f, 420.0f, 0.0f};
    camera.target = {0.0f, kTableModelPlaySurfaceHeightCm, 0.0f};
    camera.projectionPan = {panX, 0.0f, 0.0f};
    camera.fieldOfViewDegrees = 48.0f;
    return camera;
}

TrainingGoalOverlayRequest firstLesson(float panX = 0.0f)
{
    TrainingGoalOverlayRequest request;
    request.camera = overhead(panX);
    request.physicalWidth = 1280;
    request.physicalHeight = 720;
    request.uiScale = 1.0f;
    request.cueBallTargetZone = {true, {38.5f, 0.0f}, 4.5f};
    request.targetPocket = {true, 5, "side_px", {63.5f, 0.0f}, 4.3f};
    request.collinearDirectionPoints = {
        {10.0f, 0.0f}, {40.0f, 0.0f}, {63.5f, 0.0f}};
    return request;
}

std::size_t tokenCount(const HudDrawList& draw, const std::string& token)
{
    return static_cast<std::size_t>(std::count_if(draw.triangles.begin(),
        draw.triangles.end(), [&](const UiTriangle& triangle) {
            return triangle.sourceToken == token;
        }));
}

void testFirstLessonGeometry()
{
    const TrainingGoalOverlayRequest request = firstLesson();
    const TrainingGoalOverlayDrawPackage package =
        buildTrainingGoalOverlayDrawPackage(request);
    require(package.ok && package.draw.ok && package.errorCode.empty(),
        "first lesson overlay builds");
    require(package.usedSharedCameraProjection &&
            package.physicalPixelCoordinates &&
            package.transparentUnfilledMarkers &&
            package.playfieldCpuClipApplied &&
            !package.depthOcclusionApplied &&
            package.draw.primitives.empty(),
        "overlay is projected unfilled triangle geometry");
    require(package.stopZoneStrokeSegments == 64 &&
            package.targetPocketStrokeSegments > 0 &&
            package.directionDashCount > 0 &&
            package.directionArrowStrokeCount == 2,
        "zone, target pocket, and teaching direction are emitted");
    require(tokenCount(package.draw, "training_goal_stop_zone") == 128 &&
            tokenCount(package.draw, "training_goal_target_pocket") > 0 &&
            tokenCount(package.draw, "training_goal_direction") ==
                package.directionDashCount * 2 &&
            tokenCount(package.draw, "training_goal_direction_arrow") == 4,
        "semantic tokens distinguish the three non-prediction roles");
    require(package.clothSurfaceYCm == defaultTableSpec().clothSurfaceYCm,
        "world geometry uses TableSpec cloth plane");

    const SharedCameraProjection projection = buildSharedCameraProjection(
        request.camera, request.physicalWidth, request.physicalHeight);
    const HomogeneousPoint clip = transformWorldToClip(projection,
        {38.5f, request.table.clothSurfaceYCm + 0.15f, 0.0f});
    PhysicalScreenPoint expected;
    require(clipToPhysicalScreen(projection, clip, expected) &&
            std::fabs(package.stopZoneCenterPhysicalPx[0] - expected.x) < 1e-3 &&
            std::fabs(package.stopZoneCenterPhysicalPx[1] - expected.y) < 1e-3,
        "reported center is the shared projection physical pixel");
}

void testProjectionPanAndPhysicalRadius()
{
    const auto base = buildTrainingGoalOverlayDrawPackage(firstLesson());
    const auto panned = buildTrainingGoalOverlayDrawPackage(firstLesson(17.0f));
    require(base.ok && panned.ok &&
            std::fabs(base.stopZoneCenterPhysicalPx[0] -
                panned.stopZoneCenterPhysicalPx[0]) > 1.0f,
        "projection pan participates in the shared camera transform");

    TrainingGoalOverlayRequest larger = firstLesson();
    larger.targetPocket.visible = false;
    larger.collinearDirectionPoints.clear();
    larger.cueBallTargetZone.radiusCm = 9.0f;
    const auto large = buildTrainingGoalOverlayDrawPackage(larger);
    require(large.ok, "larger physical target builds");
    const auto bounds = [](const HudDrawList& draw) {
        float minimum = 1e9f;
        float maximum = -1e9f;
        for (const UiTriangle& triangle : draw.triangles) {
            if (triangle.sourceToken != "training_goal_stop_zone") continue;
            minimum = std::min({minimum, triangle.first.x, triangle.second.x,
                triangle.third.x});
            maximum = std::max({maximum, triangle.first.x, triangle.second.x,
                triangle.third.x});
        }
        return maximum - minimum;
    };
    require(bounds(large.draw) > bounds(base.draw) * 1.8f,
        "target diameter is derived from world centimetres, not fixed pixels");
}

void testIdentityAndInputFailures()
{
    TrainingGoalOverlayRequest mismatch = firstLesson();
    mismatch.targetPocket.center.x = -63.5f;
    const auto wrongPocket = buildTrainingGoalOverlayDrawPackage(mismatch);
    require(!wrongPocket.ok && wrongPocket.draw.triangles.empty() &&
            wrongPocket.errorCode ==
                "training_goal_overlay_pocket_identity_mismatch",
        "pocket id and authoritative center cannot disagree");

    TrainingGoalOverlayRequest wrongName = firstLesson();
    wrongName.targetPocket.canonicalId = "side_nx";
    const auto wrongIdentity = buildTrainingGoalOverlayDrawPackage(wrongName);
    require(!wrongIdentity.ok && wrongIdentity.draw.triangles.empty() &&
            wrongIdentity.errorCode ==
                "training_goal_overlay_pocket_identity_mismatch",
        "canonical pocket name and opening index cannot disagree");

    TrainingGoalOverlayRequest bent = firstLesson();
    bent.collinearDirectionPoints[1].z = 0.1f;
    const auto wrongLine = buildTrainingGoalOverlayDrawPackage(bent);
    require(!wrongLine.ok && wrongLine.draw.triangles.empty() &&
            wrongLine.errorCode == "training_goal_overlay_line_not_collinear",
        "direction line cannot silently bend into predicted trajectory");

    TrainingGoalOverlayRequest invalid = firstLesson();
    invalid.cueBallTargetZone.radiusCm = 0.0f;
    const auto wrongRadius = buildTrainingGoalOverlayDrawPackage(invalid);
    require(!wrongRadius.ok && wrongRadius.draw.triangles.empty() &&
            wrongRadius.errorCode == "training_goal_overlay_marker_invalid",
        "nonpositive target radius fails closed");

    TrainingGoalOverlayRequest empty;
    empty.camera = overhead();
    empty.physicalWidth = 1280;
    empty.physicalHeight = 720;
    const auto noHints = buildTrainingGoalOverlayDrawPackage(empty);
    require(!noHints.ok && noHints.errorCode == "training_goal_overlay_empty" &&
            noHints.draw.triangles.empty(),
        "missing teaching hints produce no drawable residue");
}

void testTypedTeachingHintsMapping()
{
    training::TrainingTeachingHints pot;
    pot.available = true;
    pot.targetPocketId = "side_px";
    pot.targetPocketCenterCm = {{63.5f, 0.0f}};
    pot.cueBallTargetCenterCm = {{38.5f, 0.0f}};
    pot.cueBallTargetRadiusCm = 0.0f;
    pot.cueBallTargetEnabled = false;
    pot.shotLineStartCm = {{10.0f, 0.0f}};
    pot.shotLineObjectCenterCm = {{40.0f, 0.0f}};
    const TrainingGoalOverlayRequest potRequest = makeTrainingGoalOverlayRequest(
        pot, overhead(), 1280, 720, 1.0f);
    require(!potRequest.cueBallTargetZone.visible &&
            potRequest.targetPocket.visible &&
            potRequest.targetPocket.pocketId == 5 &&
            potRequest.targetPocket.markerRadiusCm ==
                defaultPocketSpec().sideMouthWidthCm * 0.5f,
        "straight-pot hints omit the stop zone and bind the real side mouth");
    const auto potPackage = buildTrainingGoalOverlayDrawPackage(potRequest);
    require(potPackage.ok && potPackage.stopZoneStrokeSegments == 0 &&
            potPackage.targetPocketStrokeSegments > 0 &&
            potPackage.directionArrowStrokeCount == 2,
        "straight-pot lesson emits only direction and pocket guidance");

    training::TrainingTeachingHints stop = pot;
    stop.cueBallTargetRadiusCm = 4.5f;
    stop.cueBallTargetEnabled = true;
    const auto stopPackage = buildTrainingGoalOverlayDrawPackage(
        makeTrainingGoalOverlayRequest(stop, overhead(), 1280, 720, 1.0f));
    require(stopPackage.ok && stopPackage.stopZoneStrokeSegments == 64,
        "stop-shot hints add the authoritative 4.5cm target ring");

    auto cut = pot;
    cut.shotLineStartCm = {{10.0f, 15.0f}};
    cut.aimYawRadians = -0.4f;
    const auto cutRequest = makeTrainingGoalOverlayRequest(cut, overhead(), 1280, 720, 1.0f);
    const auto cutPackage = buildTrainingGoalOverlayDrawPackage(cutRequest);
    require(cutRequest.cueApproachPoints.size() == 2 &&
        cutRequest.collinearDirectionPoints.size() == 2 && cutPackage.ok &&
        cutPackage.targetPocketStrokeSegments > 0,
        "cut shots draw distinct cue approach and object-to-pocket guides");
    require(std::fabs(std::atan2(cutRequest.cueApproachPoints[1].z - cutRequest.cueApproachPoints[0].z,
            cutRequest.cueApproachPoints[1].x - cutRequest.cueApproachPoints[0].x) - cut.aimYawRadians) < 1e-5f,
        "cue approach respects calibrated spin compensation rather than inventing a yaw");
    auto breakHints = stop;
    breakHints.targetPocketEnabled = false;
    breakHints.targetPocketId.clear();
    const auto breakRequest = makeTrainingGoalOverlayRequest(breakHints, overhead(), 1280, 720, 1.0f);
    const auto breakPackage = buildTrainingGoalOverlayDrawPackage(breakRequest);
    require(!breakRequest.targetPocket.visible && breakRequest.collinearDirectionPoints.size() == 2 &&
        breakPackage.ok && breakPackage.targetPocketStrokeSegments == 0 &&
        breakPackage.stopZoneStrokeSegments > 0,
        "break control draws no invented target pocket");
}

}  // namespace

int main()
{
    try {
        testFirstLessonGeometry();
        testProjectionPanAndPhysicalRadius();
        testIdentityAndInputFailures();
        testTypedTeachingHintsMapping();
        std::cout << "PASS training goal overlay\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
