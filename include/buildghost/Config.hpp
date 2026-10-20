#pragma once

#include "buildghost/Types.hpp"

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace buildghost {

struct ModConfig {
    bool enabled{true};

    // Quick visual presets:
    //   "Classic Cyan", "Material Tint", "Crisp Wireframe", "Subtle Minimal", "High Contrast", "Custom"
    std::string preset{"Classic Cyan"};

    // "Shaded + Wireframe", "Translucent Fill", "Wireframe Only"
    std::string renderMode{"Shaded + Wireframe"};

    // "Material Adaptive", "Custom Accent"
    std::string colorMode{"Custom Accent"};

    // Translucent fill & crisp edge alpha [0.05..1.00]
    float fillOpacity{0.28f};
    float wireframeOpacity{0.86f};

    // Accent RGB when colorMode == "Custom Accent"
    int accentRed{92};
    int accentGreen{224};
    int accentBlue{255};

    // 3D depth shading & orientation guides
    bool directionalShading{true};
    bool showOrientationGuides{true};
    bool showSubTargetPip{true};
    bool subtleBreathingPulse{true};

    // Gameplay behavior options
    bool enableOffhandPreview{true};
    bool hideOnInteractiveWithoutSneak{false};

    [[nodiscard]] RgbaColor resolveBaseColor(const RgbaColor& materialTint) const noexcept {
        if (colorMode == "Material Adaptive") {
            return materialTint;
        }
        return {
            static_cast<uint8_t>(std::clamp(accentRed, 0, 255)),
            static_cast<uint8_t>(std::clamp(accentGreen, 0, 255)),
            static_cast<uint8_t>(std::clamp(accentBlue, 0, 255)),
            255
        };
    }
};

class ConfigManager {
public:
    static ConfigManager& instance() noexcept;

    void init(const std::filesystem::path& configDir);
    [[nodiscard]] ModConfig snapshot() const;

    void setEnabled(bool enabled);
    void applyPreset(std::string_view presetName);
    void applyKeyValue(std::string_view key, std::string_view value);
    void resetDefaults();

    [[nodiscard]] bool loadFromFile(const std::filesystem::path& path);
    [[nodiscard]] bool saveToFile(const std::filesystem::path& path) const;

    [[nodiscard]] static std::string serializeJson(const ModConfig& cfg);
    [[nodiscard]] static ModConfig parseJson(std::string_view jsonText);

private:
    ConfigManager() = default;
    static void clampConfig(ModConfig& cfg) noexcept;

    mutable std::mutex mutex_;
    ModConfig config_{};
    std::filesystem::path configFilePath_{};
};

} // namespace buildghost
