#pragma once

#include "placepreview/Config.hpp"
#include "placepreview/Types.hpp"

namespace placepreview {

// Frame-rate independent smooth 3D block-to-block glide & fade controller:
//  - Glides preview position smoothly across adjacent/nearby blocks instead of snapping harshly
//  - Interpolates sub-block half flips (slabs/trapdoors/stairs) and 16-angle rotations cleanly
//  - Gently fades in on initial appearance and fades out cleanly when looking at sky / out of reach
class PreviewAnimator {
public:
    void reset() noexcept;

    [[nodiscard]] PlacementPreviewState update(
        const PlacementPreviewState& target,
        const ModConfig& cfg,
        float dtSeconds);

    [[nodiscard]] bool isActive() const noexcept { return active_; }
    [[nodiscard]] Vec3 currentAnchor() const noexcept { return currentAnchor_; }

private:
    bool active_{false};
    Vec3 currentAnchor_{0.0f, 0.0f, 0.0f};
    float visibility_{0.0f};
    float appearScale_{1.0f};
    PlacementPreviewState lastTarget_{};
    std::vector<AABB> animatedLocalBoxes_;
    std::vector<OrientedBox> animatedLocalOriented_;
};

} // namespace placepreview
