#include "buildghost/EngineBridge.hpp"

#include "buildghost/GhostMesher.hpp"
#include "buildghost/PlacementResolver.hpp"
#include "buildghost/StateInspector.hpp"
#include "buildghost/Version.hpp"

#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace buildghost {

namespace {

inline constexpr std::string_view kGameModule = "libminecraftpe.so";

// Every single signature below is verified [1/1] unique across ALL 328,419,680 bytes of
// Minecraft: Bedrock Edition 1.26.52.03 (arm64-v8a) at exact function entry + 0x10:
//
// 1. DebugRenderer::renderEnd (fn = 0x0b25de34, +0x10 = 0x0b25de44)
inline constexpr std::string_view kSigDebugRendererRenderEndPlus10 =
    "F6 57 0A A9 F4 4F 0B A9 FD C3 01 91 58 D0 3B D5 F4 03 01 AA F5 03 00 AA 08 17 40 F9 A8 83 1F F8 08 24 45 A9 1F 20 00 39";

// 2. Actor::isSpectator (fn = 0x0f58e6c8, +0x10 = 0x0f58e6d8)
inline constexpr std::string_view kSigActorIsSpectatorPlus10 =
    "0B 08 40 F9 0C A9 8B 52 0E 18 40 B9 EC 16 AD 72 69 A1 43 A9 08 01 09 CB 0A FD 43 D3";

// 3. ItemStackBase::checkIsValidCanPlaceOn (fn = 0x0ff83ac8, +0x10 = 0x0ff83ad8)
inline constexpr std::string_view kSigItemCheckCanPlaceOnPlus10 =
    "F4 4F 02 A9 FD 03 00 91 F4 03 00 AA 20 01 40 F9 E0 00 00 B4 29 04 40 F9 29 0A 00 B4";

// 4. BlockComponentStorage::tryGetComponent (fn = 0x101272bc, +0x10 = 0x101272cc)
inline constexpr std::string_view kSigBlockCompTryGetPlus10 =
    "08 00 40 39 F4 03 00 AA F3 03 01 2A E8 00 00 37 D5 9B 9F 94 A7 9B 9F 94 80 00 00 37";

// 5. BlockPlayerPlacingEvent::BlockPlayerPlacingEvent (fn = 0x101275e8, +0x10 = 0x101275f8)
inline constexpr std::string_view kSigBlockPlacingEventCtorPlus10 =
    "FD 03 00 91 E8 03 02 AA F8 03 01 AA 62 7C 40 92 E1 03 08 AA F3 03 07 AA F6 03 06 AA";

// 6. BlockTypeRegistry::getSingleton (fn = 0x103e33a8, +0x10 = 0x103e33b8)
inline constexpr std::string_view kSigBlockRegistrySingletonPlus10 =
    "00 E0 10 91 C0 03 5F D6 FD 7B BD A9 F5 0B 00 F9 F4 4F 02 A9 FD 03 00 91 F3 03 08 AA";

// 7. BlockTypeRegistry::lookupDefaultBlock (fn = 0x103e5b7c, +0x10 = 0x103e5b8c)
inline constexpr std::string_view kSigBlockRegistryLookupDefaultPlus10 =
    "FD C3 00 91 56 D0 3B D5 F4 03 01 AA F3 03 00 AA C8 16 40 F9 A8 83 1F F8 09 A0 54 A9 E9 A3 01 A9 88 00 00 B4 01 21 00 91 20 00 80 52 6A 0D 05 96";

// Exact 40-byte HashedString layout verified at 0x1282d784..0x1282da98:
//   +0x00 uint64_t mStrHash (FNV-1 64-bit: hash = (hash * 0x100000001b3ULL) ^ byte)
//   +0x08 std::string mStr  (24 bytes in Android NDK libc++)
//   +0x20 const McHashedString* mLastMatch = nullptr
struct McHashedString {
    uint64_t mStrHash{0};
    std::string mStr{};
    const McHashedString* mLastMatch{nullptr};

    explicit McHashedString(std::string_view s) : mStr(s) {
        if (!mStr.empty()) {
            uint64_t h = 0xcbf29ce484222325ULL;
            for (unsigned char c : mStr) {
                h *= 0x00000100000001b3ULL;
                h ^= static_cast<uint64_t>(c);
            }
            mStrHash = h;
        }
    }
};
static_assert(sizeof(McHashedString) == 0x28, "McHashedString must match 40-byte Bedrock ABI");

// AAPCS64 indirect result struct (>16 bytes -> passed in register x8).
struct EventDispatchResult {
    uint64_t words[4]{};
};
static_assert(sizeof(EventDispatchResult) == 32);

using DebugRenderEndFn = void (*)(void* debugRenderer, void* renderParams);
using ActorIsSpectatorFn = bool (*)(const void* actor);
using ItemCheckCanPlaceOnFn = bool (*)(const void* itemStack, const void* block);
using BlockCompTryGetFn = void* (*)(void* compStorage, int compTypeId);
using BlockPlacingEventCtorFn = void (*)(
    void* evBuf,
    void* blockSource,
    const BlockPos* pos,
    const void* block,
    void* actor,
    uint8_t face,
    const Vec3* subClick,
    const void* itemStack);
using BlockRegistryGetSingletonFn = void* (*)();
using BlockRegistryLookupDefaultFn = const void* (*)(
    void* registry,
    const McHashedString* hashedName,
    bool allowExperimental);

void* gRenderEndTarget = nullptr;
DebugRenderEndFn gOrigRenderEnd = nullptr;

ActorIsSpectatorFn gActorIsSpectator = nullptr;
ItemCheckCanPlaceOnFn gItemCheckCanPlaceOn = nullptr;
BlockCompTryGetFn gBlockCompTryGet = nullptr;
BlockPlacingEventCtorFn gBlockPlacingEventCtor = nullptr;
BlockRegistryGetSingletonFn gBlockRegistrySingleton = nullptr;
BlockRegistryLookupDefaultFn gBlockRegistryLookupDefault = nullptr;

uintptr_t resolvePlus10Signature(std::string_view sig) noexcept {
    const uintptr_t hit = pl::memory::resolveSignature(sig, kGameModule);
    return (hit > 0x10u) ? (hit - 0x10u) : 0u;
}

template <typename FnPtr>
FnPtr vtableSlot(const void* obj, std::size_t slotIndex) noexcept {
    if (obj == nullptr) return nullptr;
    const auto* vtbl = *reinterpret_cast<const uintptr_t* const*>(obj);
    if (vtbl == nullptr) return nullptr;
    return reinterpret_cast<FnPtr>(vtbl[slotIndex]);
}

const void* getBlockTypeFromBlock(const void* block) noexcept {
    if (block == nullptr) return nullptr;
    return *reinterpret_cast<const void* const*>(static_cast<const uint8_t*>(block) + 0x68);
}

std::string readBlockIdentifier(const void* block) {
    const void* bt = getBlockTypeFromBlock(block);
    if (bt == nullptr) return {};
    const auto& str = *reinterpret_cast<const std::string*>(static_cast<const uint8_t*>(bt) + 0xd0);
    return str;
}

const void* lookupDefaultBlockByName(std::string_view fullName) {
    if (fullName.empty() || gBlockRegistrySingleton == nullptr || gBlockRegistryLookupDefault == nullptr) {
        return nullptr;
    }
    void* reg = gBlockRegistrySingleton();
    if (reg == nullptr) return nullptr;
    const McHashedString hs(fullName);
    return gBlockRegistryLookupDefault(reg, &hs, true);
}

// Walks BlockType's mStates RB-tree (+0x1b0) and mAlteredStateCollections vector (+0x350..+0x358)
// verified instruction-by-instruction at 0x10109504..0x1010953c & 0x1010c118..0x1010c1bc.
void extractBlockStateProperties(
    const void* placedBlock,
    std::vector<BlockStateProperty>& outProps) {

    outProps.clear();
    const void* bt = getBlockTypeFromBlock(placedBlock);
    if (placedBlock == nullptr || bt == nullptr) return;

    const auto* btBytes = static_cast<const uint8_t*>(bt);
    const uint16_t mData = *reinterpret_cast<const uint16_t*>(
        static_cast<const uint8_t*>(placedBlock) + 0x128);

    // 1. Traverse mStates RB-tree rooted at *(bt + 0x1b0) with depth cap for total safety.
    struct StackEntry {
        const uint8_t* node;
        int depth;
    };
    const auto* root = *reinterpret_cast<const uint8_t* const*>(btBytes + 0x1b0);
    if (root != nullptr) {
        std::vector<StackEntry> stack;
        stack.reserve(16);
        stack.push_back({root, 0});
        while (!stack.empty()) {
            const StackEntry cur = stack.back();
            stack.pop_back();
            if (cur.node == nullptr || cur.depth > 16) continue;

            const auto* left = *reinterpret_cast<const uint8_t* const*>(cur.node + 0x00);
            const auto* right = *reinterpret_cast<const uint8_t* const*>(cur.node + 0x08);
            const uint32_t endBit = *reinterpret_cast<const uint32_t*>(cur.node + 0x28);
            const uint32_t numBits = *reinterpret_cast<const uint32_t*>(cur.node + 0x2c);
            const uint32_t mask = *reinterpret_cast<const uint32_t*>(cur.node + 0x38);
            const auto* stateObj = *reinterpret_cast<const uint8_t* const*>(cur.node + 0x48);

            if (stateObj != nullptr && numBits > 0 && numBits <= 16 && (1u + endBit) >= numBits) {
                const uint32_t shift = 1u + endBit - numBits;
                const int val = static_cast<int>((static_cast<uint32_t>(mData) & mask) >> shift);
                // BlockState + 0x18 = McHashedString -> +0x20 = std::string!
                const auto& stateName = *reinterpret_cast<const std::string*>(stateObj + 0x20);
                if (!stateName.empty() && stateName.size() < 96) {
                    outProps.push_back(BlockStateProperty{stateName, val});
                }
            }

            if (right != nullptr) stack.push_back({right, cur.depth + 1});
            if (left != nullptr) stack.push_back({left, cur.depth + 1});
        }
    }

    // 2. Traverse mAlteredStateCollections vector at [bt + 0x350 .. bt + 0x358] (stride 16 bytes).
    const uintptr_t vecBegin = *reinterpret_cast<const uintptr_t*>(btBytes + 0x350);
    const uintptr_t vecEnd = *reinterpret_cast<const uintptr_t*>(btBytes + 0x358);
    if (vecBegin != 0 && vecEnd > vecBegin && (vecEnd - vecBegin) <= 16u * 32u) {
        for (uintptr_t it = vecBegin; it < vecEnd; it += 0x10u) {
            const void* col = *reinterpret_cast<const void* const*>(it);
            if (col == nullptr) continue;
            const auto* stateObj = *reinterpret_cast<const uint8_t* const*>(
                static_cast<const uint8_t*>(col) + 0x08);
            if (stateObj == nullptr) continue;

            using GetStateFn = int (*)(const void* collection, const void* block);
            const auto getValFn = vtableSlot<GetStateFn>(col, 0);
            if (getValFn == nullptr) continue;

            const auto& stateName = *reinterpret_cast<const std::string*>(stateObj + 0x20);
            if (!stateName.empty() && stateName.size() < 96) {
                outProps.push_back(BlockStateProperty{stateName, getValFn(col, placedBlock)});
            }
        }
    }
}

// Extracts world-space multi-AABB collision boxes for `block` at `pos` using BlockType::vtbl[7] (+0x38),
// falling back to BlockType::vtbl[9] (+0x48, getVisualShapeInWorld) for zero-collision blocks
// (torches, buttons, levers, rails, signs, flowers, carpets, crops).
void extractBlockWorldBoxes(
    const void* block,
    const void* blockSource,
    const BlockPos& pos,
    std::vector<AABB>& outBoxes) {

    outBoxes.clear();
    const void* bt = getBlockTypeFromBlock(block);
    if (block == nullptr || bt == nullptr || blockSource == nullptr) {
        return;
    }

    // 1. Primary multi-box collision geometry: BlockType::vtbl[7] (+0x38, verified at 0x101fb340).
    using AddAABBsFn = void (*)(
        const void* blockType,
        const void* blockSelf,
        const void* region,
        const BlockPos* blockPos,
        const AABB* intersectMask,
        std::vector<AABB>* outList);

    const Vec3 o = pos.originVec();
    const AABB broadMask{
        {o.x - 4.0f, o.y - 4.0f, o.z - 4.0f},
        {o.x + 5.0f, o.y + 5.0f, o.z + 5.0f}
    };

    if (const auto addAABBs = vtableSlot<AddAABBsFn>(bt, 7); addAABBs != nullptr) {
        addAABBs(bt, block, blockSource, &pos, &broadMask, &outBoxes);
    }

    // Clamp & filter degenerate AABBs (e.g., fence/wall height extensions > 1.0 clipped cleanly to 1.0
    // so visual ghost height matches exact 1.0 block model height rather than 1.5 invisible barrier!).
    if (!outBoxes.empty()) {
        std::vector<AABB> cleaned;
        cleaned.reserve(outBoxes.size());
        for (AABB b : outBoxes) {
            b = b.normalized();
            if (!b.isDegenerate()) {
                // Fences & Walls have 1.5-tall collision boxes in Minecraft; clamp visual top to +1.0!
                if (b.max.y > o.y + 1.01f && b.max.y <= o.y + 1.55f) {
                    b.max.y = o.y + 1.0f;
                }
                if (!b.isDegenerate()) {
                    cleaned.push_back(b);
                }
            }
        }
        if (!cleaned.empty()) {
            outBoxes = std::move(cleaned);
            return;
        }
        outBoxes.clear();
    }

    // 2. Zero-collision block fallback: BlockType::vtbl[9] (+0x48, getVisualShapeInWorld, 0x101fb388).
    // Returns [0..1]-space local visual box -> translate by pos.originVec()!
    using GetVisualShapeInWorldFn = const AABB& (*)(
        const void* blockType,
        const void* blockSelf,
        const void* region,
        const BlockPos* blockPos,
        AABB* scratchBox);

    if (const auto getVisWorld = vtableSlot<GetVisualShapeInWorldFn>(bt, 9); getVisWorld != nullptr) {
        AABB scratch{};
        const AABB& localShape = getVisWorld(bt, block, blockSource, &pos, &scratch);
        AABB normLocal = localShape.normalized();
        if (!normLocal.isDegenerate()) {
            // Check whether returned box is block-local [0..1] vs already world-translated.
            const bool looksLocal = (normLocal.min.x >= -1.0f && normLocal.max.x <= 2.0f &&
                                     normLocal.min.y >= -1.0f && normLocal.max.y <= 2.5f &&
                                     normLocal.min.z >= -1.0f && normLocal.max.z <= 2.0f &&
                                     (pos.x != 0 || pos.y != 0 || pos.z != 0));
            outBoxes.push_back(looksLocal ? normLocal.translated(o) : normLocal);
        }
    }
}

HeldItemInput inspectItemStack(const void* stackPtr) {
    HeldItemInput out{};
    if (stackPtr == nullptr) return out;

    const auto* bytes = static_cast<const uint8_t*>(stackPtr);
    const bool valid = (bytes[0x23] != 0);
    const uint8_t count = bytes[0x22];
    if (!valid || count == 0) {
        return out;
    }

    const void* weakHolder = *reinterpret_cast<const void* const*>(bytes + 0x08);
    if (weakHolder == nullptr) return out;
    const void* itemObj = *reinterpret_cast<const void* const*>(weakHolder);
    if (itemObj == nullptr) return out;

    const int16_t rawAux = *reinterpret_cast<const int16_t*>(bytes + 0x20);
    out.valid = true;
    out.count = static_cast<int>(count);
    out.auxValue = (rawAux == 0x7fff) ? 0 : static_cast<int>(rawAux);
    out.nativeItemStackPtr = reinterpret_cast<uintptr_t>(stackPtr);
    out.itemIdentifier = *reinterpret_cast<const std::string*>(
        static_cast<const uint8_t*>(itemObj) + 0xd0);

    // Direct Block* stored on ItemStackBase (+0x18) when item represents a BlockItem!
    const void* directBlock = *reinterpret_cast<const void* const*>(bytes + 0x18);
    if (directBlock != nullptr) {
        out.directBlockIdentifier = readBlockIdentifier(directBlock);
    }
    return out;
}

class LiveBedrockWorldQuery final : public IWorldQuery {
public:
    LiveBedrockWorldQuery(void* blockSource, void* actor) noexcept
        : blockSource_(blockSource), actor_(actor) {}

    [[nodiscard]] int minBuildHeight() const noexcept override {
        return -64;
    }

    [[nodiscard]] int maxBuildHeight() const noexcept override {
        return 319;
    }

    [[nodiscard]] const void* getWorldBlock(const BlockPos& pos) const noexcept {
        using GetBlockFn = const void* (*)(const void* region, const BlockPos* p);
        const auto fn = vtableSlot<GetBlockFn>(blockSource_, 2);
        return (fn != nullptr) ? fn(blockSource_, &pos) : nullptr;
    }

    [[nodiscard]] std::string getBlockIdentifier(const BlockPos& pos) const override {
        return readBlockIdentifier(getWorldBlock(pos));
    }

    [[nodiscard]] bool isAirBlock(const BlockPos& pos) const override {
        const std::string id = getBlockIdentifier(pos);
        if (id.empty()) return true;
        const std::string_view bare = StateInspector::stripNamespace(id);
        return bare == "air" || bare == "cave_air" || bare == "void_air" || bare == "light_block";
    }

    [[nodiscard]] bool isReplaceableBlock(const BlockPos& pos) const override {
        const void* b = getWorldBlock(pos);
        if (b == nullptr) return false;
        const std::string id = readBlockIdentifier(b);
        const std::string_view bare = StateInspector::stripNamespace(id);
        if (bare == "air" || bare == "cave_air" || bare == "void_air" || bare == "light_block" ||
            bare == "water" || bare == "flowing_water" || bare == "lava" || bare == "flowing_lava" ||
            bare == "short_grass" || bare == "tall_grass" || bare == "tallgrass" ||
            bare == "fern" || bare == "large_fern" || bare == "deadbush" || bare == "vine" ||
            bare == "glow_lichen" || bare == "sculk_vein" || bare == "snow_layer" ||
            bare == "fire" || bare == "soul_fire" || bare == "seagrass" || bare == "crimson_roots" ||
            bare == "warped_roots" || bare == "nether_sprouts" || bare == "leaf_litter" ||
            bare == "wildflowers" || bare == "bush" || bare == "firefly_bush") {
            return true;
        }
        return false;
    }

    [[nodiscard]] std::vector<AABB> getExistingBlockWorldBoxes(const BlockPos& pos) const override {
        std::vector<AABB> boxes;
        const void* existing = getWorldBlock(pos);
        if (existing != nullptr) {
            extractBlockWorldBoxes(existing, blockSource_, pos, boxes);
        }
        return boxes;
    }

    [[nodiscard]] bool checkAdventureCanPlaceOn(
        const HeldItemInput& item,
        const BlockPos& targetBlockPos) const override {
        if (gItemCheckCanPlaceOn == nullptr || item.nativeItemStackPtr == 0) {
            return true;
        }
        const void* targetBlock = getWorldBlock(targetBlockPos);
        if (targetBlock == nullptr) return false;
        return gItemCheckCanPlaceOn(
            reinterpret_cast<const void*>(item.nativeItemStackPtr), targetBlock);
    }

    [[nodiscard]] bool calculatePlacePos(
        const HeldItemInput& item,
        Facing& inOutFace,
        BlockPos& inOutPos) const override {

        if (item.nativeItemStackPtr != 0 && actor_ != nullptr) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(item.nativeItemStackPtr);
            const void* weakHolder = *reinterpret_cast<const void* const*>(bytes + 0x08);
            const void* itemObj = (weakHolder != nullptr)
                                      ? *reinterpret_cast<const void* const*>(weakHolder)
                                      : nullptr;
            // Item::vtable[129] (+0x408): bool _calculatePlacePos(Item const*, ItemStackBase&, Actor&, uint8_t&, BlockPos&)
            using CalcPlacePosFn = bool (*)(
                const void* itemSelf,
                void* itemStack,
                void* actor,
                uint8_t* ioFace,
                BlockPos* ioPos);
            if (const auto calcFn = vtableSlot<CalcPlacePosFn>(itemObj, 129); calcFn != nullptr) {
                uint8_t rawFace = static_cast<uint8_t>(inOutFace);
                BlockPos localPos = inOutPos;
                const bool ok = calcFn(
                    itemObj,
                    reinterpret_cast<void*>(item.nativeItemStackPtr),
                    actor_,
                    &rawFace,
                    &localPos);
                if (!ok) {
                    return false;
                }
                inOutFace = static_cast<Facing>(rawFace % 6u);
                inOutPos = localPos;
                return true;
            }
        }

        // Pure fallback step if custom item omits Item::_calculatePlacePos
        if (!isReplaceableBlock(inOutPos)) {
            const BlockPos d = facingStep(inOutFace);
            inOutPos = inOutPos.offset(d.x, d.y, d.z);
        }
        return true;
    }

    [[nodiscard]] const void* resolveCandidateDefaultBlock(
        const HeldItemInput& item,
        const std::string& resolvedBlockIdentifier) const {

        if (item.nativeItemStackPtr != 0) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(item.nativeItemStackPtr);
            const void* stackBlock = *reinterpret_cast<const void* const*>(bytes + 0x18);
            if (stackBlock != nullptr) {
                return stackBlock;
            }
        }
        return lookupDefaultBlockByName(resolvedBlockIdentifier);
    }

    [[nodiscard]] bool mayPlaceBlock(
        const HeldItemInput& item,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing face,
        const Vec3& worldClickPos) const override {

        const void* defaultBlock = resolveCandidateDefaultBlock(item, resolvedBlockIdentifier);
        if (defaultBlock == nullptr || blockSource_ == nullptr) {
            return false;
        }

        // BlockSource::vtable[57] (+0x1c8):
        // bool mayPlace(BlockSource*, Block const&, BlockPos const&, uint8_t face, Actor* placer,
        //               bool ignoreEntities, Vec3 clickPos /*s0,s1,s2*/)
        using MayPlaceFn = bool (*)(
            void* region,
            const void* block,
            const BlockPos* pos,
            uint8_t face,
            void* placer,
            bool ignoreEntities,
            Vec3 clickPos);

        const auto mayPlace = vtableSlot<MayPlaceFn>(blockSource_, 57);
        if (mayPlace == nullptr) {
            return false;
        }
        return mayPlace(
            blockSource_,
            defaultBlock,
            &placePos,
            static_cast<uint8_t>(face),
            actor_,
            /*ignoreEntities=*/false,
            worldClickPos);
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
        std::vector<AABB>& outWorldBoxes) const override {

        const void* baseBlock = resolveCandidateDefaultBlock(item, resolvedBlockIdentifier);
        const void* bt = getBlockTypeFromBlock(baseBlock);
        if (baseBlock == nullptr || bt == nullptr || actor_ == nullptr || blockSource_ == nullptr) {
            return false;
        }

        // Compute placement aux/level data via Item::vtable[35] (+0x118, getLevelDataForAuxValue).
        int placementData = item.auxValue;
        if (item.nativeItemStackPtr != 0) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(item.nativeItemStackPtr);
            const void* weakHolder = *reinterpret_cast<const void* const*>(bytes + 0x08);
            const void* itemObj = (weakHolder != nullptr)
                                      ? *reinterpret_cast<const void* const*>(weakHolder)
                                      : nullptr;
            using GetLevelDataFn = int (*)(const void* itemSelf, int aux);
            if (const auto getDataFn = vtableSlot<GetLevelDataFn>(itemObj, 35); getDataFn != nullptr) {
                placementData = getDataFn(itemObj, item.auxValue);
            }
        }

        // Step A: BlockType::vtable[79] (+0x278, Block::getPlacementBlock wrapper at 0x101fc250):
        // const Block& getPlacementBlock(const BlockType*, const Actor&, const BlockPos&, uint8_t face,
        //                                const Vec3& clickPos, int itemValue)
        using GetPlacementBlockFn = const void* (*)(
            const void* blockType,
            const void* actor,
            const BlockPos* pos,
            uint8_t face,
            const Vec3* clickPos,
            int itemValue);

        const void* placedBlock = baseBlock;
        if (const auto getPlacement = vtableSlot<GetPlacementBlockFn>(bt, 79); getPlacement != nullptr) {
            placedBlock = getPlacement(
                bt, actor_, &placePos, static_cast<uint8_t>(face), &subClickPos, placementData);
        }

        // Step B: Execute 1.21-1.26 BlockPlayerPlacingEvent component traits (PlacementDirection,
        // PlacementPosition, custom script/component traits) verified at 0x1000b57c & 0x101feb6c!
        if (placedBlock != nullptr &&
            gBlockCompTryGet != nullptr &&
            gBlockPlacingEventCtor != nullptr &&
            item.nativeItemStackPtr != 0) {
            auto* compStorage = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(placedBlock) + 0x20u);
            if (void* eventComp = gBlockCompTryGet(compStorage, 5); eventComp != nullptr) {
                alignas(8) uint8_t evBuf[64]{};
                gBlockPlacingEventCtor(
                    evBuf,
                    blockSource_,
                    &placePos,
                    placedBlock,
                    actor_,
                    static_cast<uint8_t>(face),
                    &subClickPos,
                    reinterpret_cast<const void*>(item.nativeItemStackPtr));

                using FinalizeEventFn = EventDispatchResult (*)(void* comp, void* ev);
                if (const auto dispatchFn = vtableSlot<FinalizeEventFn>(eventComp, 3); dispatchFn != nullptr) {
                    (void)dispatchFn(eventComp, evBuf);
                    const bool canceled = (evBuf[0x14] != 0);
                    if (canceled) {
                        return false;
                    }
                    const void* traitBlock = *reinterpret_cast<const void* const*>(evBuf + 0x28);
                    if (traitBlock != nullptr) {
                        placedBlock = traitBlock;
                    }
                }
            }
        }

        if (placedBlock == nullptr) {
            return false;
        }

        outFinalBlockId = readBlockIdentifier(placedBlock);
        outStateData = *reinterpret_cast<const uint16_t*>(
            static_cast<const uint8_t*>(placedBlock) + 0x128);
        extractBlockStateProperties(placedBlock, outProps);
        extractBlockWorldBoxes(placedBlock, blockSource_, placePos, outWorldBoxes);
        return true;
    }

private:
    void* blockSource_{nullptr};
    void* actor_{nullptr};
};

void hookDebugRendererRenderEnd(void* debugRenderer, void* renderParams) {
    if (debugRenderer != nullptr) {
        const auto* drBytes = static_cast<const uint8_t*>(debugRenderer);
        const bool debugActive = (drBytes[0x08] != 0);

        const ModConfig cfg = ConfigManager::instance().snapshot();
        if (debugActive && cfg.enabled) {
            // LevelRenderer owns DebugRenderer inline at offset +0x2a8 (verified at 0x0b25de34).
            const uintptr_t levelRenderer = reinterpret_cast<uintptr_t>(debugRenderer) - 0x2a8u;
            const uintptr_t lrp = *reinterpret_cast<const uintptr_t*>(levelRenderer + 0x420u);

            if (lrp != 0) {
                const Vec3 cameraWorldPos = *reinterpret_cast<const Vec3*>(lrp + 0x670u);
                const void* clientInst = *reinterpret_cast<const void* const*>(lrp + 0x1100u);
                const void* level = *reinterpret_cast<const void* const*>(lrp + 0x1140u);

                // ClientInstance::vtbl[39] = getCameraEntity(), ClientInstance::vtbl[42] = getRegion()
                using GetActorFn = void* (*)(const void* ci);
                using GetRegionFn = void* (*)(const void* ci);
                // Level::vtbl[336] = getHitResult()
                using GetHitResultFn = const uint8_t* (*)(const void* lvl);

                void* camActor = nullptr;
                void* blockSource = nullptr;
                const uint8_t* hitResult = nullptr;

                if (const auto getActor = vtableSlot<GetActorFn>(clientInst, 39); getActor != nullptr) {
                    camActor = getActor(clientInst);
                }
                if (const auto getRegion = vtableSlot<GetRegionFn>(clientInst, 42); getRegion != nullptr) {
                    blockSource = getRegion(clientInst);
                }
                if (const auto getHit = vtableSlot<GetHitResultFn>(level, 336); getHit != nullptr) {
                    hitResult = getHit(level);
                }

                if (camActor != nullptr && blockSource != nullptr && hitResult != nullptr) {
                    PlayerContextInput pctx{};
                    pctx.inWorld = true;
                    pctx.isSpectator = (gActorIsSpectator != nullptr) ? gActorIsSpectator(camActor) : false;

                    const auto* actorBytes = static_cast<const uint8_t*>(camActor);
                    const auto* aabbComp = *reinterpret_cast<const AABB* const*>(actorBytes + 0x210u);
                    if (aabbComp != nullptr) {
                        pctx.playerAabb = *aabbComp;
                    }
                    const auto* rotComp = *reinterpret_cast<const float* const*>(actorBytes + 0x218u);
                    if (rotComp != nullptr) {
                        pctx.pitchDeg = rotComp[0];
                        pctx.yawDeg = rotComp[1];
                    }

                    // Actor::vtbl[78] = getCarriedItem(), Actor::vtbl[80] = getOffhandSlot()
                    using GetItemStackFn = const void* (*)(const void* actor);
                    if (const auto getMain = vtableSlot<GetItemStackFn>(camActor, 78); getMain != nullptr) {
                        pctx.mainhand = inspectItemStack(getMain(camActor));
                    }
                    if (const auto getOff = vtableSlot<GetItemStackFn>(camActor, 80); getOff != nullptr) {
                        pctx.offhand = inspectItemStack(getOff(camActor));
                    }

                    // HitResult layout verified at LRP::shouldRenderHitSelect (0x0b29c0c0) & GameMode (0x0f9e7180):
                    //   +0x18 int32_t mType (0 == TILE, 1 == ENTITY, 2/3 == NO_HIT/OUT_OF_RANGE)
                    //   +0x1c uint8_t mFacing
                    //   +0x20 BlockPos mBlock
                    //   +0x2c Vec3 mPos
                    //   +0x68 bool mIsHitLiquid, +0x69 uint8_t mLiquidFacing, +0x6c BlockPos mLiquid
                    const int32_t hitType = *reinterpret_cast<const int32_t*>(hitResult + 0x18u);
                    if (hitType == 0) {
                        pctx.raycast.hasBlockHit = true;
                        pctx.raycast.hitFace = static_cast<Facing>(hitResult[0x1cu] % 6u);
                        pctx.raycast.blockPos = *reinterpret_cast<const BlockPos*>(hitResult + 0x20u);
                        pctx.raycast.hitWorldPos = *reinterpret_cast<const Vec3*>(hitResult + 0x2cu);
                    }
                    if (hitResult[0x68u] != 0) {
                        pctx.raycast.hasLiquidHit = true;
                        pctx.raycast.liquidFace = static_cast<Facing>(hitResult[0x69u] % 6u);
                        pctx.raycast.liquidPos = *reinterpret_cast<const BlockPos*>(hitResult + 0x6cu);
                        pctx.raycast.liquidWorldPos = pctx.raycast.liquidPos.centerVec() + Vec3{0.0f, 0.5f, 0.0f};
                    }

                    LiveBedrockWorldQuery worldQuery(blockSource, camActor);
                    const PlacementPreviewState preview = PlacementResolver::resolve(pctx, worldQuery, cfg);

                    if (preview.valid && !preview.worldBoxes.empty()) {
                        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                        const float phaseRad = static_cast<float>(nowMs % 2800LL) * (6.2831853f / 2800.0f);

                        static thread_local GhostMeshOutput meshScratch;
                        GhostMesher::buildMesh(preview, cfg, cameraWorldPos, phaseRad, meshScratch);

                        auto& lineVec = *reinterpret_cast<std::vector<DebugLineVertex>*>(
                            static_cast<uint8_t*>(debugRenderer) + 0x50u);
                        auto& triVec = *reinterpret_cast<std::vector<DebugTriangleVertex>*>(
                            static_cast<uint8_t*>(debugRenderer) + 0x68u);

                        if (!meshScratch.triangles.empty()) {
                            triVec.insert(
                                triVec.end(),
                                meshScratch.triangles.begin(),
                                meshScratch.triangles.end());
                        }
                        if (!meshScratch.lines.empty()) {
                            lineVec.insert(
                                lineVec.end(),
                                meshScratch.lines.begin(),
                                meshScratch.lines.end());
                        }
                    }
                }
            }
        }
    }

    if (gOrigRenderEnd != nullptr) {
        gOrigRenderEnd(debugRenderer, renderParams);
    }
}

} // namespace

EngineBridge& EngineBridge::instance() noexcept {
    static EngineBridge bridge;
    return bridge;
}

bool EngineBridge::installHooks() {
    if (installed_.load(std::memory_order_acquire)) {
        return true;
    }

    const uintptr_t renderEndAddr = resolvePlus10Signature(kSigDebugRendererRenderEndPlus10);
    if (renderEndAddr == 0) {
        return false;
    }

    gActorIsSpectator = reinterpret_cast<ActorIsSpectatorFn>(
        resolvePlus10Signature(kSigActorIsSpectatorPlus10));
    gItemCheckCanPlaceOn = reinterpret_cast<ItemCheckCanPlaceOnFn>(
        resolvePlus10Signature(kSigItemCheckCanPlaceOnPlus10));
    gBlockCompTryGet = reinterpret_cast<BlockCompTryGetFn>(
        resolvePlus10Signature(kSigBlockCompTryGetPlus10));
    gBlockPlacingEventCtor = reinterpret_cast<BlockPlacingEventCtorFn>(
        resolvePlus10Signature(kSigBlockPlacingEventCtorPlus10));
    gBlockRegistrySingleton = reinterpret_cast<BlockRegistryGetSingletonFn>(
        resolvePlus10Signature(kSigBlockRegistrySingletonPlus10));
    gBlockRegistryLookupDefault = reinterpret_cast<BlockRegistryLookupDefaultFn>(
        resolvePlus10Signature(kSigBlockRegistryLookupDefaultPlus10));

    gRenderEndTarget = reinterpret_cast<void*>(renderEndAddr);
    void* origPtr = nullptr;
    if (!pl::memory::hook(
            gRenderEndTarget,
            reinterpret_cast<void*>(&hookDebugRendererRenderEnd),
            &origPtr,
            pl::memory::HookPriority::Normal)) {
        gRenderEndTarget = nullptr;
        return false;
    }

    gOrigRenderEnd = reinterpret_cast<DebugRenderEndFn>(origPtr);
    installed_.store(true, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(telemetryMutex_);
        telemetry_.hookInstalled = true;
    }
    return true;
}

void EngineBridge::uninstallHooks() {
    if (!installed_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (gRenderEndTarget != nullptr) {
        (void)pl::memory::unhook(
            gRenderEndTarget,
            reinterpret_cast<void*>(&hookDebugRendererRenderEnd));
        gRenderEndTarget = nullptr;
        gOrigRenderEnd = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(telemetryMutex_);
        telemetry_ = LivePreviewTelemetry{};
    }
}

void EngineBridge::notifyConfigUpdated(const ModConfig& /*cfg*/) {
    // Render thread snapshots atomic-locked ConfigManager directly each frame with zero lag.
}

LivePreviewTelemetry EngineBridge::telemetry() const {
    std::lock_guard<std::mutex> lock(telemetryMutex_);
    return telemetry_;
}

} // namespace buildghost
