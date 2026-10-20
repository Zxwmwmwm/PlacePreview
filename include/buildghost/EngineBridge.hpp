#pragma once

#include "buildghost/Config.hpp"
#include "buildghost/Types.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace buildghost {

struct LivePreviewTelemetry {
    bool hookInstalled{false};
    bool activePreview{false};
    BlockPos targetPos{};
    std::string targetBlockId;
    OrientationKind orientationKind{OrientationKind::None};
    Facing facing{Facing::North};
    int pillarAxis{0};
    bool topHalf{false};
    bool isSlabMerge{false};
    std::size_t subBoxCount{0};
    uint64_t renderedFrames{0};
};

class EngineBridge {
public:
    static EngineBridge& instance() noexcept;

    [[nodiscard]] bool installHooks();
    void uninstallHooks();

    void notifyConfigUpdated(const ModConfig& cfg);
    [[nodiscard]] LivePreviewTelemetry telemetry() const;

private:
    EngineBridge() = default;

    std::atomic<bool> installed_{false};
    mutable std::mutex telemetryMutex_;
    LivePreviewTelemetry telemetry_{};
};

} // namespace buildghost
