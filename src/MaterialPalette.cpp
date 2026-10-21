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
        return {92, 224, 255, 255};
    }

    if (name == "bed") {
        return colorForDyeIndex(auxData);
    }

    if (containsToken(name, "light_blue")) return {92, 188, 238, 255};
    if (containsToken(name, "light_gray") || containsToken(name, "silver_")) return {172, 174, 170, 255};
    if (containsToken(name, "magenta")) return {204, 88, 198, 255};
    if (containsToken(name, "pink") || containsToken(name, "cherry")) return {238, 168, 188, 255};
    if (containsToken(name, "pale_oak")) return {226, 220, 214, 255};
    if (containsToken(name, "dark_oak") || containsToken(name, "darkoak")) return {112, 82, 54, 255};
    if (containsToken(name, "spruce")) return {146, 108, 68, 255};
    if (containsToken(name, "birch")) return {228, 214, 164, 255};
    if (containsToken(name, "jungle")) return {190, 140, 104, 255};
    if (containsToken(name, "acacia")) return {214, 122, 68, 255};
    if (containsToken(name, "mangrove")) return {166, 78, 70, 255};
    if (containsToken(name, "bamboo")) return {214, 196, 98, 255};
    if (containsToken(name, "crimson")) return {168, 72, 108, 255};
    if (containsToken(name, "warped")) return {54, 176, 164, 255};
    if (containsToken(name, "oak") || containsToken(name, "planks") || containsToken(name, "wood") ||
        containsToken(name, "log") || containsToken(name, "chest") || containsToken(name, "barrel") ||
        containsToken(name, "lectern") || containsToken(name, "crafting") || containsToken(name, "ladder") ||
        containsToken(name, "sign")) {
        return {198, 160, 104, 255};
    }

    if (containsToken(name, "oxidized_copper") || containsToken(name, "oxidized_cut_copper")) return {84, 194, 162, 255};
    if (containsToken(name, "weathered_copper") || containsToken(name, "weathered_cut_copper")) return {112, 180, 138, 255};
    if (containsToken(name, "exposed_copper") || containsToken(name, "exposed_cut_copper")) return {176, 146, 124, 255};
    if (containsToken(name, "copper")) return {218, 132, 96, 255};

    if (containsToken(name, "diamond")) return {94, 236, 232, 255};
    if (containsToken(name, "emerald") || containsToken(name, "slime")) return {82, 224, 118, 255};
    if (containsToken(name, "gold") || containsToken(name, "honey") || containsToken(name, "glowstone") ||
        containsToken(name, "hay")) {
        return {244, 206, 84, 255};
    }
    if (containsToken(name, "amethyst") || containsToken(name, "purpur")) return {186, 128, 224, 255};
    if (containsToken(name, "lapis") || containsToken(name, "blue")) return {82, 126, 224, 255};
    if (containsToken(name, "redstone") || containsToken(name, "tnt") || containsToken(name, "red_nether")) {
        return {226, 78, 72, 255};
    }
    if (containsToken(name, "prismarine") || containsToken(name, "sea_lantern") || containsToken(name, "cyan")) {
        return {98, 204, 196, 255};
    }
    if (containsToken(name, "quartz") || containsToken(name, "white") || containsToken(name, "snow") ||
        containsToken(name, "calcite") || containsToken(name, "diorite") || containsToken(name, "bone") ||
        containsToken(name, "iron")) {
        return {232, 236, 240, 255};
    }
    if (containsToken(name, "sand") || containsToken(name, "end_stone")) return {232, 218, 166, 255};
    if (containsToken(name, "brick") || containsToken(name, "granite") || containsToken(name, "terracotta")) {
        return {192, 116, 96, 255};
    }
    if (containsToken(name, "waterlily") || containsToken(name, "leaves") || containsToken(name, "moss") ||
        containsToken(name, "grass") || containsToken(name, "green") || containsToken(name, "lime")) {
        return {114, 198, 96, 255};
    }
    if (containsToken(name, "glass") || containsToken(name, "ice")) return {164, 228, 250, 255};
    if (containsToken(name, "deepslate") || containsToken(name, "blackstone") ||
        containsToken(name, "basalt") || containsToken(name, "obsidian") || containsToken(name, "anvil") ||
        containsToken(name, "hopper")) {
        return {124, 130, 142, 255};
    }
    if (containsToken(name, "stone") || containsToken(name, "cobble") || containsToken(name, "andesite") ||
        containsToken(name, "tuff") || containsToken(name, "furnace") || containsToken(name, "observer") ||
        containsToken(name, "piston") || containsToken(name, "dispenser") || containsToken(name, "dropper")) {
        return {168, 174, 182, 255};
    }

    return {92, 224, 255, 255};
}

} // namespace placepreview
