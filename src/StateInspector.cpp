#include "buildghost/StateInspector.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace buildghost {

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

// Minecraft Bedrock cardinal_direction / Direction::Type enum:
//   0 = south (+Z), 1 = west (-X), 2 = north (-Z), 3 = east (+X).
Facing facingFromCardinalEnum(int val) noexcept {
    switch (val & 3) {
        case 0: return Facing::South;
        case 1: return Facing::West;
        case 2: return Facing::North;
        case 3: return Facing::East;
    }
    return Facing::South;
}

// Minecraft Bedrock weirdo_direction (Stairs high-step back edge):
//   0 = east (+X), 1 = west (-X), 2 = south (+Z), 3 = north (-Z).
Facing facingFromWeirdoDirection(int val) noexcept {
    switch (val & 3) {
        case 0: return Facing::East;
        case 1: return Facing::West;
        case 2: return Facing::South;
        case 3: return Facing::North;
    }
    return Facing::North;
}

} // namespace

std::string_view StateInspector::stripNamespace(std::string_view id) noexcept {
    const std::size_t colon = id.find(':');
    return (colon == std::string_view::npos) ? id : id.substr(colon + 1);
}

bool StateInspector::isDoorBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (isTrapdoorBlock(n)) return false;
    return n == "wooden_door" || endsWith(n, "_door");
}

bool StateInspector::isBedBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "bed" || endsWith(n, "_bed");
}

bool StateInspector::isDoubleSlabBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return startsWith(n, "double_") && endsWith(n, "_slab");
}

bool StateInspector::isSlabBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (startsWith(n, "double_")) return false;
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
           n == "amethyst_cluster" ||
           endsWith(n, "_amethyst_bud") ||
           endsWith(n, "_button") ||
           n == "lever" ||
           n == "torch" ||
           n == "soul_torch" ||
           n == "redstone_torch" ||
           n == "copper_torch";
}

bool StateInspector::isHorizontalDirectionalBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return isStairBlock(n) ||
           isDoorBlock(n) ||
           isBedBlock(n) ||
           isTrapdoorBlock(n) ||
           endsWith(n, "_fence_gate") ||
           n == "fence_gate" ||
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
           endsWith(n, "_anvil") ||
           n == "anvil" ||
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
           endsWith(n, "_glazed_terracotta") ||
           endsWith(n, "wall_sign") ||
           endsWith(n, "standing_sign");
}

bool StateInspector::isWaterSurfaceBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "waterlily" || n == "lily_pad" || n == "frog_spawn" || n == "frogspawn";
}

bool StateInspector::isInteractiveBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    return n == "crafting_table" ||
           endsWith(n, "furnace") ||
           n == "smoker" ||
           n == "chest" ||
           n == "trapped_chest" ||
           n == "ender_chest" ||
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
           n == "lever" ||
           endsWith(n, "_button") ||
           isDoorBlock(n) ||
           isTrapdoorBlock(n) ||
           endsWith(n, "_fence_gate") ||
           n == "fence_gate" ||
           n == "bed" ||
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

    if (n == "bed" || endsWith(n, "_bed")) return "minecraft:" + n;
    if (n == "wooden_door") return "minecraft:wooden_door";
    if (isDoorBlock(n)) return "minecraft:" + n;

    if (n == "hopper") return "minecraft:hopper";
    if (n == "cauldron") return "minecraft:cauldron";
    if (n == "brewing_stand") return "minecraft:brewing_stand";
    if (n == "flower_pot") return "minecraft:flower_pot";
    if (n == "frame") return "minecraft:frame";
    if (n == "glow_frame") return "minecraft:glow_frame";
    if (n == "repeater") return "minecraft:unpowered_repeater";
    if (n == "comparator") return "minecraft:unpowered_comparator";
    if (n == "campfire") return "minecraft:campfire";
    if (n == "soul_campfire") return "minecraft:soul_campfire";
    if (n == "chain") return "minecraft:chain";
    if (n == "cake") return "minecraft:cake";
    if (n == "redstone") return "minecraft:redstone_wire";
    if (n == "string") return "minecraft:tripwire";
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
    if (n == "sugar_cane" || n == "reeds") return "minecraft:reeds";

    // Signs -> standing sign on top face, wall sign on horizontal faces, invalid on bottom face.
    if (endsWith(n, "_hanging_sign")) {
        return "minecraft:" + n;
    }
    if (endsWith(n, "_sign")) {
        if (hitFace == Facing::Down) return {};
        const std::string prefix = n.substr(0, n.size() - 5);
        if (prefix == "oak") {
            return (hitFace == Facing::Up) ? "minecraft:standing_sign" : "minecraft:wall_sign";
        }
        return "minecraft:" + prefix + ((hitFace == Facing::Up) ? "_standing_sign" : "_wall_sign");
    }
    if (n == "banner") {
        if (hitFace == Facing::Down) return {};
        return (hitFace == Facing::Up) ? "minecraft:standing_banner" : "minecraft:wall_banner";
    }

    return {};
}

OrientationSummary StateInspector::decodeOrientation(
    std::string_view blockIdentifier,
    const std::vector<BlockStateProperty>& props,
    Facing hitFace,
    const Vec3& subClick,
    float playerPitchDeg,
    float playerYawDeg) noexcept {

    OrientationSummary out{};
    const std::string n = lowerAscii(stripNamespace(blockIdentifier));

    // 1. Slab vertical half (state property or deterministic subClick/face threshold).
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

    // 2. Pillar / Log axial orientation (pillar_axis: 0=Y, 1=X, 2=Z).
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

    // 2.5 Hanging / standing Lanterns (floor standing vs ceiling hanging chain).
    if (n == "lantern" || n == "soul_lantern" || endsWith(n, "_copper_lantern") || n == "copper_lantern") {
        int hanging = -1;
        if (findProp(props, "hanging", hanging)) {
            out.topHalf = (hanging != 0);
        } else {
            out.topHalf = (hitFace == Facing::Down);
        }
        return out;
    }
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

    // 4. Doors (hinge side + player look direction).
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

    // 5. Beds (foot -> head horizontal facing).
    if (isBedBlock(n)) {
        out.kind = OrientationKind::Horizontal;
        int dirVal = -1;
        out.facing = findProp(props, "direction", dirVal)
                         ? facingFromCardinalEnum(dirVal)
                         : yawToPlayerLookHorizontal(playerYawDeg);
        return out;
    }

    // 6. Six-way blocks (facing_direction / minecraft:facing_direction / minecraft:block_face).
    int sixVal = -1;
    if (findProp(props, "minecraft:facing_direction", sixVal) ||
        findProp(props, "facing_direction", sixVal) ||
        findProp(props, "minecraft:block_face", sixVal) ||
        isSixWayBlock(n)) {
        out.kind = OrientationKind::SixWay;
        if (sixVal >= 0 && sixVal <= 5) {
            out.facing = facingFromSixWayValue(sixVal);
        } else if (n == "hopper") {
            // Hopper output spout connects INTO the clicked face (opposite of hitFace, Down when clicking Up).
            const Facing intoBlock = oppositeFacing(hitFace);
            out.facing = (intoBlock == Facing::Up) ? Facing::Down : intoBlock;
        } else if (n == "observer") {
            // Observer active sensor face points away from player along look vector; redstone output faces player.
            out.facing = pitchYawToSixWayLook(playerPitchDeg, playerYawDeg);
        } else if (n == "end_rod" || n == "lightning_rod" ||
                   n == "amethyst_cluster" || endsWith(n, "_amethyst_bud") ||
                   endsWith(n, "_button") || n == "lever" || endsWith(n, "torch")) {
            out.facing = hitFace;
        } else {
            // Piston, Sticky Piston, Dispenser, Dropper, Barrel: front face looks toward player!
            out.facing = oppositeFacing(pitchYawToSixWayLook(playerPitchDeg, playerYawDeg));
        }
        return out;
    }

    // 7. Horizontal 4-way blocks (minecraft:cardinal_direction / direction).
    int cardVal = -1;
    if (findProp(props, "minecraft:cardinal_direction", cardVal) ||
        findProp(props, "direction", cardVal) ||
        isHorizontalDirectionalBlock(n)) {
        out.kind = OrientationKind::Horizontal;
        if (cardVal >= 0) {
            out.facing = facingFromCardinalEnum(cardVal);
        } else if (n == "ladder" || endsWith(n, "wall_sign") || n == "wall_banner") {
            out.facing = (hitFace == Facing::Up || hitFace == Facing::Down)
                             ? oppositeFacing(yawToPlayerLookHorizontal(playerYawDeg))
                             : hitFace;
        } else {
            out.facing = oppositeFacing(yawToPlayerLookHorizontal(playerYawDeg));
        }
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

    // Measure vertical span occupied by the existing single half-slab in [o.y .. o.y + 1].
    float minLocalY = 1.0f;
    float maxLocalY = 0.0f;
    for (const auto& box : existingWorldBoxes) {
        minLocalY = std::min(minLocalY, std::clamp(box.min.y - o.y, 0.0f, 1.0f));
        maxLocalY = std::max(maxLocalY, std::clamp(box.max.y - o.y, 0.0f, 1.0f));
    }

    // Existing bottom half [0.0 .. 0.5] -> newly placed merge piece is TOP half [0.5 .. 1.0]!
    if (maxLocalY <= 0.55f && minLocalY <= 0.05f) {
        return {AABB{o.x, o.y + 0.5f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
    }
    // Existing top half [0.5 .. 1.0] -> newly placed merge piece is BOTTOM half [0.0 .. 0.5]!
    if (minLocalY >= 0.45f && maxLocalY >= 0.95f) {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.5f, o.z + 1.0f}};
    }

    return {AABB::unitAt(cellPos)};
}

std::vector<AABB> StateInspector::buildDoorWorldBoxes(
    const BlockPos& lowerPos,
    Facing playerLookHorizontal,
    bool hingeRight) {

    // Closed door leaf is 3/16 (0.1875) thick on the outside edge of the cell opposite the player's
    // look direction (when entering the doorway along `playerLookHorizontal`, the closed door sits
    // on the entry edge of `lowerPos`, spanning 2.0 blocks vertically [y .. y + 2.0]).
    constexpr float kLeafThickness = 0.1875f;
    const Vec3 o = lowerPos.originVec();

    AABB leaf{};
    Vec3 hingeEdgeMin{};
    Vec3 hingeEdgeMax{};

    switch (playerLookHorizontal) {
        case Facing::South: // +Z look -> door sits on North (-Z) entry face [z .. z + 0.1875]
            leaf = {o.x, o.y, o.z, o.x + 1.0f, o.y + 2.0f, o.z + kLeafThickness};
            if (hingeRight) { // Looking +Z, right hand is -X (West)
                hingeEdgeMin = {o.x, o.y + 0.20f, o.z - 0.015f};
                hingeEdgeMax = {o.x + 0.075f, o.y + 1.80f, o.z + kLeafThickness + 0.015f};
            } else {          // Looking +Z, left hand is +X (East)
                hingeEdgeMin = {o.x + 0.925f, o.y + 0.20f, o.z - 0.015f};
                hingeEdgeMax = {o.x + 1.0f, o.y + 1.80f, o.z + kLeafThickness + 0.015f};
            }
            break;

        case Facing::North: // -Z look -> door sits on South (+Z) entry face [z + 0.8125 .. z + 1]
            leaf = {o.x, o.y, o.z + 1.0f - kLeafThickness, o.x + 1.0f, o.y + 2.0f, o.z + 1.0f};
            if (hingeRight) { // Looking -Z, right hand is +X (East)
                hingeEdgeMin = {o.x + 0.925f, o.y + 0.20f, o.z + 1.0f - kLeafThickness - 0.015f};
                hingeEdgeMax = {o.x + 1.0f, o.y + 1.80f, o.z + 1.015f};
            } else {          // Looking -Z, left hand is -X (West)
                hingeEdgeMin = {o.x, o.y + 0.20f, o.z + 1.0f - kLeafThickness - 0.015f};
                hingeEdgeMax = {o.x + 0.075f, o.y + 1.80f, o.z + 1.015f};
            }
            break;

        case Facing::East: // +X look -> door sits on West (-X) entry face [x .. x + 0.1875]
            leaf = {o.x, o.y, o.z, o.x + kLeafThickness, o.y + 2.0f, o.z + 1.0f};
            if (hingeRight) { // Looking +X, right hand is +Z (South)
                hingeEdgeMin = {o.x - 0.015f, o.y + 0.20f, o.z + 0.925f};
                hingeEdgeMax = {o.x + kLeafThickness + 0.015f, o.y + 1.80f, o.z + 1.0f};
            } else {          // Looking +X, left hand is -Z (North)
                hingeEdgeMin = {o.x - 0.015f, o.y + 0.20f, o.z};
                hingeEdgeMax = {o.x + kLeafThickness + 0.015f, o.y + 1.80f, o.z + 0.075f};
            }
            break;

        case Facing::West: // -X look -> door sits on East (+X) entry face [x + 0.8125 .. x + 1]
        default:
            leaf = {o.x + 1.0f - kLeafThickness, o.y, o.z, o.x + 1.0f, o.y + 2.0f, o.z + 1.0f};
            if (hingeRight) { // Looking -X, right hand is -Z (North)
                hingeEdgeMin = {o.x + 1.0f - kLeafThickness - 0.015f, o.y + 0.20f, o.z};
                hingeEdgeMax = {o.x + 1.015f, o.y + 1.80f, o.z + 0.075f};
            } else {          // Looking -X, left hand is +Z (South)
                hingeEdgeMin = {o.x + 1.0f - kLeafThickness - 0.015f, o.y + 0.20f, o.z + 0.925f};
                hingeEdgeMax = {o.x + 1.015f, o.y + 1.80f, o.z + 1.0f};
            }
            break;
    }

    (void)hingeEdgeMin;
    (void)hingeEdgeMax;
    return {leaf};
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

    // 1. Main mattress & frame spanning full 2-block footprint [minY + 0.1875 .. minY + 0.5625]
    const AABB mattress{minX, minY + 0.1875f, minZ, maxX, minY + 0.5625f, maxZ};

    // 2. Four corner wooden legs [minY .. minY + 0.1875], 3/16 (0.1875) square
    constexpr float kLeg = 0.1875f;
    const AABB legNW{minX,        minY, minZ,        minX + kLeg, minY + 0.1875f, minZ + kLeg};
    const AABB legNE{maxX - kLeg, minY, minZ,        maxX,        minY + 0.1875f, minZ + kLeg};
    const AABB legSW{minX,        minY, maxZ - kLeg, minX + kLeg, minY + 0.1875f, maxZ};
    const AABB legSE{maxX - kLeg, minY, maxZ - kLeg, maxX,        minY + 0.1875f, maxZ};

    // 3. Raised 3D pillow on the head cell so foot vs head orientation is unmistakable!
    const Vec3 ho = headPos.originVec();
    AABB pillow{};
    switch (headDirection) {
        case Facing::North: // -Z head end
            pillow = {ho.x + 0.08f, minY + 0.5625f, ho.z + 0.08f, ho.x + 0.92f, minY + 0.65f, ho.z + 0.56f};
            break;
        case Facing::South: // +Z head end
            pillow = {ho.x + 0.08f, minY + 0.5625f, ho.z + 0.44f, ho.x + 0.92f, minY + 0.65f, ho.z + 0.92f};
            break;
        case Facing::West:  // -X head end
            pillow = {ho.x + 0.08f, minY + 0.5625f, ho.z + 0.08f, ho.x + 0.56f, minY + 0.65f, ho.z + 0.92f};
            break;
        case Facing::East:  // +X head end
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
        // Outer corner stair: base slab + single 0.5 x 0.5 corner step!
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
        // Inner corner stair: base slab + full primary step + perpendicular quarter step!
        const Facing oppPrimary = oppositeFacing(stairFacing);
        const Facing xDir = stairNS ? backPerpFacing : oppPrimary;
        const Facing zDir = stairNS ? oppPrimary : backPerpFacing;
        return {base, mainStep, AABB{halfXMin(xDir), y0, halfZMin(zDir), halfXMax(xDir), y1, halfZMax(zDir)}};
    }

    return {base, mainStep};
}

bool StateInspector::isFenceOrWallOrPane(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (endsWith(n, "_fence_gate") || n == "fence_gate") return false;
    return n == "fence" || endsWith(n, "_fence") ||
           n == "cobblestone_wall" || endsWith(n, "_wall") ||
           n == "iron_bars" || n == "glass_pane" || endsWith(n, "_glass_pane");
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
    const bool isPane = (n == "iron_bars" || n == "glass_pane" || endsWith(n, "_glass_pane"));

    std::vector<AABB> boxes;
    if (isWall) {
        boxes.push_back(AABB{o.x + 0.25f, o.y, o.z + 0.25f, o.x + 0.75f, o.y + 1.0f, o.z + 0.75f});
        if (connectNorth) boxes.push_back(AABB{o.x + 0.3125f, o.y, o.z, o.x + 0.6875f, o.y + 0.875f, o.z + 0.25f});
        if (connectSouth) boxes.push_back(AABB{o.x + 0.3125f, o.y, o.z + 0.75f, o.x + 0.6875f, o.y + 0.875f, o.z + 1.0f});
        if (connectWest)  boxes.push_back(AABB{o.x, o.y, o.z + 0.3125f, o.x + 0.25f, o.y + 0.875f, o.z + 0.6875f});
        if (connectEast)  boxes.push_back(AABB{o.x + 0.75f, o.y, o.z + 0.3125f, o.x + 1.0f, o.y + 0.875f, o.z + 0.6875f});
    } else if (isPane) {
        boxes.push_back(AABB{o.x + 0.4375f, o.y, o.z + 0.4375f, o.x + 0.5625f, o.y + 1.0f, o.z + 0.5625f});
        if (connectNorth) boxes.push_back(AABB{o.x + 0.4375f, o.y, o.z, o.x + 0.5625f, o.y + 1.0f, o.z + 0.4375f});
        if (connectSouth) boxes.push_back(AABB{o.x + 0.4375f, o.y, o.z + 0.5625f, o.x + 0.5625f, o.y + 1.0f, o.z + 1.0f});
        if (connectWest)  boxes.push_back(AABB{o.x, o.y, o.z + 0.4375f, o.x + 0.4375f, o.y + 1.0f, o.z + 0.5625f});
        if (connectEast)  boxes.push_back(AABB{o.x + 0.5625f, o.y, o.z + 0.4375f, o.x + 1.0f, o.y + 1.0f, o.z + 0.5625f});
    } else {
        // Fence: center post [6/16..10/16] + upper & lower horizontal rails to connected neighbors!
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

bool StateInspector::isNonSolidSupportBlock(std::string_view id) noexcept {
    const std::string n = lowerAscii(stripNamespace(id));
    if (n.empty() || n == "air" || n == "cave_air" || n == "void_air" ||
        n == "water" || n == "flowing_water" || n == "lava" || n == "flowing_lava" ||
        n == "fire" || n == "soul_fire" || n == "structure_void" || n == "light_block") {
        return true;
    }
    return n == "short_grass" || n == "tallgrass" || n == "fern" || n == "large_fern" ||
           n == "deadbush" || n == "vine" || n == "glow_lichen" || n == "sculk_vein" ||
           n == "snow_layer" || n == "carpet" || endsWith(n, "_carpet") ||
           n == "torch" || endsWith(n, "_torch") || n == "ladder" ||
           endsWith(n, "_button") || n == "lever" || n == "redstone_wire" ||
           n == "tripwire" || endsWith(n, "_sapling") || endsWith(n, "_flower") ||
           n == "dandelion" || n == "poppy" || n == "wheat" || n == "carrots" ||
           n == "potatoes" || n == "beetroot" || n == "nether_wart" || n == "reeds" ||
           isWaterSurfaceBlock(n) || endsWith(n, "sign") || endsWith(n, "banner");
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

    // 1. Torches (cannot hang upside-down from Facing::Down ceilings!)
    if (n == "torch" || n == "soul_torch" || n == "redstone_torch" ||
        n == "unlit_redstone_torch" || n == "copper_torch") {
        if (hitFace == Facing::Down) return false;
        if (hitFace == Facing::Up) return belowSolid;
        return behindSolid;
    }

    // 2. Hanging / standing Lanterns (excluding full blocks sea_lantern & jack_o_lantern)
    if (n == "lantern" || n == "soul_lantern" || endsWith(n, "_copper_lantern") || n == "copper_lantern") {
        if (hitFace == Facing::Up) return belowSolid;
        if (hitFace == Facing::Down) return aboveSolid || above == "chain" || isFenceOrWallOrPane(above);
        return belowSolid || aboveSolid || above == "chain";
    }

    // 3. Ladders (must attach to a horizontal solid wall face!)
    if (n == "ladder") {
        if (hitFace == Facing::Up || hitFace == Facing::Down) return false;
        return behindSolid;
    }

    // 4. Buttons, Levers, Tripwire Hooks, Wall Signs, Wall Banners (must attach against solid support!)
    if (endsWith(n, "_button") || n == "lever") {
        return behindSolid;
    }
    if (n == "tripwire_hook" || endsWith(n, "wall_sign") || n == "wall_banner") {
        if (hitFace == Facing::Up || hitFace == Facing::Down) return false;
        return behindSolid;
    }

    // 5. Floor-resting blocks (Carpets, Pressure Plates, Rails, Repeaters, Comparators, Standing Signs, Cakes, Pots, Wires)
    if (n == "carpet" || endsWith(n, "_carpet") ||
        endsWith(n, "_pressure_plate") ||
        n == "rail" || endsWith(n, "_rail") ||
        n == "repeater" || n == "unpowered_repeater" || n == "powered_repeater" ||
        n == "comparator" || n == "unpowered_comparator" || n == "powered_comparator" ||
        n == "standing_sign" || endsWith(n, "_standing_sign") || n == "standing_banner" ||
        n == "flower_pot" || n == "cake" || n == "redstone_wire" || n == "snow_layer") {
        return belowSolid;
    }

    // 6. Agricultural crops, Nether Wart, Saplings, Cactus
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

    if (isTrapdoorBlock(n)) {
        if (orient.topHalf) {
            return {AABB{o.x, o.y + 0.8125f, o.z, o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
        }
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.1875f, o.z + 1.0f}};
    }

    if (isWaterSurfaceBlock(n)) {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 0.09375f, o.z + 0.9375f}};
    }

    if (endsWith(n, "_carpet") || n == "carpet" || n == "moss_carpet" || endsWith(n, "_pressure_plate")) {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.0625f, o.z + 1.0f}};
    }

    if (n == "snow_layer") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.125f, o.z + 1.0f}};
    }

    if (n == "repeater" || n == "unpowered_repeater" || n == "powered_repeater" ||
        n == "comparator" || n == "unpowered_comparator" || n == "powered_comparator") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.125f, o.z + 1.0f}};
    }

    if (n == "chest" || n == "trapped_chest" || n == "ender_chest") {
        return {AABB{o.x + 0.0625f, o.y, o.z + 0.0625f, o.x + 0.9375f, o.y + 0.875f, o.z + 0.9375f}};
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
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.4375f, o.z + 1.0f}};
    }

    if (n == "stonecutter_block" || n == "stonecutter") {
        return {AABB{o.x, o.y, o.z, o.x + 1.0f, o.y + 0.5625f, o.z + 1.0f}};
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
        n == "unlit_redstone_torch" || n == "copper_torch") {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x + 0.375f, o.y + 0.1875f, o.z + 0.6875f, o.x + 0.625f, o.y + 0.8125f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x + 0.375f, o.y + 0.1875f, o.z, o.x + 0.625f, o.y + 0.8125f, o.z + 0.3125f}};
            case Facing::West:  return {AABB{o.x + 0.6875f, o.y + 0.1875f, o.z + 0.375f, o.x + 1.0f, o.y + 0.8125f, o.z + 0.625f}};
            case Facing::East:  return {AABB{o.x, o.y + 0.1875f, o.z + 0.375f, o.x + 0.3125f, o.y + 0.8125f, o.z + 0.625f}};
            default:            return {AABB{o.x + 0.40625f, o.y, o.z + 0.40625f, o.x + 0.59375f, o.y + 0.625f, o.z + 0.59375f}};
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

    if (endsWith(n, "_button") || n == "lever") {
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

    if (endsWith(n, "wall_sign")) {
        switch (orient.facing) {
            case Facing::North: return {AABB{o.x, o.y + 0.25f, o.z + 0.875f, o.x + 1.0f, o.y + 0.78125f, o.z + 1.0f}};
            case Facing::South: return {AABB{o.x, o.y + 0.25f, o.z, o.x + 1.0f, o.y + 0.78125f, o.z + 0.125f}};
            case Facing::West:  return {AABB{o.x + 0.875f, o.y + 0.25f, o.z, o.x + 1.0f, o.y + 0.78125f, o.z + 1.0f}};
            case Facing::East:
            default:            return {AABB{o.x, o.y + 0.25f, o.z, o.x + 0.125f, o.y + 0.78125f, o.z + 1.0f}};
        }
    }

    if (n == "standing_sign" || endsWith(n, "_standing_sign")) {
        return {AABB{o.x + 0.25f, o.y, o.z + 0.25f, o.x + 0.75f, o.y + 1.0f, o.z + 0.75f}};
    }

    if (n == "chain" || n == "end_rod" || n == "lightning_rod") {
        switch (orient.pillarAxis) {
            case 1:  return {AABB{o.x, o.y + 0.375f, o.z + 0.375f, o.x + 1.0f, o.y + 0.625f, o.z + 0.625f}};
            case 2:  return {AABB{o.x + 0.375f, o.y + 0.375f, o.z, o.x + 0.625f, o.y + 0.625f, o.z + 1.0f}};
            default: return {AABB{o.x + 0.375f, o.y, o.z + 0.375f, o.x + 0.625f, o.y + 1.0f, o.z + 0.625f}};
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

    return {AABB::unitAt(pos)};
}

} // namespace buildghost
