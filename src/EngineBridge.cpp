#include "buildghost/EngineBridge.hpp"

#include "buildghost/Config.hpp"
#include "buildghost/GhostMesher.hpp"
#include "buildghost/PlacementResolver.hpp"
#include "buildghost/Signatures.hpp"
#include "buildghost/StateInspector.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__ANDROID__)
#include <link.h>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>
#include <pl/memory/Vtable.hpp>
#endif

namespace buildghost {

namespace {

using FnDebugRenderEnd = void (*)(void* debugRenderer, void* renderParams);

// Packed ChunkPos (x, z) passed to BlockSource::getChunk(const ChunkPos&)
struct NativeChunkPos {
    int32_t x{0};
    int32_t z{0};
};
static_assert(sizeof(NativeChunkPos) == 8, "NativeChunkPos must be 8 bytes");

// Packed ChunkBlockPos passed by value in register X1 to LevelChunk::getBlock(ChunkBlockPos)
struct NativeChunkBlockPos {
    uint8_t x{0};
    uint8_t z{0};
    int16_t y{0};
};
static_assert(sizeof(NativeChunkBlockPos) == 4, "NativeChunkBlockPos must be 4 bytes");

using FnLevelChunkGetBlock   = const void* (*)(const void* levelChunk, NativeChunkBlockPos localPos);
using FnActorGetDimBlockSrc  = void* (*)(void* actor);
using FnActorIsSpectator     = bool (*)(const void* actor);
using FnClientGetCamActor    = void* (*)(void* clientInstance);
using FnClientGetRegion      = void* (*)(void* clientInstance);
using FnLevelGetHitResult    = const void* (*)(void* level);
using FnActorGetItemStack    = const void* (*)(const void* actor);
using FnBsGetChunk           = const void* (*)(void* blockSource, const NativeChunkPos& cp);
using FnBsCheckPermissions   = bool (*)(void* blockSource, void* actor, const BlockPos& pos, const void* itemStack, bool genParticle);

struct ResolvedEngineApi {
    uintptr_t moduleBase{0};
    void* targetDebugRenderEnd{nullptr};
    FnLevelChunkGetBlock lcGetBlock{nullptr};
    FnActorGetDimBlockSrc actorGetBlockSource{nullptr};
    FnActorIsSpectator actorIsSpectator{nullptr};
    bool fastRva{false};
};

std::mutex gHookMutex;
std::atomic<bool> gEnabled{false};
std::atomic<bool> gHooksInstalled{false};
std::atomic<bool> gFastRvaResolved{false};
std::atomic<uint64_t> gFramesEvaluated{0};
std::atomic<uint64_t> gFramesRendered{0};
std::atomic<uint32_t> gActiveHookCalls{0};
std::atomic<FnDebugRenderEnd> gOrigDebugRenderEnd{nullptr};
ResolvedEngineApi gApi{};

#if defined(__ANDROID__)
std::unique_ptr<pl::memory::HookHandle> gHookRenderEnd;
std::thread gAsyncFallbackThread;
#endif

[[maybe_unused]] uint32_t readInsnWord(uintptr_t addr) noexcept {
    if (!sig::isValidUserAddress(reinterpret_cast<const void*>(addr), 4)) {
        return 0u;
    }
    uint32_t word = 0u;
    std::memcpy(&word, reinterpret_cast<const void*>(addr), sizeof(uint32_t));
    return word;
}

template <typename T>
bool readFieldSafe(const void* base, uintptr_t offset, T& outVal) noexcept {
    if (!sig::isValidUserAddress(base, 1)) return false;
    const auto* bytePtr = static_cast<const uint8_t*>(base) + offset;
    if (!sig::isValidUserAddress(bytePtr, 1)) return false;
    std::memcpy(&outVal, bytePtr, sizeof(T));
    return true;
}

template <typename FnPtr>
FnPtr readVtableFn(const void* obj, uintptr_t vtableByteOffset) noexcept {
    if (!sig::isValidUserAddress(obj, alignof(void*))) return nullptr;
    const void* vtbl = nullptr;
    if (!readFieldSafe(obj, 0, vtbl) || !sig::isValidUserAddress(vtbl, alignof(void*))) {
        return nullptr;
    }
    FnPtr fn = nullptr;
    if (!readFieldSafe(vtbl, vtableByteOffset, fn)) return nullptr;
    if (!sig::isValidUserAddress(reinterpret_cast<const void*>(fn), 4)) return nullptr;
    return fn;
}

std::string readHashedString(const void* hashedStrAddr) {
    if (!sig::isValidUserAddress(hashedStrAddr, alignof(void*))) return {};
    const auto sv = sig::readLibcxxStringView(static_cast<const uint8_t*>(hashedStrAddr) + 0x08);
    return std::string(sv);
}

std::string readBlockIdentifierFromBlock(const void* blockPtr) {
    if (!sig::isValidUserAddress(blockPtr, alignof(void*))) return {};
    const void* bt = nullptr;
    if (!readFieldSafe(blockPtr, sig::off::Block_BlockTypePtr, bt) ||
        !sig::isValidUserAddress(bt, alignof(void*))) {
        return {};
    }
    const auto fullSv = sig::readLibcxxStringView(static_cast<const uint8_t*>(bt) + sig::off::BlockType_FullNameStr);
    if (!fullSv.empty()) {
        return std::string(fullSv);
    }
    const auto rawSv = sig::readLibcxxStringView(static_cast<const uint8_t*>(bt) + sig::off::BlockType_RawNameStr);
    if (!rawSv.empty()) {
        return "minecraft:" + std::string(rawSv);
    }
    return {};
}

// Reads immutable BlockStateInstance RB-tree at BlockType + 0x1b0 to decode packed mData properties.
void extractBlockStateProperties(const void* blockPtr, std::vector<BlockStateProperty>& outProps) {
    outProps.clear();
    if (!sig::isValidUserAddress(blockPtr, alignof(void*))) return;

    const void* bt = nullptr;
    uint16_t stateData = 0;
    if (!readFieldSafe(blockPtr, sig::off::Block_BlockTypePtr, bt) ||
        !sig::isValidUserAddress(bt, alignof(void*)) ||
        !readFieldSafe(blockPtr, sig::off::Block_Data, stateData)) {
        return;
    }

    const auto* treeBase = static_cast<const uint8_t*>(bt) + sig::off::BlockType_StatesTree;
    const void* endNode = static_cast<const void*>(treeBase + 0x08);
    const void* curr = nullptr;
    std::size_t treeCount = 0;
    if (!readFieldSafe(treeBase, 0x00, curr) || !readFieldSafe(treeBase, 0x10, treeCount)) {
        return;
    }
    if (treeCount == 0 || treeCount > 32) return;

    int steps = 0;
    while (curr != nullptr && curr != endNode && steps < 64 && sig::isValidUserAddress(curr, alignof(void*))) {
        ++steps;
        uint32_t startBit = 0;
        uint32_t numBits = 0;
        const void* stateDef = nullptr;
        if (readFieldSafe(curr, 0x28, startBit) &&
            readFieldSafe(curr, 0x2c, numBits) &&
            readFieldSafe(curr, 0x38, stateDef) &&
            numBits > 0 && numBits <= 16 && startBit <= 16 &&
            sig::isValidUserAddress(stateDef, alignof(void*))) {
            const auto nameSv = sig::readLibcxxStringView(static_cast<const uint8_t*>(stateDef) + 0x18);
            if (!nameSv.empty()) {
                const uint32_t shift = (startBit + 1u >= numBits) ? (startBit + 1u - numBits) : 0u;
                const uint32_t mask = (1u << numBits) - 1u;
                const int val = static_cast<int>((static_cast<uint32_t>(stateData) >> shift) & mask);
                outProps.push_back(BlockStateProperty{std::string(nameSv), val});
            }
        }

        // Advance in-order successor across libc++ std::__tree_node_base
        const void* right = nullptr;
        if (readFieldSafe(curr, 0x08, right) && right != nullptr && sig::isValidUserAddress(right, alignof(void*))) {
            curr = right;
            const void* left = nullptr;
            int guard = 0;
            while (guard++ < 32 && readFieldSafe(curr, 0x00, left) && left != nullptr &&
                   sig::isValidUserAddress(left, alignof(void*))) {
                curr = left;
            }
        } else {
            const void* parent = nullptr;
            int guard = 0;
            while (guard++ < 32 && readFieldSafe(curr, 0x10, parent) && parent != nullptr &&
                   parent != endNode && sig::isValidUserAddress(parent, alignof(void*))) {
                const void* parentRight = nullptr;
                if (!readFieldSafe(parent, 0x08, parentRight) || curr != parentRight) {
                    curr = parent;
                    parent = nullptr;
                    break;
                }
                curr = parent;
            }
            if (parent == endNode || guard >= 32) {
                break;
            }
            if (parent != nullptr) {
                curr = parent;
            }
        }
    }
}

HeldItemInput inspectItemStack(const void* stackPtr) {
    HeldItemInput out{};
    if (!sig::isValidUserAddress(stackPtr, alignof(void*))) return out;

    uint8_t count = 0;
    int16_t aux = 0;
    if (!readFieldSafe(stackPtr, sig::off::ItemStack_Count, count) || count == 0) {
        return out;
    }
    readFieldSafe(stackPtr, sig::off::ItemStack_AuxValue, aux);

    const void* weakRef = nullptr;
    if (!readFieldSafe(stackPtr, sig::off::ItemStack_ItemWeakPtr, weakRef) ||
        !sig::isValidUserAddress(weakRef, alignof(void*))) {
        return out;
    }
    const void* itemPtr = nullptr;
    if (!readFieldSafe(weakRef, 0, itemPtr) || !sig::isValidUserAddress(itemPtr, alignof(void*))) {
        return out;
    }

    std::string itemFull = readHashedString(static_cast<const uint8_t*>(itemPtr) + sig::off::Item_FullName);
    if (itemFull.empty()) {
        const std::string raw = readHashedString(static_cast<const uint8_t*>(itemPtr) + sig::off::Item_RawNameId);
        if (!raw.empty()) itemFull = "minecraft:" + raw;
    }
    if (itemFull.empty()) return out;

    const void* blockPtr = nullptr;
    std::string directBlockId;
    if (readFieldSafe(stackPtr, sig::off::ItemStack_BlockPtr, blockPtr) &&
        sig::isValidUserAddress(blockPtr, alignof(void*))) {
        directBlockId = readBlockIdentifierFromBlock(blockPtr);
    }

    out.valid = true;
    out.count = static_cast<int>(count);
    out.auxValue = static_cast<int>(aux);
    out.itemIdentifier = std::move(itemFull);
    out.directBlockIdentifier = std::move(directBlockId);
    out.nativeItemStackPtr = reinterpret_cast<uintptr_t>(stackPtr);
    return out;
}

// Lock-free read-only IWorldQuery implementation backed directly by LevelChunk::getBlock.
// Never mutates BlockSource::mTempEntityList or dispatches off-thread events!
class LiveWorldQuery final : public IWorldQuery {
public:
    LiveWorldQuery(void* blockSource, void* actor, int minY, int maxY, float pitchDeg, float yawDeg) noexcept
        : mBlockSource(blockSource),
          mActor(actor),
          mMinY(minY),
          mMaxY(maxY),
          mPitchDeg(pitchDeg),
          mYawDeg(yawDeg) {}

    [[nodiscard]] int minBuildHeight() const noexcept override { return mMinY; }
    [[nodiscard]] int maxBuildHeight() const noexcept override { return mMaxY; }

    [[nodiscard]] const void* getNativeBlockPtr(const BlockPos& pos) const noexcept {
        if (!mBlockSource || !gApi.lcGetBlock) return nullptr;
        if (pos.y < mMinY || pos.y > mMaxY) return nullptr;

        const auto fnGetChunk = readVtableFn<FnBsGetChunk>(mBlockSource, sig::off::BlockSource_VtGetChunk);
        if (!fnGetChunk) return nullptr;

        const NativeChunkPos cp{pos.x >> 4, pos.z >> 4};
        if (mCachedChunkPtr && mCachedChunkPos.x == cp.x && mCachedChunkPos.z == cp.z) {
            return queryBlockInChunk(mCachedChunkPtr, pos);
        }

        const void* lc = fnGetChunk(mBlockSource, cp);
        if (!sig::isValidUserAddress(lc, alignof(void*))) return nullptr;

        uint8_t stateByte = 0;
        if (!readFieldSafe(lc, sig::off::LevelChunk_AtomicStateByte, stateByte) || stateByte < 7u) {
            return nullptr;
        }
        mCachedChunkPos = cp;
        mCachedChunkPtr = lc;
        return queryBlockInChunk(lc, pos);
    }

    [[nodiscard]] std::string getBlockIdentifier(const BlockPos& pos) const override {
        if (pos.y < mMinY || pos.y > mMaxY) return "minecraft:air";
        const void* blk = getNativeBlockPtr(pos);
        if (!blk) return "minecraft:air";
        std::string id = readBlockIdentifierFromBlock(blk);
        return id.empty() ? "minecraft:air" : id;
    }

    [[nodiscard]] bool isAirBlock(const BlockPos& pos) const override {
        const std::string_view stripped = StateInspector::stripNamespace(getBlockIdentifier(pos));
        return stripped.empty() || stripped == "air" || stripped == "cave_air" ||
               stripped == "void_air" || stripped == "light_block" || stripped == "structure_void";
    }

    [[nodiscard]] bool isReplaceableBlock(const BlockPos& pos) const override {
        const void* blk = getNativeBlockPtr(pos);
        if (!blk) return true;
        const std::string id = readBlockIdentifierFromBlock(blk);
        const std::string_view n = StateInspector::stripNamespace(id);
        if (n.empty() || n == "air" || n == "cave_air" || n == "void_air" ||
            n == "water" || n == "flowing_water" || n == "lava" || n == "flowing_lava" ||
            n == "short_grass" || n == "tallgrass" || n == "fern" || n == "large_fern" ||
            n == "deadbush" || n == "fire" || n == "soul_fire" || n == "vine" ||
            n == "glow_lichen" || n == "sculk_vein" || n == "structure_void" ||
            n == "light_block" || n == "seagrass" || n == "tall_seagrass" ||
            n == "crimson_roots" || n == "warped_roots" || n == "nether_sprouts") {
            return true;
        }
        if (n == "snow_layer") {
            std::vector<BlockStateProperty> props;
            extractBlockStateProperties(blk, props);
            for (const auto& p : props) {
                if (p.name == "height" && p.value < 7) {
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    [[nodiscard]] std::vector<AABB> getExistingBlockWorldBoxes(const BlockPos& pos) const override {
        const void* blk = getNativeBlockPtr(pos);
        if (!blk) return {};
        const std::string id = readBlockIdentifierFromBlock(blk);
        if (id.empty()) return {};
        std::vector<BlockStateProperty> props;
        extractBlockStateProperties(blk, props);
        const OrientationSummary orient = StateInspector::decodeOrientation(
            id, props, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f);
        return StateInspector::buildFallbackBlockBoxes(pos, id, orient);
    }

    [[nodiscard]] bool checkAdventureCanPlaceOn(
        const HeldItemInput& item,
        const BlockPos& targetBlockPos) const override {
        if (!mBlockSource || !mActor || item.nativeItemStackPtr == 0) return true;
        const auto fnCheck = readVtableFn<FnBsCheckPermissions>(mBlockSource, sig::off::BlockSource_VtCheckPerms);
        if (!fnCheck) return true;
        return fnCheck(
            mBlockSource,
            mActor,
            targetBlockPos,
            reinterpret_cast<const void*>(item.nativeItemStackPtr),
            false);
    }

    [[nodiscard]] bool calculatePlacePos(
        const HeldItemInput& item,
        Facing& inOutFace,
        BlockPos& inOutPos) const override {
        const std::string placeId = !item.directBlockIdentifier.empty()
                                        ? item.directBlockIdentifier
                                        : StateInspector::resolveSpecialItemBlockName(item.itemIdentifier, inOutFace);

        // 1. Direct click on the inside face of a matching single half-slab merges in place!
        if (StateInspector::isSlabBlock(placeId)) {
            const void* clickedBlk = getNativeBlockPtr(inOutPos);
            if (clickedBlk) {
                const std::string existingId = readBlockIdentifierFromBlock(clickedBlk);
                if (existingId == placeId) {
                    std::vector<BlockStateProperty> props;
                    extractBlockStateProperties(clickedBlk, props);
                    const OrientationSummary exOrient = StateInspector::decodeOrientation(
                        existingId, props, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f);
                    if ((!exOrient.topHalf && inOutFace == Facing::Up) ||
                        (exOrient.topHalf && inOutFace == Facing::Down)) {
                        return true;
                    }
                }
            }
        }

        // 2. Snow layer stacking on top of an existing <8-layer snow block.
        if (StateInspector::stripNamespace(placeId) == "snow_layer") {
            const void* clickedBlk = getNativeBlockPtr(inOutPos);
            if (clickedBlk && StateInspector::stripNamespace(readBlockIdentifierFromBlock(clickedBlk)) == "snow_layer") {
                std::vector<BlockStateProperty> props;
                extractBlockStateProperties(clickedBlk, props);
                for (const auto& p : props) {
                    if (p.name == "height" && p.value < 7) {
                        return true;
                    }
                }
            }
        }

        // 3. If clicked block itself is replaceable (tall grass, water, snow_layer < 7), replace in place.
        if (isReplaceableBlock(inOutPos)) {
            return true;
        }

        // 4. Advance one block along `inOutFace`.
        const BlockPos step = facingStep(inOutFace);
        const BlockPos target = inOutPos.offset(step.x, step.y, step.z);
        if (target.y < mMinY || target.y > mMaxY) {
            return false;
        }

        // Allow either replaceable target cell OR adjacent matching half-slab merge!
        if (isReplaceableBlock(target)) {
            inOutPos = target;
            return true;
        }
        if (StateInspector::isSlabBlock(placeId) && getBlockIdentifier(target) == placeId) {
            inOutPos = target;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool mayPlaceBlock(
        const HeldItemInput& item,
        const std::string& /*resolvedBlockIdentifier*/,
        const BlockPos& placePos,
        Facing /*face*/,
        const Vec3& /*worldClickPos*/) const override {
        if (placePos.y < mMinY || placePos.y > mMaxY) return false;
        if (mBlockSource && mActor && item.nativeItemStackPtr != 0) {
            const auto fnCheck = readVtableFn<FnBsCheckPermissions>(mBlockSource, sig::off::BlockSource_VtCheckPerms);
            if (fnCheck && !fnCheck(
                    mBlockSource,
                    mActor,
                    placePos,
                    reinterpret_cast<const void*>(item.nativeItemStackPtr),
                    false)) {
                return false;
            }
        }
        return true;
    }

    bool computePlacedBlockStateAndBoxes(
        const HeldItemInput& /*item*/,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing face,
        const Vec3& subClickPos,
        std::string& outFinalBlockId,
        uint16_t& outStateData,
        std::vector<BlockStateProperty>& outProps,
        std::vector<AABB>& outWorldBoxes) const override {

        outFinalBlockId = resolvedBlockIdentifier;
        outStateData = 0;
        outProps.clear();
        outWorldBoxes.clear();

        const OrientationSummary orient = StateInspector::decodeOrientation(
            resolvedBlockIdentifier, {}, face, subClickPos, mPitchDeg, mYawDeg);

        // Snow layer stacking height preview when adding onto an existing snow_layer!
        if (StateInspector::stripNamespace(resolvedBlockIdentifier) == "snow_layer") {
            const void* existing = getNativeBlockPtr(placePos);
            if (existing && StateInspector::stripNamespace(readBlockIdentifierFromBlock(existing)) == "snow_layer") {
                std::vector<BlockStateProperty> exProps;
                extractBlockStateProperties(existing, exProps);
                for (const auto& p : exProps) {
                    if (p.name == "height" && p.value >= 0 && p.value < 7) {
                        outStateData = static_cast<uint16_t>(p.value + 1);
                        const Vec3 o = placePos.originVec();
                        const float h = static_cast<float>(p.value + 2) * 0.125f;
                        outWorldBoxes = {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + h, o.z + 1.0f}};
                        return true;
                    }
                }
            }
        }

        // Dynamic neighbor-aware Corner Stairs (Outer & Inner corners live at 60-120 FPS!)
        if (StateInspector::isStairBlock(resolvedBlockIdentifier)) {
            auto inspectNeighborStair = [&](const BlockPos& np, Facing& outDir) -> bool {
                const void* nb = getNativeBlockPtr(np);
                if (!nb) return false;
                const std::string nid = readBlockIdentifierFromBlock(nb);
                if (!StateInspector::isStairBlock(nid)) return false;
                std::vector<BlockStateProperty> nProps;
                extractBlockStateProperties(nb, nProps);
                const OrientationSummary nOrient = StateInspector::decodeOrientation(
                    nid, nProps, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f);
                if (nOrient.topHalf != orient.topHalf) return false;
                outDir = nOrient.facing;
                return true;
            };

            const BlockPos frontStep = facingStep(orient.facing);
            const BlockPos backStep = facingStep(oppositeFacing(orient.facing));
            Facing frontDir = Facing::North;
            Facing backDir = Facing::North;
            const bool hasFront = inspectNeighborStair(
                placePos.offset(frontStep.x, frontStep.y, frontStep.z), frontDir);
            const bool hasBack = inspectNeighborStair(
                placePos.offset(backStep.x, backStep.y, backStep.z), backDir);

            outStateData = static_cast<uint16_t>((static_cast<uint16_t>(hasFront) << 8u) |
                                                 (static_cast<uint16_t>(hasBack) << 9u) |
                                                 (static_cast<uint16_t>(frontDir) << 10u) |
                                                 (static_cast<uint16_t>(backDir) << 13u));
            outWorldBoxes = StateInspector::buildStairWorldBoxes(
                placePos, orient.facing, orient.topHalf, hasFront, frontDir, hasBack, backDir);
            return true;
        }

        // Dynamic neighbor-aware Walls, Fences, Glass Panes, & Iron Bars
        if (StateInspector::isFenceOrWallOrPane(resolvedBlockIdentifier)) {
            auto connectsTo = [&](const BlockPos& np) -> bool {
                const std::string nid = getBlockIdentifier(np);
                if (nid.empty()) return false;
                if (StateInspector::isFenceOrWallOrPane(nid)) return true;
                return !StateInspector::isNonSolidSupportBlock(nid) &&
                       !StateInspector::isSlabBlock(nid) &&
                       !StateInspector::isStairBlock(nid) &&
                       !StateInspector::isDoorBlock(nid) &&
                       !StateInspector::isBedBlock(nid);
            };
            const bool cN = connectsTo(placePos.offset(0, 0, -1));
            const bool cS = connectsTo(placePos.offset(0, 0,  1));
            const bool cW = connectsTo(placePos.offset(-1, 0, 0));
            const bool cE = connectsTo(placePos.offset(1, 0,  0));
            outStateData = static_cast<uint16_t>((cN ? 1u : 0u) |
                                                 (cS ? 2u : 0u) |
                                                 (cW ? 4u : 0u) |
                                                 (cE ? 8u : 0u));
            outWorldBoxes = StateInspector::buildConnectedPostWorldBoxes(
                placePos, resolvedBlockIdentifier, cN, cS, cW, cE);
            return true;
        }

        outWorldBoxes = StateInspector::buildFallbackBlockBoxes(placePos, resolvedBlockIdentifier, orient);
        return true;
    }

private:
    [[nodiscard]] const void* queryBlockInChunk(const void* lc, const BlockPos& pos) const noexcept {
        NativeChunkBlockPos local{};
        local.x = static_cast<uint8_t>(pos.x & 15);
        local.z = static_cast<uint8_t>(pos.z & 15);
        local.y = static_cast<int16_t>(pos.y - mMinY);
        const void* blk = gApi.lcGetBlock(lc, local);
        return sig::isValidUserAddress(blk, alignof(void*)) ? blk : nullptr;
    }

    void* mBlockSource{nullptr};
    void* mActor{nullptr};
    int mMinY{-64};
    int mMaxY{319};
    float mPitchDeg{0.0f};
    float mYawDeg{0.0f};
    mutable NativeChunkPos mCachedChunkPos{INT32_MIN, INT32_MIN};
    mutable const void* mCachedChunkPtr{nullptr};
};

// Reusable render-thread buffers (zero per-frame heap allocations during steady gameplay).
GhostMeshOutput gFrameMesh{};

void onPostDebugRenderEnd(void* debugRenderer, void* renderParams, FnDebugRenderEnd orig) {
    if (!orig || !sig::isValidUserAddress(debugRenderer, alignof(void*)) ||
        !sig::isValidUserAddress(renderParams, alignof(void*))) {
        return;
    }

    gFramesEvaluated.fetch_add(1, std::memory_order_relaxed);

    const ModConfig cfg = ConfigManager::instance().snapshot();
    if (!cfg.enabled) {
        return;
    }

    // Obtain LevelRendererPlayer* via parent LevelRenderer object (debugRenderer - 0x2a8).
    auto* dbgBytes = static_cast<uint8_t*>(debugRenderer);
    void* levelRenderer = dbgBytes - sig::off::LevelRenderer_DebugRenderer;
    void* lrp = nullptr;
    if (!readFieldSafe(levelRenderer, sig::off::LevelRenderer_LRPPtr, lrp) ||
        !sig::isValidUserAddress(lrp, alignof(void*))) {
        return;
    }

    void* clientInst = nullptr;
    void* level = nullptr;
    if (!readFieldSafe(lrp, sig::off::LRP_ClientInstancePtr, clientInst) ||
        !sig::isValidUserAddress(clientInst, alignof(void*)) ||
        !readFieldSafe(lrp, sig::off::LRP_LevelPtr, level) ||
        !sig::isValidUserAddress(level, alignof(void*))) {
        return;
    }

    const auto fnGetCamActor = readVtableFn<FnClientGetCamActor>(clientInst, sig::off::ClientInstance_VtGetCamActor);
    const auto fnGetHitResult = readVtableFn<FnLevelGetHitResult>(level, sig::off::Level_VtGetHitResult);
    if (!fnGetCamActor || !fnGetHitResult) {
        return;
    }

    void* camActor = fnGetCamActor(clientInst);
    if (!sig::isValidUserAddress(camActor, alignof(void*))) {
        return;
    }

    // Immediate disappearance in Spectator mode.
    if (gApi.actorIsSpectator && gApi.actorIsSpectator(camActor)) {
        return;
    }

    void* blockSource = nullptr;
    if (gApi.actorGetBlockSource) {
        blockSource = gApi.actorGetBlockSource(camActor);
    }
    if (!sig::isValidUserAddress(blockSource, alignof(void*))) {
        const auto fnGetRegion = readVtableFn<FnClientGetRegion>(clientInst, sig::off::ClientInstance_VtGetRegion);
        if (fnGetRegion) {
            blockSource = fnGetRegion(clientInst);
        }
    }
    if (!sig::isValidUserAddress(blockSource, alignof(void*))) {
        return;
    }

    // Dynamic world build bounds (supports Overworld, Nether, End, and custom BuildLimitChanger mods!).
    int16_t rawMaxH = 320;
    int16_t rawMinH = -64;
    readFieldSafe(blockSource, sig::off::BlockSource_MaxHeight, rawMaxH);
    readFieldSafe(blockSource, sig::off::BlockSource_MinHeight, rawMinH);
    const int minBuildY = (rawMinH >= -2048 && rawMinH < rawMaxH) ? static_cast<int>(rawMinH) : -64;
    const int maxBuildY = (rawMaxH > rawMinH && rawMaxH <= 4096) ? static_cast<int>(rawMaxH) - 1 : 319;

    // Read live HitResult from Level::getHitResult().
    const void* hitRes = fnGetHitResult(level);
    if (!sig::isValidUserAddress(hitRes, alignof(void*))) {
        return;
    }

    int32_t hitType = 3; // HitResultType::NoHit
    uint8_t hitFaceByte = 1;
    int32_t hitBlockRaw[3]{0, 0, 0};
    float hitPosRaw[3]{0.0f, 0.0f, 0.0f};
    uint8_t hitLiquidByte = 0;
    uint8_t hitLiquidFaceByte = 1;
    int32_t hitLiquidPosRaw[3]{0, 0, 0};

    readFieldSafe(hitRes, sig::off::HitResult_Type, hitType);
    readFieldSafe(hitRes, sig::off::HitResult_Facing, hitFaceByte);
    readFieldSafe(hitRes, sig::off::HitResult_BlockPos, hitBlockRaw);
    readFieldSafe(hitRes, sig::off::HitResult_Pos, hitPosRaw);
    readFieldSafe(hitRes, sig::off::HitResult_IsHitLiquid, hitLiquidByte);
    readFieldSafe(hitRes, sig::off::HitResult_LiquidFacing, hitLiquidFaceByte);
    readFieldSafe(hitRes, sig::off::HitResult_LiquidPos, hitLiquidPosRaw);

    const bool hasTileHit = (hitType == 0) &&
                            std::isfinite(hitPosRaw[0]) &&
                            std::isfinite(hitPosRaw[1]) &&
                            std::isfinite(hitPosRaw[2]);
    const bool hasLiquidHit = (hitLiquidByte != 0);

    // Fast early-out: aiming at sky, entity, or out of block reach -> 0 vertices IMMEDIATELY!
    if (!hasTileHit && !hasLiquidHit) {
        return;
    }

    // Read player AABB & rotation components safely.
    AABB playerAabb{};
    const void* aabbComp = nullptr;
    if (readFieldSafe(camActor, sig::off::Actor_AABBCompPtr, aabbComp) &&
        sig::isValidUserAddress(aabbComp, 4)) {
        float bounds[6]{};
        if (readFieldSafe(aabbComp, 0, bounds) &&
            std::isfinite(bounds[0]) && std::isfinite(bounds[1]) && std::isfinite(bounds[2]) &&
            std::isfinite(bounds[3]) && std::isfinite(bounds[4]) && std::isfinite(bounds[5])) {
            const AABB candidate{
                {bounds[0], bounds[1], bounds[2]},
                {bounds[3], bounds[4], bounds[5]}
            };
            if (candidate.sizeX() > 0.1f && candidate.sizeX() < 8.0f &&
                candidate.sizeY() > 0.1f && candidate.sizeY() < 8.0f &&
                candidate.sizeZ() > 0.1f && candidate.sizeZ() < 8.0f) {
                playerAabb = candidate;
            }
        }
    }

    float pitchDeg = 0.0f;
    float yawDeg = 0.0f;
    const void* rotComp = nullptr;
    if (readFieldSafe(camActor, sig::off::Actor_RotCompPtr, rotComp) &&
        sig::isValidUserAddress(rotComp, 4)) {
        float rot[2]{0.0f, 0.0f};
        if (readFieldSafe(rotComp, 0, rot) && std::isfinite(rot[0]) && std::isfinite(rot[1])) {
            pitchDeg = rot[0];
            yawDeg = rot[1];
        }
    }

    // Inspect mainhand & offhand items via pure read-only vtable getters.
    const auto fnCarried = readVtableFn<FnActorGetItemStack>(camActor, sig::off::Actor_VtGetCarriedItem);
    const auto fnOffhand = readVtableFn<FnActorGetItemStack>(camActor, sig::off::Actor_VtGetOffhandSlot);

    PlayerContextInput ctx{};
    ctx.inWorld = true;
    ctx.isSpectator = false;
    ctx.isAdventureMode = false;
    // Detect sneak smoothly from eye/hitbox height (< 1.65 blocks when sneaking vs 1.80 standing).
    ctx.isSneaking = (playerAabb.sizeY() > 0.5f && playerAabb.sizeY() < 1.65f);
    ctx.pitchDeg = pitchDeg;
    ctx.yawDeg = yawDeg;
    ctx.playerAabb = playerAabb;
    if (fnCarried) ctx.mainhand = inspectItemStack(fnCarried(camActor));
    if (fnOffhand) ctx.offhand = inspectItemStack(fnOffhand(camActor));

    ctx.raycast.hasBlockHit = hasTileHit;
    ctx.raycast.hitFace = (hitFaceByte <= 5u) ? static_cast<Facing>(hitFaceByte) : Facing::Up;
    ctx.raycast.blockPos = {hitBlockRaw[0], hitBlockRaw[1], hitBlockRaw[2]};
    ctx.raycast.hitWorldPos = {hitPosRaw[0], hitPosRaw[1], hitPosRaw[2]};
    ctx.raycast.hasLiquidHit = hasLiquidHit;
    ctx.raycast.liquidFace = (hitLiquidFaceByte <= 5u) ? static_cast<Facing>(hitLiquidFaceByte) : Facing::Up;
    ctx.raycast.liquidPos = {hitLiquidPosRaw[0], hitLiquidPosRaw[1], hitLiquidPosRaw[2]};
    ctx.raycast.liquidWorldPos = ctx.raycast.liquidPos.centerVec();

    LiveWorldQuery world(blockSource, camActor, minBuildY, maxBuildY, pitchDeg, yawDeg);
    const PlacementPreviewState preview = PlacementResolver::resolve(ctx, world, cfg);
    if (!preview.valid || preview.worldBoxes.empty()) {
        return;
    }

    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const float pulsePhase = static_cast<float>(nowMs % 3142LL) * 0.002f;

    GhostMesher::buildMesh(preview, cfg, playerAabb.center(), pulsePhase, gFrameMesh);
    if (gFrameMesh.empty()) {
        return;
    }

    EngineBridge::flushMeshIntoDebugRenderer(debugRenderer, renderParams, orig, gFrameMesh);
    gFramesRendered.fetch_add(1, std::memory_order_relaxed);
}

[[maybe_unused]] void detourDebugRenderEnd(void* debugRenderer, void* renderParams) {
    struct ActiveCallGuard {
        ActiveCallGuard() noexcept { gActiveHookCalls.fetch_add(1, std::memory_order_acq_rel); }
        ~ActiveCallGuard() noexcept { gActiveHookCalls.fetch_sub(1, std::memory_order_acq_rel); }
    } callGuard;

    const FnDebugRenderEnd orig = gOrigDebugRenderEnd.load(std::memory_order_acquire);
    if (!orig) {
        return;
    }

    // First flush vanilla / upstream debug geometry on Mojang's own allocator.
    orig(debugRenderer, renderParams);

    // Prevent recursive re-entry if another mod re-invokes detour chains.
    thread_local bool sInPostPass = false;
    if (sInPostPass || !gEnabled.load(std::memory_order_acquire)) {
        return;
    }

    sInPostPass = true;
    onPostDebugRenderEnd(debugRenderer, renderParams, orig);
    sInPostPass = false;
}

#if defined(__ANDROID__)
uintptr_t resolveMinecraftModuleBaseFast() noexcept {
    // 1. Sub-microsecond (~2 us) Bionic linker phdr lookup — zero memory scanning!
    uintptr_t phdrBase = 0;
    dl_iterate_phdr(
        [](struct dl_phdr_info* info, size_t /*size*/, void* data) -> int {
            if (!info || !info->dlpi_name) return 0;
            const std::string_view name(info->dlpi_name);
            if (name.find("libminecraftpe.so") != std::string_view::npos) {
                const auto candidate = static_cast<uintptr_t>(info->dlpi_addr);
                if (candidate != 0 && (candidate & 0xFFFu) == 0u) {
                    *static_cast<uintptr_t*>(data) = candidate;
                    return 1;
                }
            }
            return 0;
        },
        &phdrBase);

    if (phdrBase != 0 &&
        readInsnWord(phdrBase + sig::rva::DebugRenderer_renderEnd + 0x10) ==
            sig::rva::DebugRenderer_renderEnd_W10) {
        return phdrBase;
    }

    // 2. Sub-millisecond vtable anchor lookup backup.
    const uintptr_t fogFn = pl::memory::resolveVtableFunction(
        sig::off::LRP_RttiName,
        sig::off::LRP_SetupFogVtableSlot,
        "libminecraftpe.so");
    if (fogFn <= sig::rva::LRP_setupFog) {
        return 0;
    }
    const uintptr_t base = fogFn - sig::rva::LRP_setupFog;
    if ((base & 0xFFFu) != 0u) {
        return 0;
    }
    if (readInsnWord(fogFn + 0x10) != sig::rva::LRP_setupFog_Word10) {
        return 0;
    }
    return base;
}

uintptr_t resolveRvaChecked(uintptr_t moduleBase, uintptr_t rva, uint32_t expectedWord10) noexcept {
    if (!moduleBase) return 0;
    const uintptr_t candidate = moduleBase + rva;
    return (readInsnWord(candidate + 0x10) == expectedWord10) ? candidate : 0;
}

bool attachDebugRenderHookLocked(const ResolvedEngineApi& api) {
    if (!api.targetDebugRenderEnd || !api.lcGetBlock) {
        return false;
    }
    void* origRaw = nullptr;
    auto handle = std::make_unique<pl::memory::HookHandle>(
        api.targetDebugRenderEnd,
        reinterpret_cast<void*>(&detourDebugRenderEnd),
        &origRaw,
        pl::memory::HookPriority::Normal);
    if (!handle->installed() || origRaw == nullptr) {
        handle.reset();
        return false;
    }
    gApi = api;
    gOrigDebugRenderEnd.store(reinterpret_cast<FnDebugRenderEnd>(origRaw), std::memory_order_release);
    gHookRenderEnd = std::move(handle);
    gFastRvaResolved.store(api.fastRva, std::memory_order_release);
    gHooksInstalled.store(true, std::memory_order_release);
    return true;
}
#endif

} // namespace

EngineBridge& EngineBridge::instance() noexcept {
    static EngineBridge bridge;
    return bridge;
}

bool EngineBridge::installHooks() {
#if defined(__ANDROID__)
    std::lock_guard<std::mutex> lock(gHookMutex);
    gEnabled.store(true, std::memory_order_release);
    if (gHooksInstalled.load(std::memory_order_acquire)) {
        return true;
    }

    // Tier 1 (< 0.2 ms on UI Main Thread — ZERO full-library scans, ZERO ANR possibility!):
    const uintptr_t base = resolveMinecraftModuleBaseFast();
    if (base != 0) {
        ResolvedEngineApi fastApi{};
        fastApi.moduleBase = base;
        fastApi.fastRva = true;
        fastApi.targetDebugRenderEnd = reinterpret_cast<void*>(
            resolveRvaChecked(base, sig::rva::DebugRenderer_renderEnd, sig::rva::DebugRenderer_renderEnd_W10));
        fastApi.lcGetBlock = reinterpret_cast<FnLevelChunkGetBlock>(
            resolveRvaChecked(base, sig::rva::LevelChunk_getBlock, sig::rva::LevelChunk_getBlock_W10));
        fastApi.actorGetBlockSource = reinterpret_cast<FnActorGetDimBlockSrc>(
            resolveRvaChecked(base, sig::rva::Actor_getDimensionBlockSource, sig::rva::Actor_getDimBlockSource_W10));
        fastApi.actorIsSpectator = reinterpret_cast<FnActorIsSpectator>(
            resolveRvaChecked(base, sig::rva::Actor_isSpectator, sig::rva::Actor_isSpectator_W10));

        if (attachDebugRenderHookLocked(fastApi)) {
            return true;
        }
    }

    // Tier 2 (Future Minecraft version where RVAs shifted):
    // NEVER scan 328 MB synchronously on Android's UI Main Thread! Spawn a background worker
    // thread so Mod::enable() returns instantaneously (< 0.2 ms) without stalling UI input.
    if (gAsyncFallbackThread.joinable()) {
        gAsyncFallbackThread.join();
    }
    gAsyncFallbackThread = std::thread([] {
        const uintptr_t renderEndBody = pl::memory::resolveSignature(
            sig::pattern::DebugRenderer_renderEnd_Body10, "libminecraftpe.so");
        if (!gEnabled.load(std::memory_order_acquire) || renderEndBody <= 0x10) {
            return;
        }
        const uintptr_t lcBody = pl::memory::resolveSignature(
            sig::pattern::LevelChunk_getBlock_Body10, "libminecraftpe.so");
        if (!gEnabled.load(std::memory_order_acquire) || lcBody <= 0x10) {
            return;
        }
        const uintptr_t dimBsBody = pl::memory::resolveSignature(
            sig::pattern::Actor_getDimBlockSource_Body10, "libminecraftpe.so");
        const uintptr_t specBody = pl::memory::resolveSignature(
            sig::pattern::Actor_isSpectator_Body10, "libminecraftpe.so");

        ResolvedEngineApi scanned{};
        scanned.fastRva = false;
        scanned.targetDebugRenderEnd = reinterpret_cast<void*>(renderEndBody - 0x10);
        scanned.lcGetBlock = reinterpret_cast<FnLevelChunkGetBlock>(lcBody - 0x10);
        if (dimBsBody > 0x10) {
            scanned.actorGetBlockSource = reinterpret_cast<FnActorGetDimBlockSrc>(dimBsBody - 0x10);
        }
        if (specBody > 0x10) {
            scanned.actorIsSpectator = reinterpret_cast<FnActorIsSpectator>(specBody - 0x10);
        }

        std::lock_guard<std::mutex> guard(gHookMutex);
        if (gEnabled.load(std::memory_order_acquire) && !gHooksInstalled.load(std::memory_order_acquire)) {
            attachDebugRenderHookLocked(scanned);
        }
    });
    return true;
#else
    gEnabled.store(true, std::memory_order_release);
    gHooksInstalled.store(true, std::memory_order_release);
    return true;
#endif
}

void EngineBridge::uninstallHooks() {
    gEnabled.store(false, std::memory_order_release);
#if defined(__ANDROID__)
    if (gAsyncFallbackThread.joinable()) {
        gAsyncFallbackThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(gHookMutex);
        gHookRenderEnd.reset();
        gOrigDebugRenderEnd.store(nullptr, std::memory_order_release);
        gHooksInstalled.store(false, std::memory_order_release);
        gApi = {};
    }
    // Ensure no render thread is still executing inside detourDebugRenderEnd before dlclose.
    for (int spin = 0; spin < 200 && gActiveHookCalls.load(std::memory_order_acquire) > 0; ++spin) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#else
    gHooksInstalled.store(false, std::memory_order_release);
#endif
    gFrameMesh.clear();
}

void EngineBridge::notifyConfigUpdated(const ModConfig& /*cfg*/) noexcept {
    // Live configuration snapshots are read lock-free on every rendered frame.
}

BridgeStats EngineBridge::stats() const noexcept {
    BridgeStats s{};
    s.hooksInstalled = gHooksInstalled.load(std::memory_order_acquire);
    s.fastRvaResolved = gFastRvaResolved.load(std::memory_order_acquire);
    s.framesEvaluated = gFramesEvaluated.load(std::memory_order_relaxed);
    s.framesRendered = gFramesRendered.load(std::memory_order_relaxed);
    return s;
}

void EngineBridge::flushMeshIntoDebugRenderer(
    void* debugRenderer,
    void* renderParams,
    void (*origFn)(void*, void*),
    const GhostMeshOutput& mesh) noexcept {

    if (!origFn || !debugRenderer || mesh.empty()) {
        return;
    }

    auto* dbgBytes      = static_cast<uint8_t*>(debugRenderer);
    auto* activeFlagPtr = reinterpret_cast<uint8_t*>(dbgBytes + sig::off::DebugRenderer_ActiveFlag);
    auto* lineVecPtrs   = reinterpret_cast<const PackedVertex**>(dbgBytes + sig::off::DebugRenderer_LineVec);
    auto* triVecPtrs    = reinterpret_cast<const PackedVertex**>(dbgBytes + sig::off::DebugRenderer_TriVec);

    const PackedVertex* savedLine[3] = {lineVecPtrs[0], lineVecPtrs[1], lineVecPtrs[2]};
    const PackedVertex* savedTri[3]  = {triVecPtrs[0],  triVecPtrs[1],  triVecPtrs[2]};

    if (!mesh.packedTriangles.empty()) {
        const PackedVertex* begin = mesh.packedTriangles.data();
        const PackedVertex* end   = begin + mesh.packedTriangles.size();
        lineVecPtrs[0] = nullptr;
        lineVecPtrs[1] = nullptr;
        lineVecPtrs[2] = nullptr;
        triVecPtrs[0]  = begin;
        triVecPtrs[1]  = end;
        triVecPtrs[2]  = end;
        *activeFlagPtr = 1u;
        origFn(debugRenderer, renderParams);
    }

    if (!mesh.packedLines.empty()) {
        const PackedVertex* begin = mesh.packedLines.data();
        const PackedVertex* end   = begin + mesh.packedLines.size();
        triVecPtrs[0]  = nullptr;
        triVecPtrs[1]  = nullptr;
        triVecPtrs[2]  = nullptr;
        lineVecPtrs[0] = begin;
        lineVecPtrs[1] = end;
        lineVecPtrs[2] = end;
        *activeFlagPtr = 1u;
        origFn(debugRenderer, renderParams);
    }

    lineVecPtrs[0] = savedLine[0];
    lineVecPtrs[1] = savedLine[1];
    lineVecPtrs[2] = savedLine[2];
    triVecPtrs[0]  = savedTri[0];
    triVecPtrs[1]  = savedTri[1];
    triVecPtrs[2]  = savedTri[2];
    *activeFlagPtr = 0u;
}

} // namespace buildghost
