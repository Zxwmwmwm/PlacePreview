#include "placepreview/EngineBridge.hpp"

#include "placepreview/Config.hpp"
#include "placepreview/GhostMesher.hpp"
#include "placepreview/PlacementResolver.hpp"
#include "placepreview/PreviewAnimator.hpp"
#include "placepreview/Signatures.hpp"
#include "placepreview/StateInspector.hpp"

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
#include <unordered_map>
#include <vector>

#if defined(__ANDROID__)
#include <link.h>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>
#include <pl/memory/Vtable.hpp>
#endif

namespace placepreview {

namespace {

using FnDebugRenderEnd = void (*)(void* debugRenderer, void* renderParams);

struct NativeChunkPos {
    int32_t x{0};
    int32_t z{0};
};
static_assert(sizeof(NativeChunkPos) == 8, "NativeChunkPos must be 8 bytes");

struct NativeChunkBlockPos {
    uint8_t x{0};
    uint8_t z{0};
    int16_t y{0};
};
static_assert(sizeof(NativeChunkBlockPos) == 4, "NativeChunkBlockPos must be 4 bytes");

using FnLevelChunkGetBlock   = const void* (*)(const void* levelChunk, const NativeChunkBlockPos& localPos);
using FnActorGetDimBlockSrc  = void* (*)(void* actor);
using FnPlayerIsCreative     = bool (*)(const void* actor);
using FnEntityGetGameType    = int32_t (*)(const void* entityContext);
using FnLevelGetDefGameType  = int32_t (*)(const void* level);
using FnClientGetCamActor    = void* (*)(void* clientInstance);
using FnClientGetRegion      = void* (*)(void* clientInstance);
using FnLevelGetHitResult    = const void* (*)(void* level);
using FnActorGetItemStack    = const void* (*)(const void* actor);
using FnBsGetChunk           = const void* (*)(void* blockSource, const NativeChunkPos& cp);
using FnBsCheckPermissions   = bool (*)(void* blockSource, void* actor, const BlockPos& pos, const void* itemStack, bool genParticle);

// Verified BlockType primary vtable slots in 1.26.52.03 arm64-v8a:
//   slot  8 (+0x40): const AABB* getOutline(const BlockType*, const Block*, const BlockSource*, const BlockPos*, AABB* out)
//   slot 10 (+0x50): const AABB* getVisualShape(const BlockType*, const Block*, AABB* out)
//   slot 79 (+0x278): const Block* getPlacementBlock(const BlockType*, const Actor*, const BlockPos*, uint8_t face, const Vec3* clickPos, int32_t itemVal)
using FnBlockTypeGetOutline = const AABB* (*)(
    const void* blockType,
    const void* block,
    const void* blockSource,
    const BlockPos* pos,
    AABB* outBuffer);

using FnBlockTypeGetVisualShape = const AABB* (*)(
    const void* blockType,
    const void* block,
    AABB* outBuffer);

using FnBlockTypeGetPlacementBlock = const void* (*)(
    const void* blockType,
    const void* byActor,
    const BlockPos* pos,
    uint8_t face,
    const Vec3* clickPos,
    int32_t itemValue);

using FnBlockGetRandomOffset  = Vec3 (*)(const void* block, const BlockPos* pos);
using FnFlowerBedTryGetComp   = const void* (*)(const void* block);
using FnFlowerBedGetOutline   = AABB (*)(const void* comp, const void* block, const BlockPos* pos);
using FnBlockPlanterGetId     = const void* (*)();
using FnItemStackGetComp      = const void* (*)(const void* stack, const void* id);
using FnTraitStateGetValue    = int32_t (*)(const void* traitState, const void* blockType, uint16_t stateData);

struct ResolvedEngineApi {
    uintptr_t moduleBase{0};
    void* targetDebugRenderEnd{nullptr};
    const void* blockTypeRegistry{nullptr};
    FnLevelChunkGetBlock lcGetBlock{nullptr};
    FnActorGetDimBlockSrc actorGetBlockSource{nullptr};
    FnPlayerIsCreative playerIsCreative{nullptr};
    FnEntityGetGameType entityGetPlayerGameType{nullptr};
    FnBlockGetRandomOffset blockGetRandomOffset{nullptr};
    FnFlowerBedTryGetComp flowerBedTryGetComp{nullptr};
    FnFlowerBedGetOutline flowerBedGetOutline{nullptr};
    FnBlockPlanterGetId blockPlanterGetId{nullptr};
    FnItemStackGetComp itemStackGetComp{nullptr};
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

// Read-only BlockTypeRegistry cache populated lazily on render thread (zero engine heap allocations).
struct RegistryCache {
    std::size_t lastSeenCount{0};
    std::unordered_map<std::string, const void*> map;

    void clear() noexcept {
        lastSeenCount = 0;
        map.clear();
    }
};

GhostMeshOutput gFrameMesh{};
PreviewAnimator gAnimator{};
RegistryCache gRegistryCache{};
std::chrono::steady_clock::time_point gLastFrameTime{};
bool gHasLastFrameTime{false};

#if defined(__ANDROID__)
std::unique_ptr<pl::memory::HookHandle> gHookRenderEnd;
std::thread gAsyncFallbackThread;
#endif

[[maybe_unused]] uint32_t readInsnWord(uintptr_t addr) noexcept {
    if (!sig::isValidUserAddress(reinterpret_cast<const void*>(addr), 4)) {
        return 0u;
    }
    uint32_t word = 0u;
    std::memcpy(&word, sig::untagPtr(reinterpret_cast<const void*>(addr)), sizeof(uint32_t));
    return word;
}

template <typename T>
bool readFieldSafe(const void* base, uintptr_t offset, T& outVal) noexcept {
    if (!sig::isValidUserAddress(base, 1)) return false;
    const auto* bytePtr = sig::untagPtr(static_cast<const uint8_t*>(base)) + offset;
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

std::string lowerCopyAscii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::string readHashedString(const void* hashedStrAddr) {
    if (!sig::isValidUserAddress(hashedStrAddr, alignof(void*))) return {};
    const auto sv = sig::readLibcxxStringView(static_cast<const uint8_t*>(hashedStrAddr) + 0x08);
    return std::string(sv);
}

std::string readBlockIdentifierFromBlockType(const void* bt) {
    if (!sig::isValidUserAddress(bt, alignof(void*))) {
        return {};
    }
    const auto fullSv = sig::readLibcxxStringView(
        sig::untagPtr(static_cast<const uint8_t*>(bt)) + sig::off::BlockType_FullNameStr);
    if (!fullSv.empty()) {
        return std::string(fullSv);
    }
    const auto rawSv = sig::readLibcxxStringView(
        sig::untagPtr(static_cast<const uint8_t*>(bt)) + sig::off::BlockType_RawNameStr);
    if (!rawSv.empty()) {
        return "minecraft:" + std::string(rawSv);
    }
    return {};
}

std::string readBlockIdentifierFromBlock(const void* blockPtr) {
    if (!sig::isValidUserAddress(blockPtr, alignof(void*))) return {};
    const void* bt = nullptr;
    if (!readFieldSafe(blockPtr, sig::off::Block_BlockTypePtr, bt) ||
        !sig::isValidUserAddress(bt, alignof(void*))) {
        return {};
    }
    return readBlockIdentifierFromBlockType(bt);
}

// Walks BlockTypeRegistry's std::map<HashedString, shared_ptr<BlockType>> strictly read-only
// using a bounded iterative stack whenever world addon count changes.
const void* lookupNativeBlockType(std::string_view fullBlockId) {
    if (!gApi.blockTypeRegistry || fullBlockId.empty()) {
        return nullptr;
    }

    std::size_t treeCount = 0;
    const void* rootNode = nullptr;
    if (!readFieldSafe(gApi.blockTypeRegistry, sig::off::Registry_MapRootPtr, rootNode) ||
        !readFieldSafe(gApi.blockTypeRegistry, sig::off::Registry_MapSize, treeCount) ||
        treeCount == 0 || treeCount > 32768 ||
        !sig::isValidUserAddress(rootNode, alignof(void*))) {
        return nullptr;
    }

    if (treeCount != gRegistryCache.lastSeenCount || gRegistryCache.map.empty()) {
        gRegistryCache.map.clear();
        gRegistryCache.map.reserve(treeCount + 64);

        std::vector<const void*> stack;
        stack.reserve(128);
        stack.push_back(rootNode);

        std::size_t visited = 0;
        const std::size_t maxVisit = std::min<std::size_t>(treeCount * 2 + 64, 65536);
        while (!stack.empty() && visited < maxVisit) {
            const void* node = stack.back();
            stack.pop_back();
            if (!sig::isValidUserAddress(node, alignof(void*))) {
                continue;
            }
            ++visited;

            const auto keySv = sig::readLibcxxStringView(
                static_cast<const uint8_t*>(node) + sig::off::RegistryNode_KeyStr);
            const void* btPtr = nullptr;
            if (!keySv.empty() &&
                readFieldSafe(node, sig::off::RegistryNode_BlockTypePtr, btPtr) &&
                sig::isValidUserAddress(btPtr, alignof(void*))) {
                std::string k = lowerCopyAscii(keySv);
                gRegistryCache.map.emplace(std::move(k), btPtr);
            }

            const void* left = nullptr;
            const void* right = nullptr;
            if (readFieldSafe(node, sig::off::RegistryNode_Left, left) &&
                sig::isValidUserAddress(left, alignof(void*))) {
                stack.push_back(left);
            }
            if (readFieldSafe(node, sig::off::RegistryNode_Right, right) &&
                sig::isValidUserAddress(right, alignof(void*))) {
                stack.push_back(right);
            }
        }
        gRegistryCache.lastSeenCount = treeCount;
    }

    std::string query = lowerCopyAscii(fullBlockId);
    auto it = gRegistryCache.map.find(query);
    if (it != gRegistryCache.map.end()) {
        return it->second;
    }
    if (query.find(':') == std::string::npos) {
        it = gRegistryCache.map.find("minecraft:" + query);
        if (it != gRegistryCache.map.end()) {
            return it->second;
        }
    }
    return nullptr;
}

bool queryLiveBlockWorldOutline(
    const void* blockPtr,
    const void* blockSource,
    const BlockPos& pos,
    AABB& outWorldBox) noexcept {

    if (!sig::isValidUserAddress(blockPtr, alignof(void*))) {
        return false;
    }
    const void* bt = nullptr;
    if (!readFieldSafe(blockPtr, sig::off::Block_BlockTypePtr, bt) ||
        !sig::isValidUserAddress(bt, alignof(void*))) {
        return false;
    }

    // Verified slot 8 `BlockType::getOutline(bt, block, blockSource, &pos, &buf)`:
    //   - Disassembled at 0x1010a188: already delegates internally to slot 10 (`getVisualShape`) + `pos`
    //     when no `BlockSelectionBoxComponent` exists, AND intentionally returns degenerate [0..0] when
    //     `BlockSelectionBoxComponent.mEnabled == 0` (water, lava, air, structure_void, light_block).
    //   - NEVER fall back to slot 10 (`BlockType + 0x188` = [0..1]^3) when slot 8 returns degenerate,
    //     or underwater fluid blocks would become solid 1x1x1 raycast walls!
    const auto fnGetOutline = readVtableFn<FnBlockTypeGetOutline>(bt, sig::off::BlockType_VtGetOutline);
    if (fnGetOutline) {
        AABB buf{};
        const AABB* ret = fnGetOutline(bt, blockPtr, blockSource, &pos, &buf);
        const AABB* chosen = sig::isValidUserAddress(ret, 4) ? ret : &buf;
        AABB candidate = *chosen;
        if (std::isfinite(candidate.min.x) && std::isfinite(candidate.min.y) && std::isfinite(candidate.min.z) &&
            std::isfinite(candidate.max.x) && std::isfinite(candidate.max.y) && std::isfinite(candidate.max.z)) {
            candidate = candidate.normalized();
            if (!candidate.isDegenerate() &&
                candidate.sizeX() <= 3.0f && candidate.sizeY() <= 3.0f && candidate.sizeZ() <= 3.0f) {
                outWorldBox = candidate;
                return true;
            }
        }
    }
    return false;
}

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

    const auto* btBytes = sig::untagPtr(static_cast<const uint8_t*>(bt));

    // 1. Classic BlockStateInstance RB-tree at BlockType + 0x1a8
    //    Verified at 0x10548dc8 & 0x1041e0c0:
    //      +0x1a8: __begin_node_
    //      +0x1b0: __pair1_ (__end_node_ header)
    //      +0x1b8: __pair3_ (tree size)
    //      node + 0x2c: numBits, node + 0x30: endBit, node + 0x34: mask, node + 0x40: mState*
    const auto* treeBase = btBytes + sig::off::BlockType_StatesMap;
    const void* endNode = static_cast<const void*>(treeBase + 0x08);
    const void* curr = nullptr;
    std::size_t treeCount = 0;
    if (readFieldSafe(treeBase, 0x00, curr) &&
        readFieldSafe(treeBase, 0x10, treeCount) &&
        treeCount > 0 && treeCount <= 32) {
        curr = sig::untagPtr(curr);
        int steps = 0;
        while (curr != nullptr && curr != endNode && steps < 64 &&
               sig::isValidUserAddress(curr, alignof(void*))) {
            ++steps;
            uint32_t numBits = 0;
            uint32_t endBit = 0;
            uint32_t mask = 0;
            const void* stateDef = nullptr;
            if (readFieldSafe(curr, 0x2c, numBits) &&
                readFieldSafe(curr, 0x30, endBit) &&
                readFieldSafe(curr, 0x34, mask) &&
                readFieldSafe(curr, 0x40, stateDef) &&
                numBits > 0 && numBits <= 16 && endBit < 16 && endBit + 1u >= numBits &&
                sig::isValidUserAddress(stateDef, alignof(void*))) {
                const auto nameSv = sig::readLibcxxStringView(
                    sig::untagPtr(static_cast<const uint8_t*>(stateDef)) + 0x18);
                if (!nameSv.empty()) {
                    const uint32_t shift = (endBit + 1u) - numBits;
                    const int val = static_cast<int>((static_cast<uint32_t>(stateData) & mask) >> shift);
                    outProps.push_back(BlockStateProperty{std::string(nameSv), val});
                }
            }

            const void* right = nullptr;
            if (readFieldSafe(curr, 0x08, right) && right != nullptr &&
                sig::isValidUserAddress(right, alignof(void*))) {
                curr = sig::untagPtr(right);
                const void* left = nullptr;
                int guard = 0;
                while (guard++ < 32 && readFieldSafe(curr, 0x00, left) && left != nullptr &&
                       sig::isValidUserAddress(left, alignof(void*))) {
                    curr = sig::untagPtr(left);
                }
            } else {
                const void* parent = nullptr;
                int guard = 0;
                while (guard++ < 32 && readFieldSafe(curr, 0x10, parent) && parent != nullptr &&
                       sig::untagPtr(parent) != endNode && sig::isValidUserAddress(parent, alignof(void*))) {
                    parent = sig::untagPtr(parent);
                    const void* parentRight = nullptr;
                    if (!readFieldSafe(parent, 0x08, parentRight) ||
                        curr != sig::untagPtr(parentRight)) {
                        curr = parent;
                        parent = nullptr;
                        break;
                    }
                    curr = parent;
                }
                if (sig::untagPtr(parent) == endNode || guard >= 32) {
                    break;
                }
                if (parent != nullptr) {
                    curr = sig::untagPtr(parent);
                }
            }
        }
    }

    // 2. Modern 1.21-1.26 BlockTrait state vector at BlockType + 0x350..0x358
    //    Verified at 0x1010c118..0x1010c164 (used by CopperGolemStatueBlock, ShelfBlock, Wildflowers, etc.)!
    uintptr_t traitBegin = 0;
    uintptr_t traitEnd = 0;
    if (readFieldSafe(btBytes, sig::off::BlockType_TraitStatesVec, traitBegin) &&
        readFieldSafe(btBytes, sig::off::BlockType_TraitStatesVec + 8, traitEnd)) {
        traitBegin = sig::untagUserAddr(traitBegin);
        traitEnd = sig::untagUserAddr(traitEnd);
        if (traitBegin != 0 && traitEnd > traitBegin && ((traitEnd - traitBegin) % 16u) == 0u) {
            const std::size_t count = (traitEnd - traitBegin) / 16u;
            if (count <= 16) {
                for (std::size_t i = 0; i < count; ++i) {
                    const void* traitState = nullptr;
                    if (!readFieldSafe(reinterpret_cast<const void*>(traitBegin + i * 16u), 0, traitState) ||
                        !sig::isValidUserAddress(traitState, alignof(void*))) {
                        continue;
                    }
                    const void* stateDef = nullptr;
                    if (!readFieldSafe(traitState, 0x08, stateDef) ||
                        !sig::isValidUserAddress(stateDef, alignof(void*))) {
                        continue;
                    }
                    const auto nameSv = sig::readLibcxxStringView(
                        sig::untagPtr(static_cast<const uint8_t*>(stateDef)) + 0x18);
                    if (nameSv.empty()) continue;

                    bool alreadyPresent = false;
                    for (const auto& existing : outProps) {
                        if (existing.name == nameSv) {
                            alreadyPresent = true;
                            break;
                        }
                    }
                    if (alreadyPresent) continue;

                    const auto fnGetVal = readVtableFn<FnTraitStateGetValue>(traitState, 0x00);
                    if (fnGetVal) {
                        const int val = static_cast<int>(fnGetVal(traitState, bt, stateData));
                        outProps.push_back(BlockStateProperty{std::string(nameSv), val});
                    }
                }
            }
        }
    }
}

Vec3 queryNativeBlockRandomOffset(const void* blockPtr, const BlockPos& pos) noexcept {
    if (!gApi.blockGetRandomOffset || !sig::isValidUserAddress(blockPtr, alignof(void*))) {
        return {0.0f, 0.0f, 0.0f};
    }
    Vec3 off = gApi.blockGetRandomOffset(blockPtr, &pos);
    if (!std::isfinite(off.x) || std::fabs(off.x) > 0.5f) off.x = 0.0f;
    if (!std::isfinite(off.y) || std::fabs(off.y) > 0.5f) off.y = 0.0f;
    if (!std::isfinite(off.z) || std::fabs(off.z) > 0.5f) off.z = 0.0f;
    return off;
}

bool queryNativeFlowerBedOutline(const void* blockPtr, const BlockPos& pos, AABB& outBox) noexcept {
    if (!gApi.flowerBedTryGetComp || !gApi.flowerBedGetOutline ||
        !sig::isValidUserAddress(blockPtr, alignof(void*))) {
        return false;
    }
    const void* comp = gApi.flowerBedTryGetComp(blockPtr);
    if (!sig::isValidUserAddress(comp, alignof(void*))) {
        return false;
    }
    AABB candidate = gApi.flowerBedGetOutline(comp, blockPtr, &pos);
    if (std::isfinite(candidate.min.x) && std::isfinite(candidate.min.y) && std::isfinite(candidate.min.z) &&
        std::isfinite(candidate.max.x) && std::isfinite(candidate.max.y) && std::isfinite(candidate.max.z)) {
        candidate = candidate.normalized();
        if (!candidate.isDegenerate() &&
            candidate.sizeX() <= 2.0f && candidate.sizeY() <= 2.0f && candidate.sizeZ() <= 2.0f) {
            outBox = candidate;
            return true;
        }
    }
    return false;
}

const void* findFlowerBedPermutation(const void* bt, int targetGrowth, Facing targetFacing) {
    if (!sig::isValidUserAddress(bt, alignof(void*))) return nullptr;
    uintptr_t vecBegin = 0;
    uintptr_t vecEnd = 0;
    if (!readFieldSafe(bt, sig::off::BlockType_PermutationsVec, vecBegin) ||
        !readFieldSafe(bt, sig::off::BlockType_PermutationsVec + 8, vecEnd)) {
        return nullptr;
    }
    vecBegin = sig::untagUserAddr(vecBegin);
    vecEnd = sig::untagUserAddr(vecEnd);
    if (vecBegin == 0 || vecEnd <= vecBegin) return nullptr;
    const std::size_t count = (vecEnd - vecBegin) / sizeof(void*);
    if (count == 0 || count > 64) return nullptr;

    const void* growthFallback = nullptr;
    std::vector<BlockStateProperty> props;
    for (std::size_t i = 0; i < count; ++i) {
        const void* permBlk = nullptr;
        if (!readFieldSafe(reinterpret_cast<const void*>(vecBegin + i * sizeof(void*)), 0, permBlk) ||
            !sig::isValidUserAddress(permBlk, alignof(void*))) {
            continue;
        }
        extractBlockStateProperties(permBlk, props);
        int g = -1;
        Facing f = Facing::South;
        for (const auto& p : props) {
            if (p.name == "growth") {
                g = p.value;
            } else if (p.name == "minecraft:cardinal_direction" || p.name == "direction") {
                switch (p.value & 3) {
                    case 0: f = Facing::South; break;
                    case 1: f = Facing::West;  break;
                    case 2: f = Facing::North; break;
                    case 3: f = Facing::East;  break;
                }
            }
        }
        if (g == targetGrowth) {
            if (!growthFallback) growthFallback = permBlk;
            if (f == targetFacing) return permBlk;
        }
    }
    return growthFallback;
}

const void* resolveDirectItemStackBlockType(const void* stackPtr, const void* itemPtr) {
    if (!sig::isValidUserAddress(stackPtr, alignof(void*))) return nullptr;
    const void* blockPtr = nullptr;
    if (readFieldSafe(stackPtr, sig::off::ItemStack_BlockPtr, blockPtr) &&
        sig::isValidUserAddress(blockPtr, alignof(void*))) {
        const void* bt = nullptr;
        if (readFieldSafe(blockPtr, sig::off::Block_BlockTypePtr, bt) &&
            sig::isValidUserAddress(bt, alignof(void*))) {
            return bt;
        }
    }
    if (sig::isValidUserAddress(itemPtr, alignof(void*))) {
        const void* itemBt = nullptr;
        if (readFieldSafe(itemPtr, sig::off::Item_BlockTypePtr, itemBt) &&
            sig::isValidUserAddress(itemBt, alignof(void*))) {
            return itemBt;
        }
    }
    if (gApi.blockPlanterGetId && gApi.itemStackGetComp) {
        const void* compId = gApi.blockPlanterGetId();
        if (sig::isValidUserAddress(compId, alignof(void*))) {
            const void* planterComp = gApi.itemStackGetComp(stackPtr, compId);
            if (sig::isValidUserAddress(planterComp, alignof(void*))) {
                const void* compBt = nullptr;
                if (readFieldSafe(planterComp, 0x10, compBt) &&
                    sig::isValidUserAddress(compBt, alignof(void*))) {
                    return compBt;
                }
            }
        }
    }
    return nullptr;
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

    const void* directBt = resolveDirectItemStackBlockType(stackPtr, itemPtr);
    std::string directBlockId = readBlockIdentifierFromBlockType(directBt);

    out.valid = true;
    out.count = static_cast<int>(count);
    out.auxValue = static_cast<int>(aux);
    out.itemIdentifier = std::move(itemFull);
    out.directBlockIdentifier = std::move(directBlockId);
    out.nativeItemStackPtr = reinterpret_cast<uintptr_t>(stackPtr);
    return out;
}

class LiveWorldQuery final : public IWorldQuery {
public:
    LiveWorldQuery(
        void* blockSource,
        void* actor,
        int minY,
        int maxY,
        float pitchDeg,
        float yawDeg,
        bool isSneaking) noexcept
        : mBlockSource(blockSource),
          mActor(actor),
          mMinY(minY),
          mMaxY(maxY),
          mPitchDeg(pitchDeg),
          mYawDeg(yawDeg),
          mIsSneaking(isSneaking) {}

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
        const std::string blockId = getBlockIdentifier(pos);
        const std::string_view stripped = StateInspector::stripNamespace(blockId);
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
            n == "short_grass" || n == "tallgrass" || n == "tall_grass" || n == "fern" ||
            n == "large_fern" || n == "deadbush" || n == "bush" ||
            n == "short_dry_grass" || n == "tall_dry_grass" ||
            n == "fire" || n == "soul_fire" || n == "vine" ||
            n == "glow_lichen" || n == "sculk_vein" || n == "resin_clump" ||
            n == "structure_void" || n == "light_block" ||
            n == "seagrass" || n == "tall_seagrass" ||
            n == "crimson_roots" || n == "warped_roots" || n == "nether_sprouts" ||
            n == "leaf_litter") {
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
        if (id.empty() || StateInspector::isLiquidOrAirBlock(id) ||
            StateInspector::stripNamespace(id) == "barrier") {
            return {};
        }

        // Cluster blocks use exact count-aware sub-boxes for crisp raycasts:
        if (StateInspector::isSeaPickleBlock(id)) {
            int count = 1;
            std::vector<BlockStateProperty> props;
            extractBlockStateProperties(blk, props);
            for (const auto& p : props) {
                if (p.name == "cluster_count") {
                    count = std::clamp(p.value + 1, 1, 4);
                    break;
                }
            }
            return StateInspector::buildSeaPickleClusterWorldBoxes(pos, count);
        }
        if (StateInspector::isTurtleEggBlock(id)) {
            int count = 1;
            std::vector<BlockStateProperty> props;
            extractBlockStateProperties(blk, props);
            for (const auto& p : props) {
                if (p.name == "turtle_egg_count") {
                    count = std::clamp(p.value + 1, 1, 4);
                    break;
                }
            }
            return StateInspector::buildTurtleEggClusterWorldBoxes(pos, count);
        }
        if (StateInspector::isCandleBlock(id)) {
            int count = 1;
            std::vector<BlockStateProperty> props;
            extractBlockStateProperties(blk, props);
            for (const auto& p : props) {
                if (p.name == "candles") {
                    count = std::clamp(p.value + 1, 1, 4);
                    break;
                }
            }
            return StateInspector::buildCandleClusterWorldBoxes(pos, count);
        }

        if (StateInspector::isFlowerBedBlock(id)) {
            AABB liveBed{};
            if (queryNativeFlowerBedOutline(blk, pos, liveBed)) {
                return {liveBed};
            }
            int count = 1;
            std::vector<BlockStateProperty> props;
            extractBlockStateProperties(blk, props);
            for (const auto& p : props) {
                if (p.name == "growth") {
                    count = std::clamp(p.value + 1, 1, 4);
                    break;
                }
            }
            const OrientationSummary orient = StateInspector::decodeOrientation(
                id, props, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f, false);
            return StateInspector::buildFlowerBedClusterWorldBoxes(
                pos, count, orient.facing, queryNativeBlockRandomOffset(blk, pos));
        }

        // Multi-part blocks use exact multi-sub-box raycast geometry (stairs, fences, walls, hoppers).
        if (StateInspector::isStairBlock(id) ||
            StateInspector::isFenceOrWallOrPane(id) ||
            StateInspector::isDoorBlock(id) ||
            StateInspector::isBedBlock(id) ||
            StateInspector::stripNamespace(id) == "hopper") {
            std::vector<BlockStateProperty> props;
            extractBlockStateProperties(blk, props);
            const OrientationSummary orient = StateInspector::decodeOrientation(
                id, props, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f, false);
            return StateInspector::buildFallbackBlockBoxes(pos, id, orient);
        }

        // Query live BlockType::getOutline directly on the world block.
        std::vector<BlockStateProperty> props;
        extractBlockStateProperties(blk, props);
        const OrientationSummary orient = StateInspector::decodeOrientation(
            id, props, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f, false);
        const auto fallback = StateInspector::buildFallbackBlockBoxes(pos, id, orient);

        AABB liveBox{};
        if (queryLiveBlockWorldOutline(blk, mBlockSource, pos, liveBox)) {
            if (liveBox.volume() >= 0.99f && !fallback.empty() && fallback.front().volume() < 0.99f) {
                return fallback;
            }
            return {liveBox};
        }

        return fallback;
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
        std::string placeId = StateInspector::resolveSpecialItemBlockName(item.itemIdentifier, inOutFace);
        if (placeId.empty() && !item.directBlockIdentifier.empty()) {
            const std::string swapped =
                StateInspector::resolveSpecialItemBlockName(item.directBlockIdentifier, inOutFace);
            placeId = !swapped.empty() ? swapped : item.directBlockIdentifier;
        }

        // 1. Direct click on inside face of matching half-slab merges in place.
        if (StateInspector::isSlabBlock(placeId)) {
            const void* clickedBlk = getNativeBlockPtr(inOutPos);
            if (clickedBlk && readBlockIdentifierFromBlock(clickedBlk) == placeId) {
                std::vector<BlockStateProperty> props;
                extractBlockStateProperties(clickedBlk, props);
                const OrientationSummary exOrient = StateInspector::decodeOrientation(
                    placeId, props, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f, false);
                if ((!exOrient.topHalf && inOutFace == Facing::Up) ||
                    (exOrient.topHalf && inOutFace == Facing::Down)) {
                    return true;
                }
            }
        }

        // 2. Multi-cluster blocks (`candles`, `sea_pickle`, `turtle_egg`, `pink_petals`, `wildflowers`, `leaf_litter`: up to 4 per cell!).
        auto canStackClusterAt = [&](const BlockPos& cell) -> bool {
            const void* blk = getNativeBlockPtr(cell);
            if (!blk) return false;
            const std::string exId = readBlockIdentifierFromBlock(blk);
            if (exId != placeId) return false;
            std::vector<BlockStateProperty> props;
            extractBlockStateProperties(blk, props);
            if (StateInspector::isCandleBlock(placeId)) {
                for (const auto& p : props) {
                    if (p.name == "candles") return p.value < 3;
                }
            } else if (StateInspector::isSeaPickleBlock(placeId)) {
                for (const auto& p : props) {
                    if (p.name == "cluster_count") return p.value < 3;
                }
            } else if (StateInspector::isTurtleEggBlock(placeId)) {
                for (const auto& p : props) {
                    if (p.name == "turtle_egg_count") return p.value < 3;
                }
            } else if (StateInspector::isFlowerBedBlock(placeId)) {
                for (const auto& p : props) {
                    if (p.name == "growth") return p.value < 3;
                }
            }
            return false;
        };

        if ((StateInspector::isCandleBlock(placeId) ||
             StateInspector::isSeaPickleBlock(placeId) ||
             StateInspector::isTurtleEggBlock(placeId) ||
             StateInspector::isFlowerBedBlock(placeId)) &&
            canStackClusterAt(inOutPos)) {
            inOutFace = Facing::Up;
            return true;
        }

        // 3. Snow layer stacking (< 8 layers).
        if (StateInspector::stripNamespace(placeId) == "snow_layer") {
            const void* clickedBlk = getNativeBlockPtr(inOutPos);
            const std::string clickedId = clickedBlk ? readBlockIdentifierFromBlock(clickedBlk) : std::string{};
            if (!clickedId.empty() && StateInspector::stripNamespace(clickedId) == "snow_layer") {
                std::vector<BlockStateProperty> props;
                extractBlockStateProperties(clickedBlk, props);
                for (const auto& p : props) {
                    if (p.name == "height" && p.value < 7) {
                        return true;
                    }
                }
            }
        }

        // 4. Replaceable world block clicked directly -> replaces in place!
        if (isReplaceableBlock(inOutPos)) {
            if (inOutFace != Facing::Down &&
                !StateInspector::isNonSolidSupportBlock(getBlockIdentifier(inOutPos.offset(0, -1, 0)))) {
                const BlockPos oppStep = facingStep(oppositeFacing(inOutFace));
                const BlockPos behind = inOutPos.offset(oppStep.x, oppStep.y, oppStep.z);
                if (StateInspector::isNonSolidSupportBlock(getBlockIdentifier(behind))) {
                    inOutFace = Facing::Up;
                }
            }
            return true;
        }

        // 5. Advance one block along `inOutFace`.
        const BlockPos step = facingStep(inOutFace);
        const BlockPos target = inOutPos.offset(step.x, step.y, step.z);
        if (target.y < mMinY || target.y > mMaxY) {
            return false;
        }

        if (isReplaceableBlock(target)) {
            inOutPos = target;
            return true;
        }
        if (StateInspector::isSlabBlock(placeId) && getBlockIdentifier(target) == placeId) {
            inOutPos = target;
            return true;
        }
        if ((StateInspector::isCandleBlock(placeId) ||
             StateInspector::isSeaPickleBlock(placeId) ||
             StateInspector::isTurtleEggBlock(placeId) ||
             StateInspector::isFlowerBedBlock(placeId)) &&
            canStackClusterAt(target)) {
            inOutPos = target;
            inOutFace = Facing::Up;
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
        const HeldItemInput& item,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing face,
        const Vec3& subClickPos,
        std::string& outFinalBlockId,
        uint16_t& outStateData,
        std::vector<BlockStateProperty>& outProps,
        std::vector<AABB>& outWorldBoxes,
        std::vector<OrientedBox>& outOrientedBoxes) const override {

        outFinalBlockId = resolvedBlockIdentifier;
        outStateData = 0;
        outProps.clear();
        outWorldBoxes.clear();
        outOrientedBoxes.clear();

        // Resolve live BlockType* directly from ItemStack::mBlock (+0x18), Item::mBlockType (+0x138),
        // BlockPlanterItemComponent (+0x10), or read-only BlockTypeRegistry!
        const void* targetBlockType = nullptr;
        if (item.nativeItemStackPtr != 0) {
            const auto* stackPtr = reinterpret_cast<const void*>(item.nativeItemStackPtr);
            const void* weakRef = nullptr;
            const void* itemPtr = nullptr;
            if (readFieldSafe(stackPtr, sig::off::ItemStack_ItemWeakPtr, weakRef) &&
                sig::isValidUserAddress(weakRef, alignof(void*))) {
                (void)readFieldSafe(weakRef, 0, itemPtr);
            }
            const void* directBt = resolveDirectItemStackBlockType(stackPtr, itemPtr);
            if (sig::isValidUserAddress(directBt, alignof(void*)) &&
                readBlockIdentifierFromBlockType(directBt) == resolvedBlockIdentifier) {
                targetBlockType = directBt;
            }
        }
        if (!sig::isValidUserAddress(targetBlockType, alignof(void*))) {
            targetBlockType = lookupNativeBlockType(resolvedBlockIdentifier);
        }

        // Call live BlockType::getPlacementBlock (slot 79, +0x278) to get exact placed Block* state!
        const void* placedBlock = nullptr;
        if (mActor && sig::isValidUserAddress(targetBlockType, alignof(void*))) {
            const auto fnPlaceState = readVtableFn<FnBlockTypeGetPlacementBlock>(
                targetBlockType, sig::off::BlockType_VtGetPlacementBlock);
            if (fnPlaceState) {
                const void* candidate = fnPlaceState(
                    targetBlockType,
                    mActor,
                    &placePos,
                    static_cast<uint8_t>(face),
                    &subClickPos,
                    item.auxValue);
                if (sig::isValidUserAddress(candidate, alignof(void*))) {
                    placedBlock = candidate;
                    readFieldSafe(placedBlock, sig::off::Block_Data, outStateData);
                    const std::string liveId = readBlockIdentifierFromBlock(placedBlock);
                    if (!liveId.empty()) {
                        outFinalBlockId = liveId;
                    }
                    extractBlockStateProperties(placedBlock, outProps);
                }
            }
        }

        const OrientationSummary orient = StateInspector::decodeOrientation(
            outFinalBlockId, outProps, face, subClickPos, mPitchDeg, mYawDeg, mIsSneaking);

        // 1. Snow layer stacking height preview
        if (StateInspector::stripNamespace(outFinalBlockId) == "snow_layer") {
            const void* existing = getNativeBlockPtr(placePos);
            const std::string existingId = existing ? readBlockIdentifierFromBlock(existing) : std::string{};
            if (!existingId.empty() && StateInspector::stripNamespace(existingId) == "snow_layer") {
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

        // 2. Multi-cluster stacking preview (`candles`, `sea_pickle`, `turtle_egg` up to 4 per cell!)
        if (StateInspector::isCandleBlock(outFinalBlockId)) {
            int newCount = 1;
            const void* existing = getNativeBlockPtr(placePos);
            if (existing && readBlockIdentifierFromBlock(existing) == outFinalBlockId) {
                std::vector<BlockStateProperty> exProps;
                extractBlockStateProperties(existing, exProps);
                for (const auto& p : exProps) {
                    if (p.name == "candles") {
                        newCount = std::clamp(p.value + 2, 1, 4);
                        break;
                    }
                }
            }
            outStateData = static_cast<uint16_t>(newCount);
            outWorldBoxes = StateInspector::buildCandleClusterWorldBoxes(placePos, newCount);
            return true;
        }
        if (StateInspector::isSeaPickleBlock(outFinalBlockId)) {
            int newCount = 1;
            const void* existing = getNativeBlockPtr(placePos);
            if (existing && readBlockIdentifierFromBlock(existing) == outFinalBlockId) {
                std::vector<BlockStateProperty> exProps;
                extractBlockStateProperties(existing, exProps);
                for (const auto& p : exProps) {
                    if (p.name == "cluster_count") {
                        newCount = std::clamp(p.value + 2, 1, 4);
                        break;
                    }
                }
            }
            outStateData = static_cast<uint16_t>(newCount);
            outWorldBoxes = StateInspector::buildSeaPickleClusterWorldBoxes(placePos, newCount);
            return true;
        }
        if (StateInspector::isTurtleEggBlock(outFinalBlockId)) {
            int newCount = 1;
            const void* existing = getNativeBlockPtr(placePos);
            if (existing && readBlockIdentifierFromBlock(existing) == outFinalBlockId) {
                std::vector<BlockStateProperty> exProps;
                extractBlockStateProperties(existing, exProps);
                for (const auto& p : exProps) {
                    if (p.name == "turtle_egg_count") {
                        newCount = std::clamp(p.value + 2, 1, 4);
                        break;
                    }
                }
            }
            outStateData = static_cast<uint16_t>(newCount);
            outWorldBoxes = StateInspector::buildTurtleEggClusterWorldBoxes(placePos, newCount);
            return true;
        }
        if (StateInspector::isFlowerBedBlock(outFinalBlockId)) {
            int newCount = 1;
            Facing bedFacing = orient.facing;
            const void* existing = getNativeBlockPtr(placePos);
            if (existing && readBlockIdentifierFromBlock(existing) == outFinalBlockId) {
                std::vector<BlockStateProperty> exProps;
                extractBlockStateProperties(existing, exProps);
                const OrientationSummary exOrient = StateInspector::decodeOrientation(
                    outFinalBlockId, exProps, face, subClickPos, mPitchDeg, mYawDeg, mIsSneaking);
                bedFacing = exOrient.facing;
                for (const auto& p : exProps) {
                    if (p.name == "growth") {
                        newCount = std::clamp(p.value + 2, 1, 4);
                        break;
                    }
                }
            }
            outStateData = static_cast<uint16_t>((newCount << 2u) | static_cast<int>(bedFacing));
            const void* bedBlk = placedBlock;
            if (newCount > 1 && sig::isValidUserAddress(targetBlockType, alignof(void*))) {
                const void* perm = findFlowerBedPermutation(targetBlockType, newCount - 1, bedFacing);
                if (perm != nullptr) {
                    bedBlk = perm;
                }
            }
            AABB nativeBed{};
            if (bedBlk != nullptr && queryNativeFlowerBedOutline(bedBlk, placePos, nativeBed)) {
                outWorldBoxes = {nativeBed};
                return true;
            }
            const Vec3 rndOff = (bedBlk != nullptr) ? queryNativeBlockRandomOffset(bedBlk, placePos) : Vec3{};
            outWorldBoxes = StateInspector::buildFlowerBedClusterWorldBoxes(
                placePos, newCount, bedFacing, rndOff);
            return true;
        }

        // 3. Dynamic neighbor-aware Corner Stairs
        if (StateInspector::isStairBlock(outFinalBlockId)) {
            auto inspectNeighborStair = [&](const BlockPos& np, Facing& outDir) -> bool {
                const void* nb = getNativeBlockPtr(np);
                if (!nb) return false;
                const std::string nid = readBlockIdentifierFromBlock(nb);
                if (!StateInspector::isStairBlock(nid)) return false;
                std::vector<BlockStateProperty> nProps;
                extractBlockStateProperties(nb, nProps);
                const OrientationSummary nOrient = StateInspector::decodeOrientation(
                    nid, nProps, Facing::Up, Vec3{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f, false);
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

        // 4. Dynamic neighbor-aware Walls, Fences, Glass Panes, & Iron Bars
        if (StateInspector::isFenceOrWallOrPane(outFinalBlockId)) {
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
                placePos, outFinalBlockId, cN, cS, cW, cE);
            return true;
        }

        // 5. True 3D Oriented Models (16-angle standing signs, hanging signs, banners, skulls,
        //    angled wall torches, slanted lecterns) + multi-part models (hoppers, anvils, gates,
        //    cauldrons, campfires, lanterns, brewing stands, decorated pots)!
        const Vec3 nativeRndOff = (placedBlock != nullptr)
            ? queryNativeBlockRandomOffset(placedBlock, placePos)
            : Vec3{};

        auto withRndOff = [&](std::vector<AABB> boxes) -> std::vector<AABB> {
            if (std::fabs(nativeRndOff.x) > 1e-5f ||
                std::fabs(nativeRndOff.y) > 1e-5f ||
                std::fabs(nativeRndOff.z) > 1e-5f) {
                for (auto& b : boxes) {
                    b = b.translated(nativeRndOff);
                }
            }
            return boxes;
        };

        outOrientedBoxes = StateInspector::buildSpecialOrientedBoxes(placePos, outFinalBlockId, orient);
        if (!outOrientedBoxes.empty()) {
            outWorldBoxes = StateInspector::buildFallbackBlockBoxes(placePos, outFinalBlockId, orient);
            return true;
        }

        if (StateInspector::isTallPlantBlock(outFinalBlockId)) {
            if (placedBlock != nullptr) {
                AABB liveWorldBox{};
                if (queryLiveBlockWorldOutline(placedBlock, mBlockSource, placePos, liveWorldBox)) {
                    liveWorldBox.max.y = std::max(liveWorldBox.max.y, static_cast<float>(placePos.y) + 2.0f);
                    outWorldBoxes = {liveWorldBox};
                    return true;
                }
            }
            outWorldBoxes = withRndOff(StateInspector::buildFallbackBlockBoxes(placePos, outFinalBlockId, orient));
            return true;
        }

        const auto multiFallback = StateInspector::buildFallbackBlockBoxes(placePos, outFinalBlockId, orient);
        if (multiFallback.size() > 1) {
            outWorldBoxes = withRndOff(multiFallback);
            return true;
        }

        // 6. Live Engine Outline Query (`BlockType::getOutline` on the exact placed `const Block*`!)
        //    Automatically includes vanilla's per-cell RandomOffset for flowers, ferns, bamboo, etc.
        //    If live outline returns a generic full 1x1x1 default on a known sub-cube block
        //    (e.g., item frames, skulls, flower pots, conduits, heavy cores, sculk sensors,
        //    crops, wall signs), prefer the exact sub-cube geometry!
        if (placedBlock != nullptr) {
            AABB liveWorldBox{};
            if (queryLiveBlockWorldOutline(placedBlock, mBlockSource, placePos, liveWorldBox)) {
                if (liveWorldBox.volume() >= 0.99f &&
                    !multiFallback.empty() &&
                    multiFallback.front().volume() < 0.99f) {
                    outWorldBoxes = withRndOff(multiFallback);
                    return true;
                }
                outWorldBoxes = {liveWorldBox};
                return true;
            }
        }

        outWorldBoxes = withRndOff(multiFallback);
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
    bool mIsSneaking{false};
    mutable NativeChunkPos mCachedChunkPos{INT32_MIN, INT32_MIN};
    mutable const void* mCachedChunkPtr{nullptr};
};

void onPostDebugRenderEnd(void* debugRenderer, void* renderParams, FnDebugRenderEnd orig) {
    if (!orig || !sig::isValidUserAddress(debugRenderer, alignof(void*)) ||
        !sig::isValidUserAddress(renderParams, alignof(void*))) {
        return;
    }

    gFramesEvaluated.fetch_add(1, std::memory_order_relaxed);

    const auto now = std::chrono::steady_clock::now();
    float dtSeconds = 1.0f / 60.0f;
    if (gHasLastFrameTime) {
        dtSeconds = std::chrono::duration<float>(now - gLastFrameTime).count();
    }
    gLastFrameTime = now;
    gHasLastFrameTime = true;

    const ModConfig cfg = ConfigManager::instance().snapshot();
    if (!cfg.enabled) {
        gAnimator.reset();
        return;
    }

    auto* dbgBytes = static_cast<uint8_t*>(debugRenderer);
    void* levelRenderer = dbgBytes - sig::off::LevelRenderer_DebugRenderer;
    void* lrp = nullptr;
    if (!readFieldSafe(levelRenderer, sig::off::LevelRenderer_LRPPtr, lrp) ||
        !sig::isValidUserAddress(lrp, alignof(void*))) {
        gAnimator.reset();
        return;
    }

    void* clientInst = nullptr;
    void* level = nullptr;
    if (!readFieldSafe(lrp, sig::off::LRP_ClientInstancePtr, clientInst) ||
        !sig::isValidUserAddress(clientInst, alignof(void*)) ||
        !readFieldSafe(lrp, sig::off::LRP_LevelPtr, level) ||
        !sig::isValidUserAddress(level, alignof(void*))) {
        gAnimator.reset();
        return;
    }

    const auto fnGetCamActor = readVtableFn<FnClientGetCamActor>(clientInst, sig::off::ClientInstance_VtGetCamActor);
    const auto fnGetHitResult = readVtableFn<FnLevelGetHitResult>(level, sig::off::Level_VtGetHitResult);
    if (!fnGetCamActor || !fnGetHitResult) {
        gAnimator.reset();
        return;
    }

    void* camActor = fnGetCamActor(clientInst);
    if (!sig::isValidUserAddress(camActor, alignof(void*))) {
        gAnimator.reset();
        return;
    }

    int32_t gameType = -1;
    if (gApi.entityGetPlayerGameType) {
        const auto* entityCtx = static_cast<const uint8_t*>(camActor) + sig::off::Actor_EntityContext;
        gameType = gApi.entityGetPlayerGameType(entityCtx);
        if (gameType == 5) {
            const void* actorLevel = nullptr;
            if (!readFieldSafe(camActor, sig::off::Actor_LevelPtr, actorLevel) ||
                !sig::isValidUserAddress(actorLevel, alignof(void*))) {
                actorLevel = level;
            }
            const auto fnDefGt = readVtableFn<FnLevelGetDefGameType>(actorLevel, sig::off::Level_VtGetDefaultGameType);
            if (fnDefGt) {
                gameType = fnDefGt(actorLevel);
            }
        }
    }

    // Immediate disappearance in Spectator mode.
    if (gameType == 6) {
        gAnimator.reset();
        return;
    }

    const bool isCreative = (gameType == 1) || (gApi.playerIsCreative && gApi.playerIsCreative(camActor));
    const bool isAdventure = (gameType == 2);
    const float maxReachBlocks = isCreative ? 11.5f : 5.5f;

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
        gAnimator.reset();
        return;
    }

    int16_t rawMaxH = 320;
    int16_t rawMinH = -64;
    readFieldSafe(blockSource, sig::off::BlockSource_MaxHeight, rawMaxH);
    readFieldSafe(blockSource, sig::off::BlockSource_MinHeight, rawMinH);
    const int minBuildY = (rawMinH >= -2048 && rawMinH < rawMaxH) ? static_cast<int>(rawMinH) : -64;
    const int maxBuildY = (rawMaxH > rawMinH && rawMaxH <= 4096) ? static_cast<int>(rawMaxH) - 1 : 319;

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

    Vec3 playerEyePos = playerAabb.center();
    const bool isSneaking = (playerAabb.sizeY() > 0.5f && playerAabb.sizeY() < 1.65f);
    if (playerAabb.volume() > 0.0f) {
        playerEyePos = Vec3{
            playerAabb.center().x,
            playerAabb.min.y + (isSneaking ? 1.54f : 1.62f),
            playerAabb.center().z
        };
    }

    Vec3 renderCameraPos = playerEyePos;
    const void* screenCtx = nullptr;
    if (readFieldSafe(renderParams, sig::off::RenderParams_ScreenContext, screenCtx) &&
        sig::isValidUserAddress(screenCtx, alignof(void*))) {
        const void* camData = nullptr;
        if (readFieldSafe(screenCtx, sig::off::ScreenContext_CameraData, camData) &&
            sig::isValidUserAddress(camData, alignof(void*))) {
            Vec3 rawCam{};
            if (readFieldSafe(camData, sig::off::CameraData_WorldPos, rawCam) &&
                std::isfinite(rawCam.x) && std::isfinite(rawCam.y) && std::isfinite(rawCam.z)) {
                renderCameraPos = rawCam;
            }
        }
    }

    const auto fnCarried = readVtableFn<FnActorGetItemStack>(camActor, sig::off::Actor_VtGetCarriedItem);
    PlayerContextInput ctx{};
    ctx.inWorld = true;
    ctx.isSpectator = false;
    ctx.isAdventureMode = isAdventure;
    ctx.isSneaking = isSneaking;
    ctx.pitchDeg = pitchDeg;
    ctx.yawDeg = yawDeg;
    ctx.playerAabb = playerAabb;
    if (fnCarried) {
        ctx.mainhand = inspectItemStack(fnCarried(camActor));
    }

    PlacementPreviewState rawPreview{};
    if (ctx.mainhand.valid && ctx.mainhand.count > 0) {
        const Vec3 rayOrigin = ((renderCameraPos - playerEyePos).lengthSq() <= 0.85f * 0.85f)
                                   ? renderCameraPos
                                   : playerEyePos;
        const Vec3 lookDir = PlacementResolver::lookDirectionFromAngles(pitchDeg, yawDeg);

        LiveWorldQuery world(blockSource, camActor, minBuildY, maxBuildY, pitchDeg, yawDeg, isSneaking);

        bool usedAlignedEngineHit = false;
        const void* hitRes = fnGetHitResult(level);
        if (sig::isValidUserAddress(hitRes, alignof(void*))) {
            int32_t hitType = 3;
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

            if (hitType == 0 &&
                std::isfinite(hitPosRaw[0]) &&
                std::isfinite(hitPosRaw[1]) &&
                std::isfinite(hitPosRaw[2])) {
                const Vec3 hitVec{hitPosRaw[0], hitPosRaw[1], hitPosRaw[2]};
                const Vec3 delta = hitVec - rayOrigin;
                const float dist = delta.length();
                if (dist > 0.05f && dist <= maxReachBlocks + 0.75f) {
                    const float cosAngle = (delta.x * lookDir.x + delta.y * lookDir.y + delta.z * lookDir.z) / dist;
                    const BlockPos engineHitPos{hitBlockRaw[0], hitBlockRaw[1], hitBlockRaw[2]};
                    const std::string engineHitId = world.getBlockIdentifier(engineHitPos);
                    if (cosAngle >= 0.996f &&
                        !StateInspector::isLiquidOrAirBlock(engineHitId) &&
                        StateInspector::stripNamespace(engineHitId) != "barrier") {
                        ctx.raycast.hasBlockHit = true;
                        ctx.raycast.hitFace = (hitFaceByte <= 5u) ? static_cast<Facing>(hitFaceByte) : Facing::Up;
                        ctx.raycast.blockPos = engineHitPos;
                        ctx.raycast.hitWorldPos = hitVec;
                        ctx.raycast.hasLiquidHit = (hitLiquidByte != 0);
                        ctx.raycast.liquidFace = (hitLiquidFaceByte <= 5u) ? static_cast<Facing>(hitLiquidFaceByte) : Facing::Up;
                        ctx.raycast.liquidPos = {hitLiquidPosRaw[0], hitLiquidPosRaw[1], hitLiquidPosRaw[2]};
                        ctx.raycast.liquidWorldPos = ctx.raycast.liquidPos.centerVec();
                        usedAlignedEngineHit = true;
                    }
                }
            }
        }

        if (!usedAlignedEngineHit) {
            ctx.raycast = PlacementResolver::raycastWorld(rayOrigin, lookDir, maxReachBlocks, world);
        }

        if (ctx.raycast.hasBlockHit || ctx.raycast.hasLiquidHit) {
            rawPreview = PlacementResolver::resolve(ctx, world, cfg);
        }
    }

    const PlacementPreviewState animatedPreview = gAnimator.update(rawPreview, cfg, dtSeconds);
    if (!animatedPreview.valid ||
        (animatedPreview.worldBoxes.empty() && animatedPreview.orientedBoxes.empty())) {
        return;
    }

    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count();
    const float pulsePhase = static_cast<float>(nowMs % 3142LL) * 0.002f;

    GhostMesher::buildMesh(animatedPreview, cfg, renderCameraPos, pulsePhase, gFrameMesh);
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

    orig(debugRenderer, renderParams);

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
        fastApi.playerIsCreative = reinterpret_cast<FnPlayerIsCreative>(
            resolveRvaChecked(base, sig::rva::Player_isCreative, sig::rva::Player_isCreative_W10));
        fastApi.entityGetPlayerGameType = reinterpret_cast<FnEntityGetGameType>(
            resolveRvaChecked(base, sig::rva::Entity_getPlayerGameType, sig::rva::Entity_getPlayerGameType_W10));

        const void* regPtr = nullptr;
        if (readInsnWord(base + sig::rva::BlockTypeRegistry_getter) == sig::rva::BlockTypeRegistry_getter_W0 &&
            readInsnWord(base + sig::rva::BlockTypeRegistry_getter + 4) == sig::rva::BlockTypeRegistry_getter_W4 &&
            readFieldSafe(reinterpret_cast<const void*>(base + sig::rva::BlockTypeRegistry_ptr), 0, regPtr) &&
            sig::isValidUserAddress(regPtr, alignof(void*))) {
            fastApi.blockTypeRegistry = regPtr;
        }
        fastApi.blockGetRandomOffset = reinterpret_cast<FnBlockGetRandomOffset>(
            resolveRvaChecked(base, sig::rva::Block_getRandomOffset, sig::rva::Block_getRandomOffset_W10));
        fastApi.flowerBedTryGetComp = reinterpret_cast<FnFlowerBedTryGetComp>(
            resolveRvaChecked(base, sig::rva::FlowerBed_tryGetComp, sig::rva::FlowerBed_tryGetComp_W10));
        fastApi.flowerBedGetOutline = reinterpret_cast<FnFlowerBedGetOutline>(
            resolveRvaChecked(base, sig::rva::FlowerBed_getOutline, sig::rva::FlowerBed_getOutline_W10));
        fastApi.blockPlanterGetId = reinterpret_cast<FnBlockPlanterGetId>(
            resolveRvaChecked(base, sig::rva::BlockPlanterComp_getId, sig::rva::BlockPlanterComp_getId_W10));
        fastApi.itemStackGetComp = reinterpret_cast<FnItemStackGetComp>(
            resolveRvaChecked(base, sig::rva::ItemStack_getComp, sig::rva::ItemStack_getComp_W10));

        if (attachDebugRenderHookLocked(fastApi)) {
            return true;
        }
    }

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
        const uintptr_t creativeBody = pl::memory::resolveSignature(
            sig::pattern::Player_isCreative_Body10, "libminecraftpe.so");
        const uintptr_t gameTypeBody = pl::memory::resolveSignature(
            sig::pattern::Entity_getPlayerGameType_Body10, "libminecraftpe.so");

        ResolvedEngineApi scanned{};
        scanned.fastRva = false;
        scanned.targetDebugRenderEnd = reinterpret_cast<void*>(renderEndBody - 0x10);
        scanned.lcGetBlock = reinterpret_cast<FnLevelChunkGetBlock>(lcBody - 0x10);
        if (dimBsBody > 0x10) {
            scanned.actorGetBlockSource = reinterpret_cast<FnActorGetDimBlockSrc>(dimBsBody - 0x10);
        }
        if (creativeBody > 0x10) {
            scanned.playerIsCreative = reinterpret_cast<FnPlayerIsCreative>(creativeBody - 0x10);
        }
        if (gameTypeBody > 0x10) {
            scanned.entityGetPlayerGameType = reinterpret_cast<FnEntityGetGameType>(gameTypeBody - 0x10);
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
    for (int spin = 0; spin < 200 && gActiveHookCalls.load(std::memory_order_acquire) > 0; ++spin) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#else
    gHooksInstalled.store(false, std::memory_order_release);
#endif
    gFrameMesh.clear();
    gAnimator.reset();
    gRegistryCache.clear();
    gHasLastFrameTime = false;
}

void EngineBridge::notifyConfigUpdated(const ModConfig& /*cfg*/) noexcept {}

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
    auto* enabledPtr    = reinterpret_cast<uint8_t*>(dbgBytes + sig::off::DebugRenderer_EnabledByte);
    auto* lineVecPtrs   = reinterpret_cast<const PackedVertex**>(dbgBytes + sig::off::DebugRenderer_LineVec);
    auto* triVecPtrs    = reinterpret_cast<const PackedVertex**>(dbgBytes + sig::off::DebugRenderer_TriVec);

    const uint8_t savedEnabled       = *enabledPtr;
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
        *enabledPtr    = 1u;
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
        *enabledPtr    = 1u;
        origFn(debugRenderer, renderParams);
    }

    lineVecPtrs[0] = savedLine[0];
    lineVecPtrs[1] = savedLine[1];
    lineVecPtrs[2] = savedLine[2];
    triVecPtrs[0]  = savedTri[0];
    triVecPtrs[1]  = savedTri[1];
    triVecPtrs[2]  = savedTri[2];
    *enabledPtr    = savedEnabled;
    *activeFlagPtr = 0u;
}

} // namespace placepreview
