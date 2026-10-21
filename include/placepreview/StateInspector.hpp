#pragma once

#include "placepreview/Types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace placepreview {

struct OrientationSummary {
    OrientationKind kind{OrientationKind::None};
    int pillarAxis{0};            // 0 = Y, 1 = X, 2 = Z
    Facing facing{Facing::North}; // Active/front/ascend facing
    int rotation16{8};            // 0..15 ground_sign_direction
    float yawRadians{0.0f};       // Exact 3D world rotation angle
    bool topHalf{false};          // Upper slab / upside-down stair or trapdoor / hanging lantern
    bool hingeRight{false};       // Door hinge side
};

class StateInspector {
public:
    [[nodiscard]] static std::string_view stripNamespace(std::string_view id) noexcept;
    [[nodiscard]] static std::string_view stripNamespace(const char* id) noexcept {
        return stripNamespace(std::string_view(id ? id : ""));
    }
    [[nodiscard]] static std::string_view stripNamespace(const std::string& id) noexcept {
        return stripNamespace(std::string_view(id));
    }
    static std::string_view stripNamespace(std::string&&) = delete;

    [[nodiscard]] static bool isDoorBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isBedBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isSlabBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isDoubleSlabBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isStairBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isTrapdoorBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isFenceGateBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isStandingSignBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isWallSignBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isHangingSignBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isBannerBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isSkullBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isCandleBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isSeaPickleBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isTurtleEggBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isFlowerBedBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isLiquidOrAirBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isPillarBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isSixWayBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isHorizontalDirectionalBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isWaterSurfaceBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isInteractiveBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isFenceOrWallOrPane(std::string_view id) noexcept;
    [[nodiscard]] static bool isNonSolidSupportBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool hasNonCollidingPlacement(std::string_view id) noexcept;

    // Maps item OR block identifiers cleanly to the exact placed block identifier for `hitFace`
    // (handles face-dependent swaps such as sign <-> wall_sign, standing_banner <-> wall_banner,
    // item_frame -> frame, glow_item_frame -> glow_frame, painting, armor_stand, sea_pickle, etc.).
    [[nodiscard]] static std::string resolveSpecialItemBlockName(
        std::string_view itemIdentifier,
        Facing hitFace) noexcept;

    [[nodiscard]] static OrientationSummary decodeOrientation(
        std::string_view blockIdentifier,
        const std::vector<BlockStateProperty>& props,
        Facing hitFace,
        const Vec3& subClick,
        float playerPitchDeg,
        float playerYawDeg,
        bool isSneaking = false) noexcept;

    [[nodiscard]] static std::vector<AABB> computeSlabMergeDiff(
        const BlockPos& cellPos,
        const std::vector<AABB>& existingWorldBoxes);

    [[nodiscard]] static std::vector<AABB> buildDoorWorldBoxes(
        const BlockPos& lowerPos,
        Facing playerLookHorizontal,
        bool hingeRight);

    [[nodiscard]] static std::vector<AABB> buildBedWorldBoxes(
        const BlockPos& footPos,
        Facing headDirection);

    [[nodiscard]] static std::vector<AABB> buildStairWorldBoxes(
        const BlockPos& pos,
        Facing stairFacing,
        bool topHalf,
        bool hasFrontPerpStair,
        Facing frontPerpFacing,
        bool hasBackPerpStair,
        Facing backPerpFacing);

    [[nodiscard]] static std::vector<AABB> buildConnectedPostWorldBoxes(
        const BlockPos& pos,
        std::string_view blockIdentifier,
        bool connectNorth,
        bool connectSouth,
        bool connectWest,
        bool connectEast);

    [[nodiscard]] static std::vector<AABB> buildCandleClusterWorldBoxes(
        const BlockPos& pos,
        int candleCount);

    [[nodiscard]] static std::vector<AABB> buildSeaPickleClusterWorldBoxes(
        const BlockPos& pos,
        int pickleCount);

    [[nodiscard]] static std::vector<AABB> buildTurtleEggClusterWorldBoxes(
        const BlockPos& pos,
        int eggCount);

    [[nodiscard]] static std::vector<OrientedBox> buildSpecialOrientedBoxes(
        const BlockPos& pos,
        std::string_view blockIdentifier,
        const OrientationSummary& orient);

    [[nodiscard]] static bool validateBlockSupport(
        std::string_view blockIdentifier,
        const BlockPos& placePos,
        Facing hitFace,
        std::string_view belowBlockId,
        std::string_view aboveBlockId,
        std::string_view behindFaceBlockId) noexcept;

    [[nodiscard]] static std::vector<AABB> buildFallbackBlockBoxes(
        const BlockPos& pos,
        std::string_view blockIdentifier,
        const OrientationSummary& orient);
};

} // namespace placepreview
