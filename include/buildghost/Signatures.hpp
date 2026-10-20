#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace buildghost::sig {

// ============================================================================
// Verified RVAs & +0x10 instruction words for Minecraft Bedrock 1.26.52.03
// arm64-v8a (libminecraftpe.so).
//
// Instant < 0.2 ms resolution strategy (ZERO UI main-thread signature scans):
//   1. Resolve LevelRendererPlayer::vtable[20] via fast symbol/RTTI vtable lookup
//      (pl::memory::resolveVtableFunction("19LevelRendererPlayer", 20)).
//   2. Subtract LRP_setupFog (0x0b29b70c) to obtain exact page-aligned moduleBase
//      and verify readInsnWord(addr + 0x10) == LRP_setupFog_Word10.
//   3. Compute every target function directly via moduleBase + RVA and verify
//      its instruction word at +0x10 before installing any detour.
//   4. If running on a future game build where RVAs shifted, wildcard signature
//      scanning runs ONLY on a detached background worker thread so Mod::enable()
//      NEVER stalls Android's UI main thread (preventing ANRs).
// ============================================================================
namespace rva {
inline constexpr uintptr_t LRP_setupFog                 = 0x0b29b70c;
inline constexpr uint32_t  LRP_setupFog_Word10          = 0xa9085ff8u;

inline constexpr uintptr_t DebugRenderer_renderEnd      = 0x0b25de34;
inline constexpr uint32_t  DebugRenderer_renderEnd_W10  = 0xa90a57f6u;

inline constexpr uintptr_t LevelChunk_getBlock          = 0x10360594;
inline constexpr uint32_t  LevelChunk_getBlock_W10      = 0xa950a808u;

inline constexpr uintptr_t Actor_getDimensionBlockSource = 0x0f560600;
inline constexpr uint32_t  Actor_getDimBlockSource_W10   = 0xf940e400u;

inline constexpr uintptr_t Actor_isSpectator            = 0x0f58e6c8;
inline constexpr uint32_t  Actor_isSpectator_W10        = 0xf940080bu;
} // namespace rva

// Position-independent body patterns starting at +0x10 with PC-relative branches masked.
namespace pattern {
inline constexpr const char* DebugRenderer_renderEnd_Body10 =
    "F6 57 0A A9 F4 4F 0B A9 FD C3 01 91 58 D0 3B D5 "
    "F4 03 01 AA F5 03 00 AA 08 17 40 F9 A8 83 1F F8 "
    "08 24 45 A9 1F 20 00 39 1F 01 09 EB ? ? ? 54";

inline constexpr const char* LevelChunk_getBlock_Body10 =
    "08 A8 50 A9 08 79 40 B9 1F 01 09 6B ? ? ? 54 "
    "2A 04 40 93 48 FD 42 D3 09 05 00 51 1F 01 08 EB";

inline constexpr const char* Actor_getDimBlockSource_Body10 =
    "00 E4 40 F9 1F 00 00 F1 ? ? ? 54 08 00 40 F9";

inline constexpr const char* Actor_isSpectator_Body10 =
    "0B 08 40 F9 7F 01 00 F1 ? ? ? 54 68 01 40 F9";
} // namespace pattern

// ============================================================================
// Verified Class / Struct Field & Vtable Offsets (1.26.52.03 arm64-v8a)
// ============================================================================
namespace off {
inline constexpr const char* LRP_RttiName               = "19LevelRendererPlayer";
inline constexpr std::size_t LRP_SetupFogVtableSlot     = 20;

// DebugRenderer layout & LevelRenderer embedding
inline constexpr uintptr_t DebugRenderer_ActiveFlag     = 0x08;  // uint8_t
inline constexpr uintptr_t DebugRenderer_LineVec        = 0x50;  // PackedVertex*[3] (LineList, mode=4)
inline constexpr uintptr_t DebugRenderer_TriVec         = 0x68;  // PackedVertex*[3] (TriangleList, mode=2)
inline constexpr uintptr_t LevelRenderer_DebugRenderer  = 0x2a8; // LevelRenderer + 0x2a8 == DebugRenderer*
inline constexpr uintptr_t LevelRenderer_LRPPtr         = 0x420; // LevelRendererPlayer*

// LevelRendererPlayer fields
inline constexpr uintptr_t LRP_ClientInstancePtr        = 0x1100; // ClientInstance*
inline constexpr uintptr_t LRP_LevelPtr                 = 0x1140; // Level*

// ClientInstance vtable offsets (byte offsets)
inline constexpr uintptr_t ClientInstance_VtGetCamActor = 0x138;  // slot 39: Actor* getCameraEntity()
inline constexpr uintptr_t ClientInstance_VtGetRegion   = 0x150;  // slot 42: BlockSource* getRegion()

// Level vtable offsets (byte offsets)
inline constexpr uintptr_t Level_VtGetHitResult         = 0xa80;  // slot 336: HitResult* getHitResult()

// HitResult fields
inline constexpr uintptr_t HitResult_Type               = 0x18;   // int32_t (0 = Tile)
inline constexpr uintptr_t HitResult_Facing             = 0x1c;   // uint8_t (0..5)
inline constexpr uintptr_t HitResult_BlockPos           = 0x20;   // int32_t[3]
inline constexpr uintptr_t HitResult_Pos                = 0x2c;   // float[3]
inline constexpr uintptr_t HitResult_IsHitLiquid        = 0x68;   // uint8_t
inline constexpr uintptr_t HitResult_LiquidFacing       = 0x69;   // uint8_t
inline constexpr uintptr_t HitResult_LiquidPos          = 0x6c;   // int32_t[3]

// Actor fields & vtable offsets
inline constexpr uintptr_t Actor_AABBCompPtr            = 0x210;  // AABBShapeComponent* (float[6] min/max)
inline constexpr uintptr_t Actor_RotCompPtr             = 0x218;  // ActorRotationComponent* (pitch, yaw)
inline constexpr uintptr_t Actor_VtGetCarriedItem       = 0x270;  // slot 78: const ItemStack*
inline constexpr uintptr_t Actor_VtGetOffhandSlot       = 0x280;  // slot 80: const ItemStack*

// ItemStackBase & Item fields
inline constexpr uintptr_t ItemStack_ItemWeakPtr        = 0x08;   // WeakPtr<Item> -> Item**
inline constexpr uintptr_t ItemStack_BlockPtr           = 0x18;   // const Block*
inline constexpr uintptr_t ItemStack_AuxValue           = 0x20;   // int16_t
inline constexpr uintptr_t ItemStack_Count              = 0x22;   // uint8_t
inline constexpr uintptr_t Item_RawNameId               = 0xc8;   // HashedString (+0x08 std::string)
inline constexpr uintptr_t Item_FullName                = 0xf8;   // HashedString (+0x08 std::string)

// BlockSource & LevelChunk fields
inline constexpr uintptr_t BlockSource_MaxHeight        = 0x40;   // int16_t (exclusive ceiling, e.g. 320)
inline constexpr uintptr_t BlockSource_MinHeight        = 0x42;   // int16_t (inclusive floor, e.g. -64)
inline constexpr uintptr_t BlockSource_VtGetChunk       = 0x148;  // slot 41: LevelChunk* getChunk(const ChunkPos&)
inline constexpr uintptr_t BlockSource_VtCheckPerms     = 0x1e8;  // slot 61: bool checkBlockPermissions(...)
inline constexpr uintptr_t LevelChunk_AtomicStateByte   = 0xb8;   // uint8_t (7..9 == usable chunk)

// Block & BlockType fields
inline constexpr uintptr_t Block_BlockTypePtr           = 0x68;   // const BlockType*
inline constexpr uintptr_t Block_Data                   = 0x128;  // uint16_t mData
inline constexpr uintptr_t BlockType_RawNameStr         = 0x90;   // std::string (e.g. "oak_stairs")
inline constexpr uintptr_t BlockType_FullNameStr        = 0xd0;   // std::string (e.g. "minecraft:oak_stairs")
inline constexpr uintptr_t BlockType_StatesTree         = 0x1b0;  // BlockStateInstance RB-tree
} // namespace off

// ============================================================================
// Android 11-16 ARM64 Top-Byte-Ignore (Scudo 0xB4... heap tag) safe pointer check
// ============================================================================
inline constexpr uintptr_t untagUserAddr(uintptr_t addr) noexcept {
    return addr & 0x00FFFFFFFFFFFFFFull;
}

inline bool isValidUserAddress(const void* ptr, std::size_t alignReq = 1) noexcept {
    if (!ptr) return false;
    const uintptr_t raw = reinterpret_cast<uintptr_t>(ptr);
    const uintptr_t untagged = untagUserAddr(raw);
    if (untagged < 0x10000ull || untagged > 0x0000FFFFFFFFFFFFull) return false;
    if (alignReq > 1 && (untagged & (alignReq - 1u)) != 0) return false;
    return true;
}

// Safe zero-copy libc++ std::string reader (handles SSO short & heap long layouts)
inline std::string_view readLibcxxStringView(const void* strAddr) noexcept {
    if (!isValidUserAddress(strAddr, alignof(void*))) return {};
    const auto* bytes = static_cast<const uint8_t*>(strAddr);
    if ((bytes[0] & 1u) == 0u) {
        const std::size_t len = static_cast<std::size_t>(bytes[0] >> 1u);
        if (len == 0 || len > 22) return {};
        return std::string_view(reinterpret_cast<const char*>(bytes + 1), len);
    }
    std::size_t len = 0;
    const char* dataPtr = nullptr;
    std::memcpy(&len, bytes + 8, sizeof(std::size_t));
    std::memcpy(&dataPtr, bytes + 16, sizeof(const char*));
    if (len == 0 || len > 256 || !isValidUserAddress(dataPtr, 1)) return {};
    return std::string_view(dataPtr, len);
}

} // namespace buildghost::sig
