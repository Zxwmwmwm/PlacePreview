#include "buildghost/GhostMesher.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace buildghost {

namespace {

constexpr float kSurfaceExpand = 0.0028f;
constexpr float kGuideLift = 0.0065f;
constexpr float kCoplanarEps = 1.5e-3f;

struct Rect2D {
    float u0{0.0f};
    float v0{0.0f};
    float u1{0.0f};
    float v1{0.0f};

    [[nodiscard]] bool empty(float eps = 1e-4f) const noexcept {
        return (u1 - u0) <= eps || (v1 - v0) <= eps;
    }
};

// Subtracts `clip` from `src` in 2D, appending up to 4 non-overlapping remainder rectangles.
// Eliminates internal coplanar faces between touching multi-box sub-volumes (Stairs, Corner
// Stairs, Walls, Fences, Hoppers, Beds) so translucent faces NEVER double-blend at internal seams!
void subtractRect2D(const Rect2D& src, const Rect2D& clip, std::vector<Rect2D>& out) {
    const float iu0 = std::max(src.u0, clip.u0);
    const float iv0 = std::max(src.v0, clip.v0);
    const float iu1 = std::min(src.u1, clip.u1);
    const float iv1 = std::min(src.v1, clip.v1);

    if (iu1 - iu0 <= 1e-4f || iv1 - iv0 <= 1e-4f) {
        out.push_back(src);
        return;
    }

    // Left strip
    if (iu0 - src.u0 > 1e-4f) {
        out.push_back({src.u0, src.v0, iu0, src.v1});
    }
    // Right strip
    if (src.u1 - iu1 > 1e-4f) {
        out.push_back({iu1, src.v0, src.u1, src.v1});
    }
    // Bottom middle strip
    if (iv0 - src.v0 > 1e-4f) {
        out.push_back({iu0, src.v0, iu1, iv0});
    }
    // Top middle strip
    if (src.v1 - iv1 > 1e-4f) {
        out.push_back({iu0, iv1, iu1, src.v1});
    }
}

uint32_t scalePackedAbgrAlpha(uint32_t abgr, float alphaFactor) noexcept {
    const uint32_t rgb = abgr & 0x00FFFFFFu;
    const float a = static_cast<float>((abgr >> 24u) & 0xFFu) * alphaFactor;
    const uint32_t newA = static_cast<uint32_t>(std::clamp(std::lround(a), 0L, 255L));
    return rgb | (newA << 24u);
}

void pushLine(
    GhostMeshOutput& out,
    const Vec3& camPos,
    const Vec3& aWorld,
    const Vec3& bWorld,
    uint32_t abgr) {

    // 1. Exact world-space 16-byte PackedVertex pair for DebugRenderer::renderEnd (+0x50, mode=4).
    out.packedLines.push_back(PackedVertex{aWorld.x, aWorld.y, aWorld.z, abgr});
    out.packedLines.push_back(PackedVertex{bWorld.x, bWorld.y, bWorld.z, abgr});

    // 2. Host inspection descriptor.
    const Vec3 a = aWorld - camPos;
    const Vec3 b = bWorld - camPos;
    out.lines.push_back(DebugLineVertex{
        a.x, a.y, a.z, abgr,
        b.x, b.y, b.z, abgr,
        0u
    });
}

void pushQuadTwoSided(
    GhostMeshOutput& out,
    const Vec3& camPos,
    const Vec3& p0World,
    const Vec3& p1World,
    const Vec3& p2World,
    const Vec3& p3World,
    uint32_t abgr) {

    // Compute unit normal of (p1 - p0) x (p2 - p0) so interior reverse-wound faces are inset
    // slightly (0.0014m) with softer depth alpha — guaranteeing ZERO coplanar Z-fighting whether
    // the active material culls backfaces or renders double-sided!
    const Vec3 e1 = p1World - p0World;
    const Vec3 e2 = p2World - p0World;
    Vec3 nrm{
        e1.y * e2.z - e1.z * e2.y,
        e1.z * e2.x - e1.x * e2.z,
        e1.x * e2.y - e1.y * e2.x
    };
    const float nLen = nrm.length();
    const Vec3 inset = (nLen > 1e-6f) ? (nrm * (-0.0014f / nLen)) : Vec3{};
    const Vec3 b0World = p0World + inset;
    const Vec3 b1World = p1World + inset;
    const Vec3 b2World = p2World + inset;
    const Vec3 b3World = p3World + inset;
    const uint32_t backAbgr = scalePackedAbgrAlpha(abgr, 0.45f);

    // World-space 16-byte PackedVertex triangles for DebugRenderer::renderEnd (+0x68, mode=2):
    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p1World.x, p1World.y, p1World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, abgr});

    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p3World.x, p3World.y, p3World.z, abgr});

    out.packedTriangles.push_back(PackedVertex{b0World.x, b0World.y, b0World.z, backAbgr});
    out.packedTriangles.push_back(PackedVertex{b2World.x, b2World.y, b2World.z, backAbgr});
    out.packedTriangles.push_back(PackedVertex{b1World.x, b1World.y, b1World.z, backAbgr});

    out.packedTriangles.push_back(PackedVertex{b0World.x, b0World.y, b0World.z, backAbgr});
    out.packedTriangles.push_back(PackedVertex{b3World.x, b3World.y, b3World.z, backAbgr});
    out.packedTriangles.push_back(PackedVertex{b2World.x, b2World.y, b2World.z, backAbgr});

    // Host camera-relative descriptors:
    const Vec3 p0 = p0World - camPos;
    const Vec3 p1 = p1World - camPos;
    const Vec3 p2 = p2World - camPos;
    const Vec3 p3 = p3World - camPos;
    const Vec3 b0 = b0World - camPos;
    const Vec3 b1 = b1World - camPos;
    const Vec3 b2 = b2World - camPos;
    const Vec3 b3 = b3World - camPos;

    out.triangles.push_back(DebugTriangleVertex{
        p0.x, p0.y, p0.z, abgr,
        p1.x, p1.y, p1.z, abgr,
        p2.x, p2.y, p2.z, abgr,
        0u
    });
    out.triangles.push_back(DebugTriangleVertex{
        p0.x, p0.y, p0.z, abgr,
        p2.x, p2.y, p2.z, abgr,
        p3.x, p3.y, p3.z, abgr,
        0u
    });
    out.triangles.push_back(DebugTriangleVertex{
        b0.x, b0.y, b0.z, backAbgr,
        b2.x, b2.y, b2.z, backAbgr,
        b1.x, b1.y, b1.z, backAbgr,
        0u
    });
    out.triangles.push_back(DebugTriangleVertex{
        b0.x, b0.y, b0.z, backAbgr,
        b3.x, b3.y, b3.z, backAbgr,
        b2.x, b2.y, b2.z, backAbgr,
        0u
    });
}

void pushTriTwoSided(
    GhostMeshOutput& out,
    const Vec3& camPos,
    const Vec3& p0World,
    const Vec3& p1World,
    const Vec3& p2World,
    uint32_t abgr) {

    const uint32_t backAbgr = scalePackedAbgrAlpha(abgr, 0.55f);

    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p1World.x, p1World.y, p1World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, abgr});

    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, backAbgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, backAbgr});
    out.packedTriangles.push_back(PackedVertex{p1World.x, p1World.y, p1World.z, backAbgr});

    const Vec3 p0 = p0World - camPos;
    const Vec3 p1 = p1World - camPos;
    const Vec3 p2 = p2World - camPos;

    out.triangles.push_back(DebugTriangleVertex{
        p0.x, p0.y, p0.z, abgr,
        p1.x, p1.y, p1.z, abgr,
        p2.x, p2.y, p2.z, abgr,
        0u
    });
    out.triangles.push_back(DebugTriangleVertex{
        p0.x, p0.y, p0.z, backAbgr,
        p2.x, p2.y, p2.z, backAbgr,
        p1.x, p1.y, p1.z, backAbgr,
        0u
    });
}

float facePlaneCoord(const AABB& b, Facing f) noexcept {
    switch (f) {
        case Facing::Down:  return b.min.y;
        case Facing::Up:    return b.max.y;
        case Facing::North: return b.min.z;
        case Facing::South: return b.max.z;
        case Facing::West:  return b.min.x;
        case Facing::East:  return b.max.x;
    }
    return b.max.y;
}

Rect2D faceRect2D(const AABB& b, Facing f) noexcept {
    switch (f) {
        case Facing::Down:
        case Facing::Up:    return {b.min.x, b.min.z, b.max.x, b.max.z};
        case Facing::North:
        case Facing::South: return {b.min.x, b.min.y, b.max.x, b.max.y};
        case Facing::West:
        case Facing::East:  return {b.min.z, b.min.y, b.max.z, b.max.y};
    }
    return {};
}

float faceLightFactor(Facing f, bool directionalShading) noexcept {
    if (!directionalShading) return 1.0f;
    switch (f) {
        case Facing::Up:    return 1.00f;
        case Facing::North:
        case Facing::South: return 0.84f;
        case Facing::West:
        case Facing::East:  return 0.70f;
        case Facing::Down:  return 0.56f;
    }
    return 1.0f;
}

void emitFaceRectQuad(
    GhostMeshOutput& out,
    const Vec3& camPos,
    Facing f,
    float plane,
    const Rect2D& r,
    uint32_t fillAbgr,
    bool emitFill,
    uint32_t wireAbgr,
    bool emitWire) {

    const Vec3 n = facingNormal(f) * kSurfaceExpand;
    Vec3 c0{}, c1{}, c2{}, c3{};

    switch (f) {
        case Facing::Up:
            c0 = Vec3{r.u0, plane, r.v0} + n;
            c1 = Vec3{r.u0, plane, r.v1} + n;
            c2 = Vec3{r.u1, plane, r.v1} + n;
            c3 = Vec3{r.u1, plane, r.v0} + n;
            break;
        case Facing::Down:
            c0 = Vec3{r.u0, plane, r.v0} + n;
            c1 = Vec3{r.u1, plane, r.v0} + n;
            c2 = Vec3{r.u1, plane, r.v1} + n;
            c3 = Vec3{r.u0, plane, r.v1} + n;
            break;
        case Facing::South:
            c0 = Vec3{r.u0, r.v0, plane} + n;
            c1 = Vec3{r.u1, r.v0, plane} + n;
            c2 = Vec3{r.u1, r.v1, plane} + n;
            c3 = Vec3{r.u0, r.v1, plane} + n;
            break;
        case Facing::North:
            c0 = Vec3{r.u0, r.v1, plane} + n;
            c1 = Vec3{r.u1, r.v1, plane} + n;
            c2 = Vec3{r.u1, r.v0, plane} + n;
            c3 = Vec3{r.u0, r.v0, plane} + n;
            break;
        case Facing::East:
            c0 = Vec3{plane, r.v0, r.u1} + n;
            c1 = Vec3{plane, r.v0, r.u0} + n;
            c2 = Vec3{plane, r.v1, r.u0} + n;
            c3 = Vec3{plane, r.v1, r.u1} + n;
            break;
        case Facing::West:
            c0 = Vec3{plane, r.v0, r.u0} + n;
            c1 = Vec3{plane, r.v0, r.u1} + n;
            c2 = Vec3{plane, r.v1, r.u1} + n;
            c3 = Vec3{plane, r.v1, r.u0} + n;
            break;
    }

    if (emitFill) {
        pushQuadTwoSided(out, camPos, c0, c1, c2, c3, fillAbgr);
    }
    if (emitWire) {
        pushLine(out, camPos, c0, c1, wireAbgr);
        pushLine(out, camPos, c1, c2, wireAbgr);
        pushLine(out, camPos, c2, c3, wireAbgr);
        pushLine(out, camPos, c3, c0, wireAbgr);
    }
}

AABB computeUnionBounds(const std::vector<AABB>& boxes) noexcept {
    AABB u = boxes.front();
    for (std::size_t i = 1; i < boxes.size(); ++i) {
        u.min.x = std::min(u.min.x, boxes[i].min.x);
        u.min.y = std::min(u.min.y, boxes[i].min.y);
        u.min.z = std::min(u.min.z, boxes[i].min.z);
        u.max.x = std::max(u.max.x, boxes[i].max.x);
        u.max.y = std::max(u.max.y, boxes[i].max.y);
        u.max.z = std::max(u.max.z, boxes[i].max.z);
    }
    return u;
}

// Converts normalized [0..1] face UV coordinates on face `f` of `bounds` into 3D world space.
Vec3 facePointUV(const AABB& bounds, Facing f, float u, float v, float lift = kGuideLift) noexcept {
    const Vec3 n = facingNormal(f) * lift;
    switch (f) {
        case Facing::Up:
            return Vec3{bounds.min.x + bounds.sizeX() * u, bounds.max.y, bounds.min.z + bounds.sizeZ() * v} + n;
        case Facing::Down:
            return Vec3{bounds.min.x + bounds.sizeX() * u, bounds.min.y, bounds.min.z + bounds.sizeZ() * v} + n;
        case Facing::North:
            return Vec3{bounds.min.x + bounds.sizeX() * u, bounds.min.y + bounds.sizeY() * v, bounds.min.z} + n;
        case Facing::South:
            return Vec3{bounds.min.x + bounds.sizeX() * u, bounds.min.y + bounds.sizeY() * v, bounds.max.z} + n;
        case Facing::West:
            return Vec3{bounds.min.x, bounds.min.y + bounds.sizeY() * v, bounds.min.z + bounds.sizeZ() * u} + n;
        case Facing::East:
            return Vec3{bounds.max.x, bounds.min.y + bounds.sizeY() * v, bounds.min.z + bounds.sizeZ() * u} + n;
    }
    return bounds.center();
}

// Draws concentric annual rings on the two cut end-caps + longitudinal bark grain lines on the 4 bark sides!
void emitPillarOrientationGuides(
    GhostMeshOutput& out,
    const Vec3& camPos,
    const AABB& bounds,
    int pillarAxis,
    uint32_t accentLineAbgr,
    uint32_t secondaryLineAbgr) {

    const std::array<Facing, 2> endCaps =
        (pillarAxis == 0) ? std::array<Facing, 2>{Facing::Up, Facing::Down}
      : (pillarAxis == 1) ? std::array<Facing, 2>{Facing::East, Facing::West}
                          : std::array<Facing, 2>{Facing::South, Facing::North};

    // 1. Concentric octagonal end-grain rings on both cut ends.
    for (Facing cap : endCaps) {
        for (float radius : {0.34f, 0.17f}) {
            constexpr int kSegs = 8;
            for (int s = 0; s < kSegs; ++s) {
                const float a0 = static_cast<float>(s) * (6.2831853f / static_cast<float>(kSegs));
                const float a1 = static_cast<float>(s + 1) * (6.2831853f / static_cast<float>(kSegs));
                const Vec3 p0 = facePointUV(bounds, cap, 0.5f + radius * std::cos(a0), 0.5f + radius * std::sin(a0));
                const Vec3 p1 = facePointUV(bounds, cap, 0.5f + radius * std::cos(a1), 0.5f + radius * std::sin(a1));
                pushLine(out, camPos, p0, p1, accentLineAbgr);
            }
        }
    }

    // 2. Longitudinal grain flutes running along `pillarAxis` on all 4 side faces!
    const std::array<Facing, 4> barkSides =
        (pillarAxis == 0) ? std::array<Facing, 4>{Facing::North, Facing::South, Facing::West, Facing::East}
      : (pillarAxis == 1) ? std::array<Facing, 4>{Facing::Up, Facing::Down, Facing::North, Facing::South}
                          : std::array<Facing, 4>{Facing::Up, Facing::Down, Facing::West, Facing::East};

    for (Facing side : barkSides) {
        for (float t : {0.28f, 0.72f}) {
            Vec3 a{}, b{};
            // Map line parallel to the world axis `pillarAxis`:
            if (pillarAxis == 0) {
                // Vertical Y grain on N/S/E/W faces -> v runs [0.10 .. 0.90]
                a = facePointUV(bounds, side, t, 0.10f);
                b = facePointUV(bounds, side, t, 0.90f);
            } else if (pillarAxis == 1) {
                // East-West X grain -> u runs [0.10 .. 0.90] on Up/Down/North/South faces!
                a = facePointUV(bounds, side, 0.10f, t);
                b = facePointUV(bounds, side, 0.90f, t);
            } else {
                // North-South Z grain -> v runs [0.10 .. 0.90] on Up/Down, u runs [0.10 .. 0.90] on West/East!
                if (side == Facing::Up || side == Facing::Down) {
                    a = facePointUV(bounds, side, t, 0.10f);
                    b = facePointUV(bounds, side, t, 0.90f);
                } else {
                    a = facePointUV(bounds, side, 0.10f, t);
                    b = facePointUV(bounds, side, 0.90f, t);
                }
            }
            pushLine(out, camPos, a, b, secondaryLineAbgr);
        }
    }
}

// Draws active-face aperture frame + 3D directional arrow + rear pip on 6-way blocks!
void emitSixWayOrientationGuides(
    GhostMeshOutput& out,
    const Vec3& camPos,
    const AABB& bounds,
    Facing activeFace,
    uint32_t accentLineAbgr,
    uint32_t accentFillAbgr,
    bool emitFill) {

    // 1. Active front/output face beveled aperture square
    const Vec3 b0 = facePointUV(bounds, activeFace, 0.22f, 0.22f);
    const Vec3 b1 = facePointUV(bounds, activeFace, 0.78f, 0.22f);
    const Vec3 b2 = facePointUV(bounds, activeFace, 0.78f, 0.78f);
    const Vec3 b3 = facePointUV(bounds, activeFace, 0.22f, 0.78f);
    pushLine(out, camPos, b0, b1, accentLineAbgr);
    pushLine(out, camPos, b1, b2, accentLineAbgr);
    pushLine(out, camPos, b2, b3, accentLineAbgr);
    pushLine(out, camPos, b3, b0, accentLineAbgr);

    if (emitFill) {
        const Vec3 i0 = facePointUV(bounds, activeFace, 0.34f, 0.34f);
        const Vec3 i1 = facePointUV(bounds, activeFace, 0.66f, 0.34f);
        const Vec3 i2 = facePointUV(bounds, activeFace, 0.66f, 0.66f);
        const Vec3 i3 = facePointUV(bounds, activeFace, 0.34f, 0.66f);
        pushQuadTwoSided(out, camPos, i0, i1, i2, i3, accentFillAbgr);
    }

    // 2. Central 3D axis shaft + outward chevron along `facingNormal(activeFace)`
    const Vec3 center = bounds.center();
    const Vec3 dir = facingNormal(activeFace);
    const float halfSpan = 0.36f * std::min({
        std::max(bounds.sizeX(), 0.25f),
        std::max(bounds.sizeY(), 0.25f),
        std::max(bounds.sizeZ(), 0.25f)
    });
    const Vec3 tail = center - dir * halfSpan;
    const Vec3 tip = facePointUV(bounds, activeFace, 0.50f, 0.50f, 0.045f);
    pushLine(out, camPos, tail, tip, accentLineAbgr);

    const Vec3 wingA = facePointUV(bounds, activeFace, 0.36f, 0.50f, -0.05f);
    const Vec3 wingB = facePointUV(bounds, activeFace, 0.64f, 0.50f, -0.05f);
    const Vec3 wingC = facePointUV(bounds, activeFace, 0.50f, 0.36f, -0.05f);
    const Vec3 wingD = facePointUV(bounds, activeFace, 0.50f, 0.64f, -0.05f);
    pushLine(out, camPos, tip, wingA, accentLineAbgr);
    pushLine(out, camPos, tip, wingB, accentLineAbgr);
    pushLine(out, camPos, tip, wingC, accentLineAbgr);
    pushLine(out, camPos, tip, wingD, accentLineAbgr);
}

// Draws horizontal front-face bezel + top-face directional chevron pointing toward `frontFace`!
void emitHorizontalOrientationGuides(
    GhostMeshOutput& out,
    const Vec3& camPos,
    const AABB& bounds,
    Facing frontFace,
    bool topHalf,
    uint32_t accentLineAbgr,
    uint32_t accentFillAbgr,
    bool emitFill) {

    if (frontFace == Facing::Up || frontFace == Facing::Down) {
        return;
    }

    // 1. Top/bottom horizontal chevron pointing cleanly toward `frontFace`!
    const Facing capFace = topHalf ? Facing::Down : Facing::Up;
    Vec3 tip{}, leftWing{}, rightWing{};
    switch (frontFace) {
        case Facing::North: // -Z -> v = 0.16
            tip       = facePointUV(bounds, capFace, 0.50f, 0.16f);
            leftWing  = facePointUV(bounds, capFace, 0.28f, 0.44f);
            rightWing = facePointUV(bounds, capFace, 0.72f, 0.44f);
            break;
        case Facing::South: // +Z -> v = 0.84
            tip       = facePointUV(bounds, capFace, 0.50f, 0.84f);
            leftWing  = facePointUV(bounds, capFace, 0.72f, 0.56f);
            rightWing = facePointUV(bounds, capFace, 0.28f, 0.56f);
            break;
        case Facing::West:  // -X -> u = 0.16
            tip       = facePointUV(bounds, capFace, 0.16f, 0.50f);
            leftWing  = facePointUV(bounds, capFace, 0.44f, 0.72f);
            rightWing = facePointUV(bounds, capFace, 0.44f, 0.28f);
            break;
        case Facing::East:  // +X -> u = 0.84
        default:
            tip       = facePointUV(bounds, capFace, 0.84f, 0.50f);
            leftWing  = facePointUV(bounds, capFace, 0.56f, 0.28f);
            rightWing = facePointUV(bounds, capFace, 0.56f, 0.72f);
            break;
    }

    pushLine(out, camPos, leftWing, tip, accentLineAbgr);
    pushLine(out, camPos, rightWing, tip, accentLineAbgr);
    if (emitFill) {
        pushTriTwoSided(out, camPos, leftWing, tip, rightWing, accentFillAbgr);
    }

    // 2. Front-face latch / aperture marker on `frontFace`
    const Vec3 f0 = facePointUV(bounds, frontFace, 0.36f, 0.38f);
    const Vec3 f1 = facePointUV(bounds, frontFace, 0.64f, 0.38f);
    const Vec3 f2 = facePointUV(bounds, frontFace, 0.64f, 0.62f);
    const Vec3 f3 = facePointUV(bounds, frontFace, 0.36f, 0.62f);
    pushLine(out, camPos, f0, f1, accentLineAbgr);
    pushLine(out, camPos, f1, f2, accentLineAbgr);
    pushLine(out, camPos, f2, f3, accentLineAbgr);
    pushLine(out, camPos, f3, f0, accentLineAbgr);
}

} // namespace

void GhostMesher::buildMesh(
    const PlacementPreviewState& preview,
    const ModConfig& cfg,
    const Vec3& cameraWorldPos,
    float pulsePhaseRadians,
    GhostMeshOutput& outMesh) {

    outMesh.clear();
    if (!cfg.enabled || !preview.valid || preview.worldBoxes.empty()) {
        return;
    }

    const float pulseFactor = cfg.subtleBreathingPulse
                                  ? (0.90f + 0.10f * std::sin(pulsePhaseRadians))
                                  : 1.0f;

    const bool emitFill = (cfg.renderMode != "Wireframe Only");
    const bool emitWire = (cfg.renderMode != "Translucent Fill");

    const RgbaColor baseRgb = cfg.resolveBaseColor(preview.materialColor);
    const float fillAlpha = std::clamp(cfg.fillOpacity * pulseFactor, 0.04f, 0.90f);
    const float wireAlpha = std::clamp(cfg.wireframeOpacity * (0.94f + 0.06f * pulseFactor), 0.10f, 1.00f);

    static constexpr std::array<Facing, 6> kAllFaces{
        Facing::Down, Facing::Up, Facing::North, Facing::South, Facing::West, Facing::East
    };

    // Reserve approximate vertex capacity up front to avoid reallocations on the Render Thread.
    outMesh.triangles.reserve(preview.worldBoxes.size() * 28);
    outMesh.lines.reserve(preview.worldBoxes.size() * 32 + 32);
    outMesh.packedTriangles.reserve(preview.worldBoxes.size() * 84);
    outMesh.packedLines.reserve(preview.worldBoxes.size() * 64 + 64);

    // 1. Emit coplanar-culled exterior surfaces & crisp edges across ALL world sub-boxes!
    for (std::size_t i = 0; i < preview.worldBoxes.size(); ++i) {
        const AABB box = preview.worldBoxes[i].normalized();
        if (box.isDegenerate()) {
            continue;
        }

        for (Facing face : kAllFaces) {
            const float plane = facePlaneCoord(box, face);
            const Facing opp = oppositeFacing(face);

            std::vector<Rect2D> visibleRects{faceRect2D(box, face)};

            // Clip away coplanar interior contact areas shared with adjacent sub-boxes in this preview!
            for (std::size_t j = 0; j < preview.worldBoxes.size() && !visibleRects.empty(); ++j) {
                if (i == j) continue;
                const AABB other = preview.worldBoxes[j].normalized();
                if (other.isDegenerate()) continue;

                const float otherOppPlane = facePlaneCoord(other, opp);
                if (std::fabs(plane - otherOppPlane) <= kCoplanarEps) {
                    const Rect2D clipRect = faceRect2D(other, opp);
                    std::vector<Rect2D> nextRects;
                    nextRects.reserve(visibleRects.size() * 2);
                    for (const auto& r : visibleRects) {
                        subtractRect2D(r, clipRect, nextRects);
                    }
                    visibleRects = std::move(nextRects);
                }
            }

            if (visibleRects.empty()) {
                continue;
            }

            const float light = faceLightFactor(face, cfg.directionalShading);
            const RgbaColor litBase = baseRgb.shaded(light);
            const RgbaColor shadedFill = RgbaColor::withFloatAlpha(
                litBase.r,
                litBase.g,
                litBase.b,
                fillAlpha);
            const RgbaColor brightEdgeBase = baseRgb.shaded(std::min(1.15f, light + 0.18f));
            const RgbaColor edgeColor = RgbaColor::withFloatAlpha(
                brightEdgeBase.r,
                brightEdgeBase.g,
                brightEdgeBase.b,
                wireAlpha);

            for (const auto& r : visibleRects) {
                if (!r.empty()) {
                    emitFaceRectQuad(
                        outMesh,
                        cameraWorldPos,
                        face,
                        plane,
                        r,
                        shadedFill.packAbgr(),
                        emitFill,
                        edgeColor.packAbgr(),
                        emitWire);
                }
            }
        }
    }

    // 2. When merging a half-slab into a full double-slab block, draw a subtle outer full-block frame!
    if (preview.hasMergeOutlineBox && emitWire) {
        const uint32_t mergeAbgr = RgbaColor::withFloatAlpha(
            255, 255, 255, std::min(1.0f, wireAlpha * 0.45f)).packAbgr();
        for (Facing f : {Facing::Up, Facing::Down}) {
            emitFaceRectQuad(
                outMesh,
                cameraWorldPos,
                f,
                facePlaneCoord(preview.mergeOutlineBox, f),
                faceRect2D(preview.mergeOutlineBox, f),
                0u,
                false,
                mergeAbgr,
                true);
        }
    }

    // 3. Emit 3D Orientation Guides (Pillar end-grain + flutes, 6-Way active aperture + shaft,
    //    4-Way horizontal front latch + top pointer chevron)!
    if (cfg.showOrientationGuides && preview.orientationKind != OrientationKind::None) {
        const AABB bounds = computeUnionBounds(preview.worldBoxes);
        const RgbaColor guideBright = RgbaColor::withFloatAlpha(
            static_cast<uint8_t>(std::min(255, static_cast<int>(baseRgb.r) + 55)),
            static_cast<uint8_t>(std::min(255, static_cast<int>(baseRgb.g) + 55)),
            static_cast<uint8_t>(std::min(255, static_cast<int>(baseRgb.b) + 55)),
            std::min(1.0f, wireAlpha + 0.12f));
        const RgbaColor guideSoft = RgbaColor::withFloatAlpha(
            guideBright.r, guideBright.g, guideBright.b, wireAlpha * 0.62f);
        const RgbaColor guideFill = RgbaColor::withFloatAlpha(
            guideBright.r, guideBright.g, guideBright.b, std::min(0.65f, fillAlpha * 1.45f));

        switch (preview.orientationKind) {
            case OrientationKind::PillarAxis:
                emitPillarOrientationGuides(
                    outMesh, cameraWorldPos, bounds, preview.pillarAxis,
                    guideBright.packAbgr(), guideSoft.packAbgr());
                break;

            case OrientationKind::SixWay:
                emitSixWayOrientationGuides(
                    outMesh, cameraWorldPos, bounds, preview.facing,
                    guideBright.packAbgr(), guideFill.packAbgr(), emitFill);
                break;

            case OrientationKind::Horizontal:
                emitHorizontalOrientationGuides(
                    outMesh, cameraWorldPos, bounds, preview.facing, preview.topHalf,
                    guideBright.packAbgr(), guideFill.packAbgr(), emitFill);
                break;

            case OrientationKind::SlabHalf:
            case OrientationKind::None:
                break;
        }
    }

    // 4. Optional sub-target hit pip on the clicked face when placing slabs/stairs/directional blocks.
    if (cfg.showSubTargetPip &&
        (preview.orientationKind == OrientationKind::SlabHalf ||
         preview.orientationKind == OrientationKind::Horizontal ||
         preview.orientationKind == OrientationKind::SixWay)) {
        const Vec3 origin = preview.primaryPos.originVec();
        const Vec3 pipCenter = origin + preview.subClick;
        const uint32_t pipAbgr = RgbaColor::withFloatAlpha(255, 255, 255, wireAlpha * 0.55f).packAbgr();
        constexpr float kPipHalf = 0.035f;
        pushLine(outMesh, cameraWorldPos,
                 pipCenter - Vec3{kPipHalf, 0.0f, 0.0f},
                 pipCenter + Vec3{kPipHalf, 0.0f, 0.0f}, pipAbgr);
        pushLine(outMesh, cameraWorldPos,
                 pipCenter - Vec3{0.0f, kPipHalf, 0.0f},
                 pipCenter + Vec3{0.0f, kPipHalf, 0.0f}, pipAbgr);
        pushLine(outMesh, cameraWorldPos,
                 pipCenter - Vec3{0.0f, 0.0f, kPipHalf},
                 pipCenter + Vec3{0.0f, 0.0f, kPipHalf}, pipAbgr);
    }
}

} // namespace buildghost
