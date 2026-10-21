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

RgbaColor lerpColor(const RgbaColor& a, const RgbaColor& b, float t) noexcept {
    auto ch = [t](uint8_t c0, uint8_t c1) -> uint8_t {
        const float v = static_cast<float>(c0) + (static_cast<float>(c1) - static_cast<float>(c0)) * t;
        return static_cast<uint8_t>(std::clamp(std::lround(v), 0L, 255L));
    };
    return RgbaColor{ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), 255};
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

std::vector<OrientedBox> promoteLocalBoxesToOriented(const std::vector<AABB>& boxes) {
    std::vector<OrientedBox> out;
    out.reserve(boxes.size());
    for (const auto& b : boxes) {
        out.push_back(OrientedBox::fromAABB(b));
    }
    return out;
}

std::size_t nearestAabbIndex(const std::vector<AABB>& candidates, const Vec3& queryCenter) noexcept {
    std::size_t bestIdx = 0;
    float bestDistSq = 1e30f;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const float d2 = (candidates[i].center() - queryCenter).lengthSq();
        if (d2 < bestDistSq) {
            bestDistSq = d2;
            bestIdx = i;
        }
    }
    return bestIdx;
}

std::size_t nearestObbIndex(const std::vector<OrientedBox>& candidates, const Vec3& queryCenter) noexcept {
    std::size_t bestIdx = 0;
    float bestDistSq = 1e30f;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const float d2 = (candidates[i].center - queryCenter).lengthSq();
        if (d2 < bestDistSq) {
            bestDistSq = d2;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// Builds matched-cardinality (K = max(M, N)) start/end AABB pairs so any M-box shape
// smoothly splits into OR converges onto any N-box shape without ever popping!
void buildMatchedAabbPairs(
    const std::vector<AABB>& src,
    const std::vector<AABB>& dst,
    std::vector<AABB>& outSrc,
    std::vector<AABB>& outDst) {

    const std::size_t m = src.size();
    const std::size_t n = dst.size();
    const std::size_t k = std::max(m, n);
    outSrc.clear();
    outDst.clear();
    if (m == 0 || n == 0) {
        return;
    }
    outSrc.reserve(k);
    outDst.reserve(k);
    for (std::size_t i = 0; i < k; ++i) {
        if (i < m && i < n) {
            outSrc.push_back(src[i]);
            outDst.push_back(dst[i]);
        } else if (i < n) {
            outSrc.push_back(src[nearestAabbIndex(src, dst[i].center())]);
            outDst.push_back(dst[i]);
        } else {
            outSrc.push_back(src[i]);
            outDst.push_back(dst[nearestAabbIndex(dst, src[i].center())]);
        }
    }
}

void buildMatchedObbPairs(
    const std::vector<OrientedBox>& src,
    const std::vector<OrientedBox>& dst,
    std::vector<OrientedBox>& outSrc,
    std::vector<OrientedBox>& outDst) {

    const std::size_t m = src.size();
    const std::size_t n = dst.size();
    const std::size_t k = std::max(m, n);
    outSrc.clear();
    outDst.clear();
    if (m == 0 || n == 0) {
        return;
    }
    outSrc.reserve(k);
    outDst.reserve(k);
    for (std::size_t i = 0; i < k; ++i) {
        if (i < m && i < n) {
            outSrc.push_back(src[i]);
            outDst.push_back(dst[i]);
        } else if (i < n) {
            outSrc.push_back(src[nearestObbIndex(src, dst[i].center)]);
            outDst.push_back(dst[i]);
        } else {
            outSrc.push_back(src[i]);
            outDst.push_back(dst[nearestObbIndex(dst, src[i].center)]);
        }
    }
}

// Smoothly interpolates two 3D OrientedBox parts while strictly maintaining a right-handed
// orthonormal frame (`det([axisU, axisV, axisW]) == +1`, zero shear, zero NaN).
OrientedBox interpolateObb(const OrientedBox& cur, const OrientedBox& dst, float t, float& outErr) noexcept {
    OrientedBox next{};
    next.center = lerpVec3(cur.center, dst.center, t);
    next.halfExtents = lerpVec3(cur.halfExtents, dst.halfExtents, t);

    const bool curVerticalYaw = (std::fabs(cur.axisV.y - 1.0f) < 1e-3f);
    const bool dstVerticalYaw = (std::fabs(dst.axisV.y - 1.0f) < 1e-3f);
    if (curVerticalYaw && dstVerticalYaw) {
        const float curYaw = cur.axisAligned ? 0.0f : cur.yawRad;
        const float dstYaw = dst.axisAligned ? 0.0f : dst.yawRad;
        next.yawRad = curYaw + shortestAngleDelta(curYaw, dstYaw) * t;
        const float c = std::cos(next.yawRad);
        const float s = std::sin(next.yawRad);
        next.axisU = {c, 0.0f, s};
        next.axisV = {0.0f, 1.0f, 0.0f};
        next.axisW = {-s, 0.0f, c};
        next.axisAligned = (std::fabs(std::sin(next.yawRad)) < 1e-4f && std::cos(next.yawRad) > 0.9999f);
    } else {
        Vec3 u = lerpVec3(cur.axisU, dst.axisU, t).normalizedOr(dst.axisU);
        Vec3 vRaw = lerpVec3(cur.axisV, dst.axisV, t);
        vRaw = vRaw - u * vRaw.dot(u);
        if (vRaw.lengthSq() < 1e-6f) {
            const Vec3 fallback = (std::fabs(u.y) < 0.9f) ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
            vRaw = fallback - u * fallback.dot(u);
        }
        const Vec3 v = vRaw.normalizedOr(dst.axisV);
        const Vec3 w = u.cross(v).normalizedOr(dst.axisW);
        next.axisU = u;
        next.axisV = v;
        next.axisW = w;
        next.yawRad = dst.yawRad;
        next.axisAligned = false;
    }

    const float centerErr = std::sqrt((next.center - dst.center).lengthSq());
    const float extErr = std::sqrt((next.halfExtents - dst.halfExtents).lengthSq());
    const float axisErr = std::sqrt((next.axisU - dst.axisU).lengthSq()) +
                          std::sqrt((next.axisV - dst.axisV).lengthSq());
    outErr = std::max({centerErr, extErr, axisErr});
    return next;
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
    animatedColor_ = {255, 255, 255, 255};
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

    if (!cfg.smoothAnimation) {
        if (!target.valid) {
            reset();
            return PlacementPreviewState{};
        }
        active_ = true;
        currentAnchor_ = target.primaryPos.originVec();
        visibility_ = 1.0f;
        appearScale_ = 1.0f;
        animatedColor_ = target.materialColor;
        lastTarget_ = target;
        animatedLocalBoxes_ = extractLocalBoxes(target, currentAnchor_);
        animatedLocalOriented_ = extractLocalOriented(target, currentAnchor_);
        PlacementPreviewState out = target;
        out.visibilityAlpha = 1.0f;
        return out;
    }

    const float dt = std::clamp(dtSeconds, 0.0005f, 0.05f);
    const float speed = std::clamp(cfg.animationSpeed, 4.0f, 40.0f);
    const float tMove = std::clamp(1.0f - std::exp(-speed * dt), 0.0f, 1.0f);
    const float tMorph = std::clamp(1.0f - std::exp(-(speed * 1.15f) * dt), 0.0f, 1.0f);
    const float tFadeIn = std::clamp(1.0f - std::exp(-(speed * 1.40f) * dt), 0.0f, 1.0f);
    const float tFadeOut = std::clamp(1.0f - std::exp(-(speed * 1.15f) * dt), 0.0f, 1.0f);

    // 1. Target invalid (looking at sky / out of reach / holding sword or empty hand):
    //    Smoothly fade out in place instead of disappearing abruptly!
    if (!target.valid) {
        if (!active_ || visibility_ <= 0.015f) {
            reset();
            return PlacementPreviewState{};
        }
        visibility_ = visibility_ + (0.0f - visibility_) * tFadeOut;
        appearScale_ = appearScale_ + (0.94f - appearScale_) * tFadeOut;
        if (visibility_ <= 0.015f) {
            reset();
            return PlacementPreviewState{};
        }
        PlacementPreviewState fading = lastTarget_;
        fading.visibilityAlpha = visibility_;
        fading.materialColor = animatedColor_;
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
    const float jumpDistSq = active_ ? (targetAnchor - currentAnchor_).lengthSq() : 9999.0f;
    if (!active_ || jumpDistSq > 81.0f) {
        active_ = true;
        currentAnchor_ = targetAnchor;
        appearScale_ = 0.91f + (1.0f - 0.91f) * tFadeIn;
        visibility_ = std::max(visibility_, 0.18f);
        visibility_ = visibility_ + (1.0f - visibility_) * tFadeIn;
        animatedColor_ = target.materialColor;
        animatedLocalBoxes_ = targetLocalBoxes;
        animatedLocalOriented_ = targetLocalOriented;
    } else {
        // 3. Continuous 3D world-anchor glide + seamless M <-> N shape/rotation morph across
        //    BOTH adjacent block motion AND hotbar held-item switches!
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

        animatedColor_ = lerpColor(animatedColor_, target.materialColor, tMorph);

        const bool useOrientedMorph = !animatedLocalOriented_.empty() || !targetLocalOriented.empty();
        if (!useOrientedMorph) {
            // Pure AABB <-> AABB M-to-N morph (e.g. Full Cube <-> Slab <-> Stair <-> Sea Pickle <-> Frame <-> Bed!)
            std::vector<AABB> srcMatched;
            std::vector<AABB> dstMatched;
            buildMatchedAabbPairs(animatedLocalBoxes_, targetLocalBoxes, srcMatched, dstMatched);
            if (!srcMatched.empty()) {
                float maxErr = 0.0f;
                animatedLocalBoxes_.resize(srcMatched.size());
                for (std::size_t i = 0; i < srcMatched.size(); ++i) {
                    const Vec3 nextMin = lerpVec3(srcMatched[i].min, dstMatched[i].min, tMorph);
                    const Vec3 nextMax = lerpVec3(srcMatched[i].max, dstMatched[i].max, tMorph);
                    animatedLocalBoxes_[i] = AABB{nextMin, nextMax};
                    const float err = std::max(
                        std::sqrt((nextMin - dstMatched[i].min).lengthSq()),
                        std::sqrt((nextMax - dstMatched[i].max).lengthSq()));
                    maxErr = std::max(maxErr, err);
                }
                if (maxErr <= 1.2e-3f) {
                    animatedLocalBoxes_ = targetLocalBoxes;
                }
            } else {
                animatedLocalBoxes_ = targetLocalBoxes;
            }
            animatedLocalOriented_.clear();
        } else {
            // Universal 3D OrientedBox M-to-N morph across ALL rotated & axis-aligned blocks
            // (e.g. Slab / Sea Pickle / Frame <-> Skull <-> 16-angle Sign <-> Hanging Sign <-> Wall Torch!)
            const std::vector<OrientedBox> curObbs = !animatedLocalOriented_.empty()
                                                         ? animatedLocalOriented_
                                                         : promoteLocalBoxesToOriented(animatedLocalBoxes_);
            const std::vector<OrientedBox> dstObbs = !targetLocalOriented.empty()
                                                         ? targetLocalOriented
                                                         : promoteLocalBoxesToOriented(targetLocalBoxes);
            std::vector<OrientedBox> srcMatched;
            std::vector<OrientedBox> dstMatched;
            buildMatchedObbPairs(curObbs, dstObbs, srcMatched, dstMatched);

            if (!srcMatched.empty()) {
                float maxErr = 0.0f;
                animatedLocalOriented_.resize(srcMatched.size());
                for (std::size_t i = 0; i < srcMatched.size(); ++i) {
                    float partErr = 0.0f;
                    animatedLocalOriented_[i] = interpolateObb(srcMatched[i], dstMatched[i], tMorph, partErr);
                    maxErr = std::max(maxErr, partErr);
                }
                if (maxErr <= 1.5e-3f) {
                    if (targetLocalOriented.empty()) {
                        // Seamlessly hand back to exact coplanar AABB representation once settled!
                        animatedLocalBoxes_ = targetLocalBoxes;
                        animatedLocalOriented_.clear();
                    } else {
                        animatedLocalOriented_ = targetLocalOriented;
                        animatedLocalBoxes_.clear();
                    }
                } else {
                    animatedLocalBoxes_.clear();
                }
            } else {
                animatedLocalBoxes_ = targetLocalBoxes;
                animatedLocalOriented_ = targetLocalOriented;
            }
        }
    }

    lastTarget_ = target;

    PlacementPreviewState out = target;
    out.visibilityAlpha = visibility_;
    out.materialColor = animatedColor_;
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
