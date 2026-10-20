#include "buildghost/Config.hpp"
#include "buildghost/EngineBridge.hpp"
#include "buildghost/GhostMesher.hpp"
#include "buildghost/MaterialPalette.hpp"
#include "buildghost/PlacementResolver.hpp"
#include "buildghost/Signatures.hpp"
#include "buildghost/StateInspector.hpp"
#include "buildghost/Types.hpp"
#include "buildghost/Version.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <tuple>

namespace {

using namespace buildghost;

int gFailures = 0;

#define BG_EXPECT(cond)                                                                         \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " -> " #cond << std::endl; \
            ++gFailures;                                                                        \
        }                                                                                       \
    } while (false)

class MockWorld final : public IWorldQuery {
public:
    struct Cell {
        std::string id{"minecraft:air"};
        std::vector<AABB> localBoxes;
    };

    std::map<std::tuple<int, int, int>, Cell> cells;
    std::set<std::string> adventureAllowedTargets;
    bool allowMayPlace{true};
    bool injectNativeState{false};
    std::string injectedFinalBlockId;
    uint16_t injectedStateData{0};
    std::vector<BlockStateProperty> injectedProps;
    std::vector<AABB> injectedWorldBoxes;

    void setSolidBlock(const BlockPos& p, const std::string& id = "minecraft:stone") {
        cells[{p.x, p.y, p.z}] = Cell{id, {AABB{0, 0, 0, 1, 1, 1}}};
    }

    void setBottomSlab(const BlockPos& p, const std::string& id = "minecraft:oak_slab") {
        cells[{p.x, p.y, p.z}] = Cell{id, {AABB{0, 0.0f, 0, 1, 0.5f, 1}}};
    }

    void setTopSlab(const BlockPos& p, const std::string& id = "minecraft:oak_slab") {
        cells[{p.x, p.y, p.z}] = Cell{id, {AABB{0, 0.5f, 0, 1, 1.0f, 1}}};
    }

    void setReplaceable(const BlockPos& p, const std::string& id = "minecraft:tall_grass") {
        cells[{p.x, p.y, p.z}] = Cell{id, {}};
    }

    [[nodiscard]] int minBuildHeight() const noexcept override { return -64; }
    [[nodiscard]] int maxBuildHeight() const noexcept override { return 319; }

    [[nodiscard]] std::string getBlockIdentifier(const BlockPos& p) const override {
        const auto it = cells.find({p.x, p.y, p.z});
        return (it == cells.end()) ? "minecraft:air" : it->second.id;
    }

    [[nodiscard]] bool isAirBlock(const BlockPos& p) const override {
        return getBlockIdentifier(p) == "minecraft:air";
    }

    [[nodiscard]] bool isReplaceableBlock(const BlockPos& p) const override {
        const std::string id = getBlockIdentifier(p);
        return id == "minecraft:air" || id == "minecraft:tall_grass" || id == "minecraft:water";
    }

    [[nodiscard]] std::vector<AABB> getExistingBlockWorldBoxes(const BlockPos& p) const override {
        const auto it = cells.find({p.x, p.y, p.z});
        if (it == cells.end()) return {};
        std::vector<AABB> res;
        for (const auto& lb : it->second.localBoxes) {
            res.push_back(lb.translated(p.originVec()));
        }
        return res;
    }

    [[nodiscard]] bool checkAdventureCanPlaceOn(
        const HeldItemInput& /*item*/,
        const BlockPos& targetBlockPos) const override {
        return adventureAllowedTargets.count(getBlockIdentifier(targetBlockPos)) > 0;
    }

    [[nodiscard]] bool calculatePlacePos(
        const HeldItemInput& item,
        Facing& inOutFace,
        BlockPos& inOutPos) const override {

        // Emulate SlabBlockItem::_calculatePlacePos merge behavior!
        const std::string held = !item.directBlockIdentifier.empty()
                                     ? item.directBlockIdentifier
                                     : item.itemIdentifier;
        const std::string existing = getBlockIdentifier(inOutPos);
        if (StateInspector::isSlabBlock(held) && existing == held) {
            const auto boxes = getExistingBlockWorldBoxes(inOutPos);
            if (!boxes.empty()) {
                const bool existingIsBottom = (boxes.front().max.y - inOutPos.y) <= 0.55f;
                if ((existingIsBottom && inOutFace == Facing::Up) ||
                    (!existingIsBottom && inOutFace == Facing::Down)) {
                    return true; // Merge in place at `inOutPos`!
                }
            }
        }

        if (!isReplaceableBlock(inOutPos)) {
            const BlockPos d = facingStep(inOutFace);
            inOutPos = inOutPos.offset(d.x, d.y, d.z);
        }
        return true;
    }

    [[nodiscard]] bool mayPlaceBlock(
        const HeldItemInput& /*item*/,
        const std::string& resolvedBlockIdentifier,
        const BlockPos& placePos,
        Facing /*face*/,
        const Vec3& /*worldClickPos*/) const override {
        if (!allowMayPlace) return false;
        if (StateInspector::isSlabBlock(resolvedBlockIdentifier) &&
            getBlockIdentifier(placePos) == resolvedBlockIdentifier) {
            return true;
        }
        return isReplaceableBlock(placePos);
    }

    bool computePlacedBlockStateAndBoxes(
        const HeldItemInput& /*item*/,
        const std::string& /*resolvedBlockIdentifier*/,
        const BlockPos& /*placePos*/,
        Facing /*face*/,
        const Vec3& /*subClickPos*/,
        std::string& outFinalBlockId,
        uint16_t& outStateData,
        std::vector<BlockStateProperty>& outProps,
        std::vector<AABB>& outWorldBoxes) const override {
        if (!injectNativeState) {
            return false;
        }
        outFinalBlockId = injectedFinalBlockId;
        outStateData = injectedStateData;
        outProps = injectedProps;
        outWorldBoxes = injectedWorldBoxes;
        return true;
    }
};

PlayerContextInput makeDefaultPlayer() {
    PlayerContextInput p{};
    p.inWorld = true;
    p.isSpectator = false;
    p.isAdventureMode = false;
    p.isSneaking = false;
    p.pitchDeg = 20.0f;
    p.yawDeg = 0.0f; // Looking +Z (South)
    p.playerAabb = AABB{9.7f, 65.0f, 17.7f, 10.3f, 66.8f, 18.3f};
    p.mainhand = HeldItemInput{
        .valid = true,
        .count = 64,
        .auxValue = 0,
        .itemIdentifier = "minecraft:oak_planks",
        .directBlockIdentifier = "minecraft:oak_planks"
    };
    p.raycast = RaycastHitInput{
        .hasBlockHit = true,
        .hitFace = Facing::Up,
        .blockPos = {10, 64, 20},
        .hitWorldPos = {10.5f, 65.0f, 20.5f}
    };
    return p;
}

void testImmediateDisappearanceConditions() {
    MockWorld world;
    world.setSolidBlock({10, 64, 20});
    const ModConfig cfg{};
    GhostMeshOutput mesh{};

    // 1. Valid baseline shows preview and non-empty mesh
    PlayerContextInput ctx = makeDefaultPlayer();
    auto s0 = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(s0.valid);
    BG_EXPECT(s0.primaryPos == BlockPos(10, 65, 20));
    GhostMesher::buildMesh(s0, cfg, {10.0f, 66.6f, 18.0f}, 0.0f, mesh);
    BG_EXPECT(!mesh.empty());

    // 2. Sky / Out-of-reach miss -> immediate disappearance (0 triangles, 0 lines!)
    ctx.raycast.hasBlockHit = false;
    auto sMiss = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(!sMiss.valid);
    GhostMesher::buildMesh(sMiss, cfg, {10.0f, 66.6f, 18.0f}, 0.0f, mesh);
    BG_EXPECT(mesh.empty());

    // 3. Spectator mode -> immediate disappearance
    ctx = makeDefaultPlayer();
    ctx.isSpectator = true;
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);

    // 4. Holding sword / tool with empty offhand -> immediate disappearance
    ctx = makeDefaultPlayer();
    ctx.mainhand.directBlockIdentifier.clear();
    ctx.mainhand.itemIdentifier = "minecraft:diamond_sword";
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);

    // 5. Player AABB obstructing target block cell -> immediate disappearance
    ctx = makeDefaultPlayer();
    ctx.playerAabb = AABB{10.2f, 65.0f, 20.2f, 10.8f, 66.8f, 20.8f};
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);

    // 6. Above world height cap (y = 319 clicked Up -> y = 320 invalid!)
    ctx = makeDefaultPlayer();
    world.setSolidBlock({10, 319, 20});
    ctx.raycast.blockPos = {10, 319, 20};
    ctx.raycast.hitWorldPos = {10.5f, 320.0f, 20.5f};
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);

    // 7. BlockSource::mayPlace veto -> immediate disappearance
    ctx = makeDefaultPlayer();
    world.allowMayPlace = false;
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);
    world.allowMayPlace = true;

    // 8. Adventure mode CanPlaceOn enforcement
    ctx = makeDefaultPlayer();
    ctx.isAdventureMode = true;
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);
    world.adventureAllowedTargets.insert("minecraft:stone");
    BG_EXPECT(PlacementResolver::resolve(ctx, world, cfg).valid);
}

void testImmediateTargetPositionAndReplaceableUpdates() {
    MockWorld world;
    world.setSolidBlock({10, 64, 20});
    const ModConfig cfg{};
    PlayerContextInput ctx = makeDefaultPlayer();

    // Verify all 6 faces immediately step to the exact neighbor coordinate!
    const struct {
        Facing face;
        BlockPos expected;
    } cases[] = {
        {Facing::Up,    {10, 65, 20}},
        {Facing::Down,  {10, 63, 20}},
        {Facing::North, {10, 64, 19}},
        {Facing::South, {10, 64, 21}},
        {Facing::West,  { 9, 64, 20}},
        {Facing::East,  {11, 64, 20}},
    };

    for (const auto& c : cases) {
        ctx.raycast.hitFace = c.face;
        const auto st = PlacementResolver::resolve(ctx, world, cfg);
        BG_EXPECT(st.valid);
        BG_EXPECT(st.primaryPos == c.expected);
    }

    // Replaceable tall grass at (10, 65, 20) builds IN-PLACE at (10, 65, 20) instead of stepping to y=66!
    world.setReplaceable({10, 65, 20}, "minecraft:tall_grass");
    ctx.raycast.blockPos = {10, 65, 20};
    ctx.raycast.hitFace = Facing::Up;
    const auto rep = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(rep.valid);
    BG_EXPECT(rep.primaryPos == BlockPos(10, 65, 20));
}

void testOrientationsSlabsStairsPillarsDoorsBeds() {
    MockWorld world;
    for (int x = 8; x <= 14; ++x) {
        for (int z = 18; z <= 24; ++z) {
            world.setSolidBlock({x, 64, z}, "minecraft:stone");
        }
    }
    const ModConfig cfg{};

    // 1. Log / Pillar axes across Y (0), X (1), Z (2)
    PlayerContextInput ctx = makeDefaultPlayer();
    ctx.mainhand.itemIdentifier = "minecraft:oak_log";
    ctx.mainhand.directBlockIdentifier = "minecraft:oak_log";

    ctx.raycast.hitFace = Facing::Up;
    auto logY = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(logY.valid && logY.orientationKind == OrientationKind::PillarAxis && logY.pillarAxis == 0);

    world.setSolidBlock({12, 65, 20}, "minecraft:stone");
    ctx.raycast.blockPos = {12, 65, 20};
    ctx.raycast.hitFace = Facing::West;
    auto logX = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(logX.valid && logX.pillarAxis == 1 && logX.primaryPos == BlockPos(11, 65, 20));

    ctx.raycast.hitFace = Facing::South;
    auto logZ = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(logZ.valid && logZ.pillarAxis == 2 && logZ.primaryPos == BlockPos(12, 65, 21));

    // 2. Slabs: bottom half vs top half AND matching half-slab merge diff!
    ctx = makeDefaultPlayer();
    ctx.mainhand.itemIdentifier = "minecraft:oak_slab";
    ctx.mainhand.directBlockIdentifier = "minecraft:oak_slab";
    ctx.raycast.blockPos = {12, 65, 20};
    ctx.raycast.hitFace = Facing::West;

    ctx.raycast.hitWorldPos = {12.0f, 65.25f, 20.5f}; // Bottom half click
    auto slabLow = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(slabLow.valid && !slabLow.topHalf);
    BG_EXPECT(std::fabs(slabLow.worldBoxes.front().min.y - 65.0f) < 1e-3f);
    BG_EXPECT(std::fabs(slabLow.worldBoxes.front().max.y - 65.5f) < 1e-3f);

    ctx.raycast.hitWorldPos = {12.0f, 65.75f, 20.5f}; // Top half click -> immediate flip to top half!
    auto slabHigh = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(slabHigh.valid && slabHigh.topHalf);
    BG_EXPECT(std::fabs(slabHigh.worldBoxes.front().min.y - 65.5f) < 1e-3f);
    BG_EXPECT(std::fabs(slabHigh.worldBoxes.front().max.y - 66.0f) < 1e-3f);

    // Existing bottom slab at (10, 65, 20) + clicking top face -> merges IN-PLACE showing strictly [65.5..66.0]!
    world.setBottomSlab({10, 65, 20}, "minecraft:oak_slab");
    ctx.raycast.blockPos = {10, 65, 20};
    ctx.raycast.hitFace = Facing::Up;
    ctx.raycast.hitWorldPos = {10.5f, 65.5f, 20.5f};
    auto slabMerge = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(slabMerge.valid && slabMerge.isSlabMerge && slabMerge.hasMergeOutlineBox);
    BG_EXPECT(slabMerge.primaryPos == BlockPos(10, 65, 20));
    BG_EXPECT(std::fabs(slabMerge.worldBoxes.front().min.y - 65.5f) < 1e-3f);
    BG_EXPECT(std::fabs(slabMerge.worldBoxes.front().max.y - 66.0f) < 1e-3f);

    // 3. Stairs: yaw rotation across all 4 directions + upside-down sub-click + coplanar seam culling!
    ctx = makeDefaultPlayer();
    ctx.mainhand.itemIdentifier = "minecraft:oak_stairs";
    ctx.mainhand.directBlockIdentifier = "minecraft:oak_stairs";
    ctx.raycast.blockPos = {11, 64, 21};
    ctx.raycast.hitFace = Facing::Up;
    ctx.raycast.hitWorldPos = {11.5f, 65.0f, 21.5f};

    ctx.yawDeg = 0.0f;   // South (+Z)
    const auto stSouth = PlacementResolver::resolve(ctx, world, cfg);
    ctx.yawDeg = 90.0f;  // West (-X)
    const auto stWest = PlacementResolver::resolve(ctx, world, cfg);
    ctx.yawDeg = 180.0f; // North (-Z)
    const auto stNorth = PlacementResolver::resolve(ctx, world, cfg);
    ctx.yawDeg = 270.0f; // East (+X)
    const auto stEast = PlacementResolver::resolve(ctx, world, cfg);

    BG_EXPECT(stSouth.facing == Facing::South);
    BG_EXPECT(stWest.facing == Facing::West);
    BG_EXPECT(stNorth.facing == Facing::North);
    BG_EXPECT(stEast.facing == Facing::East);
    BG_EXPECT(stSouth.worldBoxes.size() == 2);

    // Verify coplanar interior face subtraction on 2-box stair removes internal double-alpha seam!
    ModConfig plainCfg = cfg;
    plainCfg.showOrientationGuides = false;
    plainCfg.showSubTargetPip = false;
    GhostMeshOutput stairMesh{}, twoSeparateMesh{};
    GhostMesher::buildMesh(stEast, plainCfg, {0, 0, 0}, 0.0f, stairMesh);

    PlacementPreviewState disconnected = stEast;
    disconnected.worldBoxes[1] = disconnected.worldBoxes[1].translated({0.0f, 2.0f, 0.0f});
    GhostMesher::buildMesh(disconnected, plainCfg, {0, 0, 0}, 0.0f, twoSeparateMesh);
    BG_EXPECT(stairMesh.triangles.size() < twoSeparateMesh.triangles.size());

    // 4. Two-block Door: valid 2-block leaf vs blocked ceiling at y + 1 immediate invalidation!
    ctx = makeDefaultPlayer();
    ctx.mainhand.directBlockIdentifier.clear();
    ctx.mainhand.itemIdentifier = "minecraft:oak_door";
    ctx.raycast.blockPos = {11, 64, 22};
    ctx.raycast.hitFace = Facing::Up;
    ctx.raycast.hitWorldPos = {11.2f, 65.0f, 22.5f};
    const auto doorLeft = PlacementResolver::resolve(ctx, world, cfg);
    ctx.raycast.hitWorldPos = {11.8f, 65.0f, 22.5f};
    const auto doorRight = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(doorLeft.valid && doorRight.valid && doorLeft.hingeRight != doorRight.hingeRight);

    world.setSolidBlock({11, 66, 22}, "minecraft:stone"); // Low overhanging ceiling blocks upper door cell!
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);

    // 5. Two-block Bed: dye tinting + head obstruction immediate invalidation!
    ctx = makeDefaultPlayer();
    ctx.mainhand.directBlockIdentifier.clear();
    ctx.mainhand.itemIdentifier = "minecraft:bed";
    ctx.mainhand.auxValue = 14; // Red bed
    ctx.yawDeg = 0.0f;          // Head at (11, 65, 22)
    ctx.raycast.blockPos = {11, 64, 21};
    ctx.raycast.hitFace = Facing::Up;
    ctx.raycast.hitWorldPos = {11.5f, 65.0f, 21.5f};
    const auto bedOk = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(bedOk.valid && bedOk.isMultiBlock && bedOk.secondaryPos == BlockPos(11, 65, 22));
    BG_EXPECT(bedOk.materialColor.r > bedOk.materialColor.b); // Red dye palette

    world.setSolidBlock({11, 65, 22}, "minecraft:stone"); // Head cell blocked!
    BG_EXPECT(!PlacementResolver::resolve(ctx, world, cfg).valid);

    // 6. Waterlily liquid-surface raycast + Offhand block fallback!
    ctx = makeDefaultPlayer();
    world.setReplaceable({13, 64, 20}, "minecraft:water");
    ctx.mainhand.itemIdentifier = "minecraft:waterlily";
    ctx.mainhand.directBlockIdentifier = "minecraft:waterlily";
    ctx.raycast.hasBlockHit = false;
    ctx.raycast.hasLiquidHit = true;
    ctx.raycast.liquidPos = {13, 64, 20};
    ctx.raycast.liquidFace = Facing::Up;
    const auto lily = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(lily.valid && lily.primaryPos == BlockPos(13, 65, 20));

    // Offhand fallback when holding diamond pickaxe in mainhand + torches in offhand
    ctx = makeDefaultPlayer();
    ctx.raycast.blockPos = {13, 64, 22};
    ctx.raycast.hitWorldPos = {13.5f, 65.0f, 22.5f};
    ctx.mainhand.directBlockIdentifier.clear();
    ctx.mainhand.itemIdentifier = "minecraft:diamond_pickaxe";
    ctx.offhand = HeldItemInput{
        .valid = true,
        .count = 32,
        .auxValue = 0,
        .itemIdentifier = "minecraft:torch",
        .directBlockIdentifier = "minecraft:torch"
    };
    const auto offhandTorch = PlacementResolver::resolve(ctx, world, cfg);
    BG_EXPECT(offhandTorch.valid && offhandTorch.isOffhand);
}

void testConfigPersistenceAndVersionMetadata() {
    BG_EXPECT(version::Name == "BuildGhost");
    BG_EXPECT(version::Version == "1.0.0");
    BG_EXPECT(version::Author == "Zxwmwmwm");

    const auto tmpDir = std::filesystem::temp_directory_path() / "buildghost_test_cfg";
    std::filesystem::remove_all(tmpDir);
    ConfigManager::instance().init(tmpDir);
    ConfigManager::instance().applyPreset("Material Tint");
    ConfigManager::instance().applyKeyValue("fillOpacity", "0.44");

    const ModConfig snap = ConfigManager::instance().snapshot();
    BG_EXPECT(std::fabs(snap.fillOpacity - 0.44f) < 1e-3f);
    BG_EXPECT(snap.colorMode == "Material Adaptive");

    const ModConfig roundTrip = ConfigManager::parseJson(ConfigManager::serializeJson(snap));
    BG_EXPECT(std::fabs(roundTrip.fillOpacity - 0.44f) < 1e-3f);
    BG_EXPECT(roundTrip.colorMode == "Material Adaptive");
    std::filesystem::remove_all(tmpDir);
}

void testNativeHardwareRegressionInvariants() {
    // 1. Exact 16-byte PackedVertex ABI & world-space coordinates (no double cam subtraction!)
    BG_EXPECT(sizeof(PackedVertex) == 16);
    BG_EXPECT(offsetof(PackedVertex, x) == 0);
    BG_EXPECT(offsetof(PackedVertex, y) == 4);
    BG_EXPECT(offsetof(PackedVertex, z) == 8);
    BG_EXPECT(offsetof(PackedVertex, abgr) == 12);

    PlacementPreviewState preview{};
    preview.valid = true;
    preview.primaryPos = {1452, 80, -2893};
    preview.blockIdentifier = "minecraft:oak_planks";
    preview.materialColor = MaterialPalette::colorForBlock(preview.blockIdentifier, 0);
    preview.worldBoxes = {AABB::unitAt(preview.primaryPos)};

    ModConfig cfg{};
    GhostMeshOutput mesh{};
    const Vec3 farCamera{1450.5f, 81.62f, -2890.25f};
    GhostMesher::buildMesh(preview, cfg, farCamera, 0.0f, mesh);

    BG_EXPECT(!mesh.packedTriangles.empty());
    BG_EXPECT(!mesh.packedLines.empty());
    BG_EXPECT(mesh.packedTriangles.size() == mesh.triangles.size() * 3);
    BG_EXPECT(mesh.packedLines.size() == mesh.lines.size() * 2);

    for (const auto& v : mesh.packedTriangles) {
        BG_EXPECT(v.x > 1451.8f && v.x < 1453.2f);
        BG_EXPECT(v.y > 79.8f && v.y < 81.2f);
        BG_EXPECT(v.z > -2893.2f && v.z < -2891.8f);
    }
    for (const auto& v : mesh.packedLines) {
        BG_EXPECT(v.x > 1451.8f && v.x < 1453.2f);
        BG_EXPECT(v.y > 79.8f && v.y < 81.2f);
        BG_EXPECT(v.z > -2893.2f && v.z < -2891.8f);
    }

    // 2. Android 13 ARM64 Scudo Top-Byte-Ignore (0xB4...) pointer validation
    const auto* scudoTagged = reinterpret_cast<const void*>(0xb40000714dc8a8d0ull);
    const auto* scudoMisaligned = reinterpret_cast<const void*>(0xb40000714dc8a8d3ull);
    const auto* lowNullPage = reinterpret_cast<const void*>(0x0000000000000028ull);
    BG_EXPECT(sig::untagUserAddr(0xb40000714dc8a8d0ull) == 0x000000714dc8a8d0ull);
    BG_EXPECT(sig::isValidUserAddress(scudoTagged, 8));
    BG_EXPECT(!sig::isValidUserAddress(scudoMisaligned, 8));
    BG_EXPECT(!sig::isValidUserAddress(nullptr, 1));
    BG_EXPECT(!sig::isValidUserAddress(lowNullPage, 1));

    // 3. Corner Stair multi-box morphing & Connected Wall/Fence arms & Support validation
    const auto straight = StateInspector::buildStairWorldBoxes(
        {0, 64, 0}, Facing::North, false, false, Facing::North, false, Facing::North);
    const auto outerCorner = StateInspector::buildStairWorldBoxes(
        {0, 64, 0}, Facing::North, false, true, Facing::East, false, Facing::North);
    const auto innerCorner = StateInspector::buildStairWorldBoxes(
        {0, 64, 0}, Facing::North, false, false, Facing::North, true, Facing::East);
    BG_EXPECT(straight.size() == 2);
    BG_EXPECT(outerCorner.size() == 2 && outerCorner[1].volume() < straight[1].volume());
    BG_EXPECT(innerCorner.size() == 3);

    const auto wallArms = StateInspector::buildConnectedPostWorldBoxes(
        {0, 64, 0}, "minecraft:cobblestone_wall", true, false, true, false);
    BG_EXPECT(wallArms.size() == 3);

    // Ceiling torch (Facing::Down) rejected; upright floor torch accepted; hanging lantern accepted
    BG_EXPECT(!StateInspector::validateBlockSupport(
        "minecraft:torch", {0, 65, 0}, Facing::Down, "minecraft:air", "minecraft:stone", "minecraft:stone"));
    BG_EXPECT(StateInspector::validateBlockSupport(
        "minecraft:torch", {0, 65, 0}, Facing::Up, "minecraft:stone", "minecraft:air", "minecraft:stone"));
    BG_EXPECT(StateInspector::validateBlockSupport(
        "minecraft:lantern", {0, 65, 0}, Facing::Down, "minecraft:air", "minecraft:stone", "minecraft:stone"));
    BG_EXPECT(!StateInspector::validateBlockSupport(
        "minecraft:wheat", {0, 65, 0}, Facing::Up, "minecraft:stone", "minecraft:air", "minecraft:stone"));
    BG_EXPECT(StateInspector::validateBlockSupport(
        "minecraft:wheat", {0, 65, 0}, Facing::Up, "minecraft:farmland", "minecraft:air", "minecraft:farmland"));

    // 4. Simulated DebugRenderer (+0x08 flag, +0x50 lineVec[3], +0x68 triVec[3]) non-owning swap
    alignas(16) uint8_t fakeDebugRenderer[0x90]{};
    alignas(16) uint8_t fakeRenderParams[0x40]{};
    const PackedVertex sentinelLine{};
    const PackedVertex sentinelTri{};
    auto* lineSlots = reinterpret_cast<const PackedVertex**>(fakeDebugRenderer + sig::off::DebugRenderer_LineVec);
    auto* triSlots  = reinterpret_cast<const PackedVertex**>(fakeDebugRenderer + sig::off::DebugRenderer_TriVec);
    lineSlots[0] = &sentinelLine;
    lineSlots[1] = &sentinelLine + 1;
    lineSlots[2] = &sentinelLine + 1;
    triSlots[0]  = &sentinelTri;
    triSlots[1]  = &sentinelTri + 1;
    triSlots[2]  = &sentinelTri + 1;

    static std::size_t sCapturedTriVerts = 0;
    static std::size_t sCapturedLineVerts = 0;
    sCapturedTriVerts = 0;
    sCapturedLineVerts = 0;

    auto mockRenderEnd = [](void* dbg, void* /*rp*/) {
        auto* bytes = static_cast<uint8_t*>(dbg);
        const uint8_t flag = bytes[sig::off::DebugRenderer_ActiveFlag];
        if (!flag) return;
        bytes[sig::off::DebugRenderer_ActiveFlag] = 0u;
        auto* lv = reinterpret_cast<const PackedVertex* const*>(bytes + sig::off::DebugRenderer_LineVec);
        auto* tv = reinterpret_cast<const PackedVertex* const*>(bytes + sig::off::DebugRenderer_TriVec);
        if (tv[0] && tv[1] > tv[0]) {
            sCapturedTriVerts += static_cast<std::size_t>(tv[1] - tv[0]);
        }
        if (lv[0] && lv[1] > lv[0]) {
            sCapturedLineVerts += static_cast<std::size_t>(lv[1] - lv[0]);
        }
    };

    EngineBridge::flushMeshIntoDebugRenderer(fakeDebugRenderer, fakeRenderParams, mockRenderEnd, mesh);
    BG_EXPECT(sCapturedTriVerts == mesh.packedTriangles.size());
    BG_EXPECT(sCapturedLineVerts == mesh.packedLines.size());
    BG_EXPECT(fakeDebugRenderer[sig::off::DebugRenderer_ActiveFlag] == 0u);
    BG_EXPECT(lineSlots[0] == &sentinelLine && lineSlots[1] == &sentinelLine + 1 && lineSlots[2] == &sentinelLine + 1);
    BG_EXPECT(triSlots[0] == &sentinelTri && triSlots[1] == &sentinelTri + 1 && triSlots[2] == &sentinelTri + 1);

    // Host lifecycle sanity check
    BG_EXPECT(EngineBridge::instance().installHooks());
    BG_EXPECT(EngineBridge::instance().stats().hooksInstalled);
    EngineBridge::instance().uninstallHooks();
    BG_EXPECT(!EngineBridge::instance().stats().hooksInstalled);
}

} // namespace

int main() {
    testImmediateDisappearanceConditions();
    testImmediateTargetPositionAndReplaceableUpdates();
    testOrientationsSlabsStairsPillarsDoorsBeds();
    testConfigPersistenceAndVersionMetadata();
    testNativeHardwareRegressionInvariants();

    if (gFailures > 0) {
        std::cerr << "BuildGhost test suite failed with " << gFailures << " error(s)." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "All BuildGhost ASan/UBSan unit & geometry tests passed (0 errors)!" << std::endl;
    return EXIT_SUCCESS;
}
