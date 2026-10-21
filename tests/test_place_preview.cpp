#include "placepreview/Config.hpp"
#include "placepreview/EngineBridge.hpp"
#include "placepreview/GhostMesher.hpp"
#include "placepreview/MaterialPalette.hpp"
#include "placepreview/PlacementResolver.hpp"
#include "placepreview/PreviewAnimator.hpp"
#include "placepreview/Signatures.hpp"
#include "placepreview/StateInspector.hpp"
#include "placepreview/Version.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <tuple>

using namespace placepreview;

namespace {

bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

class FakeWorld final : public IWorldQuery {
public:
    std::map<std::tuple<int, int, int>, std::string> blocks;
    std::map<std::tuple<int, int, int>, std::vector<AABB>> customBoxes;

    void setBlock(int x, int y, int z, std::string id, std::vector<AABB> boxes = {}) {
        blocks[{x, y, z}] = std::move(id);
        if (!boxes.empty()) {
            customBoxes[{x, y, z}] = std::move(boxes);
        }
    }

    int minBuildHeight() const noexcept override { return -64; }
    int maxBuildHeight() const noexcept override { return 319; }

    std::string getBlockIdentifier(const BlockPos& pos) const override {
        auto it = blocks.find({pos.x, pos.y, pos.z});
        return (it == blocks.end()) ? "minecraft:air" : it->second;
    }

    bool isAirBlock(const BlockPos& pos) const override {
        const std::string id = getBlockIdentifier(pos);
        const auto s = StateInspector::stripNamespace(id);
        return s.empty() || s == "air" || s == "cave_air" || s == "void_air";
    }

    bool isReplaceableBlock(const BlockPos& pos) const override {
        const std::string id = getBlockIdentifier(pos);
        const auto s = StateInspector::stripNamespace(id);
        return s.empty() || s == "air" || s == "water" || s == "flowing_water" ||
               s == "bubble_column" || s == "short_grass";
    }

    std::vector<AABB> getExistingBlockWorldBoxes(const BlockPos& pos) const override {
        auto bit = customBoxes.find({pos.x, pos.y, pos.z});
        if (bit != customBoxes.end()) {
            return bit->second;
        }
        const std::string id = getBlockIdentifier(pos);
        if (StateInspector::isLiquidOrAirBlock(id)) return {};
        const OrientationSummary o = StateInspector::decodeOrientation(
            id, {}, Facing::Up, {0.5f, 0.5f, 0.5f}, 0.0f, 0.0f);
        return StateInspector::buildFallbackBlockBoxes(pos, id, o);
    }

    bool checkAdventureCanPlaceOn(const HeldItemInput&, const BlockPos&) const override {
        return true;
    }

    bool calculatePlacePos(const HeldItemInput& item, Facing& inOutFace, BlockPos& inOutPos) const override {
        std::string placeId = StateInspector::resolveSpecialItemBlockName(item.itemIdentifier, inOutFace);
        if (placeId.empty() && !item.directBlockIdentifier.empty()) {
            const std::string swapped =
                StateInspector::resolveSpecialItemBlockName(item.directBlockIdentifier, inOutFace);
            placeId = !swapped.empty() ? swapped : item.directBlockIdentifier;
        }
        if (StateInspector::isSlabBlock(placeId) && getBlockIdentifier(inOutPos) == placeId) {
            const auto ex = getExistingBlockWorldBoxes(inOutPos);
            if (!ex.empty()) {
                const bool isTop = (ex.front().min.y > static_cast<float>(inOutPos.y) + 0.25f);
                if ((!isTop && inOutFace == Facing::Up) || (isTop && inOutFace == Facing::Down)) {
                    return true;
                }
            }
        }
        if (isReplaceableBlock(inOutPos)) return true;
        const BlockPos step = facingStep(inOutFace);
        inOutPos = inOutPos.offset(step.x, step.y, step.z);
        return isReplaceableBlock(inOutPos) ||
               (StateInspector::isSlabBlock(placeId) && getBlockIdentifier(inOutPos) == placeId);
    }

    bool mayPlaceBlock(
        const HeldItemInput&,
        const std::string&,
        const BlockPos& placePos,
        Facing,
        const Vec3&) const override {
        return placePos.y >= -64 && placePos.y <= 319;
    }

    bool computePlacedBlockStateAndBoxes(
        const HeldItemInput&,
        const std::string&,
        const BlockPos&,
        Facing,
        const Vec3&,
        std::string&,
        uint16_t&,
        std::vector<BlockStateProperty>&,
        std::vector<AABB>&,
        std::vector<OrientedBox>&) const override {
        return false;
    }
};

void testMetadataColorPickerAndDiskPersistence() {
    assert(version::Name == "Place Preview");
    assert(version::Id == "zxwmwmwm.placepreview");
    assert(version::Version == "1.0.0");
    assert(version::Author == "Zxwmwmwm");

    int r = 0, g = 0, b = 0, a = 0;
    assert(ConfigManager::parseHexColor("#DC48C8FF", r, g, b, a));
    assert(a == 0xDC && r == 0x48 && g == 0xC8 && b == 0xFF);
    assert(ConfigManager::parseHexColor("#34E29A", r, g, b, a));
    assert(a == 255 && r == 0x34 && g == 0xE2 && b == 0x9A);
    assert(ConfigManager::parseHexColor("#00FF8040", r, g, b, a));
    assert(a == 0x00 && r == 0xFF && g == 0x80 && b == 0x40);
    assert(!ConfigManager::parseHexColor("invalid", r, g, b, a));

    // Full disk roundtrip persistence check (simulating Mod Menu close/reopen + app restart!)
    const std::filesystem::path tmpDir = std::filesystem::temp_directory_path() / "placepreview_cfg_test";
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);

    ConfigManager::instance().init(tmpDir);
    ConfigManager::instance().applyKeyValue("previewColor", "#8040E0D0");
    ConfigManager::instance().applyKeyValue("renderMode", "Wireframe Only");
    ConfigManager::instance().applyKeyValue("fillOpacity", "0.52");
    ConfigManager::instance().applyKeyValue("wireframeOpacity", "0.91");
    ConfigManager::instance().applyKeyValue("animationSpeed", "26.50");
    ConfigManager::instance().applyKeyValue("showOrientationGuides", "true");
    ConfigManager::instance().setEnabled(false);

    // Simulate brand-new launch reloading from `placepreview.json` on disk:
    const std::filesystem::path savedCopy = tmpDir / "saved_copy.json";
    std::filesystem::copy_file(
        tmpDir / "placepreview.json",
        savedCopy,
        std::filesystem::copy_options::overwrite_existing,
        ec);
    ConfigManager::instance().resetDefaults();
    assert(ConfigManager::instance().snapshot().enabled);
    assert(ConfigManager::instance().loadFromFile(savedCopy));
    const ModConfig reloaded = ConfigManager::instance().snapshot();
    assert(!reloaded.enabled);
    assert(reloaded.previewColorHex() == "#8040E0D0");
    assert(reloaded.accentAlpha == 0x80 && reloaded.accentRed == 0x40 &&
           reloaded.accentGreen == 0xE0 && reloaded.accentBlue == 0xD0);
    assert(approx(reloaded.colorAlphaMultiplier(), 128.0f / 255.0f, 1e-3f));
    assert(reloaded.renderMode == "Wireframe Only");
    assert(approx(reloaded.fillOpacity, 0.52f));
    assert(approx(reloaded.wireframeOpacity, 0.91f));
    assert(approx(reloaded.animationSpeed, 26.50f));
    assert(reloaded.showOrientationGuides);

    std::filesystem::remove_all(tmpDir, ec);
    ConfigManager::instance().resetDefaults();
}

void testStandingSign16AnglesAndWallSign() {
    FakeWorld world;
    world.setBlock(10, 63, 10, "minecraft:dirt");
    world.setBlock(10, 64, 11, "minecraft:stone");
    const ModConfig cfg{};

    for (int step = 0; step < 16; ++step) {
        const float yawDeg = -180.0f + static_cast<float>(step) * 22.5f;
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.yawDeg = yawDeg;
        ctx.pitchDeg = 25.0f;
        ctx.playerAabb = AABB{10.15f, 64.0f, 10.15f, 10.75f, 65.8f, 10.75f};
        ctx.mainhand = HeldItemInput{true, 64, 0, "minecraft:oak_sign", "minecraft:standing_sign", 0};
        ctx.raycast.hasBlockHit = true;
        ctx.raycast.blockPos = {10, 63, 10};
        ctx.raycast.hitFace = Facing::Up;
        ctx.raycast.hitWorldPos = {10.5f, 64.0f, 10.5f};

        const PlacementPreviewState st = PlacementResolver::resolve(ctx, world, cfg);
        assert(st.valid);
        assert(st.blockIdentifier == "minecraft:standing_sign");
        assert(st.primaryPos == BlockPos(10, 64, 10));
        assert(st.orientationKind == OrientationKind::Sign16);
        assert(st.orientedBoxes.size() == 2);

        const OrientedBox& post = st.orientedBoxes[0];
        const OrientedBox& board = st.orientedBoxes[1];
        assert(post.halfExtents.x < 0.06f);
        assert(approx(board.halfExtents.x, 0.50f));
        assert(board.halfExtents.z < 0.06f);

        const Vec3 look = PlacementResolver::lookDirectionFromAngles(0.0f, yawDeg);
        assert(approx(board.axisU.dot(look), 0.0f, 1e-3f));
        assert(approx((board.axisW * -1.0f).dot(look * -1.0f), 1.0f, 1e-3f));
    }

    // Even when `directBlockIdentifier` is `minecraft:standing_sign`, aiming at a vertical wall
    // MUST swap cleanly to `minecraft:wall_sign` AND aiming under a ceiling MUST be invalid!
    {
        PlayerContextInput wallCtx{};
        wallCtx.inWorld = true;
        wallCtx.mainhand = HeldItemInput{true, 64, 0, "minecraft:oak_sign", "minecraft:standing_sign", 0};
        wallCtx.raycast.hasBlockHit = true;
        wallCtx.raycast.blockPos = {10, 64, 11};
        wallCtx.raycast.hitFace = Facing::North;
        wallCtx.raycast.hitWorldPos = {10.5f, 64.5f, 11.0f};
        const PlacementPreviewState wallSt = PlacementResolver::resolve(wallCtx, world, cfg);
        assert(wallSt.valid);
        assert(wallSt.blockIdentifier == "minecraft:wall_sign");
        assert(wallSt.worldBoxes.size() == 1 && wallSt.worldBoxes[0].sizeZ() < 0.12f);

        wallCtx.raycast.hitFace = Facing::Down;
        const PlacementPreviewState ceilSt = PlacementResolver::resolve(wallCtx, world, cfg);
        assert(!ceilSt.valid);
    }

    assert(StateInspector::resolveSpecialItemBlockName("minecraft:dark_oak_sign", Facing::Up) ==
           "minecraft:darkoak_standing_sign");
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:dark_oak_sign", Facing::North) ==
           "minecraft:darkoak_wall_sign");
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:oak_hanging_sign", Facing::Up).empty());
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:oak_hanging_sign", Facing::Down) ==
           "minecraft:oak_hanging_sign");
}

void testUnderwaterRaycastingAndSpecialItemOutlines() {
    FakeWorld world;
    // Build an underwater ocean room filled with `water` & `bubble_column`:
    for (int x = 358; x <= 364; ++x) {
        for (int z = -26; z <= -20; ++z) {
            world.setBlock(x, 61, z, "minecraft:stone"); // ocean floor at y=61
            world.setBlock(x, 62, z, "minecraft:water");
            world.setBlock(x, 63, z, (x == 361) ? "minecraft:bubble_column" : "minecraft:water");
            world.setBlock(x, 64, z, "minecraft:water");
        }
    }
    // Underwater stone wall at x = 363
    for (int y = 62; y <= 65; ++y) {
        for (int z = -26; z <= -20; ++z) {
            world.setBlock(363, y, z, "minecraft:stone");
        }
    }

    const ModConfig cfg{};

    // 1. Raycast from underwater camera at (360.5, 63.6, -22.5) straight toward +X wall at x=363:
    //    MUST pass straight through `water` AND `bubble_column` and hit wall face `West` at (363, 63, -23)!
    const RaycastHitInput wallRay = PlacementResolver::raycastWorld(
        {360.5f, 63.6f, -22.5f}, {1.0f, 0.0f, 0.0f}, 6.0f, world);
    assert(wallRay.hasBlockHit);
    assert(wallRay.blockPos == BlockPos(363, 63, -23));
    assert(wallRay.hitFace == Facing::West);

    // 2. Item Frame (`minecraft:item_frame` AND `minecraft:glow_frame`) on underwater wall:
    //    MUST be exact 12x12x1 thin frame flush at x = [362.9375 .. 363.0], NEVER a 1x1x1 cube!
    for (const char* frameItem : {"minecraft:item_frame", "minecraft:glow_frame"}) {
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.mainhand = HeldItemInput{true, 64, 0, frameItem, "", 0};
        ctx.raycast = wallRay;
        const PlacementPreviewState st = PlacementResolver::resolve(ctx, world, cfg);
        assert(st.valid);
        assert(st.primaryPos == BlockPos(362, 63, -23));
        assert(st.worldBoxes.size() == 1);
        assert(approx(st.worldBoxes[0].sizeX(), 0.0625f));
        assert(approx(st.worldBoxes[0].sizeY(), 0.75f));
        assert(approx(st.worldBoxes[0].sizeZ(), 0.75f));
        assert(approx(st.worldBoxes[0].max.x, 363.0f));
    }

    // 3. Painting (`minecraft:painting`): thin 16x16x1 on vertical walls, hidden on ocean floor!
    {
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.mainhand = HeldItemInput{true, 16, 0, "minecraft:painting", "", 0};
        ctx.raycast = wallRay;
        const PlacementPreviewState wallPaint = PlacementResolver::resolve(ctx, world, cfg);
        assert(wallPaint.valid && wallPaint.worldBoxes.size() == 1);
        assert(approx(wallPaint.worldBoxes[0].sizeX(), 0.0625f));
        assert(approx(wallPaint.worldBoxes[0].sizeY(), 1.0f));
        assert(approx(wallPaint.worldBoxes[0].sizeZ(), 1.0f));

        const RaycastHitInput floorRay = PlacementResolver::raycastWorld(
            {360.5f, 63.6f, -22.5f}, {0.0f, -1.0f, 0.0f}, 6.0f, world);
        assert(floorRay.hasBlockHit && floorRay.blockPos == BlockPos(360, 61, -23));
        ctx.raycast = floorRay;
        assert(!PlacementResolver::resolve(ctx, world, cfg).valid);
    }

    // 4. Sea Pickle (`minecraft:sea_pickle`) underwater on ocean floor + 1..4 cluster boxes:
    {
        const RaycastHitInput floorRay = PlacementResolver::raycastWorld(
            {360.5f, 63.6f, -22.5f}, {0.0f, -1.0f, 0.0f}, 6.0f, world);
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.mainhand = HeldItemInput{true, 64, 0, "minecraft:sea_pickle", "minecraft:sea_pickle", 0};
        ctx.raycast = floorRay;
        const PlacementPreviewState pickle = PlacementResolver::resolve(ctx, world, cfg);
        assert(pickle.valid);
        assert(pickle.primaryPos == BlockPos(360, 62, -23));
        assert(pickle.worldBoxes.size() == 1);
        assert(approx(pickle.worldBoxes[0].sizeX(), 0.25f));
        assert(approx(pickle.worldBoxes[0].sizeY(), 0.375f));
        assert(approx(pickle.worldBoxes[0].sizeZ(), 0.25f));

        for (int c = 1; c <= 4; ++c) {
            assert(static_cast<int>(StateInspector::buildSeaPickleClusterWorldBoxes({360, 62, -23}, c).size()) == c);
            assert(static_cast<int>(StateInspector::buildTurtleEggClusterWorldBoxes({360, 62, -23}, c).size()) == c);
        }
    }

    // 5. End Rod on vertical wall: mounting cap flush on wall + horizontal shaft!
    {
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.mainhand = HeldItemInput{true, 64, 0, "minecraft:end_rod", "minecraft:end_rod", 0};
        ctx.raycast = wallRay;
        const PlacementPreviewState rod = PlacementResolver::resolve(ctx, world, cfg);
        assert(rod.valid && rod.worldBoxes.size() == 2);
        assert(approx(rod.worldBoxes[0].max.x, 363.0f));
        assert(approx(rod.worldBoxes[1].min.x, 362.0f));
    }

    // 6. Skeleton Skull on wall (0.5^3 against wall) & on floor (rotated 16-angle OBB):
    {
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.yawDeg = 45.0f;
        ctx.mainhand = HeldItemInput{true, 64, 0, "minecraft:skeleton_skull", "", 0};
        ctx.raycast = wallRay;
        const PlacementPreviewState wallSkull = PlacementResolver::resolve(ctx, world, cfg);
        assert(wallSkull.valid && wallSkull.worldBoxes.size() == 1);
        assert(approx(wallSkull.worldBoxes[0].sizeX(), 0.5f));
        assert(approx(wallSkull.worldBoxes[0].max.x, 363.0f));

        ctx.raycast = PlacementResolver::raycastWorld(
            {360.5f, 63.6f, -22.5f}, {0.0f, -1.0f, 0.0f}, 6.0f, world);
        const PlacementPreviewState floorSkull = PlacementResolver::resolve(ctx, world, cfg);
        assert(floorSkull.valid && floorSkull.orientedBoxes.size() == 1);
        assert(approx(floorSkull.orientedBoxes[0].halfExtents.x, 0.25f));
    }
}

void testSmoothHotbarBlockSwitchMorphing() {
    FakeWorld world;
    for (int x = 0; x <= 6; ++x) {
        for (int z = 0; z <= 6; ++z) {
            world.setBlock(x, 63, z, "minecraft:stone");
        }
    }
    world.setBlock(2, 66, 4, "minecraft:stone"); // ceiling block for hanging sign

    ModConfig cfg{};
    cfg.smoothAnimation = true;
    cfg.animationSpeed = 18.0f;

    auto resolveFloor = [&](HeldItemInput held, float yaw = 22.5f) {
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.yawDeg = yaw;
        ctx.mainhand = std::move(held);
        ctx.raycast.hasBlockHit = true;
        ctx.raycast.blockPos = {2, 63, 4};
        ctx.raycast.hitFace = Facing::Up;
        ctx.raycast.hitWorldPos = {2.5f, 64.0f, 4.5f};
        return PlacementResolver::resolve(ctx, world, cfg);
    };

    const PlacementPreviewState cube = resolveFloor({true, 64, 0, "minecraft:stone", "minecraft:stone", 0});
    const PlacementPreviewState pickle = resolveFloor({true, 64, 0, "minecraft:sea_pickle", "minecraft:sea_pickle", 0});
    const PlacementPreviewState sign = resolveFloor({true, 16, 0, "minecraft:oak_sign", "minecraft:standing_sign", 0}, 45.0f);
    const PlacementPreviewState bed = resolveFloor({true, 1, 0, "minecraft:bed", "", 0}, 0.0f);
    const PlacementPreviewState slab = resolveFloor({true, 64, 0, "minecraft:crimson_slab", "minecraft:crimson_slab", 0});

    PreviewAnimator anim;
    PlacementPreviewState cur{};

    // 1. Settle on full 1x1x1 Stone block at (2, 64, 4)
    for (int i = 0; i < 30; ++i) cur = anim.update(cube, cfg, 1.0f / 60.0f);
    assert(cur.valid && cur.worldBoxes.size() == 1);
    assert(approx(cur.worldBoxes[0].sizeY(), 1.0f, 1e-2f));

    // 2. Switch hotbar to Sea Pickle (`sizeY = 0.375`): first frame MUST morph smoothly (`0.40 < sizeY < 0.99`), NOT snap!
    cur = anim.update(pickle, cfg, 1.0f / 60.0f);
    assert(cur.valid && cur.worldBoxes.size() == 1);
    assert(cur.worldBoxes[0].sizeY() > 0.40f && cur.worldBoxes[0].sizeY() < 0.99f);
    for (int i = 0; i < 35; ++i) cur = anim.update(pickle, cfg, 1.0f / 60.0f);
    assert(approx(cur.worldBoxes[0].sizeY(), 0.375f, 1e-2f));

    // 3. Switch hotbar from 1-AABB Sea Pickle -> 2-OBB 45-degree Standing Sign:
    //    First frame promotes smoothly into 2 orthonormal OBBs rotating & growing toward sign post + board!
    cur = anim.update(sign, cfg, 1.0f / 60.0f);
    assert(cur.valid && cur.orientedBoxes.size() == 2);
    assert(cur.orientedBoxes[1].halfExtents.x > 0.13f && cur.orientedBoxes[1].halfExtents.x < 0.49f);
    // Verify strict right-handed orthonormality (`det([U, V, W]) == +1`)!
    const float det = cur.orientedBoxes[1].axisU.cross(cur.orientedBoxes[1].axisV).dot(cur.orientedBoxes[1].axisW);
    assert(approx(det, 1.0f, 1e-3f));
    for (int i = 0; i < 35; ++i) cur = anim.update(sign, cfg, 1.0f / 60.0f);
    assert(cur.orientedBoxes.size() == 2 && approx(cur.orientedBoxes[1].halfExtents.x, 0.50f, 1e-2f));

    // 4. Switch hotbar from 2-OBB Sign -> 6-AABB Bed -> 1-AABB Slab:
    //    Smoothly morphs all 6 parts and settles back cleanly onto 1 exact AABB!
    cur = anim.update(bed, cfg, 1.0f / 60.0f);
    assert(cur.valid && cur.orientedBoxes.size() == 6);
    for (int i = 0; i < 40; ++i) cur = anim.update(bed, cfg, 1.0f / 60.0f);
    assert(cur.orientedBoxes.empty() && cur.worldBoxes.size() == 6);

    cur = anim.update(slab, cfg, 1.0f / 60.0f);
    assert(cur.valid && cur.worldBoxes.size() == 6);
    for (int i = 0; i < 40; ++i) cur = anim.update(slab, cfg, 1.0f / 60.0f);
    assert(cur.worldBoxes.size() == 1 && approx(cur.worldBoxes[0].sizeY(), 0.5f, 1e-2f));
}

void testNonPlaceableItemsNeverPreview() {
    FakeWorld world;
    world.setBlock(0, 63, 2, "minecraft:stone");
    const ModConfig cfg{};

    for (const char* nonBlockItem : {
             "minecraft:diamond_sword",
             "minecraft:netherite_pickaxe",
             "minecraft:golden_apple",
             "minecraft:water_bucket",
             "minecraft:bow",
             "minecraft:shield",
             "minecraft:ender_pearl"}) {
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.mainhand = HeldItemInput{true, 1, 0, nonBlockItem, "", 0};
        ctx.raycast.hasBlockHit = true;
        ctx.raycast.blockPos = {0, 63, 2};
        ctx.raycast.hitFace = Facing::Up;
        ctx.raycast.hitWorldPos = {0.5f, 64.0f, 2.5f};

        const PlacementPreviewState st = PlacementResolver::resolve(ctx, world, cfg);
        assert(!st.valid);
    }
}

void testSlabMergeAndDebugRendererSwapPass() {
    FakeWorld world;
    world.setBlock(3, 64, 3, "minecraft:oak_slab", {AABB{3.0f, 64.0f, 3.0f, 4.0f, 64.5f, 4.0f}});
    const ModConfig cfg{};

    PlayerContextInput ctx{};
    ctx.inWorld = true;
    ctx.mainhand = HeldItemInput{true, 16, 0, "minecraft:oak_slab", "minecraft:oak_slab", 0};
    ctx.raycast.hasBlockHit = true;
    ctx.raycast.blockPos = {3, 64, 3};
    ctx.raycast.hitFace = Facing::Up;
    ctx.raycast.hitWorldPos = {3.5f, 64.5f, 3.5f};

    const PlacementPreviewState st = PlacementResolver::resolve(ctx, world, cfg);
    assert(st.valid && st.isSlabMerge);
    assert(st.worldBoxes.size() == 1);
    assert(approx(st.worldBoxes[0].min.y, 64.5f) && approx(st.worldBoxes[0].max.y, 65.0f));

    GhostMeshOutput mesh{};
    GhostMesher::buildMesh(st, cfg, {3.5f, 66.2f, 1.2f}, 0.0f, mesh);
    assert(!mesh.empty());

    alignas(16) uint8_t fakeDebugRenderer[0x240]{};
    uint8_t fakeParams[64]{};
    static int sCalls = 0;
    sCalls = 0;
    EngineBridge::flushMeshIntoDebugRenderer(
        fakeDebugRenderer,
        fakeParams,
        [](void*, void*) { ++sCalls; },
        mesh);
    // Single-pass `"entity_alphablend"` submission (`packedTriangles` contains fill faces first,
    // alpha-blended 3D outline ribbons second, and zero unblended `"debug"` `packedLines`)!
    assert(mesh.packedLines.empty());
    assert(!mesh.packedTriangles.empty());
    assert(sCalls == 1);
}

void testSchemaV2PresetsRenderStylesMaterialAdaptiveAndOutlineOpacity() {
    auto& mgr = ConfigManager::instance();
    mgr.resetDefaults();

    FakeWorld world;
    world.setBlock(0, 63, 2, "minecraft:stone");

    auto makePreview = [&](const char* itemId, const char* blockId) {
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.mainhand = HeldItemInput{true, 64, 0, itemId, blockId, 0};
        ctx.raycast.hasBlockHit = true;
        ctx.raycast.blockPos = {0, 63, 2};
        ctx.raycast.hitFace = Facing::Up;
        ctx.raycast.hitWorldPos = {0.5f, 64.0f, 2.5f};
        return PlacementResolver::resolve(ctx, world, mgr.snapshot());
    };

    const Vec3 cam{0.5f, 65.8f, 0.2f};

    // 1. Verify ALL 6 Quick Presets + transitions between Crisp Wireframe <-> Translucent Fill <-> Shaded + Wireframe:
    for (const char* preset : {
             "Frosted White",
             "Classic Cyan",
             "Material Adaptive",
             "Crisp Wireframe",
             "Subtle Minimal",
             "High Contrast"}) {
        mgr.applyKeyValue("preset", preset);
        const ModConfig snap = mgr.snapshot();
        assert(snap.preset == preset);
        const PlacementPreviewState st = makePreview("minecraft:oak_planks", "minecraft:oak_planks");
        GhostMeshOutput mesh{};
        GhostMesher::buildMesh(st, snap, cam, 0.0f, mesh);
        assert(!mesh.empty());
        assert(mesh.packedLines.empty());
        if (std::string_view(preset) == "Crisp Wireframe") {
            assert(snap.renderMode == "Wireframe Only");
            assert(mesh.triangles.empty());
            assert(!mesh.lines.empty());
            assert(!mesh.packedTriangles.empty()); // ribbon quads in `entity_alphablend`!
        } else {
            assert(!mesh.triangles.empty());
            assert(!mesh.lines.empty());
        }
    }

    // Switching straight from Crisp Wireframe -> Translucent Fill -> Wireframe Only -> Shaded + Wireframe:
    mgr.applyKeyValue("preset", "Crisp Wireframe");
    mgr.applyKeyValue("renderMode", "Translucent Fill");
    {
        const ModConfig snap = mgr.snapshot();
        assert(snap.renderMode == "Translucent Fill");
        assert(snap.fillOpacity >= 0.05f);
        GhostMeshOutput mesh{};
        GhostMesher::buildMesh(makePreview("minecraft:oak_planks", "minecraft:oak_planks"), snap, cam, 0.0f, mesh);
        assert(!mesh.triangles.empty());
        assert(mesh.lines.empty());
        assert(mesh.packedTriangles.size() == mesh.triangles.size() * 3);
    }

    mgr.applyKeyValue("renderMode", "Wireframe Only");
    {
        const ModConfig snap = mgr.snapshot();
        assert(snap.renderMode == "Wireframe Only");
        GhostMeshOutput mesh{};
        GhostMesher::buildMesh(makePreview("minecraft:oak_planks", "minecraft:oak_planks"), snap, cam, 0.0f, mesh);
        assert(mesh.triangles.empty());
        assert(!mesh.lines.empty());
        assert(mesh.packedTriangles.size() == mesh.lines.size() * 12);
    }

    mgr.applyKeyValue("renderMode", "Shaded + Wireframe");
    {
        const ModConfig snap = mgr.snapshot();
        assert(snap.renderMode == "Shaded + Wireframe");
        GhostMeshOutput mesh{};
        GhostMesher::buildMesh(makePreview("minecraft:oak_planks", "minecraft:oak_planks"), snap, cam, 0.0f, mesh);
        assert(!mesh.triangles.empty());
        assert(!mesh.lines.empty());
        assert(mesh.packedTriangles.size() == mesh.triangles.size() * 3 + mesh.lines.size() * 12);
    }

    // 2. Verify Material Adaptive produces distinct, accurate per-material colors on GPU vertices:
    mgr.applyKeyValue("colorMode", "Material Adaptive");
    assert(mgr.snapshot().colorMode == "Material Adaptive");
    {
        const auto oak = makePreview("minecraft:oak_planks", "minecraft:oak_planks");
        const auto redWool = makePreview("minecraft:red_wool", "minecraft:red_wool");
        const auto dirt = makePreview("minecraft:dirt", "minecraft:dirt");
        const auto pickle = makePreview("minecraft:sea_pickle", "minecraft:sea_pickle");
        const auto soul = makePreview("minecraft:soul_lantern", "minecraft:soul_lantern");
        const auto frame = makePreview("minecraft:item_frame", "");

        assert(oak.materialColor.r > oak.materialColor.b);
        assert(redWool.materialColor.r > 180 && redWool.materialColor.g < 90);
        assert(dirt.materialColor.r > dirt.materialColor.b);
        assert(pickle.materialColor.g > pickle.materialColor.b);
        assert(soul.materialColor.b > soul.materialColor.r);
        (void)frame;

        GhostMeshOutput oakMesh{}, redMesh{};
        GhostMesher::buildMesh(oak, mgr.snapshot(), cam, 0.0f, oakMesh);
        GhostMesher::buildMesh(redWool, mgr.snapshot(), cam, 0.0f, redMesh);
        assert((oakMesh.packedTriangles.front().abgr & 0x00FFFFFFu) !=
               (redMesh.packedTriangles.front().abgr & 0x00FFFFFFu));
    }

    // 3. Verify Outline Opacity (`wireframeOpacity`) strictly modulates actual hardware GPU alpha
    //    bytes on the emitted `"entity_alphablend"` ribbon vertices across 0.00 -> 0.25 -> 0.70 -> 1.00!
    mgr.applyKeyValue("preset", "Crisp Wireframe");
    const auto cube = makePreview("minecraft:stone", "minecraft:stone");

    mgr.applyKeyValue("wireframeOpacity", "0.00");
    {
        GhostMeshOutput m0{};
        GhostMesher::buildMesh(cube, mgr.snapshot(), cam, 0.0f, m0);
        assert(m0.packedTriangles.empty());
    }

    uint32_t alpha25 = 0, alpha70 = 0, alpha100 = 0;
    mgr.applyKeyValue("wireframeOpacity", "0.25");
    {
        GhostMeshOutput m25{};
        GhostMesher::buildMesh(cube, mgr.snapshot(), cam, 0.0f, m25);
        assert(!m25.packedTriangles.empty());
        alpha25 = (m25.packedTriangles.back().abgr >> 24u) & 0xFFu;
    }
    mgr.applyKeyValue("wireframeOpacity", "0.70");
    {
        GhostMeshOutput m70{};
        GhostMesher::buildMesh(cube, mgr.snapshot(), cam, 0.0f, m70);
        alpha70 = (m70.packedTriangles.back().abgr >> 24u) & 0xFFu;
    }
    mgr.applyKeyValue("wireframeOpacity", "1.00");
    {
        GhostMeshOutput m100{};
        GhostMesher::buildMesh(cube, mgr.snapshot(), cam, 0.0f, m100);
        alpha100 = (m100.packedTriangles.back().abgr >> 24u) & 0xFFu;
    }
    assert(alpha25 >= 55u && alpha25 <= 72u);
    assert(alpha70 > alpha25 + 80u);
    assert(alpha100 == 255u && alpha100 > alpha70);

    mgr.resetDefaults();
}

} // namespace

int main() {
    testMetadataColorPickerAndDiskPersistence();
    testStandingSign16AnglesAndWallSign();
    testUnderwaterRaycastingAndSpecialItemOutlines();
    testSmoothHotbarBlockSwitchMorphing();
    testNonPlaceableItemsNeverPreview();
    testSlabMergeAndDebugRendererSwapPass();
    testSchemaV2PresetsRenderStylesMaterialAdaptiveAndOutlineOpacity();
    std::cout << "All Place Preview unit & sanitizer tests passed!\n";
    return 0;
}
