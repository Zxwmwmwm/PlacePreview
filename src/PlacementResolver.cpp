#include "buildghost/PlacementResolver.hpp"

#include "buildghost/MaterialPalette.hpp"
#include "buildghost/StateInspector.hpp"

#include <algorithm>

namespace buildghost {

namespace {

std::string resolvePlaceableBlockName(const HeldItemInput& item, Facing hitFace) {
    if (!item.valid || item.count <= 0) {
        return {};
    }
    if (!item.directBlockIdentifier.empty()) {
        return item.directBlockIdentifier;
    }
    return StateInspector::resolveSpecialItemBlockName(item.itemIdentifier, hitFace);
}

Vec3 computeSubClickOffset(const BlockPos& referenceCell, const Vec3& hitWorldPos, Facing hitFace) noexcept {
    Vec3 sub{
        hitWorldPos.x - static_cast<float>(referenceCell.x),
        hitWorldPos.y - static_cast<float>(referenceCell.y),
        hitWorldPos.z - static_cast<float>(referenceCell.z)
    };
    sub.x = std::clamp(sub.x, 0.0f, 1.0f);
    sub.y = std::clamp(sub.y, 0.0f, 1.0f);
    sub.z = std::clamp(sub.z, 0.0f, 1.0f);
    (void)hitFace;
    return sub;
}

PlacementPreviewState tryResolveSingleHand(
    const HeldItemInput& item,
    bool isOffhand,
    const PlayerContextInput& ctx,
    const IWorldQuery& world,
    const ModConfig& cfg) {

    PlacementPreviewState invalid{};
    if (!item.valid || item.count <= 0) {
        return invalid;
    }

    // 1. Determine active raycast target: liquid surface (for Waterlily / Frogspawn) or solid block hit.
    const std::string probeBlockId = resolvePlaceableBlockName(
        item, ctx.raycast.hasBlockHit ? ctx.raycast.hitFace : Facing::Up);
    if (probeBlockId.empty()) {
        return invalid;
    }

    const bool waterSurfaceItem = StateInspector::isWaterSurfaceBlock(probeBlockId);

    BlockPos clickedBlockPos{};
    Facing clickedFace = Facing::Up;
    Vec3 hitWorldPos{};

    if (waterSurfaceItem && ctx.raycast.hasLiquidHit) {
        clickedBlockPos = ctx.raycast.liquidPos;
        clickedFace = Facing::Up;
        hitWorldPos = ctx.raycast.liquidWorldPos;
    } else if (ctx.raycast.hasBlockHit) {
        clickedBlockPos = ctx.raycast.blockPos;
        clickedFace = ctx.raycast.hitFace;
        hitWorldPos = ctx.raycast.hitWorldPos;
    } else {
        // Immediate disappearance when pointing at sky, out of reach, or holding onto an entity.
        return invalid;
    }

    // Re-evaluate special item name with the actual hit face (e.g., Sign on bottom ceiling face is invalid).
    const std::string initialBlockId = resolvePlaceableBlockName(item, clickedFace);
    if (initialBlockId.empty()) {
        return invalid;
    }

    // Optional interactive-block guard when player is NOT sneaking.
    if (cfg.hideOnInteractiveWithoutSneak && !ctx.isSneaking) {
        const std::string targetId = world.getBlockIdentifier(clickedBlockPos);
        if (StateInspector::isInteractiveBlock(targetId)) {
            return invalid;
        }
    }

    // Adventure-mode CanPlaceOn component check on the clicked target block.
    if (ctx.isAdventureMode && !world.checkAdventureCanPlaceOn(item, clickedBlockPos)) {
        return invalid;
    }

    // 2. Special 2-block Door placement validation & exact hinge/orientation preview.
    if (StateInspector::isDoorBlock(initialBlockId)) {
        // Doors in Minecraft can ONLY be placed on top of a supporting block (clickedFace == Up,
        // or replacing a replaceable block at clickedBlockPos whose lower neighbor supports it).
        BlockPos lowerPos = clickedBlockPos;
        if (!world.isReplaceableBlock(clickedBlockPos)) {
            if (clickedFace != Facing::Up) {
                return invalid;
            }
            lowerPos = clickedBlockPos.offset(0, 1, 0);
        }
        const BlockPos upperPos = lowerPos.offset(0, 1, 0);
        const BlockPos belowPos = lowerPos.offset(0, -1, 0);

        if (lowerPos.y < world.minBuildHeight() || upperPos.y > world.maxBuildHeight()) {
            return invalid;
        }
        if (world.isAirBlock(belowPos) || world.isReplaceableBlock(belowPos)) {
            return invalid;
        }
        if (!world.isReplaceableBlock(lowerPos) || !world.isReplaceableBlock(upperPos)) {
            return invalid;
        }
        if (!world.mayPlaceBlock(item, initialBlockId, lowerPos, Facing::Up, hitWorldPos)) {
            return invalid;
        }

        const Vec3 subClick = computeSubClickOffset(clickedBlockPos, hitWorldPos, clickedFace);
        const Facing lookHoriz = yawToPlayerLookHorizontal(ctx.yawDeg);
        const OrientationSummary orient = StateInspector::decodeOrientation(
            initialBlockId, {}, clickedFace, subClick, ctx.pitchDeg, ctx.yawDeg);

        auto doorBoxes = StateInspector::buildDoorWorldBoxes(lowerPos, lookHoriz, orient.hingeRight);
        for (const auto& box : doorBoxes) {
            if (ctx.playerAabb.volume() > 0.0f && box.intersects(ctx.playerAabb)) {
                return invalid;
            }
        }

        PlacementPreviewState out{};
        out.valid = true;
        out.primaryPos = lowerPos;
        out.hasSecondaryCell = true;
        out.secondaryPos = upperPos;
        out.hitFace = clickedFace;
        out.subClick = subClick;
        out.orientationKind = OrientationKind::Horizontal;
        out.facing = lookHoriz;
        out.hingeRight = orient.hingeRight;
        out.isMultiBlock = true;
        out.isOffhand = isOffhand;
        out.blockIdentifier = initialBlockId;
        out.auxData = item.auxValue;
        out.materialColor = MaterialPalette::colorForBlock(initialBlockId, item.auxValue);
        out.worldBoxes = std::move(doorBoxes);
        return out;
    }

    // 3. Special 2-block Bed placement validation & exact foot + head + pillow preview.
    if (StateInspector::isBedBlock(initialBlockId)) {
        if (clickedFace != Facing::Up && !world.isReplaceableBlock(clickedBlockPos)) {
            return invalid;
        }
        BlockPos footPos = clickedBlockPos;
        if (!world.isReplaceableBlock(clickedBlockPos)) {
            footPos = clickedBlockPos.offset(0, 1, 0);
        }
        const Facing headDir = yawToPlayerLookHorizontal(ctx.yawDeg);
        const BlockPos step = facingStep(headDir);
        const BlockPos headPos = footPos.offset(step.x, step.y, step.z);

        if (footPos.y < world.minBuildHeight() || footPos.y > world.maxBuildHeight()) {
            return invalid;
        }
        // Both foot AND head cells must be unobstructed replaceable cells with solid support below!
        if (!world.isReplaceableBlock(footPos) || !world.isReplaceableBlock(headPos)) {
            return invalid;
        }
        const BlockPos belowFoot = footPos.offset(0, -1, 0);
        const BlockPos belowHead = headPos.offset(0, -1, 0);
        if (world.isAirBlock(belowFoot) || world.isReplaceableBlock(belowFoot) ||
            world.isAirBlock(belowHead) || world.isReplaceableBlock(belowHead)) {
            return invalid;
        }
        if (!world.mayPlaceBlock(item, initialBlockId, footPos, Facing::Up, hitWorldPos)) {
            return invalid;
        }

        auto bedBoxes = StateInspector::buildBedWorldBoxes(footPos, headDir);
        for (const auto& box : bedBoxes) {
            if (ctx.playerAabb.volume() > 0.0f && box.intersects(ctx.playerAabb)) {
                return invalid;
            }
        }

        PlacementPreviewState out{};
        out.valid = true;
        out.primaryPos = footPos;
        out.hasSecondaryCell = true;
        out.secondaryPos = headPos;
        out.hitFace = clickedFace;
        out.subClick = computeSubClickOffset(clickedBlockPos, hitWorldPos, clickedFace);
        out.orientationKind = OrientationKind::Horizontal;
        out.facing = headDir;
        out.isMultiBlock = true;
        out.isOffhand = isOffhand;
        out.blockIdentifier = initialBlockId;
        out.auxData = item.auxValue;
        out.materialColor = MaterialPalette::colorForBlock(initialBlockId, item.auxValue);
        out.worldBoxes = std::move(bedBoxes);
        return out;
    }

    // 4. Water-surface blocks (Waterlily / Frogspawn) ALWAYS place at `liquidPos.above()` (y + 1),
    //    never replacing the water block at `liquidPos` itself!
    Facing resolvedFace = clickedFace;
    BlockPos placePos = clickedBlockPos;
    if (waterSurfaceItem && ctx.raycast.hasLiquidHit) {
        placePos = ctx.raycast.liquidPos.offset(0, 1, 0);
        resolvedFace = Facing::Up;
    } else if (!world.calculatePlacePos(item, resolvedFace, placePos)) {
        return invalid;
    }

    if (placePos.y < world.minBuildHeight() || placePos.y > world.maxBuildHeight()) {
        return invalid;
    }

    // Detect whether an existing half-slab is being merged in-place into a double slab!
    const std::string existingIdAtPlace = world.getBlockIdentifier(placePos);
    const bool mergingSlabInPlace =
        StateInspector::isSlabBlock(initialBlockId) &&
        !existingIdAtPlace.empty() &&
        !world.isAirBlock(placePos) &&
        StateInspector::isSlabBlock(existingIdAtPlace);

    if (!world.mayPlaceBlock(item, initialBlockId, placePos, resolvedFace, hitWorldPos)) {
        return invalid;
    }

    // Compute [0..1]^3 sub-block click coordinates exactly as BlockItem::_useOn passes to getPlacementBlock!
    const Vec3 subClick = computeSubClickOffset(placePos, hitWorldPos, resolvedFace);

    std::string finalBlockId = initialBlockId;
    uint16_t stateData = 0;
    std::vector<BlockStateProperty> props;
    std::vector<AABB> worldBoxes;

    const bool gotNativePlacement = world.computePlacedBlockStateAndBoxes(
        item, initialBlockId, placePos, resolvedFace, subClick,
        finalBlockId, stateData, props, worldBoxes);

    OrientationSummary orient = StateInspector::decodeOrientation(
        finalBlockId.empty() ? initialBlockId : finalBlockId,
        props,
        resolvedFace,
        subClick,
        ctx.pitchDeg,
        ctx.yawDeg);

    if (!gotNativePlacement || worldBoxes.empty()) {
        worldBoxes = StateInspector::buildFallbackBlockBoxes(placePos, initialBlockId, orient);
    }

    // If merging a matching single half-slab into an existing half-slab, isolate ONLY the newly added half!
    bool hasMergeOutline = false;
    AABB mergeOutline{};
    if (mergingSlabInPlace || StateInspector::isDoubleSlabBlock(finalBlockId)) {
        const auto existingBoxes = world.getExistingBlockWorldBoxes(placePos);
        if (!existingBoxes.empty()) {
            worldBoxes = StateInspector::computeSlabMergeDiff(placePos, existingBoxes);
            orient.kind = OrientationKind::SlabHalf;
            orient.topHalf = (worldBoxes.front().min.y > static_cast<float>(placePos.y) + 0.25f);
            hasMergeOutline = true;
            mergeOutline = AABB::unitAt(placePos);
        }
    }

    // Explicit secondary safety check against player collision box (unless non-colliding visual block).
    if (ctx.playerAabb.volume() > 0.0f && !StateInspector::isWaterSurfaceBlock(finalBlockId)) {
        for (const auto& box : worldBoxes) {
            if (box.volume() > 0.02f && box.intersects(ctx.playerAabb)) {
                return invalid;
            }
        }
    }

    PlacementPreviewState out{};
    out.valid = true;
    out.primaryPos = placePos;
    out.hitFace = resolvedFace;
    out.subClick = subClick;
    out.orientationKind = orient.kind;
    out.pillarAxis = orient.pillarAxis;
    out.facing = orient.facing;
    out.topHalf = orient.topHalf;
    out.hingeRight = orient.hingeRight;
    out.isSlabMerge = mergingSlabInPlace || hasMergeOutline;
    out.isOffhand = isOffhand;
    out.blockIdentifier = finalBlockId.empty() ? initialBlockId : finalBlockId;
    out.stateData = stateData;
    out.auxData = item.auxValue;
    out.materialColor = MaterialPalette::colorForBlock(out.blockIdentifier, item.auxValue);
    out.worldBoxes = std::move(worldBoxes);
    out.hasMergeOutlineBox = hasMergeOutline;
    out.mergeOutlineBox = mergeOutline;
    return out;
}

} // namespace

PlacementPreviewState PlacementResolver::resolve(
    const PlayerContextInput& ctx,
    const IWorldQuery& world,
    const ModConfig& cfg) {

    if (!cfg.enabled || !ctx.inWorld || ctx.isSpectator) {
        return PlacementPreviewState{};
    }

    // Try primary mainhand item first.
    if (ctx.mainhand.valid && ctx.mainhand.count > 0) {
        const std::string mainBlock = resolvePlaceableBlockName(
            ctx.mainhand, ctx.raycast.hasBlockHit ? ctx.raycast.hitFace : Facing::Up);
        if (!mainBlock.empty()) {
            return tryResolveSingleHand(ctx.mainhand, /*isOffhand=*/false, ctx, world, cfg);
        }
    }

    // Optional seamless fallback to offhand block (e.g., holding Pickaxe + Torches/Blocks in offhand!).
    if (cfg.enableOffhandPreview && ctx.offhand.valid && ctx.offhand.count > 0) {
        return tryResolveSingleHand(ctx.offhand, /*isOffhand=*/true, ctx, world, cfg);
    }

    return PlacementPreviewState{};
}

} // namespace buildghost
