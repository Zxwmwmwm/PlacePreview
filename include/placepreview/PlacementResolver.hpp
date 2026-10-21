#pragma once

#include "placepreview/Config.hpp"
#include "placepreview/Types.hpp"

#include <string>
#include <vector>

namespace placepreview {

struct RaycastHitInput {
    bool hasBlockHit{false};
    Facing hitFace{Facing::Up};
    BlockPos blockPos{};
    Vec3 hitWorldPos{0.0f, 0.0f, 0.0f};

    bool hasLiquidHit{false};
    Facing liquidFace{Facing::Up};
    BlockPos liquidPos{};
    Vec3 liquidWorldPos{0.0f, 0.0f, 0.0f};
};

struct HeldItemInput {
    bool valid{false};
    int count{0};
    int auxValue{0};
    std::string itemIdentifier;
    std::string directBlockIdentifier; // Non-empty when item directly wraps a BlockType (BlockItem)
    uintptr_t nativeItemStackPtr{0};   // Optional opaque pointer for live engine calls
};

struct PlayerContextInput {
    bool inWorld{false};
    bool isSpectator{false};
    bool isAdventureMode{false};
    bool isSneaking{false};
    float pitchDeg{0.0f};
    float yawDeg{0.0f};
    AABB playerAabb{};

    HeldItemInput mainhand{};
    RaycastHitInput raycast{};
};

class IWorldQuery {
public:
    virtual ~IWorldQuery() = default;

    [[nodiscard]] virtual int minBuildHeight() const noexcept = 0;
    [[nodiscard]] virtual int maxBuildHeight() const noexcept = 0;

    [[nodiscard]] virtual std::string getBlockIdentifier(const BlockPos& pos) const = 0;
    [[nodiscard]] virtual bool isAirBlock(const BlockPos& pos) const = 0;
    [[nodiscard]] virtual bool isReplaceableBlock(const BlockPos& pos) const = 0;
    [[nodiscard]] virtual std::vector<AABB> getExistingBlockWorldBoxes(const BlockPos& pos) const = 0;

    [[nodiscard]] virtual bool checkAdventureCanPlaceOn(
        const HeldItemInput& item,
        const BlockPos& targetBlockPos) const = 0;

    [[nodiscard]] virtual bool calculatePlacePos(
        const HeldItemInput& item,
        Facing& inOutFace,
        BlockPos& inOutPos) const = 0;

    [[nodiscard]] virtual bool mayPlaceBlock(
        const HeldItemInput& item,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing face,
        const Vec3& worldClickPos) const = 0;

    virtual bool computePlacedBlockStateAndBoxes(
        const HeldItemInput& item,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing face,
        const Vec3& subClickPos,
        std::string& outFinalBlockId,
        uint16_t& outStateData,
        std::vector<BlockStateProperty>& outProps,
        std::vector<AABB>& outWorldBoxes,
        std::vector<OrientedBox>& outOrientedBoxes) const = 0;
};

class PlacementResolver {
public:
    [[nodiscard]] static Vec3 lookDirectionFromAngles(float pitchDeg, float yawDeg) noexcept;

    [[nodiscard]] static RaycastHitInput raycastWorld(
        const Vec3& origin,
        const Vec3& direction,
        float maxReachBlocks,
        const IWorldQuery& world);

    [[nodiscard]] static PlacementPreviewState resolve(
        const PlayerContextInput& ctx,
        const IWorldQuery& world,
        const ModConfig& cfg);
};

} // namespace placepreview
