#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace placepreview {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;
inline constexpr float kDegToRad = kPi / 180.0f;

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() noexcept = default;
    constexpr Vec3(float px, float py, float pz) noexcept : x(px), y(py), z(pz) {}

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }
    [[nodiscard]] constexpr Vec3 operator-(const Vec3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }
    [[nodiscard]] constexpr Vec3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }
    [[nodiscard]] constexpr float dot(const Vec3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }
    [[nodiscard]] constexpr Vec3 cross(const Vec3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    [[nodiscard]] float lengthSq() const noexcept {
        return x * x + y * y + z * z;
    }
    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSq());
    }
    [[nodiscard]] Vec3 normalizedOr(const Vec3& fallback) const noexcept {
        const float lenSq = lengthSq();
        if (lenSq < 1e-10f) return fallback;
        const float inv = 1.0f / std::sqrt(lenSq);
        return {x * inv, y * inv, z * inv};
    }
};

struct BlockPos {
    int x{0};
    int y{0};
    int z{0};

    constexpr BlockPos() noexcept = default;
    constexpr BlockPos(int px, int py, int pz) noexcept : x(px), y(py), z(pz) {}

    [[nodiscard]] constexpr bool operator==(const BlockPos& o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
    [[nodiscard]] constexpr bool operator!=(const BlockPos& o) const noexcept {
        return !(*this == o);
    }
    [[nodiscard]] constexpr BlockPos offset(int dx, int dy, int dz) const noexcept {
        return {x + dx, y + dy, z + dz};
    }
    [[nodiscard]] constexpr Vec3 originVec() const noexcept {
        return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    }
    [[nodiscard]] constexpr Vec3 centerVec() const noexcept {
        return {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f};
    }
};

struct AABB {
    Vec3 min{0.0f, 0.0f, 0.0f};
    Vec3 max{1.0f, 1.0f, 1.0f};

    constexpr AABB() noexcept = default;
    constexpr AABB(const Vec3& pMin, const Vec3& pMax) noexcept : min(pMin), max(pMax) {}
    constexpr AABB(float x0, float y0, float z0, float x1, float y1, float z1) noexcept
        : min(x0, y0, z0), max(x1, y1, z1) {}

    [[nodiscard]] constexpr AABB normalized() const noexcept {
        return {
            {std::min(min.x, max.x), std::min(min.y, max.y), std::min(min.z, max.z)},
            {std::max(min.x, max.x), std::max(min.y, max.y), std::max(min.z, max.z)}
        };
    }

    [[nodiscard]] constexpr AABB translated(const Vec3& d) const noexcept {
        return {min + d, max + d};
    }

    [[nodiscard]] constexpr AABB inflated(float e) const noexcept {
        return {
            {min.x - e, min.y - e, min.z - e},
            {max.x + e, max.y + e, max.z + e}
        };
    }

    [[nodiscard]] constexpr float sizeX() const noexcept { return max.x - min.x; }
    [[nodiscard]] constexpr float sizeY() const noexcept { return max.y - min.y; }
    [[nodiscard]] constexpr float sizeZ() const noexcept { return max.z - min.z; }

    [[nodiscard]] constexpr Vec3 center() const noexcept {
        return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
    }

    [[nodiscard]] constexpr Vec3 halfExtents() const noexcept {
        return {sizeX() * 0.5f, sizeY() * 0.5f, sizeZ() * 0.5f};
    }

    [[nodiscard]] constexpr bool isDegenerate(float eps = 1e-4f) const noexcept {
        return sizeX() <= eps || sizeY() <= eps || sizeZ() <= eps;
    }

    [[nodiscard]] constexpr float volume() const noexcept {
        if (isDegenerate(0.0f)) return 0.0f;
        return sizeX() * sizeY() * sizeZ();
    }

    [[nodiscard]] constexpr bool intersects(const AABB& o, float eps = 1e-4f) const noexcept {
        return (min.x + eps < o.max.x) && (max.x - eps > o.min.x) &&
               (min.y + eps < o.max.y) && (max.y - eps > o.min.y) &&
               (min.z + eps < o.max.z) && (max.z - eps > o.min.z);
    }

    [[nodiscard]] static constexpr AABB unitAt(const BlockPos& pos) noexcept {
        const Vec3 o = pos.originVec();
        return {o, {o.x + 1.0f, o.y + 1.0f, o.z + 1.0f}};
    }
};

// Right-handed 3D Oriented Box (`det([axisU, axisV, axisW]) == +1`) enabling exact 3D rotation
// for 16-angle standing signs, hanging signs, standing banners, skulls/heads, angled wall
// torches, levers, slanted lectern tops, and smooth continuous block-to-block animation.
struct OrientedBox {
    Vec3 center{0.5f, 0.5f, 0.5f};
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    Vec3 axisU{1.0f, 0.0f, 0.0f};
    Vec3 axisV{0.0f, 1.0f, 0.0f};
    Vec3 axisW{0.0f, 0.0f, 1.0f};
    float yawRad{0.0f};
    bool axisAligned{true};

    [[nodiscard]] bool isDegenerate(float eps = 1e-4f) const noexcept {
        return halfExtents.x <= eps || halfExtents.y <= eps || halfExtents.z <= eps;
    }

    [[nodiscard]] AABB enclosingAABB() const noexcept {
        const float ex = std::fabs(axisU.x) * halfExtents.x +
                         std::fabs(axisV.x) * halfExtents.y +
                         std::fabs(axisW.x) * halfExtents.z;
        const float ey = std::fabs(axisU.y) * halfExtents.x +
                         std::fabs(axisV.y) * halfExtents.y +
                         std::fabs(axisW.y) * halfExtents.z;
        const float ez = std::fabs(axisU.z) * halfExtents.x +
                         std::fabs(axisV.z) * halfExtents.y +
                         std::fabs(axisW.z) * halfExtents.z;
        return AABB{
            {center.x - ex, center.y - ey, center.z - ez},
            {center.x + ex, center.y + ey, center.z + ez}
        };
    }

    [[nodiscard]] static OrientedBox fromAABB(const AABB& raw) noexcept {
        const AABB b = raw.normalized();
        OrientedBox obb{};
        obb.center = b.center();
        obb.halfExtents = b.halfExtents();
        obb.axisU = {1.0f, 0.0f, 0.0f};
        obb.axisV = {0.0f, 1.0f, 0.0f};
        obb.axisW = {0.0f, 0.0f, 1.0f};
        obb.yawRad = 0.0f;
        obb.axisAligned = true;
        return obb;
    }

    // Rotates `localOffset` around the cell vertical pivot `(cellOrigin.x + 0.5, y, cellOrigin.z + 0.5)`
    // using right-handed basis U=(cos, 0, sin), V=(0, 1, 0), W=(-sin, 0, cos) so that at `yawRad`:
    //   - `U` spans the horizontal board width perpendicular to player gaze `(-sin, 0, +cos)`
    //   - `-W` points directly back toward the player!
    [[nodiscard]] static OrientedBox fromCellYaw(
        const Vec3& cellOrigin,
        const Vec3& localOffsetFromPivot,
        const Vec3& halfExt,
        float angleRad) noexcept {

        const float c = std::cos(angleRad);
        const float s = std::sin(angleRad);
        const Vec3 u{c, 0.0f, s};
        const Vec3 v{0.0f, 1.0f, 0.0f};
        const Vec3 w{-s, 0.0f, c};
        const Vec3 pivot{cellOrigin.x + 0.5f, cellOrigin.y, cellOrigin.z + 0.5f};

        OrientedBox obb{};
        obb.center = pivot + u * localOffsetFromPivot.x + v * localOffsetFromPivot.y + w * localOffsetFromPivot.z;
        obb.halfExtents = halfExt;
        obb.axisU = u;
        obb.axisV = v;
        obb.axisW = w;
        obb.yawRad = angleRad;
        obb.axisAligned = false;
        return obb;
    }
};

struct RgbaColor {
    uint8_t r{255};
    uint8_t g{255};
    uint8_t b{255};
    uint8_t a{255};

    constexpr RgbaColor() noexcept = default;
    constexpr RgbaColor(uint8_t pr, uint8_t pg, uint8_t pb, uint8_t pa = 255) noexcept
        : r(pr), g(pg), b(pb), a(pa) {}

    [[nodiscard]] constexpr uint32_t packAbgr() const noexcept {
        return static_cast<uint32_t>(r) |
               (static_cast<uint32_t>(g) << 8) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(a) << 24);
    }

    [[nodiscard]] RgbaColor shaded(float light, float alphaScale = 1.0f) const noexcept {
        const float l = std::clamp(light, 0.0f, 1.5f);
        const float af = std::clamp(static_cast<float>(a) * alphaScale, 0.0f, 255.0f);
        return {
            static_cast<uint8_t>(std::clamp(std::lround(static_cast<float>(r) * l), 0L, 255L)),
            static_cast<uint8_t>(std::clamp(std::lround(static_cast<float>(g) * l), 0L, 255L)),
            static_cast<uint8_t>(std::clamp(std::lround(static_cast<float>(b) * l), 0L, 255L)),
            static_cast<uint8_t>(std::lround(af))
        };
    }

    [[nodiscard]] static RgbaColor withFloatAlpha(uint8_t pr, uint8_t pg, uint8_t pb, float alpha01) noexcept {
        const float clamped = std::clamp(alpha01, 0.0f, 1.0f);
        return {pr, pg, pb, static_cast<uint8_t>(std::lround(clamped * 255.0f))};
    }
};

// Verified against Minecraft Bedrock 1.26.52.03 DebugRenderer::renderEnd (0x0b25de34):
// BOTH mLineVertices (+0x50, LineList mode=4, 2 vertices/line) AND mTriangleVertices (+0x68,
// TriangleList mode=2, 3 vertices/triangle) use 16-byte world-space PackedVertex elements.
struct PackedVertex {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    uint32_t abgr{0};
};
static_assert(sizeof(PackedVertex) == 16, "PackedVertex ABI must match 16-byte stride in DebugRenderer");

struct DebugLineVertex {
    float x1{0.0f};
    float y1{0.0f};
    float z1{0.0f};
    uint32_t color1{0};
    float x2{0.0f};
    float y2{0.0f};
    float z2{0.0f};
    uint32_t color2{0};
    uint32_t flags{0};
};
static_assert(sizeof(DebugLineVertex) == 36, "DebugLineVertex descriptor size");

struct DebugTriangleVertex {
    float x1{0.0f};
    float y1{0.0f};
    float z1{0.0f};
    uint32_t color1{0};
    float x2{0.0f};
    float y2{0.0f};
    float z2{0.0f};
    uint32_t color2{0};
    float x3{0.0f};
    float y3{0.0f};
    float z3{0.0f};
    uint32_t color3{0};
    uint32_t flags{0};
};
static_assert(sizeof(DebugTriangleVertex) == 52, "DebugTriangleVertex descriptor size");

// Exact Minecraft Bedrock Facing::Name enum values (0..5):
enum class Facing : uint8_t {
    Down  = 0, // -Y
    Up    = 1, // +Y
    North = 2, // -Z
    South = 3, // +Z
    West  = 4, // -X
    East  = 5  // +X
};

[[nodiscard]] constexpr Facing oppositeFacing(Facing f) noexcept {
    switch (f) {
        case Facing::Down:  return Facing::Up;
        case Facing::Up:    return Facing::Down;
        case Facing::North: return Facing::South;
        case Facing::South: return Facing::North;
        case Facing::West:  return Facing::East;
        case Facing::East:  return Facing::West;
    }
    return Facing::North;
}

[[nodiscard]] constexpr BlockPos facingStep(Facing f) noexcept {
    switch (f) {
        case Facing::Down:  return {0, -1, 0};
        case Facing::Up:    return {0,  1, 0};
        case Facing::North: return {0,  0, -1};
        case Facing::South: return {0,  0,  1};
        case Facing::West:  return {-1, 0,  0};
        case Facing::East:  return { 1, 0,  0};
    }
    return {0, 0, 0};
}

[[nodiscard]] constexpr Vec3 facingNormal(Facing f) noexcept {
    switch (f) {
        case Facing::Down:  return {0.0f, -1.0f, 0.0f};
        case Facing::Up:    return {0.0f,  1.0f, 0.0f};
        case Facing::North: return {0.0f,  0.0f, -1.0f};
        case Facing::South: return {0.0f,  0.0f,  1.0f};
        case Facing::West:  return {-1.0f, 0.0f,  0.0f};
        case Facing::East:  return { 1.0f, 0.0f,  0.0f};
    }
    return {0.0f, 1.0f, 0.0f};
}

[[nodiscard]] constexpr int facingToPillarAxis(Facing f) noexcept {
    switch (f) {
        case Facing::Down:
        case Facing::Up:    return 0; // Y
        case Facing::West:
        case Facing::East:  return 1; // X
        case Facing::North:
        case Facing::South: return 2; // Z
    }
    return 0;
}

// Minecraft Bedrock yaw convention:
//   0 deg = +Z (South), 90 deg = -X (West), 180/-180 deg = -Z (North), -90/270 deg = +X (East).
[[nodiscard]] inline Facing yawToPlayerLookHorizontal(float yawDeg) noexcept {
    float norm = std::fmod(yawDeg, 360.0f);
    if (norm < 0.0f) norm += 360.0f;
    const int quadrant = static_cast<int>(std::floor(norm / 90.0f + 0.5f)) & 3;
    switch (quadrant) {
        case 0: return Facing::South; // +Z
        case 1: return Facing::West;  // -X
        case 2: return Facing::North; // -Z
        case 3: return Facing::East;  // +X
    }
    return Facing::South;
}

// Exact 16-angle ground_sign_direction index (0..15) & world rotation angle (radians):
// Matches SignItem::_useOn (0x0fefd74c): `floor((actorYaw + 180) * 16 / 360 + 0.5) & 15`.
[[nodiscard]] inline int yawToGroundSignDirection16(float yawDeg) noexcept {
    const float scaled = (yawDeg + 180.0f) * (16.0f / 360.0f);
    return static_cast<int>(std::floor(scaled + 0.5f)) & 15;
}

[[nodiscard]] inline float groundSignDirectionToYawRad(int dir16) noexcept {
    return static_cast<float>((dir16 & 15) - 8) * (kTwoPi / 16.0f);
}

[[nodiscard]] inline float cardinalFacingToYawRad(Facing frontFacingPlayer) noexcept {
    // Basis U=(cos,0,sin), -W=(sin,0,-cos) faces `frontFacingPlayer`:
    switch (frontFacingPlayer) {
        case Facing::North: return 0.0f;            // -W = (0, 0, -1)
        case Facing::East:  return kPi * 0.5f;      // -W = (+1, 0, 0)
        case Facing::South: return kPi;             // -W = (0, 0, +1)
        case Facing::West:  return -kPi * 0.5f;     // -W = (-1, 0, 0)
        default:            return 0.0f;
    }
}

[[nodiscard]] inline Facing pitchYawToSixWayLook(float pitchDeg, float yawDeg) noexcept {
    if (pitchDeg > 60.0f) {
        return Facing::Down;
    }
    if (pitchDeg < -60.0f) {
        return Facing::Up;
    }
    return yawToPlayerLookHorizontal(yawDeg);
}

enum class OrientationKind : uint8_t {
    None = 0,
    PillarAxis, // Logs, stems, pillars, basalt, hay, bone, chains, deepslate (axis: 0=Y, 1=X, 2=Z)
    SixWay,     // Pistons, observers, dispensers, droppers, hoppers, barrels, rods (facing: Down..East)
    Horizontal, // Stairs, furnaces, chests, lecterns, looms, anvils, doors, beds, gates, shulkers
    SlabHalf,   // Slabs & vertical half merges
    Sign16      // 16-angle standing signs, hanging signs, banners, and skulls/heads
};

struct BlockStateProperty {
    std::string name;
    int value{0};
};

struct PlacementPreviewState {
    bool valid{false};

    BlockPos primaryPos{};
    bool hasSecondaryCell{false};
    BlockPos secondaryPos{};

    Facing hitFace{Facing::Up};
    Vec3 subClick{0.5f, 0.5f, 0.5f};

    OrientationKind orientationKind{OrientationKind::None};
    int pillarAxis{0};              // 0 = Y, 1 = X, 2 = Z
    Facing facing{Facing::North};   // Primary facing direction (front / output / stair ascend)
    int rotation16{8};              // 0..15 ground_sign_direction
    float yawRadians{0.0f};         // Exact 3D world rotation angle for OBB parts
    bool topHalf{false};            // Top slab / upside-down stair or trapdoor
    bool hingeRight{false};         // Door hinge side
    bool isSlabMerge{false};        // Merging single slab into existing half-slab -> double slab
    bool isMultiBlock{false};       // 2-block Door or Bed preview

    // Smooth transition opacity multiplier in [0..1] driven by PreviewAnimator.
    float visibilityAlpha{1.0f};

    std::string blockIdentifier;
    uint16_t stateData{0};
    int auxData{0};
    RgbaColor materialColor{255, 255, 255, 255};

    // World-space axis-aligned boxes (used for raycast, player AABB collision & AABB coplanar union).
    std::vector<AABB> worldBoxes;

    // Optional true 3D oriented boxes (e.g. 16-angle standing signs, hanging signs, banners,
    // skulls, tilted wall torches, slanted lectern bookstand, or animated glide frames).
    // When non-empty, GhostMesher renders these exact 3D OBB parts directly!
    std::vector<OrientedBox> orientedBoxes;

    // Optional full-cell reference outline when merging two half-slabs.
    bool hasMergeOutlineBox{false};
    AABB mergeOutlineBox{};

    [[nodiscard]] bool matchesTargetAndOrientation(const PlacementPreviewState& o) const noexcept {
        return valid == o.valid &&
               primaryPos == o.primaryPos &&
               hasSecondaryCell == o.hasSecondaryCell &&
               secondaryPos == o.secondaryPos &&
               orientationKind == o.orientationKind &&
               pillarAxis == o.pillarAxis &&
               facing == o.facing &&
               rotation16 == o.rotation16 &&
               topHalf == o.topHalf &&
               hingeRight == o.hingeRight &&
               isSlabMerge == o.isSlabMerge &&
               blockIdentifier == o.blockIdentifier &&
               stateData == o.stateData &&
               auxData == o.auxData &&
               worldBoxes.size() == o.worldBoxes.size() &&
               orientedBoxes.size() == o.orientedBoxes.size();
    }
};

} // namespace placepreview
