#pragma once

#include "placepreview/Config.hpp"
#include "placepreview/Types.hpp"

#include <vector>

namespace placepreview {

struct GhostMeshOutput {
    std::vector<DebugTriangleVertex> triangles;
    std::vector<DebugLineVertex> lines;
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
    static void buildMesh(
        const PlacementPreviewState& preview,
        const ModConfig& cfg,
        const Vec3& cameraWorldPos,
        float pulsePhaseRadians,
        GhostMeshOutput& outMesh);
};

} // namespace placepreview
