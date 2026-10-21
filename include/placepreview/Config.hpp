#pragma once

#include "placepreview/Types.hpp"

#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace placepreview {

struct ModConfig {
    int schemaVersion{3};
    bool enabled{true};

    // Quick visual presets:
    //   "Frosted White", "Classic Cyan", "Material Adaptive",
    //   "Crisp Wireframe", "Subtle Minimal", "High Contrast", "Custom"
    std::string preset{"Frosted White"};

    // "Shaded + Wireframe", "Translucent Fill", "Wireframe Only"
    std::string renderMode{"Shaded + Wireframe"};

    // "Material Adaptive", "Custom Accent"
    std::string colorMode{"Custom Accent"};

    // Clean translucent fill & crisp outline alpha [0.05..1.00]
    float fillOpacity{0.34f};
    float wireframeOpacity{0.68f};

    // Native slider/swatch picker color (#AARRGGBB / RGB + alpha tint multiplier)
    int accentRed{255};
    int accentGreen{255};
    int accentBlue{255};
    int accentAlpha{255};

    // Smooth block-to-block glide & fade animation
    bool smoothAnimation{true};
    float animationSpeed{18.0f};

    // 3D depth shading & optional schematic orientation guides
    bool directionalShading{true};
    bool showOrientationGuides{false};
    bool subtleBreathingPulse{false};

    // Bedrock-valid gameplay option
    bool hideOnInteractiveWithoutSneak{false};

    [[nodiscard]] std::string previewColorHex() const {
        char buf[16]{};
        const unsigned int a = static_cast<unsigned int>(std::clamp(accentAlpha, 0, 255));
        const unsigned int r = static_cast<unsigned int>(std::clamp(accentRed, 0, 255));
        const unsigned int g = static_cast<unsigned int>(std::clamp(accentGreen, 0, 255));
        const unsigned int b = static_cast<unsigned int>(std::clamp(accentBlue, 0, 255));
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", a, r, g, b);
        return std::string(buf);
    }

    [[nodiscard]] float colorAlphaMultiplier() const noexcept {
        return static_cast<float>(std::clamp(accentAlpha, 25, 255)) / 255.0f;
    }

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

    [[nodiscard]] static bool parseHexColor(
        std::string_view raw,
        int& outR,
        int& outG,
        int& outB,
        int& outA) noexcept;
    [[nodiscard]] static std::string serializeJson(const ModConfig& cfg);
    [[nodiscard]] static ModConfig parseJson(std::string_view jsonText);

private:
    ConfigManager() = default;
    static void clampConfig(ModConfig& cfg) noexcept;

    mutable std::mutex mutex_;
    ModConfig config_{};
    std::filesystem::path configFilePath_{};
};

} // namespace placepreview
