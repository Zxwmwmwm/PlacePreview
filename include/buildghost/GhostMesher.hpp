#pragma once

#include "buildghost/Config.hpp"
#include "buildghost/Types.hpp"

#include <vector>

namespace buildghost {

struct GhostMeshOutput {
    std::vector<DebugTriangleVertex> triangles;
    std::vector<DebugLineVertex> lines;

    void clear() noexcept {
        triangles.clear();
        lines.clear();
    }

    [[nodiscard]] bool empty() const noexcept {
        return triangles.empty() && lines.empty();
    }
};

class GhostMesher {
public:
    // Builds camera-relative translucent faces, coplanar-culled multi-box surfaces,
    // crisp outer silhouette edges, and 3D orientation guides for `preview`.
    // Emits ZERO vertices immediately if `!preview.valid` or `!cfg.enabled`.
    static void buildMesh(
        const PlacementPreviewState& preview,
        const ModConfig& cfg,
        const Vec3& cameraWorldPos,
        float pulsePhaseRadians,
        GhostMeshOutput& outMesh);
};

} // namespace buildghost
