#pragma once

#include "buildghost/Config.hpp"
#include "buildghost/Types.hpp"

#include <vector>

namespace buildghost {

struct GhostMeshOutput {
    std::vector<DebugTriangleVertex> triangles;
    std::vector<DebugLineVertex> lines;
    // Exact 16-byte world-space vertices consumed natively by DebugRenderer::renderEnd:
    //   packedTriangles.size() == 3 * triangles.size()
    //   packedLines.size()     == 2 * lines.size()
    std::vector<PackedVertex> packedTriangles;
    std::vector<PackedVertex> packedLines;

    void clear() noexcept {
        triangles.clear();
        lines.clear();
        packedTriangles.clear();
        packedLines.clear();
    }

    [[nodiscard]] bool empty() const noexcept {
        return packedTriangles.empty() && packedLines.empty();
    }
};

class GhostMesher {
public:
    // Builds world-space PackedVertex buffers (plus camera-relative inspection descriptors),
    // coplanar-culled multi-box surfaces, crisp outer silhouette edges, and 3D orientation
    // guides for `preview`. Emits ZERO vertices immediately if `!preview.valid` or `!cfg.enabled`.
    static void buildMesh(
        const PlacementPreviewState& preview,
        const ModConfig& cfg,
        const Vec3& cameraWorldPos,
        float pulsePhaseRadians,
        GhostMeshOutput& outMesh);
};

} // namespace buildghost
