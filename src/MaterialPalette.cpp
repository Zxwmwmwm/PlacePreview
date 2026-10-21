#include "placepreview/MaterialPalette.hpp"
#include "placepreview/StateInspector.hpp"

#include <array>
#include <cctype>
#include <string>

namespace placepreview {

namespace {

bool containsToken(std::string_view haystack, std::string_view needle) noexcept {
    return haystack.find(needle) != std::string_view::npos;
}

bool startsWithToken(std::string_view haystack, std::string_view prefix) noexcept {
    return haystack.size() >= prefix.size() && haystack.substr(0, prefix.size()) == prefix;
}

std::string toLowerAscii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

RgbaColor MaterialPalette::colorForDyeIndex(int auxData) noexcept {
    static constexpr std::array<RgbaColor, 16> kDyePalette{{
        {244, 246, 246, 255}, // 0  White
        {240, 134,  48, 255}, // 1  Orange
        {204,  88, 198, 255}, // 2  Magenta
        { 92, 188, 238, 255}, // 3  Light Blue
        {246, 206,  62, 255}, // 4  Yellow
        {130, 212,  56, 255}, // 5  Lime
        {244, 150, 182, 255}, // 6  Pink
        { 92,  98, 102, 255}, // 7  Gray
        {172, 174, 170, 255}, // 8  Light Gray / Silver
        { 46, 172, 184, 255}, // 9  Cyan
        {146,  72, 204, 255}, // 10 Purple
        { 68,  96, 202, 255}, // 11 Blue
        {142,  94,  58, 255}, // 12 Brown
        { 98, 146,  52, 255}, // 13 Green
        {206,  64,  60, 255}, // 14 Red
        { 58,  60,  68, 255}  // 15 Black
    }};
    const int idx = ((auxData % 16) + 16) % 16;
    return kDyePalette[static_cast<std::size_t>(idx)];
}

RgbaColor MaterialPalette::colorForBlock(std::string_view blockIdentifier, int auxData) noexcept {
    const std::string name = toLowerAscii(StateInspector::stripNamespace(blockIdentifier));
    if (name.empty()) {
        return {214, 224, 232, 255};
    }

    if (name == "bed") {
        return colorForDyeIndex(auxData);
    }

    // 1. Explicit 16-color dyed blocks & flowers
    if (containsToken(name, "light_blue")) return {92, 188, 238, 255};
    if (containsToken(name, "light_gray") || startsWithToken(name, "silver_")) return {172, 174, 170, 255};
    if (startsWithToken(name, "orange_") || containsToken(name, "_orange") ||
        name == "open_eyeblossom" || name == "torchflower") {
        return {240, 134, 48, 255};
    }
    if (containsToken(name, "magenta") || name == "lilac" || name == "allium") {
        return {204, 88, 198, 255};
    }
    if (startsWithToken(name, "yellow_") || containsToken(name, "sponge") ||
        containsToken(name, "ochre_froglight") || name == "dandelion" ||
        name == "golden_dandelion" || name == "sunflower" || name == "wildflowers" ||
        name == "firefly_bush") {
        return {246, 206, 62, 255};
    }
    if (startsWithToken(name, "lime_") || containsToken(name, "verdant_froglight")) return {130, 212, 56, 255};
    if (startsWithToken(name, "pink_") || containsToken(name, "pink_petals") ||
        name == "peony" || name == "cactus_flower" || name == "spore_blossom" ||
        containsToken(name, "cherry") || containsToken(name, "pearlescent_froglight")) {
        return {238, 162, 188, 255};
    }
    if (startsWithToken(name, "cyan_") || name == "pitcher_plant") return {46, 172, 184, 255};
    if (startsWithToken(name, "purple_")) return {146, 72, 204, 255};
    if (startsWithToken(name, "blue_") || name == "cornflower") return {68, 96, 202, 255};
    if (startsWithToken(name, "brown_") || name == "leaf_litter") return {142, 94, 58, 255};
    if (startsWithToken(name, "green_")) return {98, 146, 52, 255};
    if ((startsWithToken(name, "red_") && !containsToken(name, "red_sandstone") &&
         !containsToken(name, "red_sand") && name != "red_mushroom") ||
        name == "poppy" || name == "rose_bush") {
        return {206, 64, 60, 255};
    }
    if (startsWithToken(name, "gray_") || name == "closed_eyeblossom") return {96, 102, 108, 255};
    if (startsWithToken(name, "black_") || name == "coal_block" || name == "wither_rose") return {64, 66, 74, 255};
    if (startsWithToken(name, "white_") || name == "lily_of_the_valley" ||
        name == "oxeye_daisy" || name == "azure_bluet" || name == "dried_ghast") {
        return {244, 246, 246, 255};
    }

    // 2. Copper oxidation & resin BEFORE generic wood/chest matches (covers copper_chest & golem statues!)
    if (containsToken(name, "oxidized_copper") || containsToken(name, "oxidized_cut_copper") ||
        containsToken(name, "oxidized_chiseled_copper") || containsToken(name, "oxidized_lightning_rod")) {
        return {84, 194, 162, 255};
    }
    if (containsToken(name, "weathered_copper") || containsToken(name, "weathered_cut_copper") ||
        containsToken(name, "weathered_chiseled_copper") || containsToken(name, "weathered_lightning_rod")) {
        return {112, 180, 138, 255};
    }
    if (containsToken(name, "exposed_copper") || containsToken(name, "exposed_cut_copper") ||
        containsToken(name, "exposed_chiseled_copper") || containsToken(name, "exposed_lightning_rod")) {
        return {176, 146, 124, 255};
    }
    if (containsToken(name, "copper") || containsToken(name, "lightning_rod")) {
        return {218, 132, 96, 255};
    }
    if (containsToken(name, "resin")) return {236, 118, 52, 255};

    // 3. Wood species, shelves, signs, frames, armor stands, books, bamboo, Nether stems
    if (containsToken(name, "pale_oak")) return {226, 220, 214, 255};
    if (containsToken(name, "dark_oak") || containsToken(name, "darkoak")) return {112, 82, 54, 255};
    if (containsToken(name, "spruce") || name == "podzol") return {146, 108, 68, 255};
    if (containsToken(name, "birch") || containsToken(name, "poplar")) return {228, 214, 164, 255};
    if (containsToken(name, "jungle")) return {190, 140, 104, 255};
    if (containsToken(name, "acacia")) return {214, 122, 68, 255};
    if (containsToken(name, "mangrove")) return {166, 78, 70, 255};
    if (containsToken(name, "bamboo") || containsToken(name, "scaffolding")) return {214, 196, 98, 255};
    if (containsToken(name, "crimson") || (containsToken(name, "wart_block") && !containsToken(name, "warped"))) {
        return {168, 72, 108, 255};
    }
    if (containsToken(name, "warped")) return {54, 176, 164, 255};
    if (name == "glow_frame" || name == "glow_item_frame") return {112, 228, 196, 255};
    if (name == "frame" || name == "item_frame" || name == "painting" || name == "armor_stand") {
        return {188, 142, 88, 255};
    }
    if (containsToken(name, "oak") || containsToken(name, "planks") || containsToken(name, "wood") ||
        containsToken(name, "log") || containsToken(name, "shelf") || containsToken(name, "chest") ||
        containsToken(name, "barrel") || containsToken(name, "lectern") || containsToken(name, "crafting") ||
        containsToken(name, "ladder") || containsToken(name, "sign") || containsToken(name, "bookshelf") ||
        containsToken(name, "loom") || containsToken(name, "cartography") || containsToken(name, "fletching") ||
        containsToken(name, "composter") || containsToken(name, "beehive") || containsToken(name, "bee_nest") ||
        containsToken(name, "noteblock") || containsToken(name, "jukebox") || name == "creaking_heart" ||
        (containsToken(name, "campfire") && !containsToken(name, "soul"))) {
        return {198, 160, 104, 255};
    }

    // 4. Lighting, aquatic, & special blocks
    if (containsToken(name, "soul_lantern") || containsToken(name, "soul_torch") ||
        containsToken(name, "soul_campfire")) {
        return {88, 224, 236, 255};
    }
    if (containsToken(name, "lantern") || containsToken(name, "torch") || containsToken(name, "shroomlight") ||
        containsToken(name, "jack_o_lantern") || name == "candle") {
        return {250, 198, 86, 255};
    }
    if (containsToken(name, "end_rod")) return {240, 230, 242, 255};
    if (containsToken(name, "sea_pickle")) return {128, 178, 84, 255};
    if (containsToken(name, "turtle_egg") || containsToken(name, "sniffer_egg")) return {226, 222, 192, 255};
    if (containsToken(name, "conduit")) return {112, 214, 206, 255};
    if (containsToken(name, "heavy_core")) return {116, 124, 138, 255};
    if (containsToken(name, "tube_coral")) return {66, 116, 226, 255};
    if (containsToken(name, "brain_coral")) return {232, 128, 178, 255};
    if (containsToken(name, "bubble_coral")) return {176, 72, 206, 255};
    if (containsToken(name, "fire_coral")) return {218, 66, 62, 255};
    if (containsToken(name, "horn_coral")) return {238, 214, 72, 255};
    if (containsToken(name, "sculk")) return {38, 146, 162, 255};

    // 5. Skulls / heads
    if (containsToken(name, "wither_skeleton") || containsToken(name, "dragon_head")) return {74, 76, 86, 255};
    if (containsToken(name, "creeper_head") || containsToken(name, "zombie_head")) return {108, 176, 92, 255};
    if (containsToken(name, "piglin_head")) return {228, 166, 138, 255};
    if (containsToken(name, "skull") || containsToken(name, "head")) return {226, 224, 214, 255};

    // 6. Gems, metals, & redstone
    if (containsToken(name, "diamond") || containsToken(name, "beacon")) return {94, 236, 232, 255};
    if (containsToken(name, "emerald") || containsToken(name, "slime")) return {82, 224, 118, 255};
    if (containsToken(name, "gold") || containsToken(name, "honey") || containsToken(name, "glowstone") ||
        containsToken(name, "hay") || containsToken(name, "bell")) {
        return {244, 206, 84, 255};
    }
    if (containsToken(name, "amethyst") || containsToken(name, "purpur") || containsToken(name, "chorus")) {
        return {186, 128, 224, 255};
    }
    if (containsToken(name, "lapis")) return {68, 112, 218, 255};
    if (containsToken(name, "redstone") || containsToken(name, "tnt") || containsToken(name, "netherrack") ||
        containsToken(name, "nether_brick")) {
        return {212, 76, 72, 255};
    }
    if (containsToken(name, "prismarine") || containsToken(name, "sea_lantern")) {
        return {98, 204, 196, 255};
    }

    // 7. Foliage, crops, soil, sand, terracotta, & natural terrain
    if (containsToken(name, "pumpkin") || containsToken(name, "red_sand") || containsToken(name, "carrot")) {
        return {222, 126, 56, 255};
    }
    if (containsToken(name, "dry_grass") || name == "deadbush") {
        return {198, 168, 102, 255};
    }
    if (containsToken(name, "waterlily") || containsToken(name, "leaves") || containsToken(name, "moss") ||
        containsToken(name, "grass") || containsToken(name, "fern") || containsToken(name, "vine") ||
        containsToken(name, "kelp") || containsToken(name, "seagrass") || containsToken(name, "cactus") ||
        containsToken(name, "melon") || containsToken(name, "sugar_cane") || containsToken(name, "sapling") ||
        containsToken(name, "azalea") || containsToken(name, "dripleaf") || name == "bush") {
        return {114, 198, 96, 255};
    }
    if (containsToken(name, "dirt") || containsToken(name, "mud") || containsToken(name, "farmland") ||
        containsToken(name, "soul_sand") || containsToken(name, "soul_soil") || containsToken(name, "podzol") ||
        containsToken(name, "mycelium") || containsToken(name, "flower_pot") || containsToken(name, "decorated_pot")) {
        return {154, 114, 82, 255};
    }
    if (containsToken(name, "quartz") || containsToken(name, "snow") || containsToken(name, "calcite") ||
        containsToken(name, "diorite") || containsToken(name, "bone") || containsToken(name, "iron")) {
        return {232, 236, 240, 255};
    }
    if (containsToken(name, "sand") || containsToken(name, "end_stone")) return {232, 218, 166, 255};
    if (containsToken(name, "brick") || containsToken(name, "granite") || containsToken(name, "terracotta")) {
        return {192, 116, 96, 255};
    }
    if (containsToken(name, "clay") || containsToken(name, "gravel")) return {164, 168, 178, 255};
    if (containsToken(name, "glass") || containsToken(name, "ice")) return {164, 228, 250, 255};
    if (containsToken(name, "deepslate") || containsToken(name, "blackstone") ||
        containsToken(name, "basalt") || containsToken(name, "obsidian") || containsToken(name, "anvil") ||
        containsToken(name, "hopper") || containsToken(name, "cauldron") || containsToken(name, "netherite") ||
        containsToken(name, "ancient_debris")) {
        return {112, 118, 130, 255};
    }
    if (containsToken(name, "stone") || containsToken(name, "cobble") || containsToken(name, "andesite") ||
        containsToken(name, "tuff") || containsToken(name, "furnace") || containsToken(name, "smoker") ||
        containsToken(name, "blast_furnace") || containsToken(name, "observer") || containsToken(name, "piston") ||
        containsToken(name, "dispenser") || containsToken(name, "dropper") || containsToken(name, "crafter")) {
        return {168, 174, 182, 255};
    }

    return {204, 216, 228, 255};
}

} // namespace placepreview
