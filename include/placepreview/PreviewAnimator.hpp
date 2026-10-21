#pragma once

#include "placepreview/Config.hpp"
#include "placepreview/Types.hpp"

#include <vector>

namespace placepreview {

// Frame-rate independent smooth 3D block-to-block & held-item morph controller:
//  - Glides preview position smoothly across adjacent/nearby blocks instead of snapping harshly
//  - Smoothly morphs ANY M-part 3D shape into ANY N-part 3D shape when switching held blocks
//    (including across AABB <-> rotated OrientedBox representations such as cubes, slabs,
//     item frames, skulls, sea pickles, signs, hanging signs, stairs, beds, doors, and torches)
//  - Smoothly cross-fades adaptive material color and gently fades in/out
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
    RgbaColor animatedColor_{255, 255, 255, 255};
    PlacementPreviewState lastTarget_{};
    std::vector<AABB> animatedLocalBoxes_;
    std::vector<OrientedBox> animatedLocalOriented_;
};

} // namespace placepreview
