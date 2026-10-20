#pragma once

#include "buildghost/Config.hpp"
#include "buildghost/Types.hpp"

#include <string>
#include <vector>

namespace buildghost {

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
    HeldItemInput offhand{};
    RaycastHitInput raycast{};
};

// Clean world/engine query seam allowing 100% deterministic host ASan+UBSan testing
// AND direct 1-to-1 binding against live Minecraft Bedrock 1.26.52.03 engine methods.
class IWorldQuery {
public:
    virtual ~IWorldQuery() = default;

    // World height limits (e.g. Overworld -64..319, Nether/End 0..255).
    [[nodiscard]] virtual int minBuildHeight() const noexcept = 0;
    [[nodiscard]] virtual int maxBuildHeight() const noexcept = 0;

    // Existing block inspection at world coordinate `pos`.
    [[nodiscard]] virtual std::string getBlockIdentifier(const BlockPos& pos) const = 0;
    [[nodiscard]] virtual bool isAirBlock(const BlockPos& pos) const = 0;
    [[nodiscard]] virtual bool isReplaceableBlock(const BlockPos& pos) const = 0;
    [[nodiscard]] virtual std::vector<AABB> getExistingBlockWorldBoxes(const BlockPos& pos) const = 0;

    // Adventure-mode CanPlaceOn component check on `targetBlockPos`.
    [[nodiscard]] virtual bool checkAdventureCanPlaceOn(
        const HeldItemInput& item,
        const BlockPos& targetBlockPos) const = 0;

    // Resolves exact placement target position via Item::_calculatePlacePos (handling replaceable
    // blocks, matching half-slab in-place merges, snow/candle stacking, and face stepping).
    [[nodiscard]] virtual bool calculatePlacePos(
        const HeldItemInput& item,
        Facing& inOutFace,
        BlockPos& inOutPos) const = 0;

    // Validates placement rules + entity/player collision via BlockSource::mayPlace.
    [[nodiscard]] virtual bool mayPlaceBlock(
        const HeldItemInput& item,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing face,
        const Vec3& worldClickPos) const = 0;

    // Evaluates exact placed Block state (Block::getPlacementBlock + BlockPlayerPlacingEvent traits),
    // extracting named BlockState properties AND exact multi-AABB world geometry at `placePos`.
    virtual bool computePlacedBlockStateAndBoxes(
        const HeldItemInput& item,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing face,
        const Vec3& subClickPos,
        std::string& outFinalBlockId,
        uint16_t& outStateData,
        std::vector<BlockStateProperty>& outProps,
        std::vector<AABB>& outWorldBoxes) const = 0;
};

class PlacementResolver {
public:
    // Evaluates the complete placement preview state for the current frame.
    // Returns PlacementPreviewState{.valid = false} IMMEDIATELY whenever placement is invalid,
    // out of reach, obstructed, or holding a non-placeable item, and updates IMMEDIATELY when
    // any block coordinate, hit face, sub-click offset, player angle, or held slot changes.
    [[nodiscard]] static PlacementPreviewState resolve(
        const PlayerContextInput& ctx,
        const IWorldQuery& world,
        const ModConfig& cfg);
};

} // namespace buildghost
