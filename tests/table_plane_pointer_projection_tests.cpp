#include "render/modern/camera_projection.h"
#include "render/modern/table_plane_pointer_projection.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

void expect(bool value, const std::string& message)
{
    if (value) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

bool near(double first, double second, double tolerance = 1.0e-3)
{
    return std::fabs(first - second) <= tolerance;
}

CameraPresentation camera(float fov = 48.0f)
{
    CameraPresentation value;
    value.eye = {35.0f, 180.0f, 230.0f};
    value.target = {4.0f, 0.0f, -12.0f};
    value.fieldOfViewDegrees = fov;
    return value;
}

TablePlanePointerProjectionRequest request(CameraPresentation value)
{
    TablePlanePointerProjectionRequest result;
    result.camera = value;
    result.viewport = {960, 540, 1920, 1080};
    result.logicalX = 480.0;
    result.logicalY = 270.0;
    result.tablePlaneY = 0.0;
    return result;
}

void expectRoundTrip(float fov, const Point3& world,
    PointerProjectionViewport viewport)
{
    const CameraPresentation view = camera(fov);
    const SharedCameraProjection projection = buildSharedCameraProjection(
        view, viewport.physicalWidth, viewport.physicalHeight);
    PhysicalScreenPoint screen;
    expect(clipToPhysicalScreen(projection,
        transformWorldToClip(projection, world), screen),
        "world point projects into a finite physical screen point");
    auto input = request(view);
    input.viewport = viewport;
    input.logicalX = screen.x * viewport.logicalWidth /
        viewport.physicalWidth;
    input.logicalY = screen.y * viewport.logicalHeight /
        viewport.physicalHeight;
    input.tablePlaneY = world.y;
    const auto result = projectPointerToTablePlane(input);
    expect(result.ok && near(result.tablePoint.x, world.x, 2.0e-3) &&
        near(result.tablePoint.y, world.y, 2.0e-3) &&
        near(result.tablePoint.z, world.z, 2.0e-3),
        "pointer projection round-trips renderer camera at FOV " +
            std::to_string(fov));
}

void testCenterAndRoundTrips()
{
    auto centeredCamera = camera();
    centeredCamera.eye = {0.0f, 160.0f, 160.0f};
    centeredCamera.target = {0.0f, 0.0f, 0.0f};
    const auto center = projectPointerToTablePlane(request(centeredCamera));
    expect(center.ok && near(center.physicalX, 960.0) &&
        near(center.physicalY, 540.0) && near(center.tablePoint.x, 0.0) &&
        near(center.tablePoint.z, 0.0),
        "logical center uses the physical framebuffer and hits camera target");

    const PointerProjectionViewport retina{960, 540, 1920, 1080};
    expectRoundTrip(48.0f, {27.0f, 0.0f, -63.0f}, retina);
    expectRoundTrip(20.0f, {10.0f, 0.0f, -10.0f}, retina);
    expectRoundTrip(85.0f, {-48.0f, 0.0f, 35.0f}, retina);
    expectRoundTrip(90.0f, {42.0f, 0.0f, 18.0f}, retina);
    // Independent X/Y conversion is deliberate: platform logical and backing
    // dimensions are authoritative, not a guessed uniform backing scale.
    expectRoundTrip(55.0f, {-20.0f, 0.0f, -20.0f},
        {1000, 600, 2000, 900});
}

void testTopLeftYAxisAndOverheadCamera()
{
    CameraPresentation overhead;
    overhead.eye = {0.0f, 200.0f, 0.0f};
    overhead.target = {0.0f, 0.0f, 0.0f};
    overhead.fieldOfViewDegrees = 48.0f;
    auto top = request(overhead);
    top.logicalY = 120.0;
    auto bottom = top;
    bottom.logicalY = 420.0;
    const auto topResult = projectPointerToTablePlane(top);
    const auto bottomResult = projectPointerToTablePlane(bottom);
    expect(topResult.ok && bottomResult.ok &&
        topResult.tablePoint.z > bottomResult.tablePoint.z,
        "logical Y grows downward while top-screen rays follow renderer up");
}

void testOcclusionAndViewportBounds()
{
    auto input = request(camera());
    input.excludedPhysicalRects.push_back({900.0, 500.0, 120.0, 80.0});
    auto result = projectPointerToTablePlane(input);
    expect(!result.ok && result.errorCode == "pointer_occluded_by_ui",
        "modal physical rectangle owns the pointer before table projection");
    input.excludedPhysicalRects[0] = {0.0, 0.0, 960.0, 540.0};
    result = projectPointerToTablePlane(input);
    expect(result.ok, "modal right/bottom edge is half-open");
    input.logicalX = -0.01;
    result = projectPointerToTablePlane(input);
    expect(!result.ok && result.errorCode ==
            "pointer_projection_outside_viewport",
        "negative logical pointer is outside the viewport");
    input.logicalX = input.viewport.logicalWidth;
    result = projectPointerToTablePlane(input);
    expect(!result.ok && result.errorCode ==
            "pointer_projection_outside_viewport",
        "logical right edge is outside the half-open viewport");
}

void testRayAndClipFailures()
{
    auto parallel = request(camera());
    parallel.camera.eye = {0.0f, 20.0f, 100.0f};
    parallel.camera.target = {0.0f, 20.0f, 0.0f};
    parallel.tablePlaneY = 0.0;
    auto result = projectPointerToTablePlane(parallel);
    expect(!result.ok && result.errorCode ==
            "pointer_projection_parallel_to_table",
        "edge-on camera cannot fabricate a table intersection");

    auto behind = request(camera());
    behind.tablePlaneY = 300.0;
    result = projectPointerToTablePlane(behind);
    expect(!result.ok && result.errorCode ==
            "pointer_projection_table_behind_camera",
        "intersection behind the eye is rejected");

    auto nearClipped = request(camera());
    nearClipped.camera.eye = {0.0f, 100.0f, 0.0f};
    nearClipped.camera.target = {0.0f, 0.0f, 0.0f};
    nearClipped.tablePlaneY = 99.0;
    result = projectPointerToTablePlane(nearClipped);
    expect(!result.ok && result.errorCode ==
            "pointer_projection_outside_clip_volume",
        "intersection before the renderer near plane is rejected");

    auto farClipped = nearClipped;
    farClipped.tablePlaneY = -1801.0;
    result = projectPointerToTablePlane(farClipped);
    expect(!result.ok && result.errorCode ==
            "pointer_projection_outside_clip_volume",
        "intersection beyond the renderer far plane is rejected");

    auto invalid = request(camera());
    invalid.logicalX = std::numeric_limits<double>::infinity();
    result = projectPointerToTablePlane(invalid);
    expect(!result.ok && result.errorCode == "pointer_projection_nonfinite",
        "nonfinite input fails closed");
    invalid = request(camera());
    invalid.excludedPhysicalRects.push_back(
        {0.0, 0.0, std::numeric_limits<double>::quiet_NaN(), 1.0});
    result = projectPointerToTablePlane(invalid);
    expect(!result.ok && result.errorCode ==
            "pointer_projection_exclusion_invalid",
        "malformed modal ownership geometry fails closed");
}

}  // namespace

int main()
{
    testCenterAndRoundTrips();
    testTopLeftYAxisAndOverheadCamera();
    testOcclusionAndViewportBounds();
    testRayAndClipFailures();
    std::cout << "table-plane-pointer-projection-tests: pass\n";
    return 0;
}
