#include "placepreview/StateInspector.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace placepreview {

namespace {

std::string lowerAscii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool endsWith(std::string_view s, std::string_view suffix) noexcept {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

bool startsWith(std::string_view s, std::string_view prefix) noexcept {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool findProp(const std::vector<BlockStateProperty>& props, std::string_view key, int& outVal) noexcept {
    for (const auto& p : props) {
        if (p.name == key) {
            outVal = p.value;
            return true;
        }
    }
    return false;
}

Facing facingFromSixWayValue(int val) noexcept {
    switch (val) {
        case 0: return Facing::Down;
        case 1: return Facing::Up;
        case 2: return Facing::North;
        case 3: return Facing::South;
        case 4: return Facing::West;
        case 5: return Facing::East;
        default: return Facing::North;
    }
}

Facing facingFromCardinalEnum(int val) noexcept {
    switch (val & 3) {
        case 0: return Facing::South;
        case 1: return Facing::West;
        case 2: return Facing::North;
        case 3: return Facing::East;
    }
    return Facing::South;
}

Facing facingFromWeirdoDirection(int val) noexcept {
    switch (val & 3) {
        case 0: return Facing::East;
        case 1: return Facing::West;
        case 2: return Facing::South;
        case 3: return Facing::North;
    }
    return Facing::North;
}

bool isPureFlowerStemName(const std::string& n) noexcept {
    return endsWith(n, "_tulip") ||
           endsWith(n, "_mushroom") ||
           endsWith(n, "_fungus") ||
           n == "dandelion" ||
           n == "golden_dandelion" ||
           n == "poppy" ||
           n == "red_flower" ||
           n == "yellow_flower" ||
           n == "blue_orchid" ||
           n == "allium" ||
           n == "azure_bluet" ||
           n == "oxeye_daisy" ||
           n == "cornflower" ||
           n == "lily_of_the_valley" ||
           n == "wither_rose" ||
           n == "torchflower" ||
           n == "open_eyeblossom" ||
           n == "closed_eyeblossom" ||
           n == "eyeblossom";
}

bool isFlowerOrSmallPlantName(const std::string& n) noexcept {
    return isPureFlowerStemName(n) ||
           endsWith(n, "_sapling") ||
           endsWith(n, "_flower") ||
           endsWith(n, "_roots") ||
           endsWith(n, "_sprouts") ||
           n == "mangrove_propagule" ||
           n == "pale_hanging_moss" ||
           n == "cactus_flower" ||
           n == "bush" ||
           n == "firefly_bush" ||
           n == "sweet_berry_bush" ||
           n == "short_grass" ||
           n == "tallgrass" ||
           n == "short_dry_grass" ||
           n == "tall_dry_grass" ||
           n == "fern" ||
           n == "deadbush" ||
           n == "azalea" ||
           n == "flowering_azalea" ||
           n == "reeds" ||
           n == "bamboo" ||
           n == "bamboo_sapling" ||
           n == "kelp" ||
           n == "seagrass" ||
           n == "tall_seagrass" ||
           endsWith(n, "_coral");
}

bool isCoralFanName(const std::string& n) noexcept {
    return n == "coral_fan" ||
           n == "coral_fan_dead" ||
           startsWith(n, "coral_fan_hang") ||
           endsWith(n, "_coral_fan") ||
           endsWith(n, "_coral_wall_fan");
}

} // namespace

std::string_view StateInspector::stripNamespace(std::string_view id) noexcept {
    const std::size_t colon = id.find(':');
    return (colon == std::string_view::npos) ? id : id.substr(colon + 1);
}

bool StateInspector::isDoorBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (isTrapdoorBlock(n)) return false;
    return n == "wooden_door" || n == "iron_door" || endsWith(n, "_door");
}

bool StateInspector::isBedBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "bed" || endsWith(n, "_bed");
}

bool StateInspector::isDoubleSlabBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return (startsWith(n, "double_") && endsWith(n, "_slab")) || endsWith(n, "_double_slab");
}

bool StateInspector::isSlabBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (isDoubleSlabBlock(n)) return false;
    return endsWith(n, "_slab") || endsWith(n, "_slab2") || endsWith(n, "_slab3") || endsWith(n, "_slab4");
}

bool StateInspector::isStairBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return endsWith(n, "_stairs");
}

bool StateInspector::isTrapdoorBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "trapdoor" || endsWith(n, "_trapdoor");
}

bool StateInspector::isFenceGateBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "fence_gate" || endsWith(n, "_fence_gate");
}

bool StateInspector::isStandingSignBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "standing_sign" || endsWith(n, "_standing_sign");
}

bool StateInspector::isWallSignBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "wall_sign" || endsWith(n, "_wall_sign");
}

bool StateInspector::isHangingSignBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return endsWith(n, "_hanging_sign");
}

bool StateInspector::isBannerBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "standing_banner" || n == "wall_banner" || endsWith(n, "_banner");
}

bool StateInspector::isSkullBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "skull" || endsWith(n, "_skull") || endsWith(n, "_head");
}

bool StateInspector::isCandleBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (endsWith(n, "_candle_cake") || n == "candle_cake") return false;
    return n == "candle" || endsWith(n, "_candle");
}

bool StateInspector::isSeaPickleBlock(std::string_view id) noexcept {
    return lowerAscii(stripNamespace(id)) == "sea_pickle";
}

bool StateInspector::isTurtleEggBlock(std::string_view id) noexcept {
    return lowerAscii(stripNamespace(id)) == "turtle_egg";
}

bool StateInspector::isFlowerBedBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "pink_petals" || n == "wildflowers" || n == "leaf_litter";
}

bool StateInspector::isTallPlantBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "sunflower" ||
           n == "lilac" ||
           n == "rose_bush" ||
           n == "peony" ||
           n == "pitcher_plant" ||
           n == "tall_grass" ||
           n == "large_fern" ||
           n == "small_dripleaf_block" ||
           n == "double_plant";
}

bool StateInspector::isCopperGolemStatue(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "copper_golem_statue" || endsWith(n, "_copper_golem_statue");
}

bool StateInspector::isShelfBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return endsWith(n, "_shelf") && !endsWith(n, "bookshelf");
}

bool StateInspector::isLiquidOrAirBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n.empty() ||
           n == "air" ||
           n == "cave_air" ||
           n == "void_air" ||
           n == "water" ||
           n == "flowing_water" ||
           n == "lava" ||
           n == "flowing_lava" ||
           n == "bubble_column" ||
           n == "light_block" ||
           startsWith(n, "light_block_") ||
           n == "structure_void";
}

bool StateInspector::isPillarBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return endsWith(n, "_log") ||
           endsWith(n, "_wood") ||
           endsWith(n, "_stem") ||
           endsWith(n, "_hyphae") ||
           endsWith(n, "_pillar") ||
           n == "bamboo_block" ||
           n == "stripped_bamboo_block" ||
           n == "basalt" ||
           n == "polished_basalt" ||
           n == "bone_block" ||
           n == "hay_block" ||
           n == "chain" ||
           endsWith(n, "_chain") ||
           n == "deepslate" ||
           n == "infested_deepslate" ||
           n == "creaking_heart" ||
           endsWith(n, "_froglight");
}

bool StateInspector::isSixWayBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "piston" ||
           n == "sticky_piston" ||
           n == "observer" ||
           n == "dispenser" ||
           n == "dropper" ||
           n == "hopper" ||
           n == "barrel" ||
           n == "end_rod" ||
           n == "lightning_rod" ||
           endsWith(n, "_lightning_rod") ||
           n == "amethyst_cluster" ||
           endsWith(n, "_amethyst_bud") ||
           endsWith(n, "_button") ||
           n == "lever" ||
           n == "frame" ||
           n == "glow_frame" ||
           n == "torch" ||
           n == "soul_torch" ||
           n == "redstone_torch" ||
           n == "unlit_redstone_torch" ||
           n == "copper_torch" ||
           n == "underwater_torch" ||
           n == "vine" ||
           n == "glow_lichen" ||
           n == "sculk_vein" ||
           n == "resin_clump";
}

bool StateInspector::isHorizontalDirectionalBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return isStairBlock(n) ||
           isDoorBlock(n) ||
           isBedBlock(n) ||
           isTrapdoorBlock(n) ||
           isFenceGateBlock(n) ||
           endsWith(n, "furnace") ||
           n == "smoker" ||
           n == "crafter" ||
           n == "loom" ||
           n == "stonecutter_block" ||
           n == "stonecutter" ||
           n == "lectern" ||
           n == "chest" ||
           n == "trapped_chest" ||
           n == "ender_chest" ||
           endsWith(n, "_chest") ||
           endsWith(n, "_anvil") ||
           n == "anvil" ||
           n == "grindstone" ||
           n == "bell" ||
           n == "chiseled_bookshelf" ||
           n == "carved_pumpkin" ||
           n == "lit_pumpkin" ||
           n == "jack_o_lantern" ||
           n == "bee_nest" ||
           n == "beehive" ||
           n == "campfire" ||
           n == "soul_campfire" ||
           n == "repeater" ||
           n == "unpowered_repeater" ||
           n == "powered_repeater" ||
           n == "comparator" ||
           n == "unpowered_comparator" ||
           n == "powered_comparator" ||
           n == "ladder" ||
           n == "tripwire_hook" ||
           n == "painting" ||
           isCopperGolemStatue(n) ||
           isShelfBlock(n) ||
           isFlowerBedBlock(n) ||
           n == "dried_ghast" ||
           n == "big_dripleaf" ||
           isCoralFanName(n) ||
           endsWith(n, "_glazed_terracotta") ||
           isWallSignBlock(n);
}

bool StateInspector::isWaterSurfaceBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "waterlily" || n == "lily_pad" || n == "frog_spawn" || n == "frogspawn";
}

bool StateInspector::hasNonCollidingPlacement(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return isWaterSurfaceBlock(n) ||
           isStandingSignBlock(n) ||
           isWallSignBlock(n) ||
           isHangingSignBlock(n) ||
           isSkullBlock(n) ||
           isCandleBlock(n) ||
           isSeaPickleBlock(n) ||
           isTurtleEggBlock(n) ||
           isFlowerBedBlock(n) ||
           isTallPlantBlock(n) ||
           isCoralFanName(n) ||
           isFlowerOrSmallPlantName(n) ||
           n == "standing_banner" ||
           n == "wall_banner" ||
           n == "torch" || endsWith(n, "_torch") ||
           n == "lantern" || endsWith(n, "_lantern") ||
           n == "chain" || endsWith(n, "_chain") ||
           n == "end_rod" || n == "lightning_rod" || endsWith(n, "_lightning_rod") ||
           n == "lever" || endsWith(n, "_button") ||
           n == "frame" || n == "glow_frame" || n == "painting" ||
           n == "flower_pot" ||
           n == "amethyst_cluster" || endsWith(n, "_amethyst_bud") ||
           n == "pointed_dripstone" ||
           n == "spore_blossom" || n == "hanging_roots" || n == "cave_vines" ||
           n == "carpet" || endsWith(n, "_carpet") ||
           endsWith(n, "_pressure_plate") ||
           n == "redstone_wire" || n == "tripwire" || n == "tripwire_hook" ||
           n == "rail" || endsWith(n, "_rail") ||
           n == "ladder" || n == "vine" || n == "glow_lichen" ||
           n == "sculk_vein" || n == "resin_clump" ||
           n == "wheat" || n == "carrots" || n == "potatoes" || n == "beetroot" ||
           n == "pumpkin_stem" || n == "melon_stem" || n == "nether_wart" ||
           n == "torchflower_crop" || n == "pitcher_crop";
}

bool StateInspector::isInteractiveBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "crafting_table" ||
           endsWith(n, "furnace") ||
           n == "smoker" ||
           n == "chest" ||
           n == "trapped_chest" ||
           n == "ender_chest" ||
           endsWith(n, "_chest") ||
           n == "barrel" ||
           endsWith(n, "shulker_box") ||
           n == "anvil" ||
           endsWith(n, "_anvil") ||
           n == "enchanting_table" ||
           n == "brewing_stand" ||
           n == "grindstone" ||
           n == "smithing_table" ||
           n == "cartography_table" ||
           n == "stonecutter_block" ||
           n == "loom" ||
           n == "beacon" ||
           n == "hopper" ||
           n == "dispenser" ||
           n == "dropper" ||
           n == "crafter" ||
           n == "lectern" ||
           n == "bell" ||
           n == "lever" ||
           endsWith(n, "_button") ||
           isDoorBlock(n) ||
           isTrapdoorBlock(n) ||
           isFenceGateBlock(n) ||
           isBedBlock(n) ||
           n == "noteblock" ||
           n == "daylight_detector" ||
           n == "unpowered_repeater" ||
           n == "powered_repeater" ||
           n == "unpowered_comparator" ||
           n == "powered_comparator";
}

std::string StateInspector::resolveSpecialItemBlockName(
    std::string_view itemIdentifier,
    Facing hitFace) noexcept {

    const std::string n = lowerAscii(stripNamespace(itemIdentifier));
    if (n.empty()) return {};

    // Item Frames & Glow Item Frames (`item_frame` / `frame`, `glow_item_frame` / `glow_frame`)
    if (n == "frame" || n == "item_frame") return "minecraft:frame";
    if (n == "glow_frame" || n == "glow_item_frame") return "minecraft:glow_frame";

    // Paintings mount strictly on horizontal vertical walls (North/South/West/East), never floors/ceilings
    if (n == "painting") {
        if (hitFace == Facing::Up || hitFace == Facing::Down) return {};
        return "minecraft:painting";
    }

    // Armor Stand & End Crystal
    if (n == "armor_stand") return "minecraft:armor_stand";
    if (n == "end_crystal") return "minecraft:end_crystal";

    // Water surface items
    if (n == "lily_pad" || n == "waterlily") return "minecraft:waterlily";
    if (n == "frog_spawn" || n == "frogspawn") return "minecraft:frog_spawn";

    if (n == "bed" || endsWith(n, "_bed")) return "minecraft:" + n;
    if (n == "wooden_door") return "minecraft:wooden_door";
    if (isDoorBlock(n)) return "minecraft:" + n;

    if (n == "hopper") return "minecraft:hopper";
    if (n == "cauldron") return "minecraft:cauldron";
    if (n == "brewing_stand") return "minecraft:brewing_stand";
    if (n == "flower_pot") return "minecraft:flower_pot";
    if (n == "decorated_pot") return "minecraft:decorated_pot";
    if (n == "repeater") return "minecraft:unpowered_repeater";
    if (n == "comparator") return "minecraft:unpowered_comparator";
    if (n == "campfire") return "minecraft:campfire";
    if (n == "soul_campfire") return "minecraft:soul_campfire";
    if (n == "chain" || endsWith(n, "_chain")) return "minecraft:" + n;
    if (n == "lantern" || n == "soul_lantern" || endsWith(n, "_copper_lantern") || n == "copper_lantern") {
        return "minecraft:" + n;
    }
    if (n == "cake") return "minecraft:cake";
    if (n == "redstone") return "minecraft:redstone_wire";
    if (n == "string") return "minecraft:tripwire";
    if (n == "kelp") return "minecraft:kelp";
    if (n == "seagrass") return "minecraft:seagrass";
    if (n == "sea_pickle") return "minecraft:sea_pickle";
    if (n == "turtle_egg") return "minecraft:turtle_egg";
    if (n == "sniffer_egg") return "minecraft:sniffer_egg";
    if (n == "dragon_egg") return "minecraft:dragon_egg";
    if (n == "conduit") return "minecraft:conduit";
    if (n == "heavy_core") return "minecraft:heavy_core";
    if (n == "dried_ghast") return "minecraft:dried_ghast";
    if (n == "pink_petals" || n == "wildflowers" || n == "leaf_litter") return "minecraft:" + n;
    if (n == "wheat_seeds") return "minecraft:wheat";
    if (n == "beetroot_seeds") return "minecraft:beetroot";
    if (n == "pumpkin_seeds") return "minecraft:pumpkin_stem";
    if (n == "melon_seeds") return "minecraft:melon_stem";
    if (n == "torchflower_seeds") return "minecraft:torchflower_crop";
    if (n == "pitcher_pod") return "minecraft:pitcher_crop";
    if (n == "carrot") return "minecraft:carrots";
    if (n == "potato") return "minecraft:potatoes";
    if (n == "nether_wart") return "minecraft:nether_wart";
    if (n == "sweet_berries") return "minecraft:sweet_berry_bush";
    if (n == "glow_berries") {
        return (hitFace == Facing::Down) ? "minecraft:cave_vines" : std::string{};
    }
    if (n == "sugar_cane" || n == "reeds") return "minecraft:reeds";

    // Coral fans: floor or horizontal wall, never under ceilings
    if (isCoralFanName(n)) {
        if (hitFace == Facing::Down) return {};
        return "minecraft:" + n;
    }

    // Hanging signs attach to ceiling (Down) or horizontal walls (N/S/W/E), never floor (Up).
    if (endsWith(n, "_hanging_sign")) {
        if (hitFace == Facing::Up) return {};
        return "minecraft:" + n;
    }

    // Standard signs (handles item IDs `oak_sign` / `sign` AND direct block IDs `standing_sign` / `*_standing_sign`!):
    if (n == "sign" || n == "oak_sign" || n == "standing_sign" || n == "wall_sign" ||
        n == "oak_standing_sign" || n == "oak_wall_sign") {
        if (hitFace == Facing::Down) return {};
        return (hitFace == Facing::Up) ? "minecraft:standing_sign" : "minecraft:wall_sign";
    }
    if (endsWith(n, "_standing_sign") || endsWith(n, "_wall_sign") || endsWith(n, "_sign")) {
        if (hitFace == Facing::Down) return {};
        std::string prefix = n;
        if (endsWith(prefix, "_standing_sign")) {
            prefix.resize(prefix.size() - 14);
        } else if (endsWith(prefix, "_wall_sign")) {
            prefix.resize(prefix.size() - 10);
        } else {
            prefix.resize(prefix.size() - 5);
        }
        if (prefix == "dark_oak") {
            prefix = "darkoak";
        }
        if (prefix.empty() || prefix == "oak") {
            return (hitFace == Facing::Up) ? "minecraft:standing_sign" : "minecraft:wall_sign";
        }
        return "minecraft:" + prefix + ((hitFace == Facing::Up) ? "_standing_sign" : "_wall_sign");
    }

    // Banners: standing_banner on floor, wall_banner on walls, invalid under ceiling.
    if (n == "banner" || n == "standing_banner" || n == "wall_banner" || endsWith(n, "_banner")) {
        if (hitFace == Facing::Down) return {};
        return (hitFace == Facing::Up) ? "minecraft:standing_banner" : "minecraft:wall_banner";
    }

    // Skulls & Mob Heads: placed on floor (16-angle) or horizontal wall (4-way), invalid on ceiling.
    if (n == "skull" || endsWith(n, "_skull") || endsWith(n, "_head")) {
        if (hitFace == Facing::Down) return {};
        return "minecraft:" + n;
    }

    // Flowers, tall 2-block plants, copper golem statues, shelves, candles, slabs, stairs,
    // trapdoors, fences, walls, bars, glass panes, dripleaves, ceiling flora, & non-full-cube blocks:
    if (n == "small_dripleaf") return "minecraft:small_dripleaf_block";
    if (isFlowerOrSmallPlantName(n) ||
        isTallPlantBlock(n) ||
        isCopperGolemStatue(n) ||
        isShelfBlock(n) ||
        isCandleBlock(n) ||
        isSlabBlock(n) ||
        isStairBlock(n) ||
        isTrapdoorBlock(n) ||
        isFenceGateBlock(n) ||
        isFenceOrWallOrPane(n) ||
        n == "spore_blossom" ||
        n == "big_dripleaf" ||
        n == "hanging_roots" ||
        n == "pointed_dripstone" ||
        n == "amethyst_cluster" ||
        endsWith(n, "_amethyst_bud") ||
        n == "vine" ||
        n == "glow_lichen" ||
        n == "sculk_vein" ||
        n == "resin_clump") {
        return "minecraft:" + n;
    }

    return {};
}

OrientationSummary StateInspector::decodeOrientation(
    std::string_view blockIdentifier,
    const std::vector<BlockStateProperty>& props,
    Facing hitFace,
    const Vec3& subClick,
    float playerPitchDeg,
    float playerYawDeg,
    bool isSneaking) noexcept {

    OrientationSummary out{};
    const std::string n = lowerAscii(stripNamespace(blockIdentifier));

    // 1. 16-Angle Standing Signs, Standing Banners, & Armor Stands
    if (isStandingSignBlock(n) || n == "standing_banner" || n == "armor_stand") {
        out.kind = OrientationKind::Sign16;
        int dir16 = -1;
        if (findProp(props, "ground_sign_direction", dir16)) {
            out.rotation16 = dir16 & 15;
        } else {
            out.rotation16 = yawToGroundSignDirection16(playerYawDeg);
        }
        out.yawRadians = groundSignDirectionToYawRad(out.rotation16);
        out.facing = oppositeFacing(yawToPlayerLookHorizontal(playerYawDeg));
        return out;
    }

    // 2. Hanging Signs (ceiling 16-angle/cardinal OR horizontal wall side-arm bracket!)
    if (isHangingSignBlock(n)) {
        int hangingBit = -1;
        const bool isCeiling = findProp(props, "hanging", hangingBit)
                                   ? (hangingBit != 0)
                                   : (hitFace == Facing::Down);
        out.topHalf = isCeiling;
        if (isCeiling) {
            out.kind = OrientationKind::Sign16;
            int dir16 = -1;
            int card = -1;
            int attached = 0;
            (void)findProp(props, "attached_bit", attached);
            if (attached != 0 && findProp(props, "ground_sign_direction", dir16)) {
                out.rotation16 = dir16 & 15;
                out.yawRadians = groundSignDirectionToYawRad(out.rotation16);
            } else if (!isSneaking && findProp(props, "facing_direction", card) && card >= 2 && card <= 5) {
                out.facing = facingFromSixWayValue(card);
                out.yawRadians = cardinalFacingToYawRad(out.facing);
            } else if (isSneaking) {
                out.rotation16 = yawToGroundSignDirection16(playerYawDeg);
                out.yawRadians = groundSignDirectionToYawRad(out.rotation16);
            } else {
                out.facing = oppositeFacing(yawToPlayerLookHorizontal(playerYawDeg));
                out.yawRadians = cardinalFacingToYawRad(out.facing);
            }
        } else {
            out.kind = OrientationKind::Horizontal;
            out.facing = (hitFace == Facing::Up || hitFace == Facing::Down)
                             ? oppositeFacing(yawToPlayerLookHorizontal(playerYawDeg))
                             : hitFace;
            out.yawRadians = cardinalFacingToYawRad(out.facing);
        }
        return out;
    }

    // 3. Skulls & Heads (floor 16-angle OR wall 4-way)
    if (isSkullBlock(n)) {
        int faceDir = -1;
        if (findProp(props, "facing_direction", faceDir) && faceDir >= 2 && faceDir <= 5) {
            out.kind = OrientationKind::Horizontal;
            out.facing = facingFromSixWayValue(faceDir);
            out.yawRadians = cardinalFacingToYawRad(out.facing);
            return out;
        }
        if (hitFace != Facing::Up) {
            out.kind = OrientationKind::Horizontal;
            out.facing = hitFace;
            out.yawRadians = cardinalFacingToYawRad(out.facing);
            return out;
        }
        out.kind = OrientationKind::Sign16;
        out.rotation16 = yawToGroundSignDirection16(playerYawDeg);
        out.yawRadians = groundSignDirectionToYawRad(out.rotation16);
        out.facing = oppositeFacing(yawToPlayerLookHorizontal(playerYawDeg));
        return out;
    }

    // 4. Slab vertical half
    int vHalf = 0;
    if (isSlabBlock(n)) {
        out.kind = OrientationKind::SlabHalf;
        if (findProp(props, "minecraft:vertical_half", vHalf) || findProp(props, "top_slot_bit", vHalf)) {
            out.topHalf = (vHalf != 0);
        } else {
            out.topHalf = (hitFace == Facing::Down) ||
                          (hitFace != Facing::Up && subClick.y > 0.5f);
        }
        out.facing = out.topHalf ? Facing::Down : Facing::Up;
        return out;
    }

    // 5. Pillar / Log / Chain axial orientation
    int axisVal = -1;
    if (findProp(props, "pillar_axis", axisVal) || isPillarBlock(n)) {
        out.kind = OrientationKind::PillarAxis;
        if (axisVal >= 0 && axisVal <= 2) {
            out.pillarAxis = axisVal;
        } else {
            out.pillarAxis = facingToPillarAxis(hitFace);
        }
        out.facing = (out.pillarAxis == 0) ? Facing::Up
                   : (out.pillarAxis == 1) ? Facing::East
                                           : Facing::South;
        return out;
    }

    // 6. Hanging / standing Lanterns & Pointed Dripstone
    if (n == "lantern" || n == "soul_lantern" || endsWith(n, "_copper_lantern") || n == "copper_lantern" ||
        n == "pointed_dripstone") {
        int hanging = -1;
        if (findProp(props, "hanging", hanging)) {
            out.topHalf = (hanging != 0);
        } else {
            out.topHalf = (hitFace == Facing::Down);
        }
        return out;
    }

    // 7. Stairs
    int weirdo = -1;
    if (findProp(props, "weirdo_direction", weirdo) || isStairBlock(n)) {
        out.kind = OrientationKind::Horizontal;
        out.facing = (weirdo >= 0) ? facingFromWeirdoDirection(weirdo)
                                   : yawToPlayerLookHorizontal(playerYawDeg);
        int upside = -1;
        if (findProp(props, "upside_down_bit", upside)) {
            out.topHalf = (upside != 0);
        } else {
            out.topHalf = (hitFace == Facing::Down) ||
                          (hitFace != Facing::Up && subClick.y > 0.5f);
        }
        return out;
    }

    // 8. Doors
    if (isDoorBlock(n)) {
        out.kind = OrientationKind::Horizontal;
        int card = -1;
        out.facing = findProp(props, "minecraft:cardinal_direction", card)
                         ? facingFromCardinalEnum(card)
                         : yawToPlayerLookHorizontal(playerYawDeg);
        int hinge = -1;
        if (findProp(props, "door_hinge_bit", hinge)) {
            out.hingeRight = (hinge != 0);
        } else {
            switch (out.facing) {
                case Facing::North: out.hingeRight = (subClick.x > 0.5f); break;
                case Facing::South: out.hingeRight = (subClick.x < 0.5f); break;
                case Facing::East:  out.hingeRight = (subClick.z > 0.5f); break;
                case Facing::West:  out.hingeRight = (subClick.z < 0.5f); break;
                default:            out.hingeRight = false; break;
            }
        }
        return out;
    }

    // 9. Beds & Fence Gates (open along player's horizontal look vector)
    if (isBedBlock(n) || isFenceGateBlock(n)) {
        out.kind = OrientationKind::Horizontal;
        int dirVal = -1;
        if (findProp(props, "minecraft:cardinal_direction", dirVal) || findProp(props, "direction", dirVal)) {
            out.facing = facingFromCardinalEnum(dirVal);
        } else {
            out.facing = yawToPlayerLookHorizontal(playerYawDeg);
        }
        return out;
    }

    // 10. Six-way surface-attached & directional blocks
    int sixVal = -1;
    if (findProp(props, "minecraft:facing_direction", sixVal) ||
        findProp(props, "facing_direction", sixVal) ||
        findProp(props, "minecraft:block_face", sixVal) ||
        isSixWayBlock(n)) {
        out.kind = OrientationKind::SixWay;
        if (sixVal >= 0 && sixVal <= 5) {
            out.facing = facingFromSixWayValue(sixVal);
        } else if (n == "hopper") {
            const Facing intoBlock = oppositeFacing(hitFace);
            out.facing = (intoBlock == Facing::Up) ? Facing::Down : intoBlock;
        } else if (n == "observer") {
            out.facing = pitchYawToSixWayLook(playerPitchDeg, playerYawDeg);
        } else if (n == "end_rod" || n == "lightning_rod" || endsWith(n, "_lightning_rod") ||
                   n == "amethyst_cluster" || endsWith(n, "_amethyst_bud") ||
                   endsWith(n, "_button") || n == "lever" || n == "frame" || n == "glow_frame" ||
                   endsWith(n, "torch") || n == "vine" || n == "glow_lichen" ||
                   n == "sculk_vein" || n == "resin_clump") {
            out.facing = hitFace;
        } else {
            out.facing = oppositeFacing(pitchYawToSixWayLook(playerPitchDeg, playerYawDeg));
        }
        out.pillarAxis = facingToPillarAxis(out.facing);
        return out;
    }

    // 11. Horizontal 4-way blocks
    int cardVal = -1;
    if (findProp(props, "minecraft:cardinal_direction", cardVal) ||
        findProp(props, "direction", cardVal) ||
        isHorizontalDirectionalBlock(n)) {
        out.kind = OrientationKind::Horizontal;
        if (cardVal >= 0 && n != "painting" && !isCoralFanName(n)) {
            out.facing = facingFromCardinalEnum(cardVal);
        } else if (n == "ladder" || isWallSignBlock(n) || n == "wall_banner" ||
                   n == "tripwire_hook" || n == "painting" || isCoralFanName(n) ||
                   (isShelfBlock(n) && hitFace != Facing::Up && hitFace != Facing::Down)) {
            out.facing = hitFace;
        } else if (isFlowerBedBlock(n)) {
            out.facing = yawToPlayerLookHorizontal(playerYawDeg);
        } else {
            out.facing = oppositeFacing(yawToPlayerLookHorizontal(playerYawDeg));
        }
        out.yawRadians = cardinalFacingToYawRad(out.facing);
        int upside = -1;
        if (findProp(props, "upside_down_bit", upside) || findProp(props, "minecraft:vertical_half", upside)) {
            out.topHalf = (upside != 0);
        } else if (isTrapdoorBlock(n)) {
            out.topHalf = (hitFace == Facing::Down) ||
                          (hitFace != Facing::Up && subClick.y > 0.5f);
        }
        return out;
    }

    return out;
}

std::vector<AABB> StateInspector::computeSlabMergeDiff(
    const BlockPos& cellPos,
    const std::vector<AABB>& existingWorldBoxes) {

    const Vec3 o = cellPos.originVec();
    if (existingWorldBoxes.empty()) {
        return {AABB::unitAt(cellPos)};
    }

    float minLocalY = 1.0f;
    float maxLocalY = 0.0f;
    for (const auto& box : existingWorldBoxes) {
        minLocalY = std::min(minLocalY, std::clamp(box.min.y - o.y, 0.0f, 1.0f));
        maxLocalY = std::max(maxLocalY, std::clamp(box.max.y - o.y, 0.0f, 1.0f));
    }

    if (maxLocalY <= 0.55f && minLocalY <= 0.05f) {
        return {AABB{o.x, o.y + 0.5f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
    }
    if (minLocalY >= 0.45f && maxLocalY >= 0.95f) {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.5f, o.z + 1.0f}};
    }

    return {AABB::unitAt(cellPos)};
}

std::vector<AABB> StateInspector::buildDoorWorldBoxes(
    const BlockPos& lowerPos,
    Facing playerLookHorizontal,
    bool hingeRight) {

    (void)hingeRight;
    constexpr float kLeafThickness = 0.1875f;
    const Vec3 o = lowerPos.originVec();

    switch (playerLookHorizontal) {
        case Facing::South:
            return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 2.0f, o.z + kLeafThickness}};
        case Facing::North:
            return {AABB{o.x, o.y, o.z + 1.0f - kLeafThickness, o.x + 1.0f, o.y + 2.0f, o.z + 1.0f}};
        case Facing::East:
            return {AABB{o.x, o.y, o.z, o.x + kLeafThickness, o.y + 2.0f, o.z + 1.0f}};
        case Facing::West:
        default:
            return {AABB{o.x + 1.0f - kLeafThickness, o.y, o.z, o.x + 1.0f, o.y + 2.0f, o.z + 1.0f}};
    }
}

std::vector<AABB> StateInspector::buildBedWorldBoxes(
    const BlockPos& footPos,
    Facing headDirection) {

    const BlockPos headPos = footPos.offset(
        facingStep(headDirection).x,
        facingStep(headDirection).y,
        facingStep(headDirection).z);

    const float minX = static_cast<float>(std::min(footPos.x, headPos.x));
    const float minY = static_cast<float>(footPos.y);
    const float minZ = static_cast<float>(std::min(footPos.z, headPos.z));
    const float maxX = static_cast<float>(std::max(footPos.x, headPos.x)) + 1.0f;
    const float maxZ = static_cast<float>(std::max(footPos.z, headPos.z)) + 1.0f;

    const AABB mattress{minX, minY + 0.1875f, minZ, maxX, minY + 0.5625f, maxZ};

    constexpr float kLeg = 0.1875f;
    const AABB legNW{minX,        minY, minZ,        minX + kLeg, minY + 0.1875f, minZ + kLeg};
    const AABB legNE{maxX - kLeg, minY, minZ,        maxX,        minY + 0.1875f, minZ + kLeg};
    const AABB legSW{minX,        minY, maxZ - kLeg, minX + kLeg, minY + 0.1875f, maxZ};
    const AABB legSE{maxX - kLeg, minY, maxZ - kLeg, maxX,        minY + 0.1875f, maxZ};

    const Vec3 ho = headPos.originVec();
    AABB pillow{};
    switch (headDirection) {
        case Facing::North:
            pillow = {ho.x + 0.08f, minY + 0.5625f, ho.z + 0.08f, ho.x + 0.92f, minY + 0.65f, ho.z + 0.56f};
            break;
        case Facing::South:
            pillow = {ho.x + 0.08f, minY + 0.5625f, ho.z + 0.44f, ho.x + 0.92f, minY + 0.65f, ho.z + 0.92f};
            break;
        case Facing::West:
            pillow = {ho.x + 0.08f, minY + 0.5625f, ho.z + 0.08f, ho.x + 0.56f, minY + 0.65f, ho.z + 0.92f};
            break;
        case Facing::East:
        default:
            pillow = {ho.x + 0.44f, minY + 0.5625f, ho.z + 0.08f, ho.x + 0.92f, minY + 0.65f, ho.z + 0.92f};
            break;
    }

    return {mattress, legNW, legNE, legSW, legSE, pillow};
}

std::vector<AABB> StateInspector::buildStairWorldBoxes(
    const BlockPos& pos,
    Facing stairFacing,
    bool topHalf,
    bool hasFrontPerpStair,
    Facing frontPerpFacing,
    bool hasBackPerpStair,
    Facing backPerpFacing) {

    const Vec3 o = pos.originVec();
    const AABB base = topHalf
                          ? AABB{o.x, o.y + 0.5f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}
                          : AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.5f, o.z + 1.0f};
    const float y0 = topHalf ? o.y : (o.y + 0.5f);
    const float y1 = topHalf ? (o.y + 0.5f) : (o.y + 1.0f);

    const bool stairNS = (stairFacing == Facing::North || stairFacing == Facing::South);
    const bool frontValid = hasFrontPerpStair &&
                            ((stairNS && (frontPerpFacing == Facing::West || frontPerpFacing == Facing::East)) ||
                             (!stairNS && (frontPerpFacing == Facing::North || frontPerpFacing == Facing::South)));
    const bool backValid = !frontValid && hasBackPerpStair &&
                           ((stairNS && (backPerpFacing == Facing::West || backPerpFacing == Facing::East)) ||
                            (!stairNS && (backPerpFacing == Facing::North || backPerpFacing == Facing::South)));

    auto halfXMin = [&](Facing f) -> float { return (f == Facing::East) ? (o.x + 0.5f) : o.x; };
    auto halfXMax = [&](Facing f) -> float { return (f == Facing::West) ? (o.x + 0.5f) : (o.x + 1.0f); };
    auto halfZMin = [&](Facing f) -> float { return (f == Facing::South) ? (o.z + 0.5f) : o.z; };
    auto halfZMax = [&](Facing f) -> float { return (f == Facing::North) ? (o.z + 0.5f) : (o.z + 1.0f); };

    if (frontValid) {
        const Facing xDir = stairNS ? frontPerpFacing : stairFacing;
        const Facing zDir = stairNS ? stairFacing : frontPerpFacing;
        return {base, AABB{halfXMin(xDir), y0, halfZMin(zDir), halfXMax(xDir), y1, halfZMax(zDir)}};
    }

    AABB mainStep{};
    switch (stairFacing) {
        case Facing::East:  mainStep = {o.x + 0.5f, y0, o.z, o.x + 1.0f, y1, o.z + 1.0f}; break;
        case Facing::West:  mainStep = {o.x, y0, o.z, o.x + 0.5f, y1, o.z + 1.0f}; break;
        case Facing::South: mainStep = {o.x, y0, o.z + 0.5f, o.x + 1.0f, y1, o.z + 1.0f}; break;
        case Facing::North:
        default:            mainStep = {o.x, y0, o.z, o.x + 1.0f, y1, o.z + 0.5f}; break;
    }

    if (backValid) {
        const Facing oppPrimary = oppositeFacing(stairFacing);
        const Facing xDir = stairNS ? backPerpFacing : oppPrimary;
        const Facing zDir = stairNS ? oppPrimary : backPerpFacing;
        return {base, mainStep, AABB{halfXMin(xDir), y0, halfZMin(zDir), halfXMax(xDir), y1, halfZMax(zDir)}};
    }

    return {base, mainStep};
}

bool StateInspector::isFenceOrWallOrPane(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (isFenceGateBlock(n)) return false;
    return n == "fence" || endsWith(n, "_fence") ||
           n == "cobblestone_wall" || endsWith(n, "_wall") ||
           n == "iron_bars" || n == "copper_bars" || endsWith(n, "_copper_bars") ||
           n == "glass_pane" || endsWith(n, "_glass_pane");
}

std::vector<AABB> StateInspector::buildConnectedPostWorldBoxes(
    const BlockPos& pos,
    std::string_view blockIdentifier,
    bool connectNorth,
    bool connectSouth,
    bool connectWest,
    bool connectEast) {

    const Vec3 o = pos.originVec();
    const std::string n = lowerAscii(stripNamespace(blockIdentifier));
    const bool isWall = (n == "cobblestone_wall" || endsWith(n, "_wall"));
    const bool isPane = (n == "iron_bars" || n == "copper_bars" || endsWith(n, "_copper_bars") ||
                         n == "glass_pane" || endsWith(n, "_glass_pane"));

    std::vector<AABB> boxes;
    if (isWall) {
        const bool straightNS = connectNorth && connectSouth && !connectWest && !connectEast;
        const bool straightWE = connectWest && connectEast && !connectNorth && !connectSouth;
        if (straightNS) {
            boxes.push_back(AABB{o.x + 0.3125f, o.y, o.z, o.x + 0.6875f, o.y + 0.875f, o.z + 1.0f});
        } else if (straightWE) {
            boxes.push_back(AABB{o.x, o.y, o.z + 0.3125f, o.x + 1.0f, o.y + 0.875f, o.z + 0.6875f});
        } else {
            boxes.push_back(AABB{o.x + 0.25f, o.y, o.z + 0.25f, o.x + 0.75f, o.y + 1.0f, o.z + 0.75f});
            if (connectNorth) boxes.push_back(AABB{o.x + 0.3125f, o.y, o.z, o.x + 0.6875f, o.y + 0.875f, o.z + 0.25f});
            if (connectSouth) boxes.push_back(AABB{o.x + 0.3125f, o.y, o.z + 0.75f, o.x + 0.6875f, o.y + 0.875f, o.z + 1.0f});
            if (connectWest)  boxes.push_back(AABB{o.x, o.y, o.z + 0.3125f, o.x + 0.25f, o.y + 0.875f, o.z + 0.6875f});
            if (connectEast)  boxes.push_back(AABB{o.x + 0.75f, o.y, o.z + 0.3125f, o.x + 1.0f, o.y + 0.875f, o.z + 0.6875f});
        }
    } else if (isPane) {
        boxes.push_back(AABB{o.x + 0.4375f, o.y, o.z + 0.4375f, o.x + 0.5625f, o.y + 1.0f, o.z + 0.5625f});
        if (connectNorth) boxes.push_back(AABB{o.x + 0.4375f, o.y, o.z, o.x + 0.5625f, o.y + 1.0f, o.z + 0.4375f});
        if (connectSouth) boxes.push_back(AABB{o.x + 0.4375f, o.y, o.z + 0.5625f, o.x + 0.5625f, o.y + 1.0f, o.z + 1.0f});
        if (connectWest)  boxes.push_back(AABB{o.x, o.y, o.z + 0.4375f, o.x + 0.4375f, o.y + 1.0f, o.z + 0.5625f});
        if (connectEast)  boxes.push_back(AABB{o.x + 0.5625f, o.y, o.z + 0.4375f, o.x + 1.0f, o.y + 1.0f, o.z + 0.5625f});
    } else {
        boxes.push_back(AABB{o.x + 0.375f, o.y, o.z + 0.375f, o.x + 0.625f, o.y + 1.0f, o.z + 0.625f});
        for (const auto& [y0, y1] : std::initializer_list<std::pair<float, float>>{{0.375f, 0.5625f}, {0.75f, 0.9375f}}) {
            if (connectNorth) boxes.push_back(AABB{o.x + 0.4375f, o.y + y0, o.z, o.x + 0.5625f, o.y + y1, o.z + 0.375f});
            if (connectSouth) boxes.push_back(AABB{o.x + 0.4375f, o.y + y0, o.z + 0.625f, o.x + 0.5625f, o.y + y1, o.z + 1.0f});
            if (connectWest)  boxes.push_back(AABB{o.x, o.y + y0, o.z + 0.4375f, o.x + 0.375f, o.y + y1, o.z + 0.5625f});
            if (connectEast)  boxes.push_back(AABB{o.x + 0.625f, o.y + y0, o.z + 0.4375f, o.x + 1.0f, o.y + y1, o.z + 0.5625f});
        }
    }
    return boxes;
}

std::vector<AABB> StateInspector::buildCandleClusterWorldBoxes(
    const BlockPos& pos,
    int candleCount) {

    const Vec3 o = pos.originVec();
    const int c = std::clamp(candleCount, 1, 4);
    if (c == 1) {
        return {AABB{o.x + 0.4375f, o.y, o.z + 0.4375f, o.x + 0.5625f, o.y + 0.375f, o.z + 0.5625f}};
    }
    if (c == 2) {
        return {
            AABB{o.x + 0.3125f, o.y, o.z + 0.4375f, o.x + 0.4375f, o.y + 0.375f, o.z + 0.5625f},
            AABB{o.x + 0.5625f, o.y, o.z + 0.375f,  o.x + 0.6875f, o.y + 0.3125f, o.z + 0.50f}
        };
    }
    if (c == 3) {
        return {
            AABB{o.x + 0.4375f, o.y, o.z + 0.5625f, o.x + 0.5625f, o.y + 0.375f, o.z + 0.6875f},
            AABB{o.x + 0.3125f, o.y, o.z + 0.375f,  o.x + 0.4375f, o.y + 0.3125f, o.z + 0.50f},
            AABB{o.x + 0.5625f, o.y, o.z + 0.375f,  o.x + 0.6875f, o.y + 0.25f,  o.z + 0.50f}
        };
    }
    return {
        AABB{o.x + 0.3125f, o.y, o.z + 0.3125f, o.x + 0.4375f, o.y + 0.375f,  o.z + 0.4375f},
        AABB{o.x + 0.5625f, o.y, o.z + 0.3125f, o.x + 0.6875f, o.y + 0.3125f, o.z + 0.4375f},
        AABB{o.x + 0.3125f, o.y, o.z + 0.5625f, o.x + 0.4375f, o.y + 0.25f,   o.z + 0.6875f},
        AABB{o.x + 0.5625f, o.y, o.z + 0.5625f, o.x + 0.6875f, o.y + 0.375f,  o.z + 0.6875f}
    };
}

std::vector<AABB> StateInspector::buildSeaPickleClusterWorldBoxes(
    const BlockPos& pos,
    int pickleCount) {

    const Vec3 o = pos.originVec();
    const int c = std::clamp(pickleCount, 1, 4);
    if (c == 1) {
        return {AABB{o.x + 0.375f, o.y, o.z + 0.375f, o.x + 0.625f, o.y + 0.375f, o.z + 0.625f}};
    }
    if (c == 2) {
        return {
            AABB{o.x + 0.25f,  o.y, o.z + 0.375f, o.x + 0.50f,  o.y + 0.375f,  o.z + 0.625f},
            AABB{o.x + 0.50f,  o.y, o.z + 0.25f,  o.x + 0.75f,  o.y + 0.4375f, o.z + 0.50f}
        };
    }
    if (c == 3) {
        return {
            AABB{o.x + 0.1875f, o.y, o.z + 0.25f,   o.x + 0.4375f, o.y + 0.375f,  o.z + 0.50f},
            AABB{o.x + 0.5625f, o.y, o.z + 0.25f,   o.x + 0.8125f, o.y + 0.4375f, o.z + 0.50f},
            AABB{o.x + 0.375f,  o.y, o.z + 0.5625f, o.x + 0.625f,  o.y + 0.375f,  o.z + 0.8125f}
        };
    }
    return {
        AABB{o.x + 0.1875f, o.y, o.z + 0.1875f, o.x + 0.4375f, o.y + 0.375f,  o.z + 0.4375f},
        AABB{o.x + 0.5625f, o.y, o.z + 0.1875f, o.x + 0.8125f, o.y + 0.4375f, o.z + 0.4375f},
        AABB{o.x + 0.1875f, o.y, o.z + 0.5625f, o.x + 0.4375f, o.y + 0.4375f, o.z + 0.8125f},
        AABB{o.x + 0.5625f, o.y, o.z + 0.5625f, o.x + 0.8125f, o.y + 0.375f,  o.z + 0.8125f}
    };
}

std::vector<AABB> StateInspector::buildTurtleEggClusterWorldBoxes(
    const BlockPos& pos,
    int eggCount) {

    const Vec3 o = pos.originVec();
    const int c = std::clamp(eggCount, 1, 4);
    if (c == 1) {
        return {AABB{o.x + 0.3125f, o.y, o.z + 0.3125f, o.x + 0.6875f, o.y + 0.4375f, o.z + 0.6875f}};
    }
    if (c == 2) {
        return {
            AABB{o.x + 0.1875f, o.y, o.z + 0.25f,   o.x + 0.50f,   o.y + 0.4375f, o.z + 0.5625f},
            AABB{o.x + 0.5625f, o.y, o.z + 0.4375f, o.x + 0.8125f, o.y + 0.375f,  o.z + 0.6875f}
        };
    }
    if (c == 3) {
        return {
            AABB{o.x + 0.1875f, o.y, o.z + 0.1875f, o.x + 0.50f,   o.y + 0.4375f, o.z + 0.50f},
            AABB{o.x + 0.5625f, o.y, o.z + 0.25f,   o.x + 0.8125f, o.y + 0.375f,  o.z + 0.50f},
            AABB{o.x + 0.375f,  o.y, o.z + 0.5625f, o.x + 0.6875f, o.y + 0.4375f, o.z + 0.875f}
        };
    }
    return {
        AABB{o.x + 0.1875f, o.y, o.z + 0.1875f, o.x + 0.50f,   o.y + 0.4375f, o.z + 0.50f},
        AABB{o.x + 0.5625f, o.y, o.z + 0.1875f, o.x + 0.8125f, o.y + 0.375f,  o.z + 0.4375f},
        AABB{o.x + 0.1875f, o.y, o.z + 0.5625f, o.x + 0.4375f, o.y + 0.375f,  o.z + 0.8125f},
        AABB{o.x + 0.50f,   o.y, o.z + 0.50f,   o.x + 0.8125f, o.y + 0.4375f, o.z + 0.8125f}
    };
}

std::vector<AABB> StateInspector::buildFlowerBedClusterWorldBoxes(
    const BlockPos& pos,
    int petalCount,
    Facing cardinalFacing,
    const Vec3& randomOffset) {

    const Vec3 o = pos.originVec() + randomOffset;
    const int c = std::clamp(petalCount, 1, 4);
    constexpr float kBedH = 0.1875f; // 3/16 high in vanilla FlowerBedSelectionBox

    float x0 = 0.0f, x1 = 1.0f;
    float z0 = 0.0f, z1 = 1.0f;

    if (c == 1) {
        switch (cardinalFacing) {
            case Facing::North: x0 = 0.0f; x1 = 0.5f; z0 = 0.5f; z1 = 1.0f; break;
            case Facing::East:  x0 = 0.0f; x1 = 0.5f; z0 = 0.0f; z1 = 0.5f; break;
            case Facing::South: x0 = 0.5f; x1 = 1.0f; z0 = 0.0f; z1 = 0.5f; break;
            case Facing::West:
            default:            x0 = 0.5f; x1 = 1.0f; z0 = 0.5f; z1 = 1.0f; break;
        }
    } else if (c == 2) {
        switch (cardinalFacing) {
            case Facing::North: x0 = 0.0f; x1 = 0.5f; z0 = 0.0f; z1 = 1.0f; break;
            case Facing::East:  x0 = 0.0f; x1 = 1.0f; z0 = 0.0f; z1 = 0.5f; break;
            case Facing::South: x0 = 0.5f; x1 = 1.0f; z0 = 0.0f; z1 = 1.0f; break;
            case Facing::West:
            default:            x0 = 0.0f; x1 = 1.0f; z0 = 0.5f; z1 = 1.0f; break;
        }
    }

    return {AABB{o.x + x0, o.y, o.z + z0, o.x + x1, o.y + kBedH, o.z + z1}};
}

std::vector<OrientedBox> StateInspector::buildSpecialOrientedBoxes(
    const BlockPos& pos,
    std::string_view blockIdentifier,
    const OrientationSummary& orient) {

    const Vec3 o = pos.originVec();
    const std::string n = lowerAscii(stripNamespace(blockIdentifier));

    // 0. Copper Golem Statue (all 8 oxidation + waxed variants):
    //    True 3D directional Copper Golem silhouette facing the player (-W direction)!
    if (isCopperGolemStatue(n)) {
        const float yaw = cardinalFacingToYawRad(orient.facing);
        return {
            // Left & right copper legs (0..4 px)
            OrientedBox::fromCellYaw(o, {-2.0f / 16.0f, 2.0f / 16.0f, 0.0f}, {1.6f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f}, yaw),
            OrientedBox::fromCellYaw(o, { 2.0f / 16.0f, 2.0f / 16.0f, 0.0f}, {1.6f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f}, yaw),
            // Copper torso (4..9 px)
            OrientedBox::fromCellYaw(o, {0.0f, 6.5f / 16.0f, 0.0f}, {4.0f / 16.0f, 2.5f / 16.0f, 2.5f / 16.0f}, yaw),
            // Left & right arms (3..8.5 px)
            OrientedBox::fromCellYaw(o, {-5.0f / 16.0f, 5.75f / 16.0f, 0.0f}, {1.0f / 16.0f, 2.75f / 16.0f, 1.5f / 16.0f}, yaw),
            OrientedBox::fromCellYaw(o, { 5.0f / 16.0f, 5.75f / 16.0f, 0.0f}, {1.0f / 16.0f, 2.75f / 16.0f, 1.5f / 16.0f}, yaw),
            // Head (9..14 px) + iconic front nose protruding toward player (-Z)!
            OrientedBox::fromCellYaw(o, {0.0f, 11.5f / 16.0f, 0.0f}, {4.0f / 16.0f, 2.5f / 16.0f, 3.5f / 16.0f}, yaw),
            OrientedBox::fromCellYaw(o, {0.0f, 10.25f / 16.0f, -4.4f / 16.0f}, {1.0f / 16.0f, 1.25f / 16.0f, 0.9f / 16.0f}, yaw),
            // Top lightning-rod antenna (14..16 px)
            OrientedBox::fromCellYaw(o, {0.0f, 15.0f / 16.0f, 0.0f}, {1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f}, yaw)
        };
    }

    // 1. Exact 16-Angle Standing Sign: thin wooden post [0..14/24] + full 1.0 wide board [14/24..26/24]
    if (isStandingSignBlock(n)) {
        return {
            OrientedBox::fromCellYaw(o, {0.0f, 0.2916667f, 0.0f}, {0.0416667f, 0.2916667f, 0.0416667f}, orient.yawRadians),
            OrientedBox::fromCellYaw(o, {0.0f, 0.8333333f, 0.0f}, {0.50f,      0.25f,      0.0416667f}, orient.yawRadians)
        };
    }

    // 2. Hanging Signs (Ceiling 16-angle/4-way chains + board OR Horizontal Wall Bracket + chains + board)
    if (isHangingSignBlock(n)) {
        if (orient.topHalf) {
            return {
                OrientedBox::fromCellYaw(o, { 0.0f,  0.3125f, 0.0f}, {0.4375f, 0.3125f, 0.0625f}, orient.yawRadians),
                OrientedBox::fromCellYaw(o, {-0.25f, 0.8125f, 0.0f}, {0.045f,  0.1875f, 0.045f},  orient.yawRadians),
                OrientedBox::fromCellYaw(o, { 0.25f, 0.8125f, 0.0f}, {0.045f,  0.1875f, 0.045f},  orient.yawRadians)
            };
        }
        const float wallBoardYaw = cardinalFacingToYawRad(orient.facing) + kPi * 0.5f;
        return {
            OrientedBox::fromCellYaw(o, { 0.0f,  0.9375f, 0.0f}, {0.50f,   0.0625f, 0.125f}, wallBoardYaw),
            OrientedBox::fromCellYaw(o, { 0.0f,  0.3125f, 0.0f}, {0.4375f, 0.3125f, 0.0625f}, wallBoardYaw),
            OrientedBox::fromCellYaw(o, {-0.25f, 0.75f,   0.0f}, {0.045f,  0.125f,  0.045f},  wallBoardYaw),
            OrientedBox::fromCellYaw(o, { 0.25f, 0.75f,   0.0f}, {0.045f,  0.125f,  0.045f},  wallBoardYaw)
        };
    }

    // 3. Standing Banner: 16-angle pole + crossbar + hanging cloth drape
    if (n == "standing_banner") {
        return {
            OrientedBox::fromCellYaw(o, {0.0f, 0.875f,   0.0f},  {0.045f,  0.875f,   0.045f}, orient.yawRadians),
            OrientedBox::fromCellYaw(o, {0.0f, 1.75f,    0.0f},  {0.4375f, 0.055f,   0.055f}, orient.yawRadians),
            OrientedBox::fromCellYaw(o, {0.0f, 0.90625f, -0.09f},{0.4375f, 0.78125f, 0.035f}, orient.yawRadians)
        };
    }

    // 4. Armor Stand: stone baseplate + dual legs/posts + waist + full shoulder crossbeam facing player!
    if (n == "armor_stand") {
        return {
            OrientedBox::fromCellYaw(o, { 0.0f,  0.03125f, 0.0f}, {0.375f,  0.03125f, 0.375f},  orient.yawRadians),
            OrientedBox::fromCellYaw(o, {-0.14f, 0.6875f,  0.0f}, {0.055f,  0.625f,   0.055f},  orient.yawRadians),
            OrientedBox::fromCellYaw(o, { 0.14f, 0.6875f,  0.0f}, {0.055f,  0.625f,   0.055f},  orient.yawRadians),
            OrientedBox::fromCellYaw(o, { 0.0f,  1.375f,   0.0f}, {0.375f,  0.0625f,  0.0625f}, orient.yawRadians),
            OrientedBox::fromCellYaw(o, { 0.0f,  1.6875f,  0.0f}, {0.0625f, 0.25f,    0.0625f}, orient.yawRadians)
        };
    }

    // 5. Floor 16-Angle Skull / Mob Head
    if (isSkullBlock(n) && orient.kind == OrientationKind::Sign16) {
        return {
            OrientedBox::fromCellYaw(o, {0.0f, 0.25f, 0.0f}, {0.25f, 0.25f, 0.25f}, orient.yawRadians)
        };
    }

    // 6. Wall-Mounted Angled Torches: tilted 22.5 deg away from wall face
    if ((n == "torch" || n == "soul_torch" || n == "redstone_torch" ||
         n == "unlit_redstone_torch" || n == "copper_torch" || n == "underwater_torch") &&
        orient.facing != Facing::Up && orient.facing != Facing::Down) {

        constexpr float kTilt = 22.5f * kDegToRad;
        const float c = std::cos(kTilt);
        const float s = std::sin(kTilt);
        const Vec3 outDir = facingNormal(orient.facing);
        const Vec3 axisV = (Vec3{0.0f, c, 0.0f} + outDir * s).normalizedOr({0.0f, 1.0f, 0.0f});
        const Vec3 refSide = (std::fabs(outDir.x) > 0.5f) ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{1.0f, 0.0f, 0.0f};
        const Vec3 axisW = refSide.cross(axisV).normalizedOr({0.0f, 0.0f, 1.0f});
        const Vec3 axisU = axisV.cross(axisW).normalizedOr(refSide);

        OrientedBox torchBox{};
        torchBox.center = o + Vec3{0.5f, 0.50f, 0.5f} - outDir * 0.34f;
        torchBox.halfExtents = {0.0625f, 0.3125f, 0.0625f};
        torchBox.axisU = axisU;
        torchBox.axisV = axisV;
        torchBox.axisW = axisW;
        torchBox.axisAligned = false;
        return {torchBox};
    }

    // 7. Lectern: base + pedestal + slanted 22.5 deg reading top facing player
    if (n == "lectern") {
        const float yaw = cardinalFacingToYawRad(orient.facing);
        constexpr float kSlope = 22.5f * kDegToRad;
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        const float cs = std::cos(kSlope);
        const float ss = std::sin(kSlope);
        const Vec3 u{cy, 0.0f, sy};
        const Vec3 front{sy, 0.0f, -cy};
        const Vec3 v = (Vec3{0.0f, cs, 0.0f} + front * ss).normalizedOr({0.0f, 1.0f, 0.0f});
        const Vec3 w = u.cross(v).normalizedOr({-sy, 0.0f, cy});

        OrientedBox slantTop{};
        slantTop.center = o + Vec3{0.5f, 0.89f, 0.5f};
        slantTop.halfExtents = {0.50f, 0.0625f, 0.42f};
        slantTop.axisU = u;
        slantTop.axisV = v;
        slantTop.axisW = w;
        slantTop.yawRad = yaw;
        slantTop.axisAligned = false;

        return {
            OrientedBox::fromAABB(AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.125f, o.z + 1.0f}),
            OrientedBox::fromAABB(AABB{o.x + 0.25f, o.y + 0.125f, o.z + 0.25f, o.x + 0.75f, o.y + 0.84f, o.z + 0.75f}),
            slantTop
        };
    }

    return {};
}

bool StateInspector::isNonSolidSupportBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (isLiquidOrAirBlock(n) || n == "fire" || n == "soul_fire") {
        return true;
    }
    return isFlowerOrSmallPlantName(n) ||
           isTallPlantBlock(n) ||
           isCoralFanName(n) ||
           isSeaPickleBlock(n) ||
           isTurtleEggBlock(n) ||
           isFlowerBedBlock(n) ||
           n == "vine" || n == "glow_lichen" ||
           n == "sculk_vein" || n == "resin_clump" ||
           n == "snow_layer" || n == "carpet" || endsWith(n, "_carpet") ||
           n == "torch" || endsWith(n, "_torch") || n == "ladder" ||
           endsWith(n, "_button") || n == "lever" || n == "redstone_wire" ||
           n == "tripwire" || n == "wheat" || n == "carrots" ||
           n == "potatoes" || n == "beetroot" || n == "nether_wart" ||
           n == "frame" || n == "glow_frame" || n == "painting" ||
           isWaterSurfaceBlock(n) || isStandingSignBlock(n) || isWallSignBlock(n) ||
           isHangingSignBlock(n) || isBannerBlock(n) || isCandleBlock(n);
}

bool StateInspector::validateBlockSupport(
    std::string_view blockIdentifier,
    const BlockPos& /*placePos*/,
    Facing hitFace,
    std::string_view belowBlockId,
    std::string_view aboveBlockId,
    std::string_view behindFaceBlockId) noexcept {

    const std::string n = lowerAscii(stripNamespace(blockIdentifier));
    const std::string below = lowerAscii(stripNamespace(belowBlockId));
    const std::string above = lowerAscii(stripNamespace(aboveBlockId));
    const std::string behind = lowerAscii(stripNamespace(behindFaceBlockId));

    const bool belowSolid = !isNonSolidSupportBlock(below);
    const bool aboveSolid = !isNonSolidSupportBlock(above);
    const bool behindSolid = !isNonSolidSupportBlock(behind);

    if (n == "painting") {
        if (hitFace == Facing::Up || hitFace == Facing::Down) return false;
        return behindSolid;
    }

    if (n == "frame" || n == "glow_frame") {
        return behindSolid || isFenceOrWallOrPane(behind);
    }

    if (n == "end_crystal") {
        return below == "obsidian" || below == "bedrock";
    }

    if (n == "armor_stand") {
        return belowSolid || isSlabBlock(below);
    }

    if (n == "torch" || n == "soul_torch" || n == "redstone_torch" ||
        n == "unlit_redstone_torch" || n == "copper_torch" || n == "underwater_torch") {
        if (hitFace == Facing::Down) return false;
        if (hitFace == Facing::Up) return belowSolid || isFenceOrWallOrPane(below);
        return behindSolid;
    }

    if (n == "lantern" || n == "soul_lantern" || endsWith(n, "_copper_lantern") || n == "copper_lantern") {
        if (hitFace == Facing::Up) return belowSolid || isFenceOrWallOrPane(below);
        if (hitFace == Facing::Down) return aboveSolid || above == "chain" || endsWith(above, "_chain") || isFenceOrWallOrPane(above);
        return belowSolid || aboveSolid || above == "chain" || endsWith(above, "_chain");
    }

    if (isHangingSignBlock(n)) {
        if (hitFace == Facing::Up) return false;
        if (hitFace == Facing::Down) {
            return aboveSolid || isFenceOrWallOrPane(above) || above == "chain" ||
                   endsWith(above, "_chain") || isHangingSignBlock(above);
        }
        return true;
    }

    if (n == "ladder") {
        if (hitFace == Facing::Up || hitFace == Facing::Down) return false;
        return behindSolid;
    }

    if (isCoralFanName(n)) {
        if (hitFace == Facing::Down) return false;
        return (hitFace == Facing::Up) ? belowSolid : behindSolid;
    }

    if (endsWith(n, "_button") || n == "lever" ||
        n == "vine" || n == "glow_lichen" || n == "sculk_vein" || n == "resin_clump") {
        return behindSolid;
    }

    if (n == "spore_blossom" || n == "hanging_roots" || n == "cave_vines") {
        return aboveSolid;
    }
    if (n == "pale_hanging_moss") {
        return aboveSolid || above == "pale_hanging_moss";
    }

    if (n == "tripwire_hook" || isWallSignBlock(n) || n == "wall_banner") {
        if (hitFace == Facing::Up || hitFace == Facing::Down) return false;
        return behindSolid;
    }

    if (n == "carpet" || endsWith(n, "_carpet") ||
        endsWith(n, "_pressure_plate") ||
        n == "rail" || endsWith(n, "_rail") ||
        n == "repeater" || n == "unpowered_repeater" || n == "powered_repeater" ||
        n == "comparator" || n == "unpowered_comparator" || n == "powered_comparator" ||
        isStandingSignBlock(n) || n == "standing_banner" || isCandleBlock(n) ||
        isSeaPickleBlock(n) || isTurtleEggBlock(n) || isFlowerBedBlock(n) ||
        n == "flower_pot" || n == "cake" || n == "redstone_wire" || n == "snow_layer") {
        return belowSolid || (isCandleBlock(n) && isFenceOrWallOrPane(below));
    }

    if (n == "kelp") {
        return belowSolid || below == "kelp";
    }
    if (n == "seagrass" || endsWith(n, "_coral")) {
        return belowSolid;
    }

    if (n == "wheat" || n == "carrots" || n == "potatoes" || n == "beetroot" ||
        n == "pumpkin_stem" || n == "melon_stem" || n == "torchflower_crop" || n == "pitcher_crop") {
        return below == "farmland";
    }
    if (n == "nether_wart") {
        return below == "soul_sand";
    }
    if (n == "cactus") {
        return below == "sand" || below == "red_sand" || below == "cactus";
    }
    if (endsWith(n, "_sapling") || n == "sweet_berry_bush") {
        return below == "dirt" || below == "grass_block" || below == "grass" ||
               below == "podzol" || below == "mycelium" || below == "coarse_dirt" ||
               below == "rooted_dirt" || below == "mud" || below == "moss_block" ||
               below == "pale_moss_block" || below == "farmland";
    }

    return true;
}

std::vector<AABB> StateInspector::buildFallbackBlockBoxes(
    const BlockPos& pos,
    std::string_view blockIdentifier,
    const OrientationSummary& orient) {

    const Vec3 o = pos.originVec();
    const std::string n = lowerAscii(stripNamespace(blockIdentifier));

    if (isSlabBlock(n)) {
        if (orient.topHalf) {
            return {AABB{o.x, o.y + 0.5f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
        }
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.5f, o.z + 1.0f}};
    }

    if (isStairBlock(n)) {
        return buildStairWorldBoxes(pos, orient.facing, orient.topHalf, false, Facing::North, false, Facing::North);
    }

    if (isFenceOrWallOrPane(n)) {
        return buildConnectedPostWorldBoxes(pos, n, false, false, false, false);
    }

    if (isFenceGateBlock(n)) {
        const bool gateAlongX = (orient.facing == Facing::North || orient.facing == Facing::South);
        if (gateAlongX) {
            return {
                AABB{o.x,          o.y + 0.3125f, o.z + 0.4375f, o.x + 0.125f, o.y + 1.0f,    o.z + 0.5625f},
                AABB{o.x + 0.875f, o.y + 0.3125f, o.z + 0.4375f, o.x + 1.0f,   o.y + 1.0f,    o.z + 0.5625f},
                AABB{o.x + 0.375f, o.y + 0.375f,  o.z + 0.4375f, o.x + 0.625f, o.y + 0.9375f, o.z + 0.5625f},
                AABB{o.x + 0.125f, o.y + 0.375f,  o.z + 0.4375f, o.x + 0.875f, o.y + 0.5625f, o.z + 0.5625f},
                AABB{o.x + 0.125f, o.y + 0.75f,   o.z + 0.4375f, o.x + 0.875f, o.y + 0.9375f, o.z + 0.5625f}
            };
        }
        return {
            AABB{o.x + 0.4375f, o.y + 0.3125f, o.z,          o.x + 0.5625f, o.y + 1.0f,    o.z + 0.125f},
            AABB{o.x + 0.4375f, o.y + 0.3125f, o.z + 0.875f, o.x + 0.5625f, o.y + 1.0f,    o.z + 1.0f},
            AABB{o.x + 0.4375f, o.y + 0.375f,  o.z + 0.375f, o.x + 0.5625f, o.y + 0.9375f, o.z + 0.625f},
            AABB{o.x + 0.4375f, o.y + 0.375f,  o.z + 0.125f, o.x + 0.5625f, o.y + 0.5625f, o.z + 0.875f},
            AABB{o.x + 0.4375f, o.y + 0.75f,   o.z + 0.125f, o.x + 0.5625f, o.y + 0.9375f, o.z + 0.875f}
        };
    }

    if (isTrapdoorBlock(n)) {
        if (orient.topHalf) {
            return {AABB{o.x, o.y + 0.8125f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
        }
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.1875f, o.z + 1.0f}};
    }

    if (isWaterSurfaceBlock(n)) {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 0.09375f, o.z + 0.9375f}};
    }

    if (isCandleBlock(n)) {
        return buildCandleClusterWorldBoxes(pos, 1);
    }

    if (isSeaPickleBlock(n)) {
        return buildSeaPickleClusterWorldBoxes(pos, 1);
    }

    if (isTurtleEggBlock(n)) {
        return buildTurtleEggClusterWorldBoxes(pos, 1);
    }

    if (isFlowerBedBlock(n)) {
        return buildFlowerBedClusterWorldBoxes(pos, 1, orient.facing);
    }

    if (isCopperGolemStatue(n)) {
        // Exact bounds disassembled in CopperGolemStatueBlock (0x100df6e4): {3/16, 0, 3/16}..{13/16, 14/16, 13/16}
        return {AABB{o.x + 0.1875f, o.y, o.z + 0.1875f, o.x + 0.8125f, o.y + 0.875f, o.z + 0.8125f}};
    }

    if (isShelfBlock(n)) {
        // Exact 3/16 thin horizontal wall shelf disassembled in ShelfBlock::getOutline (0x10790db4)
        constexpr float d = 0.1875f;
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x, o.y, o.z + 1.0f - d, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x, o.y, o.z,            o.x + 1.0f, o.y + 1.0f, o.z + d}};
            case Facing::West:  return {AABB{o.x + 1.0f - d, o.y, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::East:
            default:            return {AABB{o.x,            o.y, o.z, o.x + d,    o.y + 1.0f, o.z + 1.0f}};
        }
    }

    if (isTallPlantBlock(n)) {
        return {AABB{o.x + 0.1875f, o.y, o.z + 0.1875f, o.x + 0.8125f, o.y + 2.0f, o.z + 0.8125f}};
    }

    if (n == "big_dripleaf") {
        return {AABB{o.x, o.y + 0.6875f, o.z, o.x + 1.0f, o.y + 0.9375f, o.z + 1.0f}};
    }

    if (n == "chorus_flower" || n == "chorus_plant") {
        return {AABB{o.x + 0.125f, o.y + 0.125f, o.z + 0.125f, o.x + 0.875f, o.y + 0.875f, o.z + 0.875f}};
    }

    if (n == "conduit") {
        return {AABB{o.x + 0.3125f, o.y + 0.3125f, o.z + 0.3125f, o.x + 0.6875f, o.y + 0.6875f, o.z + 0.6875f}};
    }

    if (n == "heavy_core") {
        return {AABB{o.x + 0.25f, o.y, o.z + 0.25f, o.x + 0.75f, o.y + 0.5f, o.z + 0.75f}};
    }

    if (n == "dried_ghast") {
        return {AABB{o.x + 0.1875f, o.y, o.z + 0.1875f, o.x + 0.8125f, o.y + 0.625f, o.z + 0.8125f}};
    }

    if (n == "sniffer_egg") {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.125f, o.x + 0.9375f, o.y + 1.0f, o.z + 0.875f}};
    }

    if (n == "dragon_egg") {
        return {
            AABB{o.x + 0.1875f, o.y,          o.z + 0.1875f, o.x + 0.8125f, o.y + 0.1875f, o.z + 0.8125f},
            AABB{o.x + 0.0625f, o.y + 0.1875f, o.z + 0.0625f, o.x + 0.9375f, o.y + 0.6875f, o.z + 0.9375f},
            AABB{o.x + 0.1875f, o.y + 0.6875f, o.z + 0.1875f, o.x + 0.8125f, o.y + 0.9375f, o.z + 0.8125f},
            AABB{o.x + 0.3125f, o.y + 0.9375f, o.z + 0.3125f, o.x + 0.6875f, o.y + 1.0f,    o.z + 0.6875f}
        };
    }

    if (n == "end_crystal") {
        return {
            AABB{o.x + 0.0625f, o.y,          o.z + 0.0625f, o.x + 0.9375f, o.y + 0.1875f, o.z + 0.9375f},
            AABB{o.x + 0.1875f, o.y + 0.3125f, o.z + 0.1875f, o.x + 0.8125f, o.y + 0.9375f, o.z + 0.8125f}
        };
    }

    if (n == "armor_stand") {
        return {
            AABB{o.x + 0.125f, o.y,          o.z + 0.125f, o.x + 0.875f, o.y + 0.0625f, o.z + 0.875f},
            AABB{o.x + 0.25f,  o.y + 0.0625f, o.z + 0.25f, o.x + 0.75f,  o.y + 1.9375f, o.z + 0.75f}
        };
    }

    if (n == "sculk_sensor" || n == "calibrated_sculk_sensor" || n == "sculk_shrieker") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.5f, o.z + 1.0f}};
    }

    if (endsWith(n, "_carpet") || n == "carpet" || n == "moss_carpet" || endsWith(n, "_pressure_plate")) {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.0625f, o.z + 1.0f}};
    }

    if (n == "rail" || endsWith(n, "_rail") || n == "redstone_wire" || n == "tripwire") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.0625f, o.z + 1.0f}};
    }

    if (n == "snow_layer") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.125f, o.z + 1.0f}};
    }

    if (n == "repeater" || n == "unpowered_repeater" || n == "powered_repeater" ||
        n == "comparator" || n == "unpowered_comparator" || n == "powered_comparator") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.125f, o.z + 1.0f}};
    }

    if (n == "chest" || n == "trapped_chest" || n == "ender_chest" || endsWith(n, "_chest")) {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 0.875f, o.z + 0.9375f}};
    }

    if (n == "flower_pot") {
        return {AABB{o.x + 0.3125f, o.y, o.z + 0.3125f, o.x + 0.6875f, o.y + 0.375f, o.z + 0.6875f}};
    }

    if (n == "decorated_pot") {
        return {
            AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 1.0f, o.z + 0.9375f},
            AABB{o.x + 0.25f, o.y + 1.0f, o.z + 0.25f, o.x + 0.75f, o.y + 1.1875f, o.z + 0.75f}
        };
    }

    if (n == "cake") {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 0.5f, o.z + 0.9375f}};
    }

    if (n == "cauldron") {
        return {
            AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.25f, o.z + 1.0f},
            AABB{o.x, o.y + 0.25f, o.z, o.x + 0.125f, o.y + 1.0f, o.z + 1.0f},
            AABB{o.x + 0.875f, o.y + 0.25f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f},
            AABB{o.x + 0.125f, o.y + 0.25f, o.z, o.x + 0.875f, o.y + 1.0f, o.z + 0.125f},
            AABB{o.x + 0.125f, o.y + 0.25f, o.z + 0.875f, o.x + 0.875f, o.y + 1.0f, o.z + 1.0f}
        };
    }

    if (n == "brewing_stand") {
        return {
            AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 0.125f, o.z + 0.9375f},
            AABB{o.x + 0.4375f, o.y + 0.125f, o.z + 0.4375f, o.x + 0.5625f, o.y + 0.875f, o.z + 0.5625f}
        };
    }

    if (n == "enchanting_table") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.75f, o.z + 1.0f}};
    }

    if (n == "end_portal_frame") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.8125f, o.z + 1.0f}};
    }

    if (n == "daylight_detector" || n == "daylight_detector_inverted") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.375f, o.z + 1.0f}};
    }

    if (n == "campfire" || n == "soul_campfire") {
        return {
            AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.0625f, o.z + 1.0f},
            AABB{o.x, o.y + 0.0625f, o.z + 0.0625f, o.x + 1.0f, o.y + 0.3125f, o.z + 0.3125f},
            AABB{o.x, o.y + 0.0625f, o.z + 0.6875f, o.x + 1.0f, o.y + 0.3125f, o.z + 0.9375f},
            AABB{o.x + 0.0625f, o.y + 0.1875f, o.z, o.x + 0.3125f, o.y + 0.4375f, o.z + 1.0f},
            AABB{o.x + 0.6875f, o.y + 0.1875f, o.z, o.x + 0.9375f, o.y + 0.4375f, o.z + 1.0f}
        };
    }

    if (n == "stonecutter_block" || n == "stonecutter") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.5625f, o.z + 1.0f}};
    }

    if (n == "grindstone") {
        return {AABB{o.x + 0.125f, o.y + 0.125f, o.z + 0.125f, o.x + 0.875f, o.y + 0.875f, o.z + 0.875f}};
    }

    if (n == "bell") {
        return {AABB{o.x + 0.25f, o.y + 0.1875f, o.z + 0.25f, o.x + 0.75f, o.y + 0.875f, o.z + 0.75f}};
    }

    if (n == "pointed_dripstone" || n == "bamboo") {
        return {AABB{o.x + 0.3125f, o.y, o.z + 0.3125f, o.x + 0.6875f, o.y + 1.0f, o.z + 0.6875f}};
    }

    if (n == "cactus") {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 1.0f, o.z + 0.9375f}};
    }

    if (n == "amethyst_cluster" || endsWith(n, "_amethyst_bud")) {
        const float halfW = (n == "small_amethyst_bud") ? 0.25f : 0.3125f;
        const float h = (n == "small_amethyst_bud") ? 0.25f
                      : (n == "medium_amethyst_bud") ? 0.3125f
                      : (n == "large_amethyst_bud") ? 0.375f : 0.4375f;
        switch (orient.facing) {
            case Facing::Down:  return {AABB{o.x + 0.5f - halfW, o.y + 1.0f - h, o.z + 0.5f - halfW, o.x + 0.5f + halfW, o.y + 1.0f, o.z + 0.5f + halfW}};
            case Facing::North: return {AABB{o.x + 0.5f - halfW, o.y + 0.5f - halfW, o.z + 1.0f - h, o.x + 0.5f + halfW, o.y + 0.5f + halfW, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.5f - halfW, o.y + 0.5f - halfW, o.z, o.x + 0.5f + halfW, o.y + 0.5f + halfW, o.z + h}};
            case Facing::West:  return {AABB{o.x + 1.0f - h, o.y + 0.5f - halfW, o.z + 0.5f - halfW, o.x + 1.0f, o.y + 0.5f + halfW, o.z + 0.5f + halfW}};
            case Facing::East:  return {AABB{o.x, o.y + 0.5f - halfW, o.z + 0.5f - halfW, o.x + h, o.y + 0.5f + halfW, o.z + 0.5f + halfW}};
            case Facing::Up:
            default:            return {AABB{o.x + 0.5f - halfW, o.y, o.z + 0.5f - halfW, o.x + 0.5f + halfW, o.y + h, o.z + 0.5f + halfW}};
        }
    }

    if (n == "frame" || n == "glow_frame") {
        switch (orient.facing) {
            case Facing::Down:  return {AABB{o.x + 0.125f, o.y + 0.9375f, o.z + 0.125f, o.x + 0.875f, o.y + 1.0f,    o.z + 0.875f}};
            case Facing::Up:    return {AABB{o.x + 0.125f, o.y,           o.z + 0.125f, o.x + 0.875f, o.y + 0.0625f, o.z + 0.875f}};
            case Facing::North: return {AABB{o.x + 0.125f, o.y + 0.125f,  o.z + 0.9375f, o.x + 0.875f, o.y + 0.875f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.125f, o.y + 0.125f,  o.z,           o.x + 0.875f, o.y + 0.875f, o.z + 0.0625f}};
            case Facing::West:  return {AABB{o.x + 0.9375f, o.y + 0.125f, o.z + 0.125f,  o.x + 1.0f,   o.y + 0.875f, o.z + 0.875f}};
            case Facing::East:
            default:            return {AABB{o.x,           o.y + 0.125f, o.z + 0.125f,  o.x + 0.0625f, o.y + 0.875f, o.z + 0.875f}};
        }
    }

    if (n == "painting") {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x, o.y, o.z + 0.9375f, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x, o.y, o.z,           o.x + 1.0f, o.y + 1.0f, o.z + 0.0625f}};
            case Facing::West:  return {AABB{o.x + 0.9375f, o.y, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::East:
            default:            return {AABB{o.x,           o.y, o.z, o.x + 0.0625f, o.y + 1.0f, o.z + 1.0f}};
        }
    }

    if (isCoralFanName(n)) {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x + 0.125f, o.y + 0.25f, o.z + 0.5f, o.x + 0.875f, o.y + 0.75f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.125f, o.y + 0.25f, o.z,        o.x + 0.875f, o.y + 0.75f, o.z + 0.5f}};
            case Facing::West:  return {AABB{o.x + 0.5f,   o.y + 0.25f, o.z + 0.125f, o.x + 1.0f, o.y + 0.75f, o.z + 0.875f}};
            case Facing::East:  return {AABB{o.x,          o.y + 0.25f, o.z + 0.125f, o.x + 0.5f, o.y + 0.75f, o.z + 0.875f}};
            default:            return {AABB{o.x + 0.125f, o.y,         o.z + 0.125f, o.x + 0.875f, o.y + 0.25f, o.z + 0.875f}};
        }
    }

    if (isSkullBlock(n)) {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x + 0.25f, o.y + 0.25f, o.z + 0.50f, o.x + 0.75f, o.y + 0.75f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.25f, o.y + 0.25f, o.z,         o.x + 0.75f, o.y + 0.75f, o.z + 0.50f}};
            case Facing::West:  return {AABB{o.x + 0.50f, o.y + 0.25f, o.z + 0.25f, o.x + 1.0f,  o.y + 0.75f, o.z + 0.75f}};
            case Facing::East:  return {AABB{o.x,         o.y + 0.25f, o.z + 0.25f, o.x + 0.50f, o.y + 0.75f, o.z + 0.75f}};
            default:            return {AABB{o.x + 0.25f, o.y,         o.z + 0.25f, o.x + 0.75f, o.y + 0.50f, o.z + 0.75f}};
        }
    }

    if (n == "wall_banner") {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x + 0.0625f, o.y, o.z + 0.875f, o.x + 0.9375f, o.y + 0.9375f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.0625f, o.y, o.z,          o.x + 0.9375f, o.y + 0.9375f, o.z + 0.125f}};
            case Facing::West:  return {AABB{o.x + 0.875f,  o.y, o.z + 0.0625f, o.x + 1.0f,   o.y + 0.9375f, o.z + 0.9375f}};
            case Facing::East:
            default:            return {AABB{o.x,           o.y, o.z + 0.0625f, o.x + 0.125f, o.y + 0.9375f, o.z + 0.9375f}};
        }
    }

    if (n == "standing_banner") {
        return {AABB{o.x + 0.25f, o.y, o.z + 0.25f, o.x + 0.75f, o.y + 1.75f, o.z + 0.75f}};
    }

    if (n == "anvil" || endsWith(n, "_anvil")) {
        const bool alongX = (orient.facing == Facing::North || orient.facing == Facing::South);
        return {
            AABB{o.x + 0.125f, o.y, o.z + 0.125f, o.x + 0.875f, o.y + 0.25f, o.z + 0.875f},
            AABB{o.x + 0.25f, o.y + 0.25f, o.z + 0.25f, o.x + 0.75f, o.y + 0.625f, o.z + 0.75f},
            alongX ? AABB{o.x, o.y + 0.625f, o.z + 0.1875f, o.x + 1.0f, o.y + 1.0f, o.z + 0.8125f}
                   : AABB{o.x + 0.1875f, o.y + 0.625f, o.z, o.x + 0.8125f, o.y + 1.0f, o.z + 1.0f}
        };
    }

    if (n == "lantern" || n == "soul_lantern" || endsWith(n, "_copper_lantern") || n == "copper_lantern") {
        if (orient.topHalf) {
            return {
                AABB{o.x + 0.3125f, o.y + 0.0625f, o.z + 0.3125f, o.x + 0.6875f, o.y + 0.5f, o.z + 0.6875f},
                AABB{o.x + 0.4375f, o.y + 0.5f, o.z + 0.4375f, o.x + 0.5625f, o.y + 1.0f, o.z + 0.5625f}
            };
        }
        return {
            AABB{o.x + 0.3125f, o.y, o.z + 0.3125f, o.x + 0.6875f, o.y + 0.4375f, o.z + 0.6875f},
            AABB{o.x + 0.375f, o.y + 0.4375f, o.z + 0.375f, o.x + 0.625f, o.y + 0.5625f, o.z + 0.625f}
        };
    }

    if (n == "torch" || n == "soul_torch" || n == "redstone_torch" ||
        n == "unlit_redstone_torch" || n == "copper_torch" || n == "underwater_torch") {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x + 0.375f, o.y + 0.1875f, o.z + 0.6875f, o.x + 0.625f, o.y + 0.8125f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.375f, o.y + 0.1875f, o.z, o.x + 0.625f, o.y + 0.8125f, o.z + 0.3125f}};
            case Facing::West:  return {AABB{o.x + 0.6875f, o.y + 0.1875f, o.z + 0.375f, o.x + 1.0f, o.y + 0.8125f, o.z + 0.625f}};
            case Facing::East:  return {AABB{o.x, o.y + 0.1875f, o.z + 0.375f, o.x + 0.3125f, o.y + 0.8125f, o.z + 0.625f}};
            default:            return {AABB{o.x + 0.4375f, o.y, o.z + 0.4375f, o.x + 0.5625f, o.y + 0.625f, o.z + 0.5625f}};
        }
    }

    if (n == "ladder") {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x, o.y, o.z + 0.875f, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 0.125f}};
            case Facing::West:  return {AABB{o.x + 0.875f, o.y, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::East:
            default:            return {AABB{o.x, o.y, o.z, o.x + 0.125f, o.y + 1.0f, o.z + 1.0f}};
        }
    }

    if (n == "vine" || n == "glow_lichen" || n == "sculk_vein" || n == "resin_clump") {
        switch (orient.facing) {
            case Facing::Down:  return {AABB{o.x, o.y + 0.9375f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::Up:    return {AABB{o.x, o.y,           o.z, o.x + 1.0f, o.y + 0.0625f, o.z + 1.0f}};
            case Facing::North: return {AABB{o.x, o.y, o.z + 0.9375f, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x, o.y, o.z,           o.x + 1.0f, o.y + 1.0f, o.z + 0.0625f}};
            case Facing::West:  return {AABB{o.x + 0.9375f, o.y, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
            case Facing::East:
            default:            return {AABB{o.x,           o.y, o.z, o.x + 0.0625f, o.y + 1.0f, o.z + 1.0f}};
        }
    }

    if (n == "spore_blossom" || n == "hanging_roots" || n == "cave_vines") {
        return {AABB{o.x + 0.125f, o.y + 0.75f, o.z + 0.125f, o.x + 0.875f, o.y + 1.0f, o.z + 0.875f}};
    }

    if (endsWith(n, "_button") || n == "lever" || n == "tripwire_hook") {
        switch (orient.facing) {
            case Facing::Down:  return {AABB{o.x + 0.3125f, o.y + 0.8125f, o.z + 0.375f, o.x + 0.6875f, o.y + 1.0f, o.z + 0.625f}};
            case Facing::Up:    return {AABB{o.x + 0.3125f, o.y, o.z + 0.375f, o.x + 0.6875f, o.y + 0.1875f, o.z + 0.625f}};
            case Facing::North: return {AABB{o.x + 0.3125f, o.y + 0.375f, o.z + 0.8125f, o.x + 0.6875f, o.y + 0.625f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.3125f, o.y + 0.375f, o.z, o.x + 0.6875f, o.y + 0.625f, o.z + 0.1875f}};
            case Facing::West:  return {AABB{o.x + 0.8125f, o.y + 0.375f, o.z + 0.3125f, o.x + 1.0f, o.y + 0.625f, o.z + 0.6875f}};
            case Facing::East:
            default:            return {AABB{o.x, o.y + 0.375f, o.z + 0.3125f, o.x + 0.1875f, o.y + 0.625f, o.z + 0.6875f}};
        }
    }

    if (isWallSignBlock(n)) {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x, o.y + 0.2708f, o.z + 0.9167f, o.x + 1.0f, o.y + 0.7708f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x, o.y + 0.2708f, o.z,           o.x + 1.0f, o.y + 0.7708f, o.z + 0.0833f}};
            case Facing::West:  return {AABB{o.x + 0.9167f, o.y + 0.2708f, o.z, o.x + 1.0f, o.y + 0.7708f, o.z + 1.0f}};
            case Facing::East:
            default:            return {AABB{o.x,           o.y + 0.2708f, o.z, o.x + 0.0833f, o.y + 0.7708f, o.z + 1.0f}};
        }
    }

    if (isStandingSignBlock(n)) {
        return {AABB{o.x + 0.25f, o.y, o.z + 0.25f, o.x + 0.75f, o.y + 1.0f, o.z + 0.75f}};
    }

    if (isHangingSignBlock(n)) {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 1.0f, o.z + 0.9375f}};
    }

    // End Rod & Lightning Rod: square mounting cap flush against the attached surface + slender shaft!
    if (n == "end_rod" || n == "lightning_rod" || endsWith(n, "_lightning_rod")) {
        switch (orient.facing) {
            case Facing::Down:
                return {
                    AABB{o.x + 0.375f,  o.y + 0.75f, o.z + 0.375f,  o.x + 0.625f,  o.y + 1.0f,  o.z + 0.625f},
                    AABB{o.x + 0.4375f, o.y,         o.z + 0.4375f, o.x + 0.5625f, o.y + 0.75f, o.z + 0.5625f}
                };
            case Facing::North:
                return {
                    AABB{o.x + 0.375f,  o.y + 0.375f,  o.z + 0.75f, o.x + 0.625f,  o.y + 0.625f,  o.z + 1.0f},
                    AABB{o.x + 0.4375f, o.y + 0.4375f, o.z,         o.x + 0.5625f, o.y + 0.5625f, o.z + 0.75f}
                };
            case Facing::South:
                return {
                    AABB{o.x + 0.375f,  o.y + 0.375f,  o.z,         o.x + 0.625f,  o.y + 0.625f,  o.z + 0.25f},
                    AABB{o.x + 0.4375f, o.y + 0.4375f, o.z + 0.25f, o.x + 0.5625f, o.y + 0.5625f, o.z + 1.0f}
                };
            case Facing::West:
                return {
                    AABB{o.x + 0.75f, o.y + 0.375f,  o.z + 0.375f,  o.x + 1.0f,  o.y + 0.625f,  o.z + 0.625f},
                    AABB{o.x,         o.y + 0.4375f, o.z + 0.4375f, o.x + 0.75f, o.y + 0.5625f, o.z + 0.5625f}
                };
            case Facing::East:
                return {
                    AABB{o.x,         o.y + 0.375f,  o.z + 0.375f,  o.x + 0.25f, o.y + 0.625f,  o.z + 0.625f},
                    AABB{o.x + 0.25f, o.y + 0.4375f, o.z + 0.4375f, o.x + 1.0f,  o.y + 0.5625f, o.z + 0.5625f}
                };
            case Facing::Up:
            default:
                return {
                    AABB{o.x + 0.375f,  o.y,         o.z + 0.375f,  o.x + 0.625f,  o.y + 0.25f, o.z + 0.625f},
                    AABB{o.x + 0.4375f, o.y + 0.25f, o.z + 0.4375f, o.x + 0.5625f, o.y + 1.0f,  o.z + 0.5625f}
                };
        }
    }

    if (n == "chain" || endsWith(n, "_chain")) {
        switch (orient.pillarAxis) {
            case 1:  return {AABB{o.x, o.y + 0.40625f, o.z + 0.40625f, o.x + 1.0f, o.y + 0.59375f, o.z + 0.59375f}};
            case 2:  return {AABB{o.x + 0.40625f, o.y + 0.40625f, o.z, o.x + 0.59375f, o.y + 0.59375f, o.z + 1.0f}};
            default: return {AABB{o.x + 0.40625f, o.y, o.z + 0.40625f, o.x + 0.59375f, o.y + 1.0f, o.z + 0.59375f}};
        }
    }

    if (n == "hopper") {
        std::vector<AABB> boxes;
        boxes.push_back(AABB{o.x, o.y + 0.625f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f});
        boxes.push_back(AABB{o.x + 0.25f, o.y + 0.25f, o.z + 0.25f, o.x + 0.75f, o.y + 0.625f, o.z + 0.75f});
        switch (orient.facing) {
            case Facing::North:
                boxes.push_back(AABB{o.x + 0.375f, o.y + 0.25f, o.z, o.x + 0.625f, o.y + 0.5f, o.z + 0.25f});
                break;
            case Facing::South:
                boxes.push_back(AABB{o.x + 0.375f, o.y + 0.25f, o.z + 0.75f, o.x + 0.625f, o.y + 0.5f, o.z + 1.0f});
                break;
            case Facing::West:
                boxes.push_back(AABB{o.x, o.y + 0.25f, o.z + 0.375f, o.x + 0.25f, o.y + 0.5f, o.z + 0.625f});
                break;
            case Facing::East:
                boxes.push_back(AABB{o.x + 0.75f, o.y + 0.25f, o.z + 0.375f, o.x + 1.0f, o.y + 0.5f, o.z + 0.625f});
                break;
            case Facing::Down:
            case Facing::Up:
            default:
                boxes.push_back(AABB{o.x + 0.375f, o.y, o.z + 0.375f, o.x + 0.625f, o.y + 0.25f, o.z + 0.625f});
                break;
        }
        return boxes;
    }

    if (n == "wheat" || n == "carrots" || n == "potatoes" || n == "beetroot" ||
        n == "pumpkin_stem" || n == "melon_stem" || n == "nether_wart" ||
        n == "torchflower_crop" || n == "pitcher_crop") {
        return {AABB{o.x + 0.125f, o.y, o.z + 0.125f, o.x + 0.875f, o.y + 0.25f, o.z + 0.875f}};
    }

    if (n == "cactus_flower") {
        return {AABB{o.x + 0.125f, o.y, o.z + 0.125f, o.x + 0.875f, o.y + 0.75f, o.z + 0.875f}};
    }

    if (isPureFlowerStemName(n)) {
        // Exact vanilla FlowerBlock visual bounds: centered slender 6x10x6 px stem + blossom!
        return {AABB{o.x + 0.3125f, o.y, o.z + 0.3125f, o.x + 0.6875f, o.y + 0.625f, o.z + 0.6875f}};
    }

    if (isFlowerOrSmallPlantName(n)) {
        return {AABB{o.x + 0.125f, o.y, o.z + 0.125f, o.x + 0.875f, o.y + 0.8125f, o.z + 0.875f}};
    }

    return {AABB::unitAt(pos)};
}

} // namespace placepreview
