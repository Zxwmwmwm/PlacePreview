#include "placepreview/PlacementResolver.hpp"

#include "placepreview/MaterialPalette.hpp"
#include "placepreview/StateInspector.hpp"

#include <algorithm>

namespace placepreview {

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

Vec3 computeSubClickOffset(const BlockPos& referenceCell, const Vec3& hitWorldPos, Facing /*hitFace*/) noexcept {
    Vec3 sub{
        hitWorldPos.x - static_cast<float>(referenceCell.x),
        hitWorldPos.y - static_cast<float>(referenceCell.y),
        hitWorldPos.z - static_cast<float>(referenceCell.z)
    };
    sub.x = std::clamp(sub.x, 0.0f, 1.0f);
    sub.y = std::clamp(sub.y, 0.0f, 1.0f);
    sub.z = std::clamp(sub.z, 0.0f, 1.0f);
    return sub;
}

bool rayIntersectAabb(
    const Vec3& origin,
    const Vec3& dir,
    const AABB& rawBox,
    float maxDist,
    float& outT,
    Facing& outFace) noexcept {

    const AABB box = rawBox.normalized();
    float tNear = -1e30f;
    float tFar  =  1e30f;
    Facing enterFace = Facing::Up;

    const float orig[3]  = {origin.x, origin.y, origin.z};
    const float d[3]     = {dir.x, dir.y, dir.z};
    const float bmin[3]  = {box.min.x, box.min.y, box.min.z};
    const float bmax[3]  = {box.max.x, box.max.y, box.max.z};
    const Facing negF[3] = {Facing::West, Facing::Down, Facing::North};
    const Facing posF[3] = {Facing::East, Facing::Up,   Facing::South};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(d[axis]) < 1e-7f) {
            if (orig[axis] < bmin[axis] || orig[axis] > bmax[axis]) {
                return false;
            }
        } else {
            const float inv = 1.0f / d[axis];
            float t0 = (bmin[axis] - orig[axis]) * inv;
            float t1 = (bmax[axis] - orig[axis]) * inv;
            Facing axisFace = negF[axis];
            if (d[axis] < 0.0f) {
                std::swap(t0, t1);
                axisFace = posF[axis];
            }
            if (t0 > tNear) {
                tNear = t0;
                enterFace = axisFace;
            }
            if (t1 < tFar) {
                tFar = t1;
            }
            if (tNear > tFar || tFar < 0.0f || tNear > maxDist) {
                return false;
            }
        }
    }

    if (tNear < -1e-4f) {
        return false;
    }
    outT = std::max(0.0f, tNear);
    outFace = enterFace;
    return outT <= maxDist;
}

bool isLiquidName(std::string_view stripped) noexcept {
    return stripped == "water" || stripped == "flowing_water" ||
           stripped == "lava" || stripped == "flowing_lava";
}

} // namespace

Vec3 PlacementResolver::lookDirectionFromAngles(float pitchDeg, float yawDeg) noexcept {
    const float pitchRad = pitchDeg * kDegToRad;
    const float yawRad   = yawDeg * kDegToRad;
    const float cosPitch = std::cos(pitchRad);
    return Vec3{
        -std::sin(yawRad) * cosPitch,
        -std::sin(pitchRad),
         std::cos(yawRad) * cosPitch
    };
}

RaycastHitInput PlacementResolver::raycastWorld(
    const Vec3& origin,
    const Vec3& direction,
    float maxReachBlocks,
    const IWorldQuery& world) {

    RaycastHitInput result{};
    const float dirLenSq = direction.lengthSq();
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z) ||
        !std::isfinite(dirLenSq) || dirLenSq < 1e-8f || maxReachBlocks <= 0.1f) {
        return result;
    }

    const float invLen = 1.0f / std::sqrt(dirLenSq);
    const Vec3 dir{direction.x * invLen, direction.y * invLen, direction.z * invLen};

    int vx = static_cast<int>(std::floor(origin.x));
    int vy = static_cast<int>(std::floor(origin.y));
    int vz = static_cast<int>(std::floor(origin.z));

    const int stepX = (dir.x >= 0.0f) ? 1 : -1;
    const int stepY = (dir.y >= 0.0f) ? 1 : -1;
    const int stepZ = (dir.z >= 0.0f) ? 1 : -1;

    const float tDeltaX = (std::fabs(dir.x) > 1e-7f) ? (1.0f / std::fabs(dir.x)) : 1e30f;
    const float tDeltaY = (std::fabs(dir.y) > 1e-7f) ? (1.0f / std::fabs(dir.y)) : 1e30f;
    const float tDeltaZ = (std::fabs(dir.z) > 1e-7f) ? (1.0f / std::fabs(dir.z)) : 1e30f;

    float tMaxX = (std::fabs(dir.x) > 1e-7f)
                      ? (((dir.x >= 0.0f ? static_cast<float>(vx + 1) : static_cast<float>(vx)) - origin.x) / dir.x)
                      : 1e30f;
    float tMaxY = (std::fabs(dir.y) > 1e-7f)
                      ? (((dir.y >= 0.0f ? static_cast<float>(vy + 1) : static_cast<float>(vy)) - origin.y) / dir.y)
                      : 1e30f;
    float tMaxZ = (std::fabs(dir.z) > 1e-7f)
                      ? (((dir.z >= 0.0f ? static_cast<float>(vz + 1) : static_cast<float>(vz)) - origin.z) / dir.z)
                      : 1e30f;

    const int minY = world.minBuildHeight();
    const int maxY = world.maxBuildHeight();
    const int maxSteps = std::clamp(static_cast<int>(std::ceil(maxReachBlocks * 3.0f)) + 6, 8, 96);

    float tCurrent = 0.0f;
    for (int step = 0; step < maxSteps && tCurrent <= maxReachBlocks; ++step) {
        if (vy >= minY && vy <= maxY) {
            const BlockPos cell{vx, vy, vz};
            const std::string blockId = world.getBlockIdentifier(cell);
            const std::string_view stripped = StateInspector::stripNamespace(blockId);

            if (!stripped.empty() &&
                stripped != "air" &&
                stripped != "cave_air" &&
                stripped != "void_air" &&
                stripped != "light_block" &&
                stripped != "structure_void") {

                if (isLiquidName(stripped)) {
                    if (!result.hasLiquidHit) {
                        const std::string aboveId = world.getBlockIdentifier(cell.offset(0, 1, 0));
                        const float topH = isLiquidName(StateInspector::stripNamespace(aboveId)) ? 1.0f : 0.90f;
                        const AABB liqBox{
                            {static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)},
                            {static_cast<float>(vx + 1), static_cast<float>(vy) + topH, static_cast<float>(vz + 1)}
                        };
                        float tLiq = 0.0f;
                        Facing fLiq = Facing::Up;
                        if (rayIntersectAabb(origin, dir, liqBox, maxReachBlocks, tLiq, fLiq)) {
                            result.hasLiquidHit = true;
                            result.liquidFace = fLiq;
                            result.liquidPos = cell;
                            result.liquidWorldPos = origin + dir * tLiq;
                        }
                    }
                } else {
                    std::vector<AABB> boxes = world.getExistingBlockWorldBoxes(cell);
                    if (boxes.empty()) {
                        if (stripped == "snow_layer" || stripped == "glow_lichen" || stripped == "sculk_vein") {
                            boxes.push_back(AABB{
                                {static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)},
                                {static_cast<float>(vx + 1), static_cast<float>(vy) + 0.125f, static_cast<float>(vz + 1)}
                            });
                        } else {
                            boxes.push_back(AABB{
                                {static_cast<float>(vx) + 0.125f, static_cast<float>(vy), static_cast<float>(vz) + 0.125f},
                                {static_cast<float>(vx) + 0.875f, static_cast<float>(vy) + 0.75f, static_cast<float>(vz) + 0.875f}
                            });
                        }
                    }

                    float bestT = maxReachBlocks + 1.0f;
                    Facing bestFace = Facing::Up;
                    bool hitAny = false;
                    for (const auto& box : boxes) {
                        if (box.isDegenerate()) continue;
                        float tBox = 0.0f;
                        Facing fBox = Facing::Up;
                        if (rayIntersectAabb(origin, dir, box, maxReachBlocks, tBox, fBox) && tBox < bestT) {
                            bestT = tBox;
                            bestFace = fBox;
                            hitAny = true;
                        }
                    }

                    if (hitAny && bestT <= maxReachBlocks) {
                        result.hasBlockHit = true;
                        result.hitFace = bestFace;
                        result.blockPos = cell;
                        result.hitWorldPos = origin + dir * bestT;
                        return result;
                    }
                }
            }
        } else if ((vy < minY && stepY < 0) || (vy > maxY && stepY > 0)) {
            break;
        }

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                vx += stepX;
                tCurrent = tMaxX;
                tMaxX += tDeltaX;
            } else {
                vz += stepZ;
                tCurrent = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                vy += stepY;
                tCurrent = tMaxY;
                tMaxY += tDeltaY;
            } else {
                vz += stepZ;
                tCurrent = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        }
    }

    return result;
}

PlacementPreviewState PlacementResolver::resolve(
    const PlayerContextInput& ctx,
    const IWorldQuery& world,
    const ModConfig& cfg) {

    PlacementPreviewState invalid{};
    if (!cfg.enabled || !ctx.inWorld || ctx.isSpectator) {
        return invalid;
    }

    const HeldItemInput& item = ctx.mainhand;
    if (!item.valid || item.count <= 0) {
        return invalid;
    }

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
        return invalid;
    }

    // Re-evaluate special item block name using actual hitFace (e.g., sign on bottom face -> invalid).
    const std::string initialBlockId = resolvePlaceableBlockName(item, clickedFace);
    if (initialBlockId.empty()) {
        return invalid;
    }

    if (cfg.hideOnInteractiveWithoutSneak && !ctx.isSneaking) {
        const std::string targetId = world.getBlockIdentifier(clickedBlockPos);
        if (StateInspector::isInteractiveBlock(targetId)) {
            return invalid;
        }
    }

    if (ctx.isAdventureMode && !world.checkAdventureCanPlaceOn(item, clickedBlockPos)) {
        return invalid;
    }

    // 1. Two-block Door placement validation & exact hinge/orientation preview.
    if (StateInspector::isDoorBlock(initialBlockId)) {
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
            initialBlockId, {}, clickedFace, subClick, ctx.pitchDeg, ctx.yawDeg, ctx.isSneaking);

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
        out.blockIdentifier = initialBlockId;
        out.auxData = item.auxValue;
        out.materialColor = MaterialPalette::colorForBlock(initialBlockId, item.auxValue);
        out.worldBoxes = std::move(doorBoxes);
        return out;
    }

    // 2. Two-block Bed placement validation & exact foot + head + pillow preview.
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
        out.blockIdentifier = initialBlockId;
        out.auxData = item.auxValue;
        out.materialColor = MaterialPalette::colorForBlock(initialBlockId, item.auxValue);
        out.worldBoxes = std::move(bedBoxes);
        return out;
    }

    // 3. Water-surface blocks or standard single-cell placement resolution.
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

    // Re-evaluate special item block name if `calculatePlacePos` adjusted `resolvedFace`
    // (for example, clicking the side of short_grass replaces on the floor at Facing::Up!).
    const std::string placedSpecialId = resolvePlaceableBlockName(item, resolvedFace);
    if (placedSpecialId.empty()) {
        return invalid;
    }

    const std::string existingIdAtPlace = world.getBlockIdentifier(placePos);
    const bool mergingSlabInPlace =
        StateInspector::isSlabBlock(placedSpecialId) &&
        !existingIdAtPlace.empty() &&
        !world.isAirBlock(placePos) &&
        StateInspector::isSlabBlock(existingIdAtPlace);

    if (!world.mayPlaceBlock(item, placedSpecialId, placePos, resolvedFace, hitWorldPos)) {
        return invalid;
    }

    {
        const BlockPos belowPos = placePos.offset(0, -1, 0);
        const BlockPos abovePos = placePos.offset(0, 1, 0);
        const BlockPos oppStep = facingStep(oppositeFacing(resolvedFace));
        const BlockPos behindPos = placePos.offset(oppStep.x, oppStep.y, oppStep.z);
        if (!StateInspector::validateBlockSupport(
                placedSpecialId,
                placePos,
                resolvedFace,
                world.getBlockIdentifier(belowPos),
                world.getBlockIdentifier(abovePos),
                world.getBlockIdentifier(behindPos))) {
            return invalid;
        }
    }

    const Vec3 subClick = computeSubClickOffset(placePos, hitWorldPos, resolvedFace);

    std::string finalBlockId = placedSpecialId;
    uint16_t stateData = 0;
    std::vector<BlockStateProperty> props;
    std::vector<AABB> worldBoxes;
    std::vector<OrientedBox> orientedBoxes;

    const bool gotNativePlacement = world.computePlacedBlockStateAndBoxes(
        item, placedSpecialId, placePos, resolvedFace, subClick,
        finalBlockId, stateData, props, worldBoxes, orientedBoxes);

    const std::string effectiveId = finalBlockId.empty() ? placedSpecialId : finalBlockId;
    OrientationSummary orient = StateInspector::decodeOrientation(
        effectiveId,
        props,
        resolvedFace,
        subClick,
        ctx.pitchDeg,
        ctx.yawDeg,
        ctx.isSneaking);

    if (orientedBoxes.empty()) {
        orientedBoxes = StateInspector::buildSpecialOrientedBoxes(placePos, effectiveId, orient);
    }

    if (!gotNativePlacement || worldBoxes.empty()) {
        if (StateInspector::isFenceOrWallOrPane(effectiveId)) {
            auto connectsTo = [&](const BlockPos& np) -> bool {
                const std::string nid = world.getBlockIdentifier(np);
                if (nid.empty()) return false;
                if (StateInspector::isFenceOrWallOrPane(nid)) return true;
                return !StateInspector::isNonSolidSupportBlock(nid) &&
                       !StateInspector::isSlabBlock(nid) &&
                       !StateInspector::isStairBlock(nid) &&
                       !StateInspector::isDoorBlock(nid) &&
                       !StateInspector::isBedBlock(nid);
            };
            worldBoxes = StateInspector::buildConnectedPostWorldBoxes(
                placePos,
                effectiveId,
                connectsTo(placePos.offset(0, 0, -1)),
                connectsTo(placePos.offset(0, 0,  1)),
                connectsTo(placePos.offset(-1, 0, 0)),
                connectsTo(placePos.offset(1, 0,  0)));
        } else {
            worldBoxes = StateInspector::buildFallbackBlockBoxes(placePos, effectiveId, orient);
        }
    }

    bool hasMergeOutline = false;
    AABB mergeOutline{};
    if (mergingSlabInPlace || StateInspector::isDoubleSlabBlock(effectiveId)) {
        const auto existingBoxes = world.getExistingBlockWorldBoxes(placePos);
        if (!existingBoxes.empty()) {
            worldBoxes = StateInspector::computeSlabMergeDiff(placePos, existingBoxes);
            orientedBoxes.clear();
            orient.kind = OrientationKind::SlabHalf;
            orient.topHalf = (worldBoxes.front().min.y > static_cast<float>(placePos.y) + 0.25f);
            hasMergeOutline = true;
            mergeOutline = AABB::unitAt(placePos);
        }
    }

    // Player collision guard ONLY for blocks with solid placement collision!
    // Non-colliding blocks (standing signs, torches, carpets, buttons, pressure plates, lily pads)
    // can legitimately be placed right next to / intersecting the player's AABB just like vanilla!
    if (ctx.playerAabb.volume() > 0.0f && !StateInspector::hasNonCollidingPlacement(effectiveId)) {
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
    out.rotation16 = orient.rotation16;
    out.yawRadians = orient.yawRadians;
    out.topHalf = orient.topHalf;
    out.hingeRight = orient.hingeRight;
    out.isSlabMerge = mergingSlabInPlace || hasMergeOutline;
    out.blockIdentifier = effectiveId;
    out.stateData = stateData;
    out.auxData = item.auxValue;
    out.materialColor = MaterialPalette::colorForBlock(out.blockIdentifier, item.auxValue);
    out.worldBoxes = std::move(worldBoxes);
    out.orientedBoxes = std::move(orientedBoxes);
    out.hasMergeOutlineBox = hasMergeOutline;
    out.mergeOutlineBox = mergeOutline;
    return out;
}

} // namespace placepreview
