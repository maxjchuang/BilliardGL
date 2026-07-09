#include "table_specs.h"

namespace billiardgl {

BallSpec defaultBallSpec()
{
    return BallSpec{kChineseBallRadiusCm};
}

TableSpec defaultTableSpec()
{
    return TableSpec{kChinesePlayfieldWidthCm, kChinesePlayfieldLengthCm, kChineseTableHeightCm};
}

PocketSpec defaultPocketSpec()
{
    return PocketSpec{
        kChineseCornerPocketMouthWidthCm,
        kChineseSidePocketMouthWidthCm,
        kChinesePocketDropZoneDepthCm};
}

std::array<PocketOpening, 6> buildPocketOpenings(const TableSpec& table, const PocketSpec& pockets)
{
    const float halfWidth = table.playfieldWidthCm / 2.0f;
    const float halfLength = table.playfieldLengthCm / 2.0f;
    return {{
        PocketOpening{PocketKind::Corner, -halfWidth, -halfLength, pockets.cornerMouthWidthCm, pockets.dropZoneDepthCm},
        PocketOpening{PocketKind::Corner, halfWidth, -halfLength, pockets.cornerMouthWidthCm, pockets.dropZoneDepthCm},
        PocketOpening{PocketKind::Corner, -halfWidth, halfLength, pockets.cornerMouthWidthCm, pockets.dropZoneDepthCm},
        PocketOpening{PocketKind::Corner, halfWidth, halfLength, pockets.cornerMouthWidthCm, pockets.dropZoneDepthCm},
        PocketOpening{PocketKind::Side, -halfWidth, 0.0f, pockets.sideMouthWidthCm, pockets.dropZoneDepthCm},
        PocketOpening{PocketKind::Side, halfWidth, 0.0f, pockets.sideMouthWidthCm, pockets.dropZoneDepthCm},
    }};
}

}  // namespace billiardgl
