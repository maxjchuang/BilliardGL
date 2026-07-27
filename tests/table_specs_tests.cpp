#include "table_specs.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.001f)
{
    return std::fabs(a - b) < epsilon;
}

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    const billiardgl::BallSpec ball = billiardgl::defaultBallSpec();
    if (!nearlyEqual(ball.radiusCm, 2.8575f)) {
        return fail("ball radius should be half of 5.715cm");
    }
    if (!nearlyEqual(ball.diameterCm(), 5.715f)) {
        return fail("ball diameter should be 5.715cm");
    }

    const billiardgl::TableSpec table = billiardgl::defaultTableSpec();
    if (!nearlyEqual(table.playfieldLengthCm, 254.0f)) {
        return fail("playfield length should match Chinese billiards target");
    }
    if (!nearlyEqual(table.playfieldWidthCm, 127.0f)) {
        return fail("playfield width should match Chinese billiards target");
    }
    if (!nearlyEqual(table.heightCm, 85.0f)) {
        return fail("table height should use the selected centimeter target");
    }
    if (!nearlyEqual(
            billiardgl::kTableModelBallClearanceWidthCm *
                billiardgl::kTableModelWidthScale,
            table.playfieldWidthCm) ||
        !nearlyEqual(
            billiardgl::kTableModelBallClearanceLengthCm *
                billiardgl::kTableModelLengthScale,
            table.playfieldLengthCm)) {
        return fail("table model scale should align its legacy cushion geometry with physics");
    }

    const billiardgl::PocketSpec pockets = billiardgl::defaultPocketSpec();
    if (!nearlyEqual(pockets.cornerMouthWidthCm, 13.2f)) {
        return fail("corner pocket mouth should use the selected Chinese-table value");
    }
    if (!nearlyEqual(pockets.sideMouthWidthCm, 8.6f)) {
        return fail("side pocket mouth should use the selected Chinese-table value");
    }
    if (!nearlyEqual(pockets.dropZoneDepthCm, 6.0f)) {
        return fail("drop zone depth should be explicit");
    }

    const std::array<billiardgl::PocketOpening, 6> openings =
        billiardgl::buildPocketOpenings(table, pockets);
    int cornerCount = 0;
    int sideCount = 0;
    for (std::size_t i = 0; i < openings.size(); ++i) {
        if (openings[i].kind == billiardgl::PocketKind::Corner) {
            ++cornerCount;
        }
        if (openings[i].kind == billiardgl::PocketKind::Side) {
            ++sideCount;
        }
    }
    if (cornerCount != 4 || sideCount != 2) {
        return fail("pocket openings should contain four corners and two sides");
    }

    if (!nearlyEqual(openings[0].centerX, -table.playfieldWidthCm / 2.0f) ||
        !nearlyEqual(openings[0].centerZ, -table.playfieldLengthCm / 2.0f)) {
        return fail("first corner pocket should be at the near-left playfield corner");
    }
    if (!nearlyEqual(openings[4].centerX, -table.playfieldWidthCm / 2.0f) ||
        !nearlyEqual(openings[4].centerZ, 0.0f)) {
        return fail("first side pocket should be centered on the left long rail");
    }

    return EXIT_SUCCESS;
}
