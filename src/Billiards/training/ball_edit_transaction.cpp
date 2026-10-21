#include "training/ball_edit_transaction.h"

#include "table_specs.h"

#include <cmath>

namespace billiardgl::training {

BallEditTransaction::BallEditTransaction(
    const GameState& base, const PhysicsProfile& profile)
    : candidate_(base), profile_(profile)
{
}

EditResult BallEditTransaction::setBall(
    int index, const Point3& position, bool pocketed)
{
    if (committed_) return EditResult{false, "edit_already_committed"};
    if (index < 0 || index >= kBallCount)
        return EditResult{false, "edit_ball_index_out_of_range"};
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z))
        return EditResult{false, "edit_nonfinite_position"};
    BallState& ball = candidate_.balls[index];
    ball.position = position;
    ball.pocketed = pocketed;
    resetBallMotion(ball);
    resetPocketInteraction(ball);
    return EditResult{true, ""};
}

EditResult BallEditTransaction::validate() const
{
    if (candidate_.balls[0].pocketed)
        return EditResult{false, "edit_cue_ball_required"};
    const float radius = profile_.ball.radiusCm;
    const float halfWidth = profile_.tableBoundary.playfieldWidthCm * 0.5f;
    const float halfLength = profile_.tableBoundary.playfieldLengthCm * 0.5f;
    const TableSpec table{profile_.tableBoundary.playfieldWidthCm,
        profile_.tableBoundary.playfieldLengthCm, kChineseTableHeightCm};
    const PocketSpec pockets{profile_.tableBoundary.cornerMouthWidthCm,
        profile_.tableBoundary.sideMouthWidthCm,
        profile_.tableBoundary.captureDepthCm};
    const auto openings = buildPocketOpenings(table, pockets);
    for (int index = 0; index < kBallCount; ++index) {
        const BallState& ball = candidate_.balls[index];
        if (ball.pocketed) continue;
        if (!std::isfinite(ball.position.x) || !std::isfinite(ball.position.z))
            return EditResult{false, "edit_nonfinite_position"};
        for (const PocketOpening& opening : openings) {
            const float dx = ball.position.x - opening.centerX;
            const float dz = ball.position.z - opening.centerZ;
            const float forbidden = opening.mouthWidthCm * 0.5f + radius;
            if (dx * dx + dz * dz < forbidden * forbidden)
                return EditResult{false, "edit_position_inside_pocket"};
        }
        if (std::fabs(ball.position.x) > halfWidth - radius ||
            std::fabs(ball.position.z) > halfLength - radius)
            return EditResult{false, "edit_position_outside_table"};
        for (int other = index + 1; other < kBallCount; ++other) {
            const BallState& second = candidate_.balls[other];
            if (second.pocketed) continue;
            const float dx = ball.position.x - second.position.x;
            const float dz = ball.position.z - second.position.z;
            const float minimum = radius * 2.0f;
            if (dx * dx + dz * dz < minimum * minimum - 0.000001f)
                return EditResult{false, "edit_ball_overlap"};
        }
    }
    return EditResult{true, ""};
}

EditResult BallEditTransaction::commit(GameState& target)
{
    if (committed_) return EditResult{false, "edit_already_committed"};
    const EditResult checked = validate();
    if (!checked.ok) return checked;
    target = candidate_;
    target.ballsMoving = false;
    committed_ = true;
    return EditResult{true, ""};
}

}  // namespace billiardgl::training
