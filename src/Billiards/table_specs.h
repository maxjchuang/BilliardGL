#pragma once

#include <array>

namespace billiardgl {

constexpr float kChineseBallDiameterCm = 5.715f;
constexpr float kChineseBallRadiusCm = kChineseBallDiameterCm / 2.0f;
constexpr float kChinesePlayfieldLengthCm = 254.0f;
constexpr float kChinesePlayfieldWidthCm = 127.0f;
constexpr float kChineseTableHeightCm = 85.0f;
constexpr float kChineseCornerPocketMouthWidthCm = 13.2f;
constexpr float kChineseSidePocketMouthWidthCm = 8.6f;
constexpr float kChinesePocketDropZoneDepthCm = 6.0f;

struct BallSpec {
    float radiusCm;

    float diameterCm() const
    {
        return radiusCm * 2.0f;
    }
};

struct TableSpec {
    float playfieldWidthCm;
    float playfieldLengthCm;
    float heightCm;
};

struct PocketSpec {
    float cornerMouthWidthCm;
    float sideMouthWidthCm;
    float dropZoneDepthCm;
};

enum class PocketKind {
    Corner,
    Side
};

struct PocketOpening {
    PocketKind kind;
    float centerX;
    float centerZ;
    float mouthWidthCm;
    float dropZoneDepthCm;
};

BallSpec defaultBallSpec();
TableSpec defaultTableSpec();
PocketSpec defaultPocketSpec();
std::array<PocketOpening, 6> buildPocketOpenings(const TableSpec& table, const PocketSpec& pockets);

}  // namespace billiardgl
