#include "render_state.h"

#include <algorithm>

namespace billiardgl {
namespace {

float lerp(float from, float to, float alpha)
{
    return from + (to - from) * alpha;
}

Point3 lerpPoint(const Point3& from, const Point3& to, float alpha)
{
    return Point3{
        lerp(from.x, to.x, alpha),
        lerp(from.y, to.y, alpha),
        lerp(from.z, to.z, alpha)};
}

bool sameContinuousBall(const BallState& previous, const BallState& current)
{
    return previous.pocketed == current.pocketed &&
        previous.pocketInteraction.pocketId ==
            current.pocketInteraction.pocketId &&
        previous.pocketInteraction.captureSequence ==
            current.pocketInteraction.captureSequence;
}

}  // namespace

GameState interpolateRenderState(const GameState& previous,
    const GameState& current, double alpha)
{
    GameState rendered = current;
    const float fraction = static_cast<float>(
        std::clamp(alpha, 0.0, 1.0));
    // A collision makes the trajectory inside this physics tick piecewise
    // linear. Interpolating the two tick endpoints with one straight segment
    // can draw a ball through a cushion or another ball even though both
    // solved endpoints are valid. Present the post-solve positions for that
    // tick; ordinary motion remains interpolated below.
    const bool collisionTick = current.events.ballCollision ||
        current.events.railCollision;

    for (int index = 0; index < kBallCount; ++index) {
        const BallState& from = previous.balls[index];
        const BallState& to = current.balls[index];
        if (!sameContinuousBall(from, to)) continue;
        if (!collisionTick) {
            rendered.balls[index].position = lerpPoint(
                from.position, to.position, fraction);
        }
        rendered.balls[index].rotationAngle = lerp(
            from.rotationAngle, to.rotationAngle, fraction);
    }

    if (rendered.camera.anchorMode == CameraAnchorMode::FollowCueBall) {
        const Point3& cuePosition = rendered.balls[0].position;
        rendered.camera.target[0] = cuePosition.x;
        rendered.camera.target[1] = cuePosition.y;
        rendered.camera.target[2] = cuePosition.z;
        updateCameraEye(rendered);
    }

    return rendered;
}

}  // namespace billiardgl
