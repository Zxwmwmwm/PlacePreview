#pragma once

#include "buildghost/Types.hpp"

#include <string_view>
#include <vector>

namespace buildghost {

struct OrientationSummary {
    OrientationKind kind{OrientationKind::None};
    int pillarAxis{0};            // 0 = Y, 1 = X, 2 = Z
    Facing facing{Facing::North}; // Active/front/ascend facing
    bool topHalf{false};          // Upper slab / upside-down stair or trapdoor
    bool hingeRight{false};       // Door hinge side
};

class StateInspector {
public:
    // Classify block family helpers (case-insensitive namespace-stripped matching).
    [[nodiscard]] static std::string_view stripNamespace(std::string_view id) noexcept;
    [[nodiscard]] static bool isDoorBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isBedBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isSlabBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isDoubleSlabBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isStairBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isTrapdoorBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isPillarBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isSixWayBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isHorizontalDirectionalBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isWaterSurfaceBlock(std::string_view id) noexcept;
    [[nodiscard]] static bool isInteractiveBlock(std::string_view id) noexcept;

    // Map special non-BlockItem item identifiers to their placed block identifiers
    // (e.g. "minecraft:bed" -> "minecraft:bed", "minecraft:oak_door" -> "minecraft:oak_door",
    //  "minecraft:oak_sign" -> "minecraft:standing_sign" / "minecraft:wall_sign",
    //  "minecraft:hopper" -> "minecraft:hopper", "minecraft:repeater" -> "minecraft:unpowered_repeater").
    [[nodiscard]] static std::string resolveSpecialItemBlockName(
        std::string_view itemIdentifier,
        Facing hitFace) noexcept;

    // Decode exact OrientationSummary from extracted BlockState properties, falling back
    // cleanly to deterministic placement math if custom blocks omit vanilla state names.
    [[nodiscard]] static OrientationSummary decodeOrientation(
        std::string_view blockIdentifier,
        const std::vector<BlockStateProperty>& props,
        Facing hitFace,
        const Vec3& subClick,
        float playerPitchDeg,
        float playerYawDeg) noexcept;

    // Subtract existing single-slab box(es) from merged full-block box(es) so slab merges
    // preview the exact newly-added upper or lower half-slab rather than overlapping the old slab.
    [[nodiscard]] static std::vector<AABB> computeSlabMergeDiff(
        const BlockPos& cellPos,
        const std::vector<AABB>& existingWorldBoxes);

    // Deterministic 3D sub-box builders for multi-block Doors, Beds, Stairs, and Slabs.
    [[nodiscard]] static std::vector<AABB> buildDoorWorldBoxes(
        const BlockPos& lowerPos,
        Facing playerLookHorizontal,
        bool hingeRight);

    [[nodiscard]] static std::vector<AABB> buildBedWorldBoxes(
        const BlockPos& footPos,
        Facing headDirection);

    [[nodiscard]] static std::vector<AABB> buildFallbackBlockBoxes(
        const BlockPos& pos,
        std::string_view blockIdentifier,
        const OrientationSummary& orient);
};

} // namespace buildghost
