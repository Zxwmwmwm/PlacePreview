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
        return s.empty() || s == "air";
    }

    bool isReplaceableBlock(const BlockPos& pos) const override {
        const std::string id = getBlockIdentifier(pos);
        const auto s = StateInspector::stripNamespace(id);
        return s.empty() || s == "air" || s == "water" || s == "short_grass";
    }

    std::vector<AABB> getExistingBlockWorldBoxes(const BlockPos& pos) const override {
        auto bit = customBoxes.find({pos.x, pos.y, pos.z});
        if (bit != customBoxes.end()) {
            return bit->second;
        }
        const std::string id = getBlockIdentifier(pos);
        if (isAirBlock(pos)) return {};
        const OrientationSummary o = StateInspector::decodeOrientation(
            id, {}, Facing::Up, {0.5f, 0.5f, 0.5f}, 0.0f, 0.0f);
        return StateInspector::buildFallbackBlockBoxes(pos, id, o);
    }

    bool checkAdventureCanPlaceOn(const HeldItemInput&, const BlockPos&) const override {
        return true;
    }

    bool calculatePlacePos(const HeldItemInput& item, Facing& inOutFace, BlockPos& inOutPos) const override {
        const std::string placeId = !item.directBlockIdentifier.empty()
                                        ? item.directBlockIdentifier
                                        : StateInspector::resolveSpecialItemBlockName(item.itemIdentifier, inOutFace);
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

void testMetadataAndHexColorPicker() {
    assert(version::Name == "Place Preview");
    assert(version::Id == "zxwmwmwm.placepreview");
    assert(version::Version == "1.0.0");
    assert(version::Author == "Zxwmwmwm");

    int r = 0, g = 0, b = 0, a = 0;
    assert(ConfigManager::parseHexColor("#DC48C8FF", r, g, b, a));
    assert(a == 0xDC && r == 0x48 && g == 0xC8 && b == 0xFF);
    assert(ConfigManager::parseHexColor("#34E29A", r, g, b, a));
    assert(a == 255 && r == 0x34 && g == 0xE2 && b == 0x9A);
    assert(!ConfigManager::parseHexColor("invalid", r, g, b, a));

    ConfigManager::instance().resetDefaults();
    ConfigManager::instance().applyKeyValue("previewColor", "#CC40E0D0");
    const ModConfig snap = ConfigManager::instance().snapshot();
    assert(snap.accentRed == 0x40);
    assert(snap.accentGreen == 0xE0);
    assert(snap.accentBlue == 0xD0);
    assert(snap.accentAlpha == 0xCC);
    assert(snap.preset == "Custom");
    assert(snap.previewColorHex() == "#CC40E0D0");

    const std::string serialized = ConfigManager::serializeJson(snap);
    assert(serialized.find("enableOffhandPreview") == std::string::npos);
    const ModConfig reparsed = ConfigManager::parseJson(serialized);
    assert(reparsed.previewColorHex() == "#CC40E0D0");
    ConfigManager::instance().resetDefaults();
}

void testStandingSign16AnglesAndWallSign() {
    FakeWorld world;
    world.setBlock(10, 63, 10, "minecraft:dirt");
    world.setBlock(10, 64, 11, "minecraft:stone");
    const ModConfig cfg{};

    // Test all 16 angles around 360 degrees!
    for (int step = 0; step < 16; ++step) {
        const float yawDeg = -180.0f + static_cast<float>(step) * 22.5f;
        PlayerContextInput ctx{};
        ctx.inWorld = true;
        ctx.yawDeg = yawDeg;
        ctx.pitchDeg = 25.0f;
        // Player standing right next to cell (10, 64, 10)
        ctx.playerAabb = AABB{10.15f, 64.0f, 10.15f, 10.75f, 65.8f, 10.75f};
        ctx.mainhand = HeldItemInput{true, 64, 0, "minecraft:oak_sign", "", 0};
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

        // Post = narrow wooden post (halfExtents.x < 0.05), Board = full 1.0 block width (halfExtents.x == 0.50)
        const OrientedBox& post = st.orientedBoxes[0];
        const OrientedBox& board = st.orientedBoxes[1];
        assert(post.halfExtents.x < 0.06f);
        assert(approx(board.halfExtents.x, 0.50f));
        assert(board.halfExtents.z < 0.06f);

        // Verify exact perpendicularity to player gaze vector!
        const Vec3 look = PlacementResolver::lookDirectionFromAngles(0.0f, yawDeg);
        assert(approx(board.axisU.dot(look), 0.0f, 1e-3f));
        assert(approx((board.axisW * -1.0f).dot(look * -1.0f), 1.0f, 1e-3f));

        GhostMeshOutput mesh{};
        GhostMesher::buildMesh(st, cfg, {10.5f, 65.62f, 8.5f}, 0.0f, mesh);
        assert(!mesh.empty());
        assert(mesh.packedTriangles.size() == mesh.triangles.size() * 3);
        assert(mesh.packedLines.size() == mesh.lines.size() * 2);
    }

    // Dark oak sign resolves to Mojang's exact `darkoak_standing_sign` / `darkoak_wall_sign`!
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:dark_oak_sign", Facing::Up) ==
           "minecraft:darkoak_standing_sign");
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:dark_oak_sign", Facing::North) ==
           "minecraft:darkoak_wall_sign");
    // Signs cannot be placed upside-down under a ceiling (`Facing::Down`)!
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:oak_sign", Facing::Down).empty());
    // Hanging signs attach to ceiling (`Down`) or walls (`North`), never floor (`Up`)!
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:oak_hanging_sign", Facing::Up).empty());
    assert(StateInspector::resolveSpecialItemBlockName("minecraft:oak_hanging_sign", Facing::Down) ==
           "minecraft:oak_hanging_sign");
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

void testSmoothBlockToBlockAnimatorAndFade() {
    FakeWorld world;
    for (int x = 0; x <= 6; ++x) {
        world.setBlock(x, 63, 4, "minecraft:stone");
    }

    ModConfig cfg{};
    cfg.smoothAnimation = true;
    cfg.animationSpeed = 18.0f;

    PlayerContextInput ctx{};
    ctx.inWorld = true;
    ctx.mainhand = HeldItemInput{true, 64, 0, "minecraft:oak_planks", "minecraft:oak_planks", 0};
    ctx.raycast.hasBlockHit = true;
    ctx.raycast.blockPos = {1, 63, 4};
    ctx.raycast.hitFace = Facing::Up;
    ctx.raycast.hitWorldPos = {1.5f, 64.0f, 4.5f};

    PreviewAnimator anim;
    PlacementPreviewState t1 = PlacementResolver::resolve(ctx, world, cfg);
    assert(t1.valid && t1.primaryPos == BlockPos(1, 64, 4));

    // Settle on cell (1, 64, 4)
    PlacementPreviewState cur{};
    for (int i = 0; i < 30; ++i) {
        cur = anim.update(t1, cfg, 1.0f / 60.0f);
    }
    assert(cur.valid);
    assert(approx(cur.worldBoxes[0].min.x, 1.0f, 1e-2f));
    assert(approx(cur.visibilityAlpha, 1.0f, 1e-2f));

    // Look smoothly over to cell (2, 64, 4): first frame MUST glide smoothly between 1.0 and 2.0!
    ctx.raycast.blockPos = {2, 63, 4};
    ctx.raycast.hitWorldPos = {2.5f, 64.0f, 4.5f};
    PlacementPreviewState t2 = PlacementResolver::resolve(ctx, world, cfg);
    const PlacementPreviewState mid = anim.update(t2, cfg, 1.0f / 60.0f);
    assert(mid.valid);
    assert(mid.worldBoxes[0].min.x > 1.05f && mid.worldBoxes[0].min.x < 1.95f);

    // Within 0.45s at 60 FPS, settles cleanly onto exact (2.0, 64.0, 4.0)!
    for (int i = 0; i < 28; ++i) {
        cur = anim.update(t2, cfg, 1.0f / 60.0f);
    }
    assert(approx(cur.worldBoxes[0].min.x, 2.0f, 1e-2f));

    // Look up at sky (`invalid`): fades smoothly downward instead of popping in 1 frame, then turns off cleanly!
    const PlacementPreviewState sky{};
    const PlacementPreviewState fade1 = anim.update(sky, cfg, 1.0f / 60.0f);
    assert(fade1.valid);
    assert(fade1.visibilityAlpha > 0.05f && fade1.visibilityAlpha < 0.95f);

    for (int i = 0; i < 40; ++i) {
        cur = anim.update(sky, cfg, 1.0f / 60.0f);
    }
    assert(!cur.valid);
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
    assert(sCalls == 2);
}

} // namespace

int main() {
    testMetadataAndHexColorPicker();
    testStandingSign16AnglesAndWallSign();
    testNonPlaceableItemsNeverPreview();
    testSmoothBlockToBlockAnimatorAndFade();
    testSlabMergeAndDebugRendererSwapPass();
    std::cout << "All Place Preview unit & sanitizer tests passed!\n";
    return 0;
}
