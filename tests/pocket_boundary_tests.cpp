#include "pocket_boundary.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
bool close(double a, double b, double tolerance = 1e-5)
{
    return std::fabs(a - b) <= tolerance;
}
billiardgl::Point3 localPoint(const billiardgl::PocketBoundaryFrame& frame,
    double depth, double offset)
{
    billiardgl::Point3 value = frame.mouthCenter;
    value.x += static_cast<float>(-frame.inward.x * depth + frame.tangent.x * offset);
    value.z += static_cast<float>(-frame.inward.z * depth + frame.tangent.z * offset);
    return value;
}
}

int main()
{
    billiardgl::TableBoundaryProperties boundary;
    boundary.cornerThroatWidthCm = 10.0f;
    boundary.sideThroatWidthCm = 7.0f;
    boundary.jawRadiusCm = 2.5f;
    boundary.throatDepthCm = 3.0f;
    boundary.captureDepthCm = 6.0f;
    const auto frames = billiardgl::buildPocketBoundaryFrames(boundary);
    int corners = 0;
    int sides = 0;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        expect(frames[index].pocketId == static_cast<int>(index),
            "pocket IDs must be stable");
        expect(close(std::hypot(frames[index].inward.x, frames[index].inward.z), 1.0),
            "pocket inward axis must be unit length");
        expect(close(frames[index].inward.x * frames[index].tangent.x +
            frames[index].inward.z * frames[index].tangent.z, 0.0),
            "pocket local axes must be orthogonal");
        if (frames[index].kind == billiardgl::PocketKind::Corner) ++corners;
        else ++sides;
    }
    expect(corners == 4 && sides == 2, "geometry must contain four corner and two side pockets");

    const billiardgl::PocketBoundaryFrame& side = frames[4];
    const double radius = 2.8575;
    expect(billiardgl::classifyPocketPoint(
        side, localPoint(side, -1.0, 0.0), radius).region ==
        billiardgl::PocketBoundaryRegion::Outside,
        "table-side point should be outside the pocket");
    expect(billiardgl::classifyPocketPoint(
        side, localPoint(side, 1.0, 0.0), radius).region ==
        billiardgl::PocketBoundaryRegion::Approaching,
        "centered mouth point should approach the throat");
    expect(billiardgl::classifyPocketPoint(
        side, localPoint(side, 4.0, 0.0), radius).region ==
        billiardgl::PocketBoundaryRegion::Throat,
        "centered point beyond throat plane should enter throat state");
    expect(billiardgl::classifyPocketPoint(
        side, localPoint(side, 6.1, 0.0), radius).region ==
        billiardgl::PocketBoundaryRegion::Capture,
        "centered point beyond capture plane should be captured");
    expect(billiardgl::classifyPocketPoint(
        side, localPoint(side, 2.0, 4.0), radius).region ==
        billiardgl::PocketBoundaryRegion::Solid,
        "offset point beyond radius-reduced corridor should be solid");

    billiardgl::TableBoundaryProperties narrow = boundary;
    narrow.sideThroatWidthCm = 5.0f;
    const auto narrowFrames = billiardgl::buildPocketBoundaryFrames(narrow);
    const auto narrowQuery = billiardgl::classifyPocketPoint(
        narrowFrames[4], localPoint(narrowFrames[4], 4.0, 0.0), radius);
    expect(!narrowQuery.passable && narrowQuery.region ==
        billiardgl::PocketBoundaryRegion::Solid,
        "a throat narrower than the ball diameter must be impassable");

    const auto throat = billiardgl::sweepPocketBoundary(
        side, localPoint(side, -2.0, 0.0), localPoint(side, 5.0, 0.0), radius);
    expect(throat.kind == billiardgl::PocketBoundaryEventKind::Throat &&
        close(throat.local.depthCm, boundary.throatDepthCm, 1e-4),
        "center sweep must find the exact throat plane");
    const auto capture = billiardgl::sweepPocketBoundary(
        side, localPoint(side, 4.0, 0.0), localPoint(side, 8.0, 0.0), radius);
    expect(capture.kind == billiardgl::PocketBoundaryEventKind::Capture &&
        close(capture.local.depthCm, boundary.captureDepthCm, 1e-4),
        "throat sweep must find the exact capture plane");

    const billiardgl::Point3 jawStart = localPoint(side, -6.0, 4.0);
    const billiardgl::Point3 jawEnd = localPoint(side, 2.0, 4.0);
    const auto jaw = billiardgl::sweepPocketBoundary(
        side, jawStart, jawEnd, radius);
    expect(jaw.kind == billiardgl::PocketBoundaryEventKind::LeftJaw ||
        jaw.kind == billiardgl::PocketBoundaryEventKind::RightJaw,
        "high-speed offset sweep must hit a radius-expanded jaw");
    expect(close(std::hypot(jaw.inwardNormal.x, jaw.inwardNormal.z), 1.0),
        "jaw event must expose a unit local normal");

    billiardgl::PocketBoundaryFrame dual = side;
    dual.mouthWidthCm = 4.0f;
    const std::vector<billiardgl::PocketBoundaryEvent> dualEvents =
        billiardgl::sweepPocketBoundaryEvents(
            dual, localPoint(dual, -10.0, 0.0),
            localPoint(dual, 1.0, 0.0), radius);
    std::vector<billiardgl::PocketBoundaryEvent> dualJaws;
    for (const billiardgl::PocketBoundaryEvent& event : dualEvents) {
        if (event.kind == billiardgl::PocketBoundaryEventKind::LeftJaw ||
            event.kind == billiardgl::PocketBoundaryEventKind::RightJaw) {
            dualJaws.push_back(event);
        }
    }
    expect(dualJaws.size() == 2 &&
        close(dualJaws[0].fraction, dualJaws[1].fraction, 1e-10),
        "symmetric equal-TOI jaw sweeps must retain both physical constraints");

    const auto mirrored = billiardgl::sweepPocketBoundary(
        frames[5], localPoint(frames[5], -2.0, 0.0),
        localPoint(frames[5], 5.0, 0.0), radius);
    expect(mirrored.kind == throat.kind && close(mirrored.fraction, throat.fraction),
        "opposite side pockets must preserve mirror-equivalent sweep times");
    return 0;
}
