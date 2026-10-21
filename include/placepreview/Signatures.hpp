#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace placepreview::sig {

// ============================================================================
// Verified RVAs & +0x10 instruction words for Minecraft Bedrock 1.26.52.03
// arm64-v8a (libminecraftpe.so).
//
// Instant < 0.2 ms resolution strategy (ZERO UI main-thread signature scans):
//   1. Resolve module base via dl_iterate_phdr + instruction word guard at
//      DebugRenderer_renderEnd + 0x10 (fallback: LevelRendererPlayer::vtable[20]).
//   2. Compute every target function directly via moduleBase + RVA and verify
//      its instruction word before installing any detour or reading globals.
//   3. If running on a future game build where RVAs shifted, wildcard signature
//      scanning runs ONLY on a background worker thread so Mod::enable() NEVER
//      stalls Android's UI main thread (preventing ANRs).
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

inline constexpr uintptr_t Player_isCreative             = 0x0f58e6c8;
inline constexpr uint32_t  Player_isCreative_W10         = 0xf940080bu;

inline constexpr uintptr_t Entity_getPlayerGameType      = 0x0f9ba104;
inline constexpr uint32_t  Entity_getPlayerGameType_W10  = 0xcb0b0108u;

// Verified BlockTypeRegistry singleton getter & global instance (disassembled @ 0x103e33a8):
//   103e33a8: b00187a0  adrp x0, 0x134d8000
//   103e33ac: 9110e000  add  x0, x0, #0x438
//   103e33b0: d65f03c0  ret
inline constexpr uintptr_t BlockTypeRegistry_getter      = 0x103e33a8;
inline constexpr uint32_t  BlockTypeRegistry_getter_W0   = 0xb00187a0u;
inline constexpr uint32_t  BlockTypeRegistry_getter_W4   = 0x9110e000u;
inline constexpr uintptr_t BlockTypeRegistry_global      = 0x134d8438;
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

inline constexpr const char* Player_isCreative_Body10 =
    "0B 08 40 F9 7F 01 00 F1 ? ? ? 54 68 01 40 F9";

inline constexpr const char* Entity_getPlayerGameType_Body10 =
    "08 01 0B CB 08 FD 43 D3 08 05 00 51 0C 01 0A 8A";
} // namespace pattern

// ============================================================================
// Verified Class / Struct Field & Vtable Offsets (1.26.52.03 arm64-v8a)
// ============================================================================
namespace off {
inline constexpr const char* LRP_RttiName               = "19LevelRendererPlayer";
inline constexpr std::size_t LRP_SetupFogVtableSlot     = 20;

// DebugRenderer layout & LevelRenderer / ScreenContext camera embedding
inline constexpr uintptr_t DebugRenderer_ActiveFlag     = 0x08;   // uint8_t
inline constexpr uintptr_t DebugRenderer_EnabledByte    = 0x01db; // uint8_t
inline constexpr uintptr_t DebugRenderer_LineVec        = 0x50;   // PackedVertex*[3] (LineList, mode=4)
inline constexpr uintptr_t DebugRenderer_TriVec        = 0x68;    // PackedVertex*[3] (TriangleList, mode=2)
inline constexpr uintptr_t LevelRenderer_DebugRenderer  = 0x2a8;  // LevelRenderer + 0x2a8 == DebugRenderer*
inline constexpr uintptr_t LevelRenderer_LRPPtr         = 0x420;  // LevelRendererPlayer*
inline constexpr uintptr_t RenderParams_ScreenContext   = 0x20;   // ScreenContext*
inline constexpr uintptr_t ScreenContext_CameraData     = 0xa8;   // CameraData*
inline constexpr uintptr_t CameraData_WorldPos          = 0x28;   // Vec3 (interpolated world camera eye pos)

// LevelRendererPlayer fields
inline constexpr uintptr_t LRP_ClientInstancePtr        = 0x1100; // ClientInstance*
inline constexpr uintptr_t LRP_LevelPtr                 = 0x1140; // Level*

// ClientInstance vtable offsets (byte offsets)
inline constexpr uintptr_t ClientInstance_VtGetCamActor = 0x138;  // slot 39: Actor* getCameraEntity()
inline constexpr uintptr_t ClientInstance_VtGetRegion   = 0x150;  // slot 42: BlockSource* getRegion()

// Level vtable offsets (byte offsets)
inline constexpr uintptr_t Level_VtGetDefaultGameType   = 0x468;  // slot 141: GameType getDefaultGameType()
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
inline constexpr uintptr_t Actor_EntityContext          = 0x08;   // EntityContext embedded field
inline constexpr uintptr_t Actor_LevelPtr               = 0x1d0;  // Level*
inline constexpr uintptr_t Actor_AABBCompPtr            = 0x210;  // AABBShapeComponent* (float[6] min/max)
inline constexpr uintptr_t Actor_RotCompPtr             = 0x218;  // ActorRotationComponent* (pitch, yaw)
inline constexpr uintptr_t Actor_VtGetCarriedItem       = 0x270;  // slot 78: const ItemStack*

// ItemStackBase & Item fields (verified via 0x0ff81e94 / 0x0ff88c20 getters)
inline constexpr uintptr_t ItemStack_ItemWeakPtr        = 0x08;   // WeakPtr<Item> -> Item**
inline constexpr uintptr_t ItemStack_BlockPtr           = 0x18;   // const Block* mBlock
inline constexpr uintptr_t ItemStack_AuxValue           = 0x20;   // int16_t
inline constexpr uintptr_t ItemStack_Count              = 0x22;   // uint8_t
inline constexpr uintptr_t Item_RawNameId               = 0xa8;   // HashedString (+0x08 std::string at +0xb0)
inline constexpr uintptr_t Item_FullName                = 0xe8;   // HashedString (+0x08 std::string at +0xf0)

// BlockSource & LevelChunk fields
inline constexpr uintptr_t BlockSource_MaxHeight        = 0x40;   // int16_t (exclusive ceiling, e.g. 320)
inline constexpr uintptr_t BlockSource_MinHeight        = 0x42;   // int16_t (inclusive floor, e.g. -64)
inline constexpr uintptr_t BlockSource_VtGetChunk       = 0x148;  // slot 41: LevelChunk* getChunk(const ChunkPos&)
inline constexpr uintptr_t BlockSource_VtCheckPerms     = 0x1e8;  // slot 61: bool checkBlockPermissions(...)
inline constexpr uintptr_t LevelChunk_AtomicStateByte   = 0xb8;   // uint8_t (7..9 == usable chunk)

// Block & BlockType fields + verified BlockType primary vtable slots (0x130da380)
inline constexpr uintptr_t Block_BlockTypePtr           = 0x68;   // const BlockType*
inline constexpr uintptr_t Block_Data                   = 0x128;  // uint16_t mData
inline constexpr uintptr_t BlockType_RawNameStr         = 0x90;   // std::string (e.g. "oak_stairs")
inline constexpr uintptr_t BlockType_FullNameStr        = 0xd0;   // std::string (e.g. "minecraft:oak_stairs")
inline constexpr uintptr_t BlockType_DefaultVisualShape = 0x188;  // AABB mVisualShape [0..1]^3
inline constexpr uintptr_t BlockType_StatesTree         = 0x1b0;  // BlockStateInstance RB-tree
inline constexpr uintptr_t BlockType_DefaultStatePtr    = 0x200;  // const Block* mDefaultState

inline constexpr uintptr_t BlockType_VtGetOutline       = 0x40;   // slot 8: const AABB& getOutline(const Block&, const IConstBlockSource&, const BlockPos&, AABB&)
inline constexpr uintptr_t BlockType_VtGetVisualShape   = 0x50;   // slot 10: const AABB& getVisualShape(const Block&, AABB&)
inline constexpr uintptr_t BlockType_VtGetPlacementBlock= 0x278;  // slot 79: const Block& getPlacementBlock(const Actor&, const BlockPos&, FacingID, const Vec3&, int)

// Read-only BlockTypeRegistry std::map<HashedString, std::shared_ptr<BlockType>> offsets (0x103e6394..0x103e64bc)
inline constexpr uintptr_t Registry_MapRootPtr          = 0x08;   // __tree_node_base* root
inline constexpr uintptr_t Registry_MapSize             = 0x10;   // size_t nodeCount
inline constexpr uintptr_t RegistryNode_Left            = 0x00;   // const void*
inline constexpr uintptr_t RegistryNode_Right           = 0x08;   // const void*
inline constexpr uintptr_t RegistryNode_KeyStr          = 0x28;   // HashedString (+0x20) + 0x08 => std::string
inline constexpr uintptr_t RegistryNode_BlockTypePtr    = 0x48;   // const BlockType* (__shared_ptr::__ptr_)
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

} // namespace placepreview::sig
