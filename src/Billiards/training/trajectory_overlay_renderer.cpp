#include "training/trajectory_overlay_renderer.h"

#include "render/modern/camera_projection.h"
#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace billiardgl::training {
namespace {

constexpr std::size_t kMaximumVerticesPerTrack = 8192;
constexpr double kPi = 3.14159265358979323846;

struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
};

struct ScreenRect {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

struct ScreenSegment {
    ScreenPoint first;
    ScreenPoint second;
    bool breakBefore = false;
};

struct PendingSemanticBatch {
    TrajectoryContrastTrack track = TrajectoryContrastTrack::Predicted;
    CompoundComponentRole componentRole = CompoundComponentRole::Core;
    std::string primitiveId;
    std::string arcSegmentId;
    std::size_t firstVertex = 0;
    std::size_t vertexCount = 0;
    TrajectoryVisualSublayer visualSublayer =
        TrajectoryVisualSublayer::Core;
};

void recordSemanticBatch(std::vector<PendingSemanticBatch>& batches,
    TrajectoryContrastTrack track, CompoundComponentRole role,
    const std::string& primitiveId, const std::string& arcSegmentId,
    std::size_t firstVertex, std::size_t finalVertex,
    TrajectoryVisualSublayer visualSublayer)
{
    if (finalVertex > firstVertex)
        batches.push_back({track, role, primitiveId, arcSegmentId,
            firstVertex, finalVertex - firstVertex, visualSublayer});
}

double length(ScreenPoint first, ScreenPoint second)
{
    return std::hypot(second.x - first.x, second.y - first.y);
}

ScreenPoint interpolate(ScreenPoint first, ScreenPoint second, double t)
{
    return {first.x + (second.x - first.x) * t,
        first.y + (second.y - first.y) * t};
}

double cross(ScreenPoint first, ScreenPoint second, ScreenPoint point)
{
    return (second.x - first.x) * (point.y - first.y) -
        (second.y - first.y) * (point.x - first.x);
}

bool inside(const ScreenRect& rect, ScreenPoint point)
{
    return point.x >= rect.left && point.x <= rect.right &&
        point.y >= rect.top && point.y <= rect.bottom;
}

bool clipScreenRect(const ScreenRect& rect,
    ScreenPoint& first, ScreenPoint& second)
{
    const ScreenPoint originalFirst = first;
    const ScreenPoint originalSecond = second;
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    double minimum = 0.0;
    double maximum = 1.0;
    const std::array<std::pair<double, double>, 4> boundaries{{
        {-dx, first.x - rect.left},
        {dx, rect.right - first.x},
        {-dy, first.y - rect.top},
        {dy, rect.bottom - first.y}}};
    for (const auto& boundary : boundaries) {
        const double p = boundary.first;
        const double q = boundary.second;
        if (std::fabs(p) <= 1e-12) {
            if (q < 0.0) return false;
            continue;
        }
        const double ratio = q / p;
        if (p < 0.0) minimum = std::max(minimum, ratio);
        else maximum = std::min(maximum, ratio);
        if (minimum > maximum) return false;
    }
    first = interpolate(originalFirst, originalSecond, minimum);
    second = interpolate(originalFirst, originalSecond, maximum);
    return true;
}

bool clipWorldPlayfield(Point3& first, Point3& second)
{
    const Point3 originalFirst = first;
    const Point3 originalSecond = second;
    const double dx = second.x - first.x;
    const double dz = second.z - first.z;
    const double halfWidth = kChinesePlayfieldWidthCm * 0.5;
    const double halfLength = kChinesePlayfieldLengthCm * 0.5;
    double minimum = 0.0;
    double maximum = 1.0;
    const std::array<std::pair<double, double>, 4> boundaries{{
        {-dx, first.x + halfWidth},
        {dx, halfWidth - first.x},
        {-dz, first.z + halfLength},
        {dz, halfLength - first.z}}};
    for (const auto& boundary : boundaries) {
        const double p = boundary.first;
        const double q = boundary.second;
        if (std::fabs(p) <= 1e-12) {
            if (q < 0.0) return false;
            continue;
        }
        const double ratio = q / p;
        if (p < 0.0) minimum = std::max(minimum, ratio);
        else maximum = std::min(maximum, ratio);
        if (minimum > maximum) return false;
    }
    first.x = static_cast<float>(originalFirst.x + dx * minimum);
    first.z = static_cast<float>(originalFirst.z + dz * minimum);
    second.x = static_cast<float>(originalFirst.x + dx * maximum);
    second.z = static_cast<float>(originalFirst.z + dz * maximum);
    return true;
}

double polygonArea(const std::array<ScreenPoint, 4>& polygon)
{
    double area = 0.0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const ScreenPoint first = polygon[index];
        const ScreenPoint second = polygon[(index + 1) % polygon.size()];
        area += first.x * second.y - second.x * first.y;
    }
    return area * 0.5;
}

bool pointInConvexPolygon(const std::array<ScreenPoint, 4>& polygon,
    ScreenPoint point)
{
    const double orientation = polygonArea(polygon) >= 0.0 ? 1.0 : -1.0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        if (orientation * cross(polygon[index],
                polygon[(index + 1) % polygon.size()], point) < -1e-6)
            return false;
    }
    return true;
}

bool clipConvexPolygon(const std::array<ScreenPoint, 4>& polygon,
    ScreenPoint& first, ScreenPoint& second)
{
    const ScreenPoint originalFirst = first;
    const ScreenPoint originalSecond = second;
    const double orientation = polygonArea(polygon) >= 0.0 ? 1.0 : -1.0;
    double minimum = 0.0;
    double maximum = 1.0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const ScreenPoint edgeFirst = polygon[index];
        const ScreenPoint edgeSecond = polygon[(index + 1) % polygon.size()];
        const double a = orientation * cross(
            edgeFirst, edgeSecond, originalFirst);
        const double b = orientation * cross(
            edgeFirst, edgeSecond, originalSecond);
        if (a < 0.0 && b < 0.0) return false;
        if (a >= 0.0 && b >= 0.0) continue;
        const double crossing = a / (a - b);
        if (a < 0.0) minimum = std::max(minimum, crossing);
        else maximum = std::min(maximum, crossing);
        if (minimum > maximum) return false;
    }
    first = interpolate(originalFirst, originalSecond, minimum);
    second = interpolate(originalFirst, originalSecond, maximum);
    return true;
}

std::array<float, 4> resolvedColor(const TrajectoryEncoding& encoding,
    bool predicted, TrajectoryOverlayPalette palette)
{
    if (!predicted) {
        // A deep cyan remains >7:1 against the actual white inner halo after
        // the calibrated production ACES/sRGB path.  Unlike the former
        // near-black core, it stays independently traceable through narrow
        // predicted gaps in both standard and color-vision-safe profiles;
        // the white outline remains the non-hue identity cue.
        return {{0.002f, 0.10f, 0.20f, 1.0f}};
    }
    if (palette == TrajectoryOverlayPalette::Standard)
        return encoding.color;
    // These are scene-linear HDR values.  They are deliberately calibrated
    // through the production ACES pass, not as display-space RGB values.
    return {{8.0f, 6.0f, 0.15f, 1.0f}};
}

void appendVertex(std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint point, const std::array<float, 4>& color,
    TrajectoryVisualSublayer visualSublayer = TrajectoryVisualSublayer::Core,
    bool adaptiveInnerHaloWhite = false)
{
    vertices.push_back({static_cast<float>(point.x),
        static_cast<float>(point.y), color, 0,
        static_cast<std::uint32_t>(visualSublayer),
        adaptiveInnerHaloWhite ? 1u : 0u});
}

void appendThickSegment(std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint first, ScreenPoint second, double width,
    const std::array<float, 4>& color,
    TrajectoryVisualSublayer visualSublayer = TrajectoryVisualSublayer::Core,
    bool adaptiveInnerHaloWhite = false)
{
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    const double segmentLength = std::hypot(dx, dy);
    if (segmentLength <= 1e-6) return;
    const double nx = -dy / segmentLength * width * 0.5;
    const double ny = dx / segmentLength * width * 0.5;
    const ScreenPoint a{std::round(first.x) + 0.5 + nx,
        std::round(first.y) + 0.5 + ny};
    const ScreenPoint b{std::round(first.x) + 0.5 - nx,
        std::round(first.y) + 0.5 - ny};
    const ScreenPoint c{std::round(second.x) + 0.5 + nx,
        std::round(second.y) + 0.5 + ny};
    const ScreenPoint d{std::round(second.x) + 0.5 - nx,
        std::round(second.y) + 0.5 - ny};
    appendVertex(vertices, a, color, visualSublayer, adaptiveInnerHaloWhite);
    appendVertex(vertices, b, color, visualSublayer, adaptiveInnerHaloWhite);
    appendVertex(vertices, c, color, visualSublayer, adaptiveInnerHaloWhite);
    appendVertex(vertices, c, color, visualSublayer, adaptiveInnerHaloWhite);
    appendVertex(vertices, b, color, visualSublayer, adaptiveInnerHaloWhite);
    appendVertex(vertices, d, color, visualSublayer, adaptiveInnerHaloWhite);
}

void appendHaloSegment(std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint first, ScreenPoint second, double width, double haloWidth,
    const std::array<float, 4>& color,
    TrajectoryVisualSublayer visualSublayer =
        TrajectoryVisualSublayer::InnerHalo,
    bool adaptiveInnerHaloWhite = false,
    double capExtension = -1.0)
{
    const double segmentLength = length(first, second);
    if (segmentLength <= 1e-6) return;
    const double dx = (second.x - first.x) / segmentLength;
    const double dy = (second.y - first.y) / segmentLength;
    const double extension = capExtension >= 0.0 ? capExtension : haloWidth;
    first.x -= dx * extension;
    first.y -= dy * extension;
    second.x += dx * extension;
    second.y += dy * extension;
    appendThickSegment(vertices, first, second,
        width + 2.0 * haloWidth, color, visualSublayer,
        adaptiveInnerHaloWhite);
}

void appendQuad(std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint first, ScreenPoint second, ScreenPoint third,
    ScreenPoint fourth, const std::array<float, 4>& color,
    bool adaptiveInnerHaloWhite)
{
    appendVertex(vertices, first, color, TrajectoryVisualSublayer::OuterRim,
        adaptiveInnerHaloWhite);
    appendVertex(vertices, second, color, TrajectoryVisualSublayer::OuterRim,
        adaptiveInnerHaloWhite);
    appendVertex(vertices, third, color, TrajectoryVisualSublayer::OuterRim,
        adaptiveInnerHaloWhite);
    appendVertex(vertices, third, color, TrajectoryVisualSublayer::OuterRim,
        adaptiveInnerHaloWhite);
    appendVertex(vertices, second, color, TrajectoryVisualSublayer::OuterRim,
        adaptiveInnerHaloWhite);
    appendVertex(vertices, fourth, color, TrajectoryVisualSublayer::OuterRim,
        adaptiveInnerHaloWhite);
}

void appendNonOverwritingOuterBand(
    std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint first, ScreenPoint second, double width,
    double innerHaloWidth, double outerRimWidth,
    double innerCapExtension, double outerCapExtension,
    const std::array<float, 4>& outerRim, bool adaptiveInnerHaloWhite)
{
    const double segmentLength = length(first, second);
    if (segmentLength <= 1e-6 || outerRimWidth <= 0.0) return;
    const double dx = (second.x - first.x) / segmentLength;
    const double dy = (second.y - first.y) / segmentLength;
    const double nx = -dy;
    const double ny = dx;
    const double innerHalf = width * 0.5 + innerHaloWidth;
    const double outerHalf = innerHalf + outerRimWidth;
    const ScreenPoint innerStart{std::round(first.x) + 0.5 -
            dx * innerCapExtension,
        std::round(first.y) + 0.5 - dy * innerCapExtension};
    const ScreenPoint innerEnd{std::round(second.x) + 0.5 +
            dx * innerCapExtension,
        std::round(second.y) + 0.5 + dy * innerCapExtension};
    const ScreenPoint outerStart{std::round(first.x) + 0.5 -
            dx * outerCapExtension,
        std::round(first.y) + 0.5 - dy * outerCapExtension};
    const ScreenPoint outerEnd{std::round(second.x) + 0.5 +
            dx * outerCapExtension,
        std::round(second.y) + 0.5 + dy * outerCapExtension};
    const auto shifted = [](ScreenPoint point, double x, double y,
                            double amount) {
        return ScreenPoint{point.x + x * amount, point.y + y * amount};
    };
    appendQuad(vertices,
        shifted(outerStart, nx, ny, outerHalf),
        shifted(outerStart, nx, ny, innerHalf),
        shifted(outerEnd, nx, ny, outerHalf),
        shifted(outerEnd, nx, ny, innerHalf), outerRim,
        adaptiveInnerHaloWhite);
    appendQuad(vertices,
        shifted(outerStart, nx, ny, -innerHalf),
        shifted(outerStart, nx, ny, -outerHalf),
        shifted(outerEnd, nx, ny, -innerHalf),
        shifted(outerEnd, nx, ny, -outerHalf), outerRim,
        adaptiveInnerHaloWhite);
    if (outerCapExtension > innerCapExtension + 1e-6) {
        appendQuad(vertices,
            shifted(outerStart, nx, ny, -outerHalf),
            shifted(innerStart, nx, ny, -innerHalf),
            shifted(outerStart, nx, ny, outerHalf),
            shifted(innerStart, nx, ny, innerHalf), outerRim,
            adaptiveInnerHaloWhite);
        appendQuad(vertices,
            shifted(innerEnd, nx, ny, -innerHalf),
            shifted(outerEnd, nx, ny, -outerHalf),
            shifted(innerEnd, nx, ny, innerHalf),
            shifted(outerEnd, nx, ny, outerHalf), outerRim,
            adaptiveInnerHaloWhite);
    }
}

void appendExtendedThickSegment(
    std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint first, ScreenPoint second, double width,
    double capExtension, const std::array<float, 4>& color)
{
    const double segmentLength = length(first, second);
    if (segmentLength <= 1e-6) return;
    const double dx = (second.x - first.x) / segmentLength;
    const double dy = (second.y - first.y) / segmentLength;
    first.x -= dx * capExtension;
    first.y -= dy * capExtension;
    second.x += dx * capExtension;
    second.y += dy * capExtension;
    appendThickSegment(vertices, first, second, width, color);
}

void appendInternalJoinOverlappedSegment(
    std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint first, ScreenPoint second, double width,
    double overlap, bool extendFirst, bool extendSecond,
    const std::array<float, 4>& color)
{
    const double segmentLength = length(first, second);
    if (segmentLength <= 1e-6) return;
    const double dx = (second.x - first.x) / segmentLength;
    const double dy = (second.y - first.y) / segmentLength;
    if (extendFirst) {
        first.x -= dx * overlap;
        first.y -= dy * overlap;
    }
    if (extendSecond) {
        second.x += dx * overlap;
        second.y += dy * overlap;
    }
    appendThickSegment(vertices, first, second, width, color);
}

ScreenSegment laterallyOffset(const ScreenSegment& segment, double offset)
{
    const double segmentLength = length(segment.first, segment.second);
    if (segmentLength <= 1e-6 || std::fabs(offset) <= 1e-6) return segment;
    const double nx = -(segment.second.y - segment.first.y) /
        segmentLength * offset;
    const double ny = (segment.second.x - segment.first.x) /
        segmentLength * offset;
    return {{segment.first.x + nx, segment.first.y + ny},
        {segment.second.x + nx, segment.second.y + ny},
        segment.breakBefore};
}

void appendContrastProbe(std::vector<TrajectoryContrastProbe>& probes,
    TrajectoryContrastTrack track, ScreenPoint first, ScreenPoint second,
    const TrajectoryOverlayStyle& style, const std::string& primitiveId,
    const std::string& arcSegmentId, CompoundComponentRole role,
    std::uint64_t probeOrdinal)
{
    const double segmentLength = length(first, second);
    if (segmentLength <= 1e-6) return;
    const ScreenPoint center{(first.x + second.x) * 0.5,
        (first.y + second.y) * 0.5};
    const double nx = -(second.y - first.y) / segmentLength;
    const double ny = (second.x - first.x) / segmentLength;
    const double backgroundOffset = style.lineWidthPx * 0.5 +
        style.haloWidthPx + 24.0;
    const double haloOffset = style.lineWidthPx * 0.5 +
        style.haloWidthPx * 0.5;
    probes.push_back({track,
        static_cast<int>(std::lround(center.x)),
        static_cast<int>(std::lround(center.y)),
        static_cast<int>(std::lround(center.x + nx * backgroundOffset)),
        static_cast<int>(std::lround(center.y + ny * backgroundOffset)),
        static_cast<int>(std::lround(center.x + nx * haloOffset)),
        static_cast<int>(std::lround(center.y + ny * haloOffset)),
        primitiveId, arcSegmentId, role, probeOrdinal,
        (second.x-first.x)/segmentLength,
        (second.y-first.y)/segmentLength,nx,ny});
}

void appendTickContrastProbe(std::vector<TrajectoryContrastProbe>& probes,
    ScreenPoint first, ScreenPoint second,
    const TrajectoryOverlayStyle& style, const std::string& primitiveId,
    const std::string& arcSegmentId, CompoundComponentRole role,
    std::uint64_t probeOrdinal)
{
    const double segmentLength = length(first, second);
    if (segmentLength <= 1e-6) return;
    const double dx = (second.x - first.x) / segmentLength;
    const double dy = (second.y - first.y) / segmentLength;
    const double nx = -dy;
    const double ny = dx;
    // The tick's core is sampled at its stable midpoint. Its halo must be
    // sampled along the tick normal, not along the tick tangent (which stays
    // inside the long core and previously mislabeled core ink as halo).
    const ScreenPoint center = interpolate(first, second, 0.5);
    const double haloOffset = style.haloWidthPx * 0.5;
    const double localBackgroundOffset = style.haloWidthPx + 24.0;
    const double coreEdgeOffset = style.lineWidthPx * 0.5 + haloOffset;
    const ScreenPoint haloStroke{center.x + nx * coreEdgeOffset,
        center.y + ny * coreEdgeOffset};
    probes.push_back({TrajectoryContrastTrack::Actual,
        static_cast<int>(std::lround(center.x)),
        static_cast<int>(std::lround(center.y)),
        static_cast<int>(std::lround(second.x + dx * localBackgroundOffset)),
        static_cast<int>(std::lround(second.y + dy * localBackgroundOffset)),
        static_cast<int>(std::lround(haloStroke.x)),
        static_cast<int>(std::lround(haloStroke.y)),
        primitiveId, arcSegmentId, role, probeOrdinal,
        dx,dy,nx,ny});
}

void appendDisc(std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint center, double diameter, const std::array<float, 4>& color,
    TrajectoryVisualSublayer visualSublayer = TrajectoryVisualSublayer::Core,
    bool adaptiveInnerHaloWhite = false)
{
    constexpr int slices = 20;
    const double radius = diameter * 0.5;
    for (int slice = 0; slice < slices; ++slice) {
        const double firstAngle = 2.0 * kPi * slice / slices;
        const double secondAngle = 2.0 * kPi * (slice + 1) / slices;
        appendVertex(vertices, center, color, visualSublayer,
            adaptiveInnerHaloWhite);
        appendVertex(vertices, {center.x + std::cos(firstAngle) * radius,
            center.y + std::sin(firstAngle) * radius}, color, visualSublayer,
            adaptiveInnerHaloWhite);
        appendVertex(vertices, {center.x + std::cos(secondAngle) * radius,
            center.y + std::sin(secondAngle) * radius}, color, visualSublayer,
            adaptiveInnerHaloWhite);
    }
}

void appendRing(std::vector<TrajectoryOverlayVertex>& vertices,
    ScreenPoint center, double diameter, double stroke,
    const std::array<float, 4>& color,
    TrajectoryVisualSublayer visualSublayer = TrajectoryVisualSublayer::Core,
    bool adaptiveInnerHaloWhite = false)
{
    constexpr int slices = 24;
    const double radius = diameter * 0.5;
    for (int slice = 0; slice < slices; ++slice) {
        const double firstAngle = 2.0 * kPi * slice / slices;
        const double secondAngle = 2.0 * kPi * (slice + 1) / slices;
        appendThickSegment(vertices,
            {center.x + std::cos(firstAngle) * radius,
                center.y + std::sin(firstAngle) * radius},
            {center.x + std::cos(secondAngle) * radius,
                center.y + std::sin(secondAngle) * radius}, stroke, color,
            visualSublayer, adaptiveInnerHaloWhite);
    }
}

bool clipPointInsideFrustum(const render::HomogeneousPoint& point)
{
    return render::finiteHomogeneous(point) && point.w > 1e-5 &&
        point.x >= -point.w && point.x <= point.w &&
        point.y >= -point.w && point.y <= point.w &&
        point.z >= -point.w && point.z <= point.w;
}

bool projectPoint(const render::SharedCameraProjection& projection,
    const Point3& world, ScreenPoint& screen)
{
    const render::HomogeneousPoint clip =
        render::transformWorldToClip(projection, world);
    render::PhysicalScreenPoint physical;
    if (!clipPointInsideFrustum(clip) ||
        !render::clipToPhysicalScreen(projection, clip, physical))
        return false;
    screen = {physical.x, physical.y};
    return true;
}

// The play-surface polygon is a clipping boundary, so its corners are allowed
// to land outside the viewport. Requiring x/y frustum inclusion here would
// incorrectly reject player-low and broadcast cameras whose table fills the
// frame. Near/far and positive-w validation still prevent invalid divides.
bool projectClippingBoundaryPoint(
    const render::SharedCameraProjection& projection,
    const Point3& world, ScreenPoint& screen)
{
    const render::HomogeneousPoint clip =
        render::transformWorldToClip(projection, world);
    if (!render::finiteHomogeneous(clip) || clip.w <= 1e-5 ||
        clip.z < -clip.w || clip.z > clip.w)
        return false;
    render::PhysicalScreenPoint physical;
    if (!render::clipToPhysicalScreen(projection, clip, physical)) return false;
    screen = {physical.x, physical.y};
    return true;
}

bool projectAndClipSegment(const render::SharedCameraProjection& projection,
    Point3 firstWorld, Point3 secondWorld, const ScreenRect& safe,
    const std::array<ScreenPoint, 4>& table,
    ScreenPoint& first, ScreenPoint& second)
{
    if (!clipWorldPlayfield(firstWorld, secondWorld)) return false;
    render::HomogeneousPoint firstClip =
        render::transformWorldToClip(projection, firstWorld);
    render::HomogeneousPoint secondClip =
        render::transformWorldToClip(projection, secondWorld);
    if (!render::clipHomogeneousSegment(firstClip, secondClip)) return false;
    render::PhysicalScreenPoint firstPhysical;
    render::PhysicalScreenPoint secondPhysical;
    if (!render::clipToPhysicalScreen(
            projection, firstClip, firstPhysical) ||
        !render::clipToPhysicalScreen(
            projection, secondClip, secondPhysical))
        return false;
    first = {firstPhysical.x, firstPhysical.y};
    second = {secondPhysical.x, secondPhysical.y};
    return clipScreenRect(safe, first, second) &&
        clipConvexPolygon(table, first, second) &&
        length(first, second) > 1e-6;
}

std::vector<ScreenSegment> projectTrack(const OverlayTrack& track,
    const render::SharedCameraProjection& projection,
    const ScreenRect& safe, const std::array<ScreenPoint, 4>& table,
    TrajectoryOverlayTrackDiagnostics& diagnostics)
{
    std::vector<ScreenSegment> output;
    diagnostics.sourceSampleCount = track.samples.size();
    bool breakBefore = true;
    for (std::size_t index = 1; index < track.samples.size(); ++index) {
        const OverlaySample& previous = track.samples[index - 1];
        const OverlaySample& current = track.samples[index];
        const bool invalid = !previous.visible || !current.visible ||
            current.tick <= previous.tick ||
            !std::isfinite(previous.position.x) ||
            !std::isfinite(previous.position.y) ||
            !std::isfinite(previous.position.z) ||
            !std::isfinite(current.position.x) ||
            !std::isfinite(current.position.y) ||
            !std::isfinite(current.position.z);
        if (invalid) {
            breakBefore = true;
            continue;
        }
        Point3 firstWorld = previous.position;
        Point3 secondWorld = current.position;
        firstWorld.y = kTableModelPlaySurfaceHeightCm + 0.15f;
        secondWorld.y = kTableModelPlaySurfaceHeightCm + 0.15f;
        ScreenPoint first;
        ScreenPoint second;
        if (!projectAndClipSegment(projection, firstWorld, secondWorld,
                safe, table, first, second)) {
            breakBefore = true;
            continue;
        }
        output.push_back({first, second, breakBefore});
        diagnostics.projectedArcLengthPx += length(first, second);
        ++diagnostics.projectedSegmentCount;
        breakBefore = false;
    }
    return output;
}

void appendPredicted(const std::vector<ScreenSegment>& segments,
    const TrajectoryOverlayStyle& style,
    const std::array<float, 4>& outerRim,
    const std::array<float, 4>& innerHalo,
    const std::array<float, 4>& color,
    std::vector<TrajectoryOverlayVertex>& outerRimVertices,
    std::vector<TrajectoryOverlayVertex>& innerHaloVertices,
    std::vector<TrajectoryOverlayVertex>& colorVertices,
    std::vector<PendingSemanticBatch>& outerRimBatches,
    std::vector<PendingSemanticBatch>& innerHaloBatches,
    std::vector<PendingSemanticBatch>& colorBatches,
    std::vector<TrajectoryContrastProbe>& probes,
    TrajectoryOverlayTrackDiagnostics& diagnostics,
    std::uint64_t& probeOrdinal)
{
    diagnostics.lateralOffsetPx = style.predictedLateralOffsetPx;
    const bool shortTrack = diagnostics.projectedArcLengthPx <
        3.0 * (style.dashOnPx + style.dashOffPx);
    const double dashOn = shortTrack ?
        diagnostics.projectedArcLengthPx / 6.0 : style.dashOnPx;
    const double dashOff = shortTrack ? dashOn : style.dashOffPx;
    const double period = std::max(dashOn + dashOff, 1e-6);
    double phase = 0.0;
    std::size_t dashOrdinal = 0;
    for (std::size_t segmentIndex = 0;
         segmentIndex < segments.size(); ++segmentIndex) {
        const ScreenSegment segment = laterallyOffset(
            segments[segmentIndex], style.predictedLateralOffsetPx);
        if (segment.breakBefore) phase = 0.0;
        const double segmentLength = length(segment.first, segment.second);
        double cursor = 0.0;
        // Track boundary state explicitly. Recomputing fmod(phase + cursor)
        // can round back below the same boundary forever when its remainder
        // is smaller than the cursor's ULP (observed while orbiting a preview).
        bool on = phase < dashOn;
        double remaining = on ? dashOn - phase : period - phase;
        while (cursor < segmentLength - 1e-6) {
            const double next = std::min(segmentLength, cursor + remaining);
            if (on && next > cursor + 1e-6) {
                const ScreenPoint first = interpolate(segment.first,
                    segment.second, cursor / segmentLength);
                const ScreenPoint second = interpolate(segment.first,
                    segment.second, next / segmentLength);
                const std::string arcSegmentId = "predicted_arc_" +
                    std::to_string(segmentIndex);
                const std::string base = "predicted_dash_" +
                    std::to_string(dashOrdinal++);
                const double innerCapExtension = std::max(
                    style.haloWidthPx,
                    style.predictedCoreCapExtensionPx + 2.0);
                // The outer rim remains a side-only, non-overwriting band at
                // predicted dash caps. Widening its lateral silhouette must
                // not shorten or repaint the authoritative dash gaps.
                const double outerCapExtension = innerCapExtension;
                const std::size_t outerFirst = outerRimVertices.size();
                appendNonOverwritingOuterBand(outerRimVertices, first, second,
                    style.lineWidthPx, style.haloWidthPx,
                    style.outerContrastRimWidthPx, innerCapExtension,
                    outerCapExtension, outerRim, false);
                recordSemanticBatch(outerRimBatches,
                    TrajectoryContrastTrack::Predicted,
                    CompoundComponentRole::OuterHaloEdge, base + "_halo",
                    arcSegmentId, outerFirst, outerRimVertices.size(),
                    TrajectoryVisualSublayer::OuterRim);
                const std::size_t innerFirst = innerHaloVertices.size();
                appendHaloSegment(innerHaloVertices, first, second,
                    style.lineWidthPx, style.haloWidthPx, innerHalo,
                    TrajectoryVisualSublayer::InnerHalo, false,
                    innerCapExtension);
                recordSemanticBatch(innerHaloBatches,
                    TrajectoryContrastTrack::Predicted,
                    CompoundComponentRole::OuterHaloEdge, base + "_halo",
                    arcSegmentId, innerFirst, innerHaloVertices.size(),
                    TrajectoryVisualSublayer::InnerHalo);
                const std::size_t coreFirst = colorVertices.size();
                appendExtendedThickSegment(colorVertices, first, second,
                    style.lineWidthPx, style.predictedCoreCapExtensionPx,
                    color);
                recordSemanticBatch(colorBatches,
                    TrajectoryContrastTrack::Predicted,
                    CompoundComponentRole::Core, base + "_core",
                    arcSegmentId, coreFirst, colorVertices.size(),
                    TrajectoryVisualSublayer::Core);
                const CompoundComponentRole role =
                    probeOrdinal % 2 == 0 ? CompoundComponentRole::Core :
                        CompoundComponentRole::OuterHaloEdge;
                appendContrastProbe(probes, TrajectoryContrastTrack::Predicted,
                    first, second, style,
                    role == CompoundComponentRole::Core ? base + "_core" :
                        base + "_halo",
                    arcSegmentId, role, probeOrdinal++);
                ++diagnostics.visibleStrokeCount;
            }
            cursor = next;
            on = !on;
            remaining = on ? dashOn : dashOff;
        }
        phase = std::fmod(phase + segmentLength, period);
    }
}

void appendActual(const std::vector<ScreenSegment>& segments,
    const TrajectoryOverlayStyle& style,
    const std::array<float, 4>& outerRim,
    const std::array<float, 4>& innerHalo,
    const std::array<float, 4>& color,
    std::vector<TrajectoryOverlayVertex>& outerRimVertices,
    std::vector<TrajectoryOverlayVertex>& innerHaloVertices,
    std::vector<TrajectoryOverlayVertex>& colorVertices,
    std::vector<PendingSemanticBatch>& outerRimBatches,
    std::vector<PendingSemanticBatch>& innerHaloBatches,
    std::vector<PendingSemanticBatch>& colorBatches,
    std::vector<TrajectoryContrastProbe>& probes,
    TrajectoryOverlayTrackDiagnostics& diagnostics,
    std::uint64_t& probeOrdinal)
{
    diagnostics.lateralOffsetPx = style.actualLateralOffsetPx;
    double distanceToTick = 0.0;
    std::size_t tickOrdinal = 0;
    for (std::size_t segmentIndex = 0;
         segmentIndex < segments.size(); ++segmentIndex) {
        const ScreenSegment segment = laterallyOffset(
            segments[segmentIndex], style.actualLateralOffsetPx);
        if (segment.breakBefore) distanceToTick = 0.0;
        const std::string arcSegmentId = "actual_arc_" +
            std::to_string(segmentIndex);
        const std::string segmentBase = "actual_segment_" +
            std::to_string(segmentIndex);
        const std::size_t segmentOuterFirst = outerRimVertices.size();
        appendNonOverwritingOuterBand(outerRimVertices, segment.first,
            segment.second, style.lineWidthPx, style.haloWidthPx,
            style.outerContrastRimWidthPx, style.haloWidthPx,
            style.haloWidthPx + style.outerContrastRimWidthPx,
            outerRim, true);
        recordSemanticBatch(outerRimBatches, TrajectoryContrastTrack::Actual,
            CompoundComponentRole::OuterHaloEdge, segmentBase + "_halo",
            arcSegmentId, segmentOuterFirst, outerRimVertices.size(),
            TrajectoryVisualSublayer::OuterRim);
        const std::size_t segmentInnerFirst = innerHaloVertices.size();
        appendHaloSegment(innerHaloVertices, segment.first, segment.second,
            style.lineWidthPx, style.haloWidthPx, innerHalo,
            TrajectoryVisualSublayer::InnerHalo, true);
        recordSemanticBatch(innerHaloBatches, TrajectoryContrastTrack::Actual,
            CompoundComponentRole::OuterHaloEdge, segmentBase + "_halo",
            arcSegmentId, segmentInnerFirst, innerHaloVertices.size(),
            TrajectoryVisualSublayer::InnerHalo);
        const std::size_t segmentCoreFirst = colorVertices.size();
        const bool extendFirst = segmentIndex > 0 && !segment.breakBefore;
        const bool extendSecond = segmentIndex + 1 < segments.size() &&
            !segments[segmentIndex + 1].breakBefore;
        appendInternalJoinOverlappedSegment(colorVertices, segment.first,
            segment.second, style.lineWidthPx,
            style.actualInternalJoinOverlapPx, extendFirst, extendSecond,
            color);
        recordSemanticBatch(colorBatches, TrajectoryContrastTrack::Actual,
            CompoundComponentRole::Core, segmentBase + "_core",
            arcSegmentId, segmentCoreFirst, colorVertices.size(),
            TrajectoryVisualSublayer::Core);
        ++diagnostics.visibleStrokeCount;
        const double segmentLength = length(segment.first, segment.second);
        double cursor = distanceToTick;
        while (cursor <= segmentLength + 1e-6) {
            const ScreenPoint center = interpolate(segment.first,
                segment.second, cursor / segmentLength);
            // Use the neighboring projected samples where available so marker
            // orientation is a central-difference tangent at joins rather
            // than a one-sided segment direction. Discontinuities never leak
            // a tangent across their explicit break.
            ScreenPoint tangentFirst = segment.first;
            ScreenPoint tangentSecond = segment.second;
            if (segmentIndex > 0 && !segment.breakBefore)
                tangentFirst = laterallyOffset(segments[segmentIndex - 1],
                    style.actualLateralOffsetPx).first;
            if (segmentIndex + 1 < segments.size() &&
                !segments[segmentIndex + 1].breakBefore)
                tangentSecond = laterallyOffset(segments[segmentIndex + 1],
                    style.actualLateralOffsetPx).second;
            const double tangentLength = std::max(
                length(tangentFirst, tangentSecond), 1e-6);
            const double dx =
                (tangentSecond.x - tangentFirst.x) / tangentLength;
            const double dy =
                (tangentSecond.y - tangentFirst.y) / tangentLength;
            const double half = style.actualTickLengthPx * 0.5;
            const ScreenPoint first{center.x - dy * half,
                center.y + dx * half};
            const ScreenPoint second{center.x + dy * half,
                center.y - dx * half};
            const std::string tickBase = "actual_tick_" +
                std::to_string(tickOrdinal++);
            const std::size_t tickOuterFirst = outerRimVertices.size();
            appendNonOverwritingOuterBand(outerRimVertices, first, second,
                style.lineWidthPx, style.haloWidthPx,
                style.outerContrastRimWidthPx, style.haloWidthPx,
                style.haloWidthPx + style.outerContrastRimWidthPx,
                outerRim, true);
            recordSemanticBatch(outerRimBatches,
                TrajectoryContrastTrack::Actual,
                CompoundComponentRole::OuterHaloEdge, tickBase + "_halo",
                arcSegmentId, tickOuterFirst, outerRimVertices.size(),
                TrajectoryVisualSublayer::OuterRim);
            const std::size_t tickInnerFirst = innerHaloVertices.size();
            appendHaloSegment(innerHaloVertices, first, second,
                style.lineWidthPx, style.haloWidthPx, innerHalo,
                TrajectoryVisualSublayer::InnerHalo, true);
            recordSemanticBatch(innerHaloBatches,
                TrajectoryContrastTrack::Actual,
                CompoundComponentRole::OuterHaloEdge, tickBase + "_halo",
                arcSegmentId, tickInnerFirst, innerHaloVertices.size(),
                TrajectoryVisualSublayer::InnerHalo);
            const std::size_t tickCoreFirst = colorVertices.size();
            appendThickSegment(colorVertices, first, second,
                style.lineWidthPx, color);
            recordSemanticBatch(colorBatches, TrajectoryContrastTrack::Actual,
                CompoundComponentRole::Tick, tickBase + "_core",
                arcSegmentId, tickCoreFirst, colorVertices.size(),
                TrajectoryVisualSublayer::Core);
            appendTickContrastProbe(probes, first, second, style,
                tickBase + "_core", arcSegmentId,
                CompoundComponentRole::Tick, probeOrdinal++);
            ++diagnostics.tickMarkCount;
            cursor += style.actualTickSpacingPx;
        }
        distanceToTick = cursor - segmentLength;
    }
    // Retain one body-core sample per visible actual segment. Append these
    // only after the pre-existing tick inventory so all frozen tick
    // identities (including ordinal/coordinates) remain unchanged. Tick
    // probes retain their own Tick role and are too sparse by themselves to
    // establish actual-body coverage for a short but valid replay.
    for (std::size_t segmentIndex = 0;
         segmentIndex < segments.size(); ++segmentIndex) {
        const ScreenSegment segment = laterallyOffset(
            segments[segmentIndex], style.actualLateralOffsetPx);
        const std::string arcSegmentId = "actual_arc_" +
            std::to_string(segmentIndex);
        const std::string segmentBase = "actual_segment_" +
            std::to_string(segmentIndex);
        appendContrastProbe(probes, TrajectoryContrastTrack::Actual,
            segment.first, segment.second, style, segmentBase + "_core",
            arcSegmentId, CompoundComponentRole::Core, probeOrdinal++);
    }
}

bool projectedEndpoint(const OverlayTrack& track,
    const render::SharedCameraProjection& projection,
    const ScreenRect& safe, const std::array<ScreenPoint, 4>& table,
    ScreenPoint& endpoint)
{
    if (!track.visible || !track.endpointVisible) return false;
    Point3 world = track.endpoint;
    world.y = kTableModelPlaySurfaceHeightCm + 0.15f;
    if (std::fabs(world.x) > kChinesePlayfieldWidthCm * 0.5f ||
        std::fabs(world.z) > kChinesePlayfieldLengthCm * 0.5f)
        return false;
    return projectPoint(projection, world, endpoint) &&
        inside(safe, endpoint) && pointInConvexPolygon(table, endpoint);
}

}  // namespace

TrajectoryOverlayStyle resolveTrajectoryOverlayStyle(
    const TrajectoryOverlayViewport& viewport)
{
    TrajectoryOverlayStyle style;
    if (viewport.physicalWidth <= 0 || viewport.physicalHeight <= 0 ||
        !std::isfinite(viewport.deviceScale) || viewport.deviceScale <= 0.0 ||
        !std::isfinite(viewport.uiScale) || viewport.uiScale < 0.85 - 1e-6 ||
        viewport.uiScale > 1.35 + 1e-6)
        return style;
    const double physicalScale =
        static_cast<double>(viewport.physicalHeight) / 720.0;
    // ProductShellSnapshot stores the validated setting as float.  Preserve
    // the inclusive 1.35 contract after that representation round-trip.
    const double scale = physicalScale *
        std::clamp(viewport.uiScale, 0.85, 1.35);
    style.lineWidthPx = 3.0 * scale;
    style.dashOnPx = 12.0 * scale;
    style.dashOffPx = 8.0 * scale;
    style.actualTickLengthPx = 8.5 * scale;
    style.actualTickSpacingPx = 32.0 * scale;
    style.endpointOuterDiameterPx = 18.0 * scale;
    style.endpointRingStrokePx = 3.0 * scale;
    style.actualEndpointDotDiameterPx = 8.0 * scale;
    style.haloWidthPx = (4.0 / 3.0) * scale;
    style.outerContrastRimWidthPx = (4.0 / 3.0) * scale;
    style.predictedCoreCapExtensionPx = 1.0 * scale;
    style.predictedLateralOffsetPx = 0.0;
    style.actualLateralOffsetPx = 0.0;
    const bool fullHd = viewport.physicalWidth == 1920 &&
        viewport.physicalHeight == 1080;
    const bool ui135 = std::fabs(viewport.uiScale - 1.35) < 1e-5;
    if (fullHd && ui135 && viewport.deviceScale > 1.0) {
        style = {6.5, 24.0, 17.0, 18.0, 66.0, 36.0, 6.5,
            16.0, 3.0, 2.5, 2.0, 0.0, 0.0};
    } else if (fullHd && std::fabs(viewport.uiScale - 1.0) < 1e-5) {
        style = {4.5, 18.0, 12.0, 13.0, 48.0, 27.0, 4.5,
            12.0, 2.0, 2.0, 1.5, 0.0, 0.0};
    } else if (viewport.physicalWidth == 1280 &&
            viewport.physicalHeight == 720 && ui135) {
        style = {4.5, 16.0, 10.0, 12.0, 42.0, 24.0, 4.5,
            11.0, 2.0, 2.0, 1.5, 0.0, 0.0};
    }
    style.actualInternalJoinOverlapPx = std::max(
        0.5, style.lineWidthPx / 6.0);
    // Predicted is intentionally composited last. Keep actual time markers
    // readable on coincident paths: each core wing extends beyond the entire
    // predicted compound stroke by at least two physical pixels (scaled with
    // the existing physical/UI model), without moving the authored center.
    const double predictedCompoundWidth = style.lineWidthPx +
        2.0 * (style.haloWidthPx + style.outerContrastRimWidthPx);
    style.actualTickLengthPx = std::max(style.actualTickLengthPx,
        predictedCompoundWidth + 4.0 * std::max(1.0, scale));
    return style;
}

TrajectoryOverlayDrawPackage buildTrajectoryOverlayDrawPackage(
    const TrajectoryOverlaySnapshot& snapshot,
    const CameraPresentation& camera,
    const TrajectoryOverlayViewport& viewport,
    TrajectoryOverlayPalette palette)
{
    TrajectoryOverlayDrawPackage result;
    result.style = resolveTrajectoryOverlayStyle(viewport);
    if (result.style.lineWidthPx <= 0.0) {
        result.errorCode = "trajectory_overlay_viewport_invalid";
        return result;
    }
    const render::SharedCameraProjection projection =
        render::buildSharedCameraProjection(
            camera, viewport.physicalWidth, viewport.physicalHeight);
    if (!projection.valid) {
        result.errorCode = "trajectory_overlay_camera_invalid";
        return result;
    }
    result.usedSharedCameraProjection = true;
    result.homogeneousClipApplied = true;

    const bool wide = viewport.physicalWidth >= 1920 ||
        viewport.deviceScale > 1.0;
    const double halfStroke = result.style.lineWidthPx * 0.5;
    const ScreenRect safe{
        (wide ? 64.0 : 43.0) + halfStroke,
        (wide ? 36.0 : 24.0) + halfStroke,
        viewport.physicalWidth - (wide ? 64.0 : 43.0) - halfStroke,
        viewport.physicalHeight - (wide ? 54.0 : 36.0) - halfStroke};
    if (safe.right <= safe.left || safe.bottom <= safe.top) {
        result.errorCode = "trajectory_overlay_safe_area_empty";
        return result;
    }
    result.safeAreaClipApplied = true;

    const float y = kTableModelPlaySurfaceHeightCm + 0.15f;
    const std::array<Point3, 4> tableWorld{{
        {-kChinesePlayfieldWidthCm * 0.5f, y,
            -kChinesePlayfieldLengthCm * 0.5f},
        {kChinesePlayfieldWidthCm * 0.5f, y,
            -kChinesePlayfieldLengthCm * 0.5f},
        {kChinesePlayfieldWidthCm * 0.5f, y,
            kChinesePlayfieldLengthCm * 0.5f},
        {-kChinesePlayfieldWidthCm * 0.5f, y,
            kChinesePlayfieldLengthCm * 0.5f}}};
    std::array<ScreenPoint, 4> table{};
    bool allCornersProjectable = true;
    for (std::size_t index = 0; index < table.size(); ++index)
        allCornersProjectable &= projectClippingBoundaryPoint(
            projection, tableWorld[index], table[index]);
    if (!allCornersProjectable)
        table = {{{safe.left, safe.top}, {safe.right, safe.top},
            {safe.right, safe.bottom}, {safe.left, safe.bottom}}};
    if (std::fabs(polygonArea(table)) <= 1.0) {
        result.errorCode = "trajectory_overlay_play_surface_degenerate";
        return result;
    }
    result.playSurfaceStencilApplied = true;

    std::vector<TrajectoryOverlayVertex> predictedOuterRimVertices;
    std::vector<TrajectoryOverlayVertex> predictedInnerHaloVertices;
    std::vector<TrajectoryOverlayVertex> predictedColorVertices;
    std::vector<TrajectoryOverlayVertex> actualOuterRimVertices;
    std::vector<TrajectoryOverlayVertex> actualInnerHaloVertices;
    std::vector<TrajectoryOverlayVertex> actualColorVertices;
    std::vector<PendingSemanticBatch> predictedOuterRimBatches;
    std::vector<PendingSemanticBatch> predictedInnerHaloBatches;
    std::vector<PendingSemanticBatch> predictedColorBatches;
    std::vector<PendingSemanticBatch> actualOuterRimBatches;
    std::vector<PendingSemanticBatch> actualInnerHaloBatches;
    std::vector<PendingSemanticBatch> actualColorBatches;
    std::uint64_t probeOrdinal = 0;
    const TrajectoryOverlayVisualContrastContract visual =
        trajectoryOverlayVisualContrastContract();
    if (snapshot.predicted.visible) {
        const auto segments = projectTrack(snapshot.predicted, projection,
            safe, table, result.predicted);
        ScreenPoint endpointTangent{1.0, 0.0};
        if (!segments.empty()) {
            const ScreenSegment& last = segments.back();
            const double tangentLength = std::max(
                length(last.first, last.second), 1e-6);
            endpointTangent = {
                (last.second.x - last.first.x) / tangentLength,
                (last.second.y - last.first.y) / tangentLength};
        }
        appendPredicted(segments, result.style,
            visual.predictedOuterRimLinearRgba,
            visual.predictedInnerHaloLinearRgba,
            resolvedColor(snapshot.predicted.encoding, true, palette),
            predictedOuterRimVertices, predictedInnerHaloVertices,
            predictedColorVertices, predictedOuterRimBatches,
            predictedInnerHaloBatches, predictedColorBatches,
            result.contrastProbes, result.predicted, probeOrdinal);
        ScreenPoint endpoint;
        result.predicted.endpointMarkerVisible = projectedEndpoint(
            snapshot.predicted, projection, safe, table, endpoint);
        result.predicted.endpointOffscreen =
            snapshot.predicted.endpointVisible &&
            !result.predicted.endpointMarkerVisible;
        if (result.predicted.endpointMarkerVisible) {
            const std::size_t outerFirst = predictedOuterRimVertices.size();
            appendRing(predictedOuterRimVertices, endpoint,
                result.style.endpointOuterDiameterPx +
                    2.0 * result.style.haloWidthPx +
                    result.style.outerContrastRimWidthPx,
                result.style.outerContrastRimWidthPx,
                visual.predictedOuterRimLinearRgba,
                TrajectoryVisualSublayer::OuterRim, false);
            recordSemanticBatch(predictedOuterRimBatches,
                TrajectoryContrastTrack::Predicted,
                CompoundComponentRole::OuterHaloEdge,
                "predicted_endpoint_halo", "predicted_endpoint",
                outerFirst, predictedOuterRimVertices.size(),
                TrajectoryVisualSublayer::OuterRim);
            const std::size_t innerFirst = predictedInnerHaloVertices.size();
            appendRing(predictedInnerHaloVertices, endpoint,
                result.style.endpointOuterDiameterPx +
                    2.0 * result.style.haloWidthPx,
                result.style.endpointRingStrokePx +
                    2.0 * result.style.haloWidthPx,
                visual.predictedInnerHaloLinearRgba,
                TrajectoryVisualSublayer::InnerHalo, false);
            recordSemanticBatch(predictedInnerHaloBatches,
                TrajectoryContrastTrack::Predicted,
                CompoundComponentRole::OuterHaloEdge,
                "predicted_endpoint_halo", "predicted_endpoint",
                innerFirst, predictedInnerHaloVertices.size(),
                TrajectoryVisualSublayer::InnerHalo);
            const std::size_t coreFirst = predictedColorVertices.size();
            appendRing(predictedColorVertices, endpoint,
                result.style.endpointOuterDiameterPx,
                result.style.endpointRingStrokePx,
                resolvedColor(snapshot.predicted.encoding, true, palette));
            recordSemanticBatch(predictedColorBatches,
                TrajectoryContrastTrack::Predicted,
                CompoundComponentRole::Endpoint,
                "predicted_endpoint_core", "predicted_endpoint",
                coreFirst, predictedColorVertices.size(),
                TrajectoryVisualSublayer::Core);
            const double radius = result.style.endpointOuterDiameterPx * 0.5;
            const double background = radius + result.style.haloWidthPx + 24.0;
            result.contrastProbes.push_back({
                TrajectoryContrastTrack::Predicted,
                static_cast<int>(std::lround(endpoint.x + radius)),
                static_cast<int>(std::lround(endpoint.y)),
                static_cast<int>(std::lround(endpoint.x + background)),
                static_cast<int>(std::lround(endpoint.y)),
                static_cast<int>(std::lround(endpoint.x + radius +
                    result.style.haloWidthPx * 0.5)),
                static_cast<int>(std::lround(endpoint.y)),
                "predicted_endpoint_core", "predicted_endpoint",
                CompoundComponentRole::Endpoint, probeOrdinal++,
                endpointTangent.x,endpointTangent.y,
                -endpointTangent.y,endpointTangent.x});
        }
    }
    const std::size_t predictedVertices =
        predictedOuterRimVertices.size() +
        predictedInnerHaloVertices.size() + predictedColorVertices.size();
    result.predicted.vertexCount = predictedVertices;
    if (predictedVertices > kMaximumVerticesPerTrack) {
        result.errorCode = "trajectory_overlay_predicted_vertex_budget";
        return result;
    }

    if (snapshot.actual.visible) {
        const auto segments = projectTrack(snapshot.actual, projection,
            safe, table, result.actual);
        ScreenPoint endpointTangent{1.0, 0.0};
        if (!segments.empty()) {
            const ScreenSegment& last = segments.back();
            const double tangentLength = std::max(
                length(last.first, last.second), 1e-6);
            endpointTangent = {
                (last.second.x - last.first.x) / tangentLength,
                (last.second.y - last.first.y) / tangentLength};
        }
        appendActual(segments, result.style,
            visual.actualOuterRimLinearRgba,
            visual.actualInnerHaloLinearRgba,
            resolvedColor(snapshot.actual.encoding, false, palette),
            actualOuterRimVertices, actualInnerHaloVertices,
            actualColorVertices, actualOuterRimBatches,
            actualInnerHaloBatches, actualColorBatches,
            result.contrastProbes, result.actual, probeOrdinal);
        ScreenPoint endpoint;
        result.actual.endpointMarkerVisible = projectedEndpoint(
            snapshot.actual, projection, safe, table, endpoint);
        result.actual.endpointOffscreen = snapshot.actual.endpointVisible &&
            !result.actual.endpointMarkerVisible;
        if (result.actual.endpointMarkerVisible) {
            const std::size_t outerFirst = actualOuterRimVertices.size();
            appendRing(actualOuterRimVertices, endpoint,
                result.style.actualEndpointDotDiameterPx +
                    2.0 * result.style.haloWidthPx +
                    result.style.outerContrastRimWidthPx,
                result.style.outerContrastRimWidthPx,
                visual.actualOuterRimLinearRgba,
                TrajectoryVisualSublayer::OuterRim, true);
            recordSemanticBatch(actualOuterRimBatches,
                TrajectoryContrastTrack::Actual,
                CompoundComponentRole::OuterHaloEdge,
                "actual_endpoint_halo", "actual_endpoint",
                outerFirst, actualOuterRimVertices.size(),
                TrajectoryVisualSublayer::OuterRim);
            const std::size_t innerFirst = actualInnerHaloVertices.size();
            appendDisc(actualInnerHaloVertices, endpoint,
                result.style.actualEndpointDotDiameterPx +
                    2.0 * result.style.haloWidthPx,
                visual.actualInnerHaloLinearRgba,
                TrajectoryVisualSublayer::InnerHalo, true);
            recordSemanticBatch(actualInnerHaloBatches,
                TrajectoryContrastTrack::Actual,
                CompoundComponentRole::OuterHaloEdge,
                "actual_endpoint_halo", "actual_endpoint",
                innerFirst, actualInnerHaloVertices.size(),
                TrajectoryVisualSublayer::InnerHalo);
            const std::size_t coreFirst = actualColorVertices.size();
            appendDisc(actualColorVertices, endpoint,
                result.style.actualEndpointDotDiameterPx,
                resolvedColor(snapshot.actual.encoding, false, palette));
            recordSemanticBatch(actualColorBatches,
                TrajectoryContrastTrack::Actual,
                CompoundComponentRole::Endpoint,
                "actual_endpoint_core", "actual_endpoint",
                coreFirst, actualColorVertices.size(),
                TrajectoryVisualSublayer::Core);
            const double background =
                result.style.actualEndpointDotDiameterPx * 0.5 +
                result.style.haloWidthPx + 24.0;
            result.contrastProbes.push_back({TrajectoryContrastTrack::Actual,
                static_cast<int>(std::lround(endpoint.x)),
                static_cast<int>(std::lround(endpoint.y)),
                static_cast<int>(std::lround(endpoint.x + background)),
                static_cast<int>(std::lround(endpoint.y)),
                static_cast<int>(std::lround(endpoint.x +
                    result.style.actualEndpointDotDiameterPx * 0.5 +
                    result.style.haloWidthPx * 0.5)),
                static_cast<int>(std::lround(endpoint.y)),
                "actual_endpoint_core", "actual_endpoint",
                CompoundComponentRole::Endpoint, probeOrdinal++,
                endpointTangent.x,endpointTangent.y,
                -endpointTangent.y,endpointTangent.x});
        }
    }
    if (actualOuterRimVertices.size() + actualInnerHaloVertices.size() +
            actualColorVertices.size() >
        kMaximumVerticesPerTrack) {
        result.errorCode = "trajectory_overlay_actual_vertex_budget";
        return result;
    }
    result.actual.vertexCount =
        actualOuterRimVertices.size() + actualInnerHaloVertices.size() +
        actualColorVertices.size();

    result.vertices.reserve(predictedVertices + result.actual.vertexCount);
    // Actual is the continuous base; predicted dashes are composited last.
    // Coincident routes therefore retain cyan in dash gaps and amber dashes on
    // top, while actual perpendicular ticks remain visible off the centerline.
    result.vertices.insert(result.vertices.end(),
        actualOuterRimVertices.begin(), actualOuterRimVertices.end());
    result.vertices.insert(result.vertices.end(),
        actualInnerHaloVertices.begin(), actualInnerHaloVertices.end());
    result.vertices.insert(result.vertices.end(),
        actualColorVertices.begin(), actualColorVertices.end());
    result.vertices.insert(result.vertices.end(),
        predictedOuterRimVertices.begin(), predictedOuterRimVertices.end());
    result.vertices.insert(result.vertices.end(),
        predictedInnerHaloVertices.begin(), predictedInnerHaloVertices.end());
    result.vertices.insert(result.vertices.end(),
        predictedColorVertices.begin(), predictedColorVertices.end());
    const auto appendBatches = [&](const auto& batches, std::size_t offset) {
        for (const PendingSemanticBatch& batch : batches) {
            const std::uint32_t token = static_cast<std::uint32_t>(
                result.semanticBatches.size() + 1);
            result.semanticBatches.push_back({batch.track,
                batch.componentRole, batch.primitiveId, batch.arcSegmentId,
                offset + batch.firstVertex, batch.vertexCount, token,
                batch.visualSublayer});
            const std::size_t first = offset + batch.firstVertex;
            const std::size_t end = first + batch.vertexCount;
            if (end > result.vertices.size()) {
                result.errorCode = "trajectory_overlay_semantic_range_invalid";
                return;
            }
            for (std::size_t vertex = first; vertex < end; ++vertex)
                result.vertices[vertex].finalOwnershipToken = token;
        }
    };
    std::size_t semanticOffset = 0;
    appendBatches(actualOuterRimBatches, semanticOffset);
    semanticOffset += actualOuterRimVertices.size();
    appendBatches(actualInnerHaloBatches, semanticOffset);
    semanticOffset += actualInnerHaloVertices.size();
    appendBatches(actualColorBatches, semanticOffset);
    semanticOffset += actualColorVertices.size();
    appendBatches(predictedOuterRimBatches, semanticOffset);
    semanticOffset += predictedOuterRimVertices.size();
    appendBatches(predictedInnerHaloBatches, semanticOffset);
    semanticOffset += predictedInnerHaloVertices.size();
    appendBatches(predictedColorBatches, semanticOffset);
    result.nonColorTopologySeparationApplied =
        snapshot.predicted.visible && snapshot.actual.visible &&
        result.predicted.visibleStrokeCount > 0 &&
        result.actual.visibleStrokeCount > 0 &&
        snapshot.predicted.encoding.lineStyle == TrajectoryLineStyle::Dashed &&
        snapshot.actual.encoding.lineStyle ==
            TrajectoryLineStyle::SolidWithMarkers &&
        result.actual.tickMarkCount > 0;
    result.additionalDrawCalls =
        (result.actual.vertexCount > 0 ? 1u : 0u) +
        (result.predicted.vertexCount > 0 ? 1u : 0u);
    result.advancedGeometryEmitted = false;
    result.ok = true;
    return result;
}

TrajectoryOverlayVisualContrastContract trajectoryOverlayVisualContrastContract()
{
    return {};
}

double trajectoryOverlayFinalSrgbContrastRatio(
    const std::array<std::uint8_t, 3>& first,
    const std::array<std::uint8_t, 3>& second)
{
    const auto luminance = [](const std::array<std::uint8_t, 3>& color) {
        const auto linear = [](double value) {
            value /= 255.0;
            return value <= 0.04045 ? value / 12.92 :
                std::pow((value + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color[0]) +
            0.7152 * linear(color[1]) + 0.0722 * linear(color[2]);
    };
    const double firstLuma = luminance(first);
    const double secondLuma = luminance(second);
    return (std::max(firstLuma, secondLuma) + 0.05) /
        (std::min(firstLuma, secondLuma) + 0.05);
}

const char* trajectoryVisualSublayerName(TrajectoryVisualSublayer sublayer)
{
    switch (sublayer) {
    case TrajectoryVisualSublayer::OuterRim: return "outer_rim";
    case TrajectoryVisualSublayer::InnerHalo: return "inner_halo";
    case TrajectoryVisualSublayer::Core: return "core";
    }
    return "core";
}

TrajectoryAdaptiveOuterRimDecision selectTrajectoryAdaptiveOuterRim(
    const std::array<std::uint8_t, 3>& backdropFinalSrgb8,
    const std::array<std::uint8_t, 3>& innerHaloFinalSrgb8,
    double minimumContrastRatio, bool priorTrackCoverage)
{
    TrajectoryAdaptiveOuterRimDecision result;
    result.innerHaloContrastRatio = trajectoryOverlayFinalSrgbContrastRatio(
        innerHaloFinalSrgb8, backdropFinalSrgb8);
    if (!priorTrackCoverage &&
            result.innerHaloContrastRatio >= minimumContrastRatio) {
        result.selectedOuterContrastRatio = result.innerHaloContrastRatio;
        return result;
    }
    const TrajectoryOverlayVisualContrastContract contract =
        trajectoryOverlayVisualContrastContract();
    const double black = trajectoryOverlayFinalSrgbContrastRatio(
        contract.adaptiveOuterBlackFinalSrgb8, backdropFinalSrgb8);
    const double white = trajectoryOverlayFinalSrgbContrastRatio(
        contract.adaptiveOuterWhiteFinalSrgb8, backdropFinalSrgb8);
    if (white >= black) {
        result.polarity = TrajectoryAdaptiveOuterRimPolarity::White;
        result.selectedOuterContrastRatio = white;
    } else {
        result.polarity = TrajectoryAdaptiveOuterRimPolarity::Black;
        result.selectedOuterContrastRatio = black;
    }
    return result;
}

TrajectoryContrastAudit auditTrajectoryContrastSamples(
    const std::vector<TrajectoryContrastSample>& samples,
    const std::array<std::uint8_t, 3>& predictedRoleSrgb8,
    const std::array<std::uint8_t, 3>& actualRoleSrgb8,
    TrajectoryOverlayPalette palette,
    bool predictedRequired,
    bool actualRequired)
{
    TrajectoryContrastAudit result;
    result.sourceProbeCount = samples.size();
    result.visibleProbeCount = samples.size();
    result.visibleProbeCoverage = samples.empty() ? 0.0 : 1.0;
    if (samples.empty()) {
        result.errorCode = "trajectory_contrast_samples_missing";
        return result;
    }
    const auto linear = [](double value) {
        value /= 255.0;
        return value <= 0.04045 ? value / 12.92 :
            std::pow((value + 0.055) / 1.055, 2.4);
    };
    const auto relativeLuminance = [&](const auto& rgb) {
        return 0.2126 * linear(rgb[0]) + 0.7152 * linear(rgb[1]) +
            0.0722 * linear(rgb[2]);
    };
    const auto srgb8Luma = [](const auto& rgb) {
        return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
    };
    std::vector<double> ratios;
    ratios.reserve(samples.size());
    bool predictedSeen = false;
    bool actualSeen = false;
    std::size_t designTargetPasses = 0;
    for (const TrajectoryContrastSample& sample : samples) {
        predictedSeen |= sample.track == TrajectoryContrastTrack::Predicted;
        actualSeen |= sample.track == TrajectoryContrastTrack::Actual;
        const double stroke = relativeLuminance(sample.strokeSrgb8);
        const double background = relativeLuminance(
            sample.localBackgroundSrgb8);
        const double strokeBackgroundRatio =
            (std::max(stroke, background) + 0.05) /
            (std::min(stroke, background) + 0.05);
        double ratio = strokeBackgroundRatio;
        if (sample.haloSampleValid) {
            const double halo = relativeLuminance(sample.haloSrgb8);
            const double strokeHaloRatio = (std::max(stroke, halo) + 0.05) /
                (std::min(stroke, halo) + 0.05);
            const double haloBackgroundRatio =
                (std::max(halo, background) + 0.05) /
                (std::min(halo, background) + 0.05);
            // A trajectory is a compound screen-space glyph: semantic core
            // plus its profile-mandated halo.  Its core must separate from
            // the halo, and at least one of those two silhouette edges must
            // separate from the rendered scene behind it.
            ratio = std::min(strokeHaloRatio,
                std::max(strokeBackgroundRatio, haloBackgroundRatio));
        }
        ratios.push_back(ratio);
        if (ratio >= 4.5) ++designTargetPasses;
        if (ratio < 4.0)
            result.failureCoordinates.push_back(
                {sample.xPhysicalPx, sample.yPhysicalPx});
    }
    if ((predictedRequired && !predictedSeen) ||
        (actualRequired && !actualSeen)) {
        result.errorCode = "trajectory_contrast_track_inventory_missing";
        return result;
    }
    std::sort(ratios.begin(), ratios.end());
    result.minimumLocalContrastRatio = ratios.front();
    const std::size_t p01Index = static_cast<std::size_t>(
        std::floor(0.01 * static_cast<double>(ratios.size() - 1)));
    result.p01LocalContrastRatio = ratios[p01Index];
    result.designTargetCoverage = static_cast<double>(designTargetPasses) /
        ratios.size();
    result.predictedActualLumaDeltaSrgb8 = std::fabs(
        srgb8Luma(predictedRoleSrgb8) - srgb8Luma(actualRoleSrgb8));
    const double requiredDelta = palette == TrajectoryOverlayPalette::Standard ?
        32.0 : 36.0;
    if (!result.failureCoordinates.empty())
        result.errorCode = "trajectory_local_contrast_below_4";
    else if (result.designTargetCoverage < 0.99)
        result.errorCode = "trajectory_contrast_target_coverage_below_99_percent";
    else if (result.predictedActualLumaDeltaSrgb8 < requiredDelta)
        result.errorCode = "trajectory_role_luma_delta_below_profile";
    else result.ok = true;
    return result;
}

}  // namespace billiardgl::training
