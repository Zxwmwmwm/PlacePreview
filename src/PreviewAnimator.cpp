#include "placepreview/PreviewAnimator.hpp"

#include <algorithm>
#include <cmath>

namespace placepreview {

namespace {

Vec3 lerpVec3(const Vec3& a, const Vec3& b, float t) noexcept {
    return a + (b - a) * t;
}

float shortestAngleDelta(float fromRad, float toRad) noexcept {
    return std::atan2(std::sin(toRad - fromRad), std::cos(toRad - fromRad));
}

AABB scaleAabbAroundCenter(const AABB& b, const Vec3& pivot, float scale) noexcept {
    return AABB{
        pivot + (b.min - pivot) * scale,
        pivot + (b.max - pivot) * scale
    };
}

OrientedBox scaleObbAroundPivot(const OrientedBox& obb, const Vec3& pivot, float scale) noexcept {
    OrientedBox out = obb;
    out.center = pivot + (obb.center - pivot) * scale;
    out.halfExtents = obb.halfExtents * scale;
    return out;
}

std::vector<AABB> extractLocalBoxes(const PlacementPreviewState& s, const Vec3& anchor) {
    std::vector<AABB> out;
    out.reserve(s.worldBoxes.size());
    for (const auto& b : s.worldBoxes) {
        out.push_back(b.normalized().translated(anchor * -1.0f));
    }
    return out;
}

std::vector<OrientedBox> extractLocalOriented(const PlacementPreviewState& s, const Vec3& anchor) {
    std::vector<OrientedBox> out;
    out.reserve(s.orientedBoxes.size());
    for (const auto& obb : s.orientedBoxes) {
        OrientedBox local = obb;
        local.center = obb.center - anchor;
        out.push_back(local);
    }
    return out;
}

void writeWorldGeometry(
    PlacementPreviewState& outState,
    const Vec3& worldAnchor,
    const Vec3& targetOrigin,
    float scale,
    const std::vector<AABB>& localBoxes,
    const std::vector<OrientedBox>& localOriented) {

    const Vec3 cellCenter = worldAnchor + Vec3{0.5f, 0.5f, 0.5f};
    outState.worldBoxes.clear();
    outState.worldBoxes.reserve(localBoxes.size());
    for (const auto& lb : localBoxes) {
        const AABB shifted = lb.translated(worldAnchor);
        outState.worldBoxes.push_back(
            (std::fabs(scale - 1.0f) < 1e-4f)
                ? shifted
                : scaleAabbAroundCenter(shifted, cellCenter, scale));
    }

    outState.orientedBoxes.clear();
    outState.orientedBoxes.reserve(localOriented.size());
    for (const auto& lo : localOriented) {
        OrientedBox shifted = lo;
        shifted.center = lo.center + worldAnchor;
        outState.orientedBoxes.push_back(
            (std::fabs(scale - 1.0f) < 1e-4f)
                ? shifted
                : scaleObbAroundPivot(shifted, cellCenter, scale));
    }

    if (outState.hasMergeOutlineBox) {
        outState.mergeOutlineBox = outState.mergeOutlineBox.translated(worldAnchor - targetOrigin);
    }
}

} // namespace

void PreviewAnimator::reset() noexcept {
    active_ = false;
    currentAnchor_ = {0.0f, 0.0f, 0.0f};
    visibility_ = 0.0f;
    appearScale_ = 1.0f;
    lastTarget_ = {};
    animatedLocalBoxes_.clear();
    animatedLocalOriented_.clear();
}

PlacementPreviewState PreviewAnimator::update(
    const PlacementPreviewState& target,
    const ModConfig& cfg,
    float dtSeconds) {

    if (!cfg.enabled) {
        reset();
        return PlacementPreviewState{};
    }

    // Instant mode when smooth transition is disabled by the user in Mod Menu.
    if (!cfg.smoothAnimation) {
        if (!target.valid) {
            reset();
            return PlacementPreviewState{};
        }
        active_ = true;
        currentAnchor_ = target.primaryPos.originVec();
        visibility_ = 1.0f;
        appearScale_ = 1.0f;
        lastTarget_ = target;
        animatedLocalBoxes_ = extractLocalBoxes(target, currentAnchor_);
        animatedLocalOriented_ = extractLocalOriented(target, currentAnchor_);
        PlacementPreviewState out = target;
        out.visibilityAlpha = 1.0f;
        return out;
    }

    const float dt = std::clamp(dtSeconds, 0.0005f, 0.05f);
    const float speed = std::clamp(cfg.animationSpeed, 4.0f, 36.0f);
    const float tMove = std::clamp(1.0f - std::exp(-speed * dt), 0.0f, 1.0f);
    const float tMorph = std::clamp(1.0f - std::exp(-(speed * 1.25f) * dt), 0.0f, 1.0f);
    const float tFadeIn = std::clamp(1.0f - std::exp(-(speed * 1.40f) * dt), 0.0f, 1.0f);
    const float tFadeOut = std::clamp(1.0f - std::exp(-(speed * 1.15f) * dt), 0.0f, 1.0f);

    // 1. Target invalid (looking at sky / out of reach / holding tool): smoothly fade out in place!
    if (!target.valid) {
        if (!active_ || visibility_ <= 0.015f) {
            reset();
            return PlacementPreviewState{};
        }
        visibility_ = lerpVec3({visibility_, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, tFadeOut).x;
        appearScale_ = appearScale_ + (0.94f - appearScale_) * tFadeOut;
        if (visibility_ <= 0.015f) {
            reset();
            return PlacementPreviewState{};
        }
        PlacementPreviewState fading = lastTarget_;
        fading.visibilityAlpha = visibility_;
        writeWorldGeometry(
            fading,
            currentAnchor_,
            lastTarget_.primaryPos.originVec(),
            appearScale_,
            animatedLocalBoxes_,
            animatedLocalOriented_);
        return fading;
    }

    const Vec3 targetAnchor = target.primaryPos.originVec();
    const std::vector<AABB> targetLocalBoxes = extractLocalBoxes(target, targetAnchor);
    const std::vector<OrientedBox> targetLocalOriented = extractLocalOriented(target, targetAnchor);

    // 2. First appearance from hidden state OR teleport across > 9 blocks:
    //    Snap world anchor cleanly to the target cell and softly expand & fade in!
    const float jumpDistSq = active_ ? (targetAnchor - currentAnchor_).lengthSq() : 9999.0f;
    if (!active_ || jumpDistSq > 81.0f) {
        active_ = true;
        currentAnchor_ = targetAnchor;
        appearScale_ = 0.91f + (1.0f - 0.91f) * tFadeIn;
        visibility_ = std::max(visibility_, 0.18f);
        visibility_ = visibility_ + (1.0f - visibility_) * tFadeIn;
        animatedLocalBoxes_ = targetLocalBoxes;
        animatedLocalOriented_ = targetLocalOriented;
    } else {
        // 3. Continuous 3D block-to-block glide + local shape/rotation morph!
        currentAnchor_ = lerpVec3(currentAnchor_, targetAnchor, tMove);
        if ((targetAnchor - currentAnchor_).lengthSq() < 2.25e-6f) {
            currentAnchor_ = targetAnchor;
        }

        visibility_ = visibility_ + (1.0f - visibility_) * tFadeIn;
        if (1.0f - visibility_ < 0.004f) {
            visibility_ = 1.0f;
        }
        appearScale_ = appearScale_ + (1.0f - appearScale_) * tFadeIn;
        if (std::fabs(1.0f - appearScale_) < 0.002f) {
            appearScale_ = 1.0f;
        }

        // Morph local AABB sub-boxes smoothly when topology count matches (e.g. bottom <-> top slab!)
        if (animatedLocalBoxes_.size() == targetLocalBoxes.size()) {
            for (std::size_t i = 0; i < targetLocalBoxes.size(); ++i) {
                animatedLocalBoxes_[i].min = lerpVec3(animatedLocalBoxes_[i].min, targetLocalBoxes[i].min, tMorph);
                animatedLocalBoxes_[i].max = lerpVec3(animatedLocalBoxes_[i].max, targetLocalBoxes[i].max, tMorph);
            }
        } else {
            animatedLocalBoxes_ = targetLocalBoxes;
            appearScale_ = std::min(appearScale_, 0.95f);
        }

        // Morph 3D oriented boxes (including shortest-arc 16-angle sign/banner/skull rotation!)
        if (animatedLocalOriented_.size() == targetLocalOriented.size()) {
            for (std::size_t i = 0; i < targetLocalOriented.size(); ++i) {
                auto& cur = animatedLocalOriented_[i];
                const auto& dst = targetLocalOriented[i];
                cur.center = lerpVec3(cur.center, dst.center, tMorph);
                cur.halfExtents = lerpVec3(cur.halfExtents, dst.halfExtents, tMorph);
                if (!cur.axisAligned && !dst.axisAligned &&
                    std::fabs(cur.axisV.y - 1.0f) < 1e-3f && std::fabs(dst.axisV.y - 1.0f) < 1e-3f) {
                    // Pure Y-yaw OBB (standing sign, hanging sign, banner, floor skull):
                    // preserve exact orthonormal unit axes throughout rotation!
                    cur.yawRad += shortestAngleDelta(cur.yawRad, dst.yawRad) * tMorph;
                    const float c = std::cos(cur.yawRad);
                    const float s = std::sin(cur.yawRad);
                    cur.axisU = {c, 0.0f, s};
                    cur.axisV = {0.0f, 1.0f, 0.0f};
                    cur.axisW = {-s, 0.0f, c};
                    cur.axisAligned = false;
                } else {
                    cur.axisU = lerpVec3(cur.axisU, dst.axisU, tMorph).normalizedOr(dst.axisU);
                    cur.axisV = lerpVec3(cur.axisV, dst.axisV, tMorph).normalizedOr(dst.axisV);
                    cur.axisW = cur.axisU.cross(cur.axisV).normalizedOr(dst.axisW);
                    cur.yawRad = dst.yawRad;
                    cur.axisAligned = dst.axisAligned;
                }
            }
        } else {
            animatedLocalOriented_ = targetLocalOriented;
        }
    }

    lastTarget_ = target;
    PlacementPreviewState out = target;
    out.visibilityAlpha = visibility_;
    writeWorldGeometry(
        out,
        currentAnchor_,
        targetAnchor,
        appearScale_,
        animatedLocalBoxes_,
        animatedLocalOriented_);
    return out;
}

} // namespace placepreview
