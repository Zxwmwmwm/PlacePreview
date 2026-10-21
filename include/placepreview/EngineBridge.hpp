#pragma once

#include <cstdint>

namespace placepreview {

struct ModConfig;
struct GhostMeshOutput;

struct BridgeStats {
    bool hooksInstalled{false};
    bool fastRvaResolved{false};
    uint64_t framesEvaluated{0};
    uint64_t framesRendered{0};
};

class EngineBridge {
public:
    static EngineBridge& instance() noexcept;

    bool installHooks();
    void uninstallHooks();
    void notifyConfigUpdated(const ModConfig& cfg) noexcept;
    [[nodiscard]] BridgeStats stats() const noexcept;

    static void flushMeshIntoDebugRenderer(
        void* debugRenderer,
        void* renderParams,
        void (*origFn)(void*, void*),
        const GhostMeshOutput& mesh) noexcept;

private:
    EngineBridge() = default;
};

} // namespace placepreview
