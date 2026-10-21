#include "placepreview/GhostMesher.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace placepreview {

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

void subtractRect2D(const Rect2D& src, const Rect2D& clip, std::vector<Rect2D>& out) {
    const float iu0 = std::max(src.u0, clip.u0);
    const float iv0 = std::max(src.v0, clip.v0);
    const float iu1 = std::min(src.u1, clip.u1);
    const float iv1 = std::min(src.v1, clip.v1);

    if (iu1 - iu0 <= 1e-4f || iv1 - iv0 <= 1e-4f) {
        out.push_back(src);
        return;
    }

    if (iu0 - src.u0 > 1e-4f) {
        out.push_back({src.u0, src.v0, iu0, src.v1});
    }
    if (src.u1 - iu1 > 1e-4f) {
        out.push_back({iu1, src.v0, src.u1, src.v1});
    }
    if (iv0 - src.v0 > 1e-4f) {
        out.push_back({iu0, src.v0, iu1, iv0});
    }
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

    out.packedLines.push_back(PackedVertex{aWorld.x, aWorld.y, aWorld.z, abgr});
    out.packedLines.push_back(PackedVertex{bWorld.x, bWorld.y, bWorld.z, abgr});

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

    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p1World.x, p1World.y, p1World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, abgr});

    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p3World.x, p3World.y, p3World.z, abgr});

    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p1World.x, p1World.y, p1World.z, abgr});

    out.packedTriangles.push_back(PackedVertex{p0World.x, p0World.y, p0World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p3World.x, p3World.y, p3World.z, abgr});
    out.packedTriangles.push_back(PackedVertex{p2World.x, p2World.y, p2World.z, abgr});

    const Vec3 p0 = p0World - camPos;
    const Vec3 p1 = p1World - camPos;
    const Vec3 p2 = p2World - camPos;
    const Vec3 p3 = p3World - camPos;

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
        p0.x, p0.y, p0.z, abgr,
        p2.x, p2.y, p2.z, abgr,
        p1.x, p1.y, p1.z, abgr,
        0u
    });
    out.triangles.push_back(DebugTriangleVertex{
        p0.x, p0.y, p0.z, abgr,
        p3.x, p3.y, p3.z, abgr,
        p2.x, p2.y, p2.z, abgr,
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

struct EmittedEdge {
    Vec3 a{};
    Vec3 b{};
};

void pushLineDedup(
    GhostMeshOutput& out,
    const Vec3& camPos,
    std::vector<EmittedEdge>& emittedEdges,
    const Vec3& aWorld,
    const Vec3& bWorld,
    uint32_t abgr) {

    constexpr float kDupEpsSq = 2.5e-4f;
    for (const auto& e : emittedEdges) {
        const bool sameDir = (e.a - aWorld).lengthSq() <= kDupEpsSq && (e.b - bWorld).lengthSq() <= kDupEpsSq;
        const bool revDir  = (e.a - bWorld).lengthSq() <= kDupEpsSq && (e.b - aWorld).lengthSq() <= kDupEpsSq;
        if (sameDir || revDir) {
            return;
        }
    }
    emittedEdges.push_back(EmittedEdge{aWorld, bWorld});
    pushLine(out, camPos, aWorld, bWorld, abgr);
}

bool faceVisibleFromCamera(const AABB& b, Facing f, const Vec3& camPos) noexcept {
    switch (f) {
        case Facing::Up:    return camPos.y > b.max.y - 1e-3f;
        case Facing::Down:  return camPos.y < b.min.y + 1e-3f;
        case Facing::South: return camPos.z > b.max.z - 1e-3f;
        case Facing::North: return camPos.z < b.min.z + 1e-3f;
        case Facing::East:  return camPos.x > b.max.x - 1e-3f;
        case Facing::West:  return camPos.x < b.min.x + 1e-3f;
    }
    return true;
}

float faceLightFactor(Facing f, bool directionalShading) noexcept {
    if (!directionalShading) return 1.0f;
    switch (f) {
        case Facing::Up:    return 1.00f;
        case Facing::North:
        case Facing::South: return 0.86f;
        case Facing::West:
        case Facing::East:  return 0.73f;
        case Facing::Down:  return 0.60f;
    }
    return 1.0f;
}

float normalLightFactor(const Vec3& worldNormal, bool directionalShading) noexcept {
    if (!directionalShading) return 1.0f;
    // Matches vanilla Minecraft directional lighting cleanly across any 3D angle:
    // +Y = 1.00, +/-Z = 0.86, +/-X = 0.73, -Y = 0.60.
    const float ny = worldNormal.y;
    const float horizontal = (ny >= 0.0f)
                                 ? (0.86f * worldNormal.z * worldNormal.z +
                                    0.73f * worldNormal.x * worldNormal.x +
                                    1.00f * ny * ny)
                                 : (0.86f * worldNormal.z * worldNormal.z +
                                    0.73f * worldNormal.x * worldNormal.x +
                                    0.60f * ny * ny);
    return std::clamp(horizontal, 0.58f, 1.0f);
}

void emitOrientedBox(
    GhostMeshOutput& out,
    const Vec3& camPos,
    std::vector<EmittedEdge>& emittedEdges,
    const OrientedBox& obb,
    const RgbaColor& baseRgb,
    float fillAlpha,
    float wireAlpha,
    bool directionalShading,
    bool emitFill,
    bool emitWire) {

    if (obb.isDegenerate()) {
        return;
    }

    const Vec3 U = obb.axisU * (obb.halfExtents.x + kSurfaceExpand);
    const Vec3 V = obb.axisV * (obb.halfExtents.y + kSurfaceExpand);
    const Vec3 W = obb.axisW * (obb.halfExtents.z + kSurfaceExpand);
    const Vec3 C = obb.center;

    struct ObbFaceDesc {
        Vec3 normal;
        Vec3 c0;
        Vec3 c1;
        Vec3 c2;
        Vec3 c3;
    };

    // All 6 faces wound right-handed so (c1 - c0) x (c2 - c0) points along `normal`:
    const std::array<ObbFaceDesc, 6> faces{{
        // +V (top)
        {obb.axisV,          C - U + V - W, C - U + V + W, C + U + V + W, C + U + V - W},
        // -V (bottom)
        {obb.axisV * -1.0f,  C - U - V - W, C + U - V - W, C + U - V + W, C - U - V + W},
        // +W (back)
        {obb.axisW,          C - U - V + W, C + U - V + W, C + U + V + W, C - U + V + W},
        // -W (front — faces player on 16-angle signs/banners/skulls!)
        {obb.axisW * -1.0f,  C - U + V - W, C + U + V - W, C + U - V - W, C - U - V - W},
        // +U (right)
        {obb.axisU,          C + U - V + W, C + U - V - W, C + U + V - W, C + U + V + W},
        // -U (left)
        {obb.axisU * -1.0f,  C - U - V - W, C - U - V + W, C - U + V + W, C - U + V - W}
    }};

    bool anyCameraFacing = false;
    std::array<bool, 6> vis{};
    for (std::size_t i = 0; i < 6; ++i) {
        const Vec3 faceCenter = (faces[i].c0 + faces[i].c2) * 0.5f;
        vis[i] = ((camPos - faceCenter).dot(faces[i].normal) > -1e-3f);
        if (vis[i]) {
            anyCameraFacing = true;
        }
    }

    for (std::size_t i = 0; i < 6; ++i) {
        if (anyCameraFacing && !vis[i]) {
            continue;
        }
        const float light = normalLightFactor(faces[i].normal, directionalShading);
        const RgbaColor litBase = baseRgb.shaded(light);
        const uint32_t fillAbgr = RgbaColor::withFloatAlpha(
            litBase.r, litBase.g, litBase.b, fillAlpha).packAbgr();
        const RgbaColor brightEdge = baseRgb.shaded(std::min(1.0f, light + 0.14f));
        const uint32_t wireAbgr = RgbaColor::withFloatAlpha(
            brightEdge.r, brightEdge.g, brightEdge.b, wireAlpha).packAbgr();

        if (emitFill) {
            pushQuadTwoSided(out, camPos, faces[i].c0, faces[i].c1, faces[i].c2, faces[i].c3, fillAbgr);
        }
        if (emitWire) {
            pushLineDedup(out, camPos, emittedEdges, faces[i].c0, faces[i].c1, wireAbgr);
            pushLineDedup(out, camPos, emittedEdges, faces[i].c1, faces[i].c2, wireAbgr);
            pushLineDedup(out, camPos, emittedEdges, faces[i].c2, faces[i].c3, wireAbgr);
            pushLineDedup(out, camPos, emittedEdges, faces[i].c3, faces[i].c0, wireAbgr);
        }
    }
}

void emitFaceRectQuad(
    GhostMeshOutput& out,
    const Vec3& camPos,
    std::vector<EmittedEdge>& emittedEdges,
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
        pushLineDedup(out, camPos, emittedEdges, c0, c1, wireAbgr);
        pushLineDedup(out, camPos, emittedEdges, c1, c2, wireAbgr);
        pushLineDedup(out, camPos, emittedEdges, c2, c3, wireAbgr);
        pushLineDedup(out, camPos, emittedEdges, c3, c0, wireAbgr);
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

    for (Facing cap : endCaps) {
        for (float radius : {0.34f, 0.17f}) {
            constexpr int kSegs = 8;
            for (int s = 0; s < kSegs; ++s) {
                const float a0 = static_cast<float>(s) * (kTwoPi / static_cast<float>(kSegs));
                const float a1 = static_cast<float>(s + 1) * (kTwoPi / static_cast<float>(kSegs));
                const Vec3 p0 = facePointUV(bounds, cap, 0.5f + radius * std::cos(a0), 0.5f + radius * std::sin(a0));
                const Vec3 p1 = facePointUV(bounds, cap, 0.5f + radius * std::cos(a1), 0.5f + radius * std::sin(a1));
                pushLine(out, camPos, p0, p1, accentLineAbgr);
            }
        }
    }

    const std::array<Facing, 4> barkSides =
        (pillarAxis == 0) ? std::array<Facing, 4>{Facing::North, Facing::South, Facing::West, Facing::East}
      : (pillarAxis == 1) ? std::array<Facing, 4>{Facing::Up, Facing::Down, Facing::North, Facing::South}
                          : std::array<Facing, 4>{Facing::Up, Facing::Down, Facing::West, Facing::East};

    for (Facing side : barkSides) {
        for (float t : {0.28f, 0.72f}) {
            Vec3 a{}, b{};
            if (pillarAxis == 0) {
                a = facePointUV(bounds, side, t, 0.10f);
                b = facePointUV(bounds, side, t, 0.90f);
            } else if (pillarAxis == 1) {
                a = facePointUV(bounds, side, 0.10f, t);
                b = facePointUV(bounds, side, 0.90f, t);
            } else {
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

void emitSixWayOrientationGuides(
    GhostMeshOutput& out,
    const Vec3& camPos,
    const AABB& bounds,
    Facing activeFace,
    uint32_t accentLineAbgr,
    uint32_t accentFillAbgr,
    bool emitFill) {

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
}

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

    const Facing capFace = topHalf ? Facing::Down : Facing::Up;
    Vec3 tip{}, leftWing{}, rightWing{};
    switch (frontFace) {
        case Facing::North:
            tip       = facePointUV(bounds, capFace, 0.50f, 0.16f);
            leftWing  = facePointUV(bounds, capFace, 0.28f, 0.44f);
            rightWing = facePointUV(bounds, capFace, 0.72f, 0.44f);
            break;
        case Facing::South:
            tip       = facePointUV(bounds, capFace, 0.50f, 0.84f);
            leftWing  = facePointUV(bounds, capFace, 0.72f, 0.56f);
            rightWing = facePointUV(bounds, capFace, 0.28f, 0.56f);
            break;
        case Facing::West:
            tip       = facePointUV(bounds, capFace, 0.16f, 0.50f);
            leftWing  = facePointUV(bounds, capFace, 0.44f, 0.72f);
            rightWing = facePointUV(bounds, capFace, 0.44f, 0.28f);
            break;
        case Facing::East:
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
}

} // namespace

void GhostMesher::buildMesh(
    const PlacementPreviewState& preview,
    const ModConfig& cfg,
    const Vec3& cameraWorldPos,
    float pulsePhaseRadians,
    GhostMeshOutput& outMesh) {

    outMesh.clear();
    if (!cfg.enabled || !preview.valid ||
        (preview.worldBoxes.empty() && preview.orientedBoxes.empty()) ||
        preview.visibilityAlpha <= 0.01f) {
        return;
    }

    const float pulseFactor = cfg.subtleBreathingPulse
                                  ? (0.90f + 0.10f * std::sin(pulsePhaseRadians))
                                  : 1.0f;
    const float vis = std::clamp(preview.visibilityAlpha, 0.0f, 1.0f) * cfg.colorAlphaMultiplier();
    if (vis <= 0.008f) {
        return;
    }

    const bool emitFill = (cfg.renderMode != "Wireframe Only");
    const bool emitWire = (cfg.renderMode != "Translucent Fill");

    const RgbaColor baseRgb = cfg.resolveBaseColor(preview.materialColor);
    const float fillAlpha = std::clamp(cfg.fillOpacity * pulseFactor * vis, 0.01f, 0.90f);
    const float wireAlpha = std::clamp(cfg.wireframeOpacity * (0.94f + 0.06f * pulseFactor) * vis, 0.02f, 1.00f);

    static constexpr std::array<Facing, 6> kAllFaces{
        Facing::Down, Facing::Up, Facing::North, Facing::South, Facing::West, Facing::East
    };

    const std::size_t partCount = !preview.orientedBoxes.empty()
                                      ? preview.orientedBoxes.size()
                                      : preview.worldBoxes.size();
    outMesh.triangles.reserve(partCount * 28);
    outMesh.lines.reserve(partCount * 32 + 32);
    outMesh.packedTriangles.reserve(partCount * 84);
    outMesh.packedLines.reserve(partCount * 64 + 64);

    std::vector<EmittedEdge> emittedEdges;
    emittedEdges.reserve(partCount * 12 + 8);

    // 1A. True 3D Oriented Box path (16-angle standing signs, hanging signs, banners, skulls,
    //     angled wall torches, slanted lecterns, and smooth rotated animations!)
    if (!preview.orientedBoxes.empty()) {
        for (const auto& obb : preview.orientedBoxes) {
            emitOrientedBox(
                outMesh,
                cameraWorldPos,
                emittedEdges,
                obb,
                baseRgb,
                fillAlpha,
                wireAlpha,
                cfg.directionalShading,
                emitFill,
                emitWire);
        }
        return;
    }

    // 1B. Coplanar-culled multi-AABB exterior path (Stairs, Corners, Slabs, Doors, Beds, Fences, Walls, Hoppers, Cubes)
    for (std::size_t i = 0; i < preview.worldBoxes.size(); ++i) {
        const AABB box = preview.worldBoxes[i].normalized();
        if (box.isDegenerate()) {
            continue;
        }

        bool anyCameraFacing = false;
        for (Facing fCheck : kAllFaces) {
            if (faceVisibleFromCamera(box, fCheck, cameraWorldPos)) {
                anyCameraFacing = true;
                break;
            }
        }

        for (Facing face : kAllFaces) {
            if (anyCameraFacing && !faceVisibleFromCamera(box, face, cameraWorldPos)) {
                continue;
            }

            const float plane = facePlaneCoord(box, face);
            const Facing opp = oppositeFacing(face);

            std::vector<Rect2D> visibleRects{faceRect2D(box, face)};

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
                litBase.r, litBase.g, litBase.b, fillAlpha);
            const RgbaColor brightEdgeBase = baseRgb.shaded(std::min(1.0f, light + 0.14f));
            const RgbaColor edgeColor = RgbaColor::withFloatAlpha(
                brightEdgeBase.r, brightEdgeBase.g, brightEdgeBase.b, wireAlpha);

            for (const auto& r : visibleRects) {
                if (!r.empty()) {
                    emitFaceRectQuad(
                        outMesh,
                        cameraWorldPos,
                        emittedEdges,
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

    if (preview.hasMergeOutlineBox && emitWire) {
        const uint32_t mergeAbgr = RgbaColor::withFloatAlpha(
            255, 255, 255, std::min(1.0f, wireAlpha * 0.45f)).packAbgr();
        for (Facing f : kAllFaces) {
            if (faceVisibleFromCamera(preview.mergeOutlineBox, f, cameraWorldPos)) {
                emitFaceRectQuad(
                    outMesh,
                    cameraWorldPos,
                    emittedEdges,
                    f,
                    facePlaneCoord(preview.mergeOutlineBox, f),
                    faceRect2D(preview.mergeOutlineBox, f),
                    0u,
                    false,
                    mergeAbgr,
                    true);
            }
        }
    }

    if (cfg.showOrientationGuides && preview.orientationKind != OrientationKind::None &&
        !preview.worldBoxes.empty()) {
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
            case OrientationKind::Sign16:
            case OrientationKind::None:
                break;
        }
    }
}

} // namespace placepreview
