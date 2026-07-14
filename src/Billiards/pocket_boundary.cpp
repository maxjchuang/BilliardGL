#include "pocket_boundary.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace billiardgl {
namespace {

Point3 point(float x, float z)
{
    Point3 value;
    value.x = x;
    value.z = z;
    return value;
}

Point3 add(const Point3& first, const Point3& second, double scale)
{
    return point(
        static_cast<float>(first.x + second.x * scale),
        static_cast<float>(first.z + second.z * scale));
}

double dot2(const Point3& first, const Point3& second)
{
    return first.x * second.x + first.z * second.z;
}

double length2(const Point3& value)
{
    return std::sqrt(dot2(value, value));
}

Point3 normalized(float x, float z)
{
    const double length = std::hypot(x, z);
    return point(static_cast<float>(x / length), static_cast<float>(z / length));
}

double channelWidth(const PocketBoundaryFrame& frame, double depth)
{
    if (depth <= 0.0) return frame.mouthWidthCm;
    if (depth >= frame.throatDepthCm) return frame.throatWidthCm;
    const double fraction = depth / frame.throatDepthCm;
    return frame.mouthWidthCm +
        (frame.throatWidthCm - frame.mouthWidthCm) * fraction;
}

Point3 jawCenter(const PocketBoundaryFrame& frame, double side)
{
    return add(frame.mouthCenter, frame.tangent,
        side * (frame.mouthWidthCm * 0.5 + frame.jawRadiusCm));
}

bool circleFirstHit(const Point3& start, const Point3& delta,
    const Point3& center, double radius, double& fraction)
{
    const Point3 relative = point(start.x - center.x, start.z - center.z);
    const double a = dot2(delta, delta);
    if (a <= 1e-18) return false;
    const double b = 2.0 * dot2(relative, delta);
    const double c = dot2(relative, relative) - radius * radius;
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < -1e-10) return false;
    const double root = std::sqrt(std::max(0.0, discriminant));
    const double values[2] = {(-b - root) / (2.0 * a), (-b + root) / (2.0 * a)};
    for (double value : values) {
        if (value < -1e-10 || value > 1.0 + 1e-10) continue;
        const Point3 hit = add(start, delta, value);
        const Point3 outward = point(hit.x - center.x, hit.z - center.z);
        if (dot2(delta, outward) >= 1e-10) continue;
        fraction = std::max(0.0, std::min(1.0, value));
        return true;
    }
    return false;
}

void considerPlane(const PocketBoundaryFrame& frame, const Point3& start,
    const Point3& delta, double targetDepth, double ballRadiusCm,
    PocketBoundaryEventKind kind, PocketBoundaryEvent& best)
{
    const PocketLocalPoint from = pocketLocalPoint(frame, start);
    const PocketLocalPoint to = pocketLocalPoint(frame, add(start, delta, 1.0));
    if (to.depthCm <= from.depthCm + 1e-12 || from.depthCm > targetDepth ||
        to.depthCm < targetDepth) return;
    const double fraction = (targetDepth - from.depthCm) /
        (to.depthCm - from.depthCm);
    const Point3 position = add(start, delta, fraction);
    const PocketBoundaryQuery query = classifyPocketPoint(
        frame, position, ballRadiusCm);
    if (!query.passable || fraction >= best.fraction - 1e-10) return;
    best.kind = kind;
    best.pocketId = frame.pocketId;
    best.fraction = fraction;
    best.position = position;
    best.inwardNormal = frame.inward;
    best.local = query.local;
    best.passable = true;
}

}  // namespace

std::array<PocketBoundaryFrame, 6> buildPocketBoundaryFrames(
    const TableBoundaryProperties& boundary)
{
    const float halfWidth = boundary.playfieldWidthCm * 0.5f;
    const float halfLength = boundary.playfieldLengthCm * 0.5f;
    std::array<PocketBoundaryFrame, 6> frames;
    int index = 0;
    for (int zSign = -1; zSign <= 1; zSign += 2) {
        for (int xSign = -1; xSign <= 1; xSign += 2) {
            PocketBoundaryFrame& frame = frames[index];
            frame.pocketId = index++;
            frame.kind = PocketKind::Corner;
            frame.mouthCenter = point(xSign * halfWidth, zSign * halfLength);
            frame.inward = normalized(-xSign, -zSign);
            frame.tangent = point(-frame.inward.z, frame.inward.x);
            frame.mouthWidthCm = boundary.cornerMouthWidthCm;
            frame.throatWidthCm = boundary.cornerThroatWidthCm;
            frame.jawRadiusCm = boundary.jawRadiusCm;
            frame.throatDepthCm = boundary.throatDepthCm;
            frame.captureDepthCm = boundary.captureDepthCm;
        }
    }
    for (int xSign = -1; xSign <= 1; xSign += 2) {
        PocketBoundaryFrame& frame = frames[index];
        frame.pocketId = index++;
        frame.kind = PocketKind::Side;
        frame.mouthCenter = point(xSign * halfWidth, 0.0f);
        frame.inward = point(-static_cast<float>(xSign), 0.0f);
        frame.tangent = point(0.0f, static_cast<float>(xSign));
        frame.mouthWidthCm = boundary.sideMouthWidthCm;
        frame.throatWidthCm = boundary.sideThroatWidthCm;
        frame.jawRadiusCm = boundary.jawRadiusCm;
        frame.throatDepthCm = boundary.throatDepthCm;
        frame.captureDepthCm = boundary.captureDepthCm;
    }
    return frames;
}

PocketLocalPoint pocketLocalPoint(
    const PocketBoundaryFrame& frame, const Point3& position)
{
    const Point3 relative = point(
        position.x - frame.mouthCenter.x,
        position.z - frame.mouthCenter.z);
    PocketLocalPoint local;
    local.depthCm = -dot2(relative, frame.inward);
    local.offsetCm = dot2(relative, frame.tangent);
    return local;
}

PocketBoundaryQuery classifyPocketPoint(
    const PocketBoundaryFrame& frame, const Point3& position,
    double ballRadiusCm)
{
    PocketBoundaryQuery result;
    result.local = pocketLocalPoint(frame, position);
    const double width = channelWidth(frame, result.local.depthCm);
    result.availableWidthCm = width - 2.0 * ballRadiusCm;
    result.passable = result.availableWidthCm >= -1e-10 &&
        std::fabs(result.local.offsetCm) <=
            std::max(0.0, result.availableWidthCm * 0.5) + 1e-10;
    result.throatSignedDistanceCm = result.local.depthCm - frame.throatDepthCm;
    result.captureSignedDistanceCm = result.local.depthCm - frame.captureDepthCm;
    if (!std::isfinite(result.local.depthCm) ||
        !std::isfinite(result.local.offsetCm) || !std::isfinite(ballRadiusCm) ||
        ballRadiusCm <= 0.0) {
        result.region = PocketBoundaryRegion::Solid;
    } else if (!result.passable) {
        result.region = PocketBoundaryRegion::Solid;
    } else if (result.local.depthCm < 0.0) {
        result.region = PocketBoundaryRegion::Outside;
    } else if (result.local.depthCm < frame.throatDepthCm) {
        result.region = PocketBoundaryRegion::Approaching;
    } else if (result.local.depthCm < frame.captureDepthCm) {
        result.region = PocketBoundaryRegion::Throat;
    } else {
        result.region = PocketBoundaryRegion::Capture;
    }
    return result;
}

PocketBoundaryEvent sweepPocketBoundary(
    const PocketBoundaryFrame& frame, const Point3& start,
    const Point3& end, double ballRadiusCm)
{
    PocketBoundaryEvent best;
    best.pocketId = frame.pocketId;
    const Point3 delta = point(end.x - start.x, end.z - start.z);
    if (!std::isfinite(ballRadiusCm) || ballRadiusCm <= 0.0 ||
        !std::isfinite(delta.x) || !std::isfinite(delta.z)) return best;

    const double effectiveJawRadius = frame.jawRadiusCm + ballRadiusCm;
    for (int side = -1; side <= 1; side += 2) {
        double fraction = 1.0;
        const Point3 center = jawCenter(frame, side);
        if (!circleFirstHit(start, delta, center, effectiveJawRadius, fraction)) continue;
        const Point3 position = add(start, delta, fraction);
        const PocketLocalPoint local = pocketLocalPoint(frame, position);
        if (local.depthCm < -effectiveJawRadius - 1e-8 ||
            local.depthCm > frame.throatDepthCm + effectiveJawRadius) continue;
        if (fraction < best.fraction - 1e-10) {
            const Point3 towardBall = point(position.x - center.x, position.z - center.z);
            const double normalLength = length2(towardBall);
            best.kind = side < 0 ? PocketBoundaryEventKind::LeftJaw :
                PocketBoundaryEventKind::RightJaw;
            best.fraction = fraction;
            best.position = position;
            best.inwardNormal = point(
                static_cast<float>(towardBall.x / normalLength),
                static_cast<float>(towardBall.z / normalLength));
            best.local = local;
            best.passable = false;
        } else if (std::fabs(fraction - best.fraction) <= 1e-10 &&
                   best.kind != PocketBoundaryEventKind::None) {
            best.kind = PocketBoundaryEventKind::Ambiguous;
        }
    }
    considerPlane(frame, start, delta, frame.throatDepthCm, ballRadiusCm,
        PocketBoundaryEventKind::Throat, best);
    considerPlane(frame, start, delta, frame.captureDepthCm, ballRadiusCm,
        PocketBoundaryEventKind::Capture, best);
    return best;
}

const char* pocketBoundaryEventKindName(PocketBoundaryEventKind kind)
{
    switch (kind) {
    case PocketBoundaryEventKind::None: return "none";
    case PocketBoundaryEventKind::StraightRail: return "straight_rail";
    case PocketBoundaryEventKind::LeftJaw: return "left_jaw";
    case PocketBoundaryEventKind::RightJaw: return "right_jaw";
    case PocketBoundaryEventKind::Throat: return "throat";
    case PocketBoundaryEventKind::Capture: return "capture";
    case PocketBoundaryEventKind::Ambiguous: return "ambiguous";
    }
    return "none";
}

const char* pocketBoundaryRegionName(PocketBoundaryRegion region)
{
    switch (region) {
    case PocketBoundaryRegion::Outside: return "outside";
    case PocketBoundaryRegion::Approaching: return "approaching";
    case PocketBoundaryRegion::Throat: return "throat";
    case PocketBoundaryRegion::Capture: return "capture";
    case PocketBoundaryRegion::Solid: return "solid";
    }
    return "outside";
}

}  // namespace billiardgl
