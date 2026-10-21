#include "placepreview/ModMenuUi.hpp"

#include "placepreview/ColorPreviewOverlay.hpp"
#include "placepreview/Config.hpp"
#include "placepreview/Version.hpp"

#include <pl/Logger.hpp>
#include <pl/ModMenu.hpp>
#include <pl/ModMenuConfig.hpp>

#include <cstdio>
#include <string>

namespace placepreview::ui {

namespace {

constexpr const char* kModuleId = "placepreview.main";

pl::log::Logger& uiLog() {
    static pl::log::Logger sLogger{std::string(version::Name)};
    return sLogger;
}

std::string fmtFloat(float v) {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return std::string(buf);
}

std::string boolStr(bool v) {
    return v ? "true" : "false";
}

bool isVisualSwatchKey(std::string_view key) noexcept {
    return key == "preset" || key == "renderMode" || key == "colorMode" ||
           key == "colorSwatch" || key == "accentRed" || key == "accentGreen" ||
           key == "accentBlue" || key == "accentAlpha" || key == "previewColor" ||
           key == "fillOpacity" || key == "wireframeOpacity" || key == "resetDefaults";
}

std::string buildSchemaJson(const ModConfig& cfg) {
    const ModConfig def{};

    pl::modmenu::ConfigSchemaBuilder builder;
    builder
        .defaultCategory("color")
        .category(
            "color",
            "Color & Style",
            "Quick presets, render style, live RGB sliders + swatch chips with real-time top-screen color swatch preview, and opacity.")
        .category(
            "motion",
            "Motion & Behavior",
            "Smooth 3D glide & item-switch morph speed, surface shading, directional cues, and behavior.");

    builder.node({
        .id = "preset",
        .key = "preset",
        .category = "color",
        .title = "Quick Preset",
        .description = "Apply a curated render style, color, and opacity profile with one tap.",
        .section = "Presets & Style",
        .type = pl::modmenu::ConfigControlTypeV2::Choice,
        .choiceStyle = pl::modmenu::ConfigChoiceStyleV2::Chips,
        .defaultValue = def.preset,
        .currentValue = cfg.preset,
        .hasCurrentValue = true,
        .options = {
            {"Frosted White", "Frosted White", "Clean bright white shaded fill + crisp outline"},
            {"Classic Cyan", "Classic Cyan", "Translucent cyan preview with bright edges"},
            {"Material Adaptive", "Material Adaptive", "Automatically tints to match held block material"},
            {"Crisp Wireframe", "Crisp Wireframe", "Pure clean 3D block outline with zero fill"},
            {"Subtle Minimal", "Subtle Minimal", "Soft low-opacity fill and outline"},
            {"High Contrast", "High Contrast", "Vivid golden-amber preview for bright biomes"},
            {"Custom", "Custom", "User-customized style, color, and opacity settings"},
        },
    });

    builder.node({
        .id = "renderMode",
        .key = "renderMode",
        .category = "color",
        .title = "Render Style",
        .description = "Choose shaded 3D fill + crisp wireframe, pure wireframe, or translucent fill.",
        .section = "Presets & Style",
        .type = pl::modmenu::ConfigControlTypeV2::Choice,
        .choiceStyle = pl::modmenu::ConfigChoiceStyleV2::Segmented,
        .defaultValue = def.renderMode,
        .currentValue = cfg.renderMode,
        .hasCurrentValue = true,
        .options = {
            {"Shaded + Wireframe", "Shaded + Wireframe", "Translucent shaded 3D faces with crisp edges"},
            {"Wireframe Only", "Wireframe Only", "Clean outer 3D block outline only"},
            {"Translucent Fill", "Translucent Fill", "Shaded translucent 3D faces without edge lines"},
        },
    });

    builder.node({
        .id = "colorMode",
        .key = "colorMode",
        .category = "color",
        .title = "Color Source",
        .description = "Use your custom color below or automatically tint to match the held block's material.",
        .section = "Live Color Picker",
        .type = pl::modmenu::ConfigControlTypeV2::Choice,
        .choiceStyle = pl::modmenu::ConfigChoiceStyleV2::Segmented,
        .defaultValue = def.colorMode,
        .currentValue = cfg.colorMode,
        .hasCurrentValue = true,
        .options = {
            {"Custom Color", "Custom Color", "Use your selected custom color below"},
            {"Material Adaptive", "Material Adaptive", "Automatically match held block material color"},
        },
    });

    builder.node({
        .id = "colorSwatch",
        .key = "colorSwatch",
        .category = "color",
        .title = "Color Swatch",
        .description = "Tap any color swatch or drag Red / Green / Blue below — live color preview appears right above this menu!",
        .section = "Live Color Picker",
        .type = pl::modmenu::ConfigControlTypeV2::Choice,
        .choiceStyle = pl::modmenu::ConfigChoiceStyleV2::Chips,
        .defaultValue = def.colorSwatch,
        .currentValue = cfg.colorSwatch,
        .hasCurrentValue = true,
        .options = {
            {"White", "White", "#FFFFFF crisp white"},
            {"Cyan", "Cyan", "#48C8FF sky cyan"},
            {"Emerald", "Emerald", "#52E888 vivid emerald"},
            {"Gold", "Gold", "#FFD43B warm amber"},
            {"Coral", "Coral", "#FF7662 soft coral"},
            {"Violet", "Violet", "#B674FF bright violet"},
            {"Red", "Red", "#F44444 crimson red"},
            {"Blue", "Blue", "#4E8EFF cobalt blue"},
            {"Custom", "Custom", "Custom RGB slider / hex color"},
        },
    });

    builder.node({
        .id = "accentRed",
        .key = "accentRed",
        .category = "color",
        .title = "Red Channel (R)",
        .description = "Drag to preview Red (0–255) live above the Mod Menu without closing it.",
        .section = "Live Color Picker",
        .type = pl::modmenu::ConfigControlTypeV2::SliderInt,
        .defaultValue = std::to_string(def.accentRed),
        .currentValue = std::to_string(cfg.accentRed),
        .hasCurrentValue = true,
        .minValue = "0",
        .maxValue = "255",
        .step = "1",
    });

    builder.node({
        .id = "accentGreen",
        .key = "accentGreen",
        .category = "color",
        .title = "Green Channel (G)",
        .description = "Drag to preview Green (0–255) live above the Mod Menu without closing it.",
        .section = "Live Color Picker",
        .type = pl::modmenu::ConfigControlTypeV2::SliderInt,
        .defaultValue = std::to_string(def.accentGreen),
        .currentValue = std::to_string(cfg.accentGreen),
        .hasCurrentValue = true,
        .minValue = "0",
        .maxValue = "255",
        .step = "1",
    });

    builder.node({
        .id = "accentBlue",
        .key = "accentBlue",
        .category = "color",
        .title = "Blue Channel (B)",
        .description = "Drag to preview Blue (0–255) live above the Mod Menu without closing it.",
        .section = "Live Color Picker",
        .type = pl::modmenu::ConfigControlTypeV2::SliderInt,
        .defaultValue = std::to_string(def.accentBlue),
        .currentValue = std::to_string(cfg.accentBlue),
        .hasCurrentValue = true,
        .minValue = "0",
        .maxValue = "255",
        .step = "1",
    });

    builder.node({
        .id = "accentAlpha",
        .key = "accentAlpha",
        .category = "color",
        .title = "Color Intensity (Alpha)",
        .description = "Global color alpha multiplier (16–255) previewed live above the Mod Menu.",
        .section = "Live Color Picker",
        .type = pl::modmenu::ConfigControlTypeV2::SliderInt,
        .defaultValue = std::to_string(def.accentAlpha),
        .currentValue = std::to_string(cfg.accentAlpha),
        .hasCurrentValue = true,
        .minValue = "16",
        .maxValue = "255",
        .step = "1",
    });

    builder.node({
        .id = "previewColor",
        .key = "previewColor",
        .category = "color",
        .title = "Preview Color",
        .description = "Tap for Hex / RGBA popup or adjust inline above — floating swatch previews your color live without closing Mod Menu.",
        .section = "Live Color Picker",
        .type = pl::modmenu::ConfigControlTypeV2::Color,
        .defaultValue = def.previewColorHex(),
        .currentValue = cfg.previewColorHex(),
        .hasCurrentValue = true,
    });

    builder.node({
        .id = "fillOpacity",
        .key = "fillOpacity",
        .category = "color",
        .title = "Fill Opacity",
        .description = "Opacity of translucent 3D block faces (0.00 = hidden, 1.00 = solid).",
        .section = "Opacity",
        .type = pl::modmenu::ConfigControlTypeV2::SliderFloat,
        .defaultValue = fmtFloat(def.fillOpacity),
        .currentValue = fmtFloat(cfg.fillOpacity),
        .hasCurrentValue = true,
        .minValue = "0.00",
        .maxValue = "1.00",
        .step = "0.02",
    });

    builder.node({
        .id = "wireframeOpacity",
        .key = "wireframeOpacity",
        .category = "color",
        .title = "Outline Opacity",
        .description = "Opacity of outer 3D block edges (0.00 = hidden, 1.00 = full brightness).",
        .section = "Opacity",
        .type = pl::modmenu::ConfigControlTypeV2::SliderFloat,
        .defaultValue = fmtFloat(def.wireframeOpacity),
        .currentValue = fmtFloat(cfg.wireframeOpacity),
        .hasCurrentValue = true,
        .minValue = "0.00",
        .maxValue = "1.00",
        .step = "0.02",
    });

    builder.node({
        .id = "smoothAnimation",
        .key = "smoothAnimation",
        .category = "motion",
        .title = "Smooth Block Transition",
        .description = "Smoothly glide across positions and morph 3D shapes when switching blocks.",
        .section = "Motion",
        .type = pl::modmenu::ConfigControlTypeV2::Toggle,
        .defaultValue = boolStr(def.smoothAnimation),
        .currentValue = boolStr(cfg.smoothAnimation),
        .hasCurrentValue = true,
    });

    builder.node({
        .id = "animationSpeed",
        .key = "animationSpeed",
        .category = "motion",
        .title = "Transition Speed",
        .description = "Higher values glide and morph faster; lower values float more gently.",
        .section = "Motion",
        .type = pl::modmenu::ConfigControlTypeV2::SliderFloat,
        .defaultValue = fmtFloat(def.animationSpeed),
        .currentValue = fmtFloat(cfg.animationSpeed),
        .hasCurrentValue = true,
        .minValue = "4.00",
        .maxValue = "40.00",
        .step = "1.00",
        .visibleWhen = {{"smoothAnimation", pl::modmenu::ConfigConditionOpV2::Truthy, "true"}},
    });

    builder.node({
        .id = "directionalShading",
        .key = "directionalShading",
        .category = "motion",
        .title = "3D Surface Shading",
        .description = "Subtle directional face lighting for clear 3D depth perception.",
        .section = "Appearance",
        .type = pl::modmenu::ConfigControlTypeV2::Toggle,
        .defaultValue = boolStr(def.directionalShading),
        .currentValue = boolStr(cfg.directionalShading),
        .hasCurrentValue = true,
    });

    builder.node({
        .id = "showOrientationGuides",
        .key = "showOrientationGuides",
        .category = "motion",
        .title = "Directional Guide",
        .description = "Orientation cue on directional blocks (stairs, observers, pillars, doors).",
        .section = "Appearance",
        .type = pl::modmenu::ConfigControlTypeV2::Toggle,
        .defaultValue = boolStr(def.showOrientationGuides),
        .currentValue = boolStr(cfg.showOrientationGuides),
        .hasCurrentValue = true,
    });

    builder.node({
        .id = "subtleBreathingPulse",
        .key = "subtleBreathingPulse",
        .category = "motion",
        .title = "Subtle Breathing Pulse",
        .description = "Gentle opacity pulse while aiming at a target surface.",
        .section = "Appearance",
        .type = pl::modmenu::ConfigControlTypeV2::Toggle,
        .defaultValue = boolStr(def.subtleBreathingPulse),
        .currentValue = boolStr(cfg.subtleBreathingPulse),
        .hasCurrentValue = true,
    });

    builder.node({
        .id = "hideOnInteractiveWithoutSneak",
        .key = "hideOnInteractiveWithoutSneak",
        .category = "motion",
        .title = "Hide on Chests / Tables Unless Sneaking",
        .description = "Hide preview when aiming at chests, crafting tables, or furnaces while standing.",
        .section = "Behavior",
        .type = pl::modmenu::ConfigControlTypeV2::Toggle,
        .defaultValue = boolStr(def.hideOnInteractiveWithoutSneak),
        .currentValue = boolStr(cfg.hideOnInteractiveWithoutSneak),
        .hasCurrentValue = true,
    });

    builder.node({
        .id = "resetDefaults",
        .key = "resetDefaults",
        .category = "motion",
        .title = "Reset All Settings to Defaults",
        .description = "Restore default preset, render style, color, animation, and opacity settings.",
        .section = "Maintenance",
        .type = pl::modmenu::ConfigControlTypeV2::Button,
        .actionValue = "true",
    });

    return builder.toJson();
}

} // namespace

bool registerMenu(std::string_view ownerModId) {
    const ModConfig cfg = ConfigManager::instance().snapshot();

    pl::modmenu::ModuleBuilder builder{kModuleId, std::string(version::Name)};
    builder.description(std::string(version::Description))
        .modId(ownerModId.empty() ? std::string(version::Id) : std::string(ownerModId))
        .defaultEnabled(cfg.enabled)
        .hideInHudEditor(true)
        .onToggle([](std::string_view, bool enabled) {
            ConfigManager::instance().setEnabled(enabled);
        })
        .onConfigChanged([](std::string_view, std::string_view key, std::string_view value) {
            ConfigManager::instance().applyKeyValue(key, value);
            const ModConfig snap = ConfigManager::instance().snapshot();
            if (key == "resetDefaults") {
                pl::modmenu::setModuleEnabled(kModuleId, snap.enabled);
            }
            if (isVisualSwatchKey(key)) {
                showLiveColorPreview(snap);
            }
            // Synchronize Schema V2 JSON `current_value`s on EVERY change so closing
            // and reopening Mod Menu NEVER reverts any slider, color, preset, or toggle!
            refreshSchema();
        });

    builder
        .config("preset", "Quick Preset", pl::modmenu::ConfigType::Radio, cfg.preset,
                "Frosted White,Classic Cyan,Material Adaptive,Crisp Wireframe,Subtle Minimal,High Contrast,Custom")
        .config("renderMode", "Render Style", pl::modmenu::ConfigType::Radio, cfg.renderMode,
                "Shaded + Wireframe,Wireframe Only,Translucent Fill")
        .config("colorMode", "Color Source", pl::modmenu::ConfigType::Radio, cfg.colorMode,
                "Custom Color,Material Adaptive")
        .config("colorSwatch", "Color Swatch", pl::modmenu::ConfigType::Radio, cfg.colorSwatch,
                "White,Cyan,Emerald,Gold,Coral,Violet,Red,Blue,Custom")
        .config("accentRed", "Red Channel (R)", pl::modmenu::ConfigType::SliderInt, std::to_string(cfg.accentRed), "0", "255")
        .config("accentGreen", "Green Channel (G)", pl::modmenu::ConfigType::SliderInt, std::to_string(cfg.accentGreen), "0", "255")
        .config("accentBlue", "Blue Channel (B)", pl::modmenu::ConfigType::SliderInt, std::to_string(cfg.accentBlue), "0", "255")
        .config("accentAlpha", "Color Intensity (Alpha)", pl::modmenu::ConfigType::SliderInt, std::to_string(cfg.accentAlpha), "16", "255")
        .config("previewColor", "Preview Color", pl::modmenu::ConfigType::Color, cfg.previewColorHex())
        .config("fillOpacity", "Fill Opacity", pl::modmenu::ConfigType::SliderFloat, fmtFloat(cfg.fillOpacity), "0.00", "1.00")
        .config("wireframeOpacity", "Outline Opacity", pl::modmenu::ConfigType::SliderFloat, fmtFloat(cfg.wireframeOpacity), "0.00", "1.00")
        .config("smoothAnimation", "Smooth Block Transition", pl::modmenu::ConfigType::Toggle, boolStr(cfg.smoothAnimation))
        .config("animationSpeed", "Transition Speed", pl::modmenu::ConfigType::SliderFloat, fmtFloat(cfg.animationSpeed), "4.00", "40.00", "smoothAnimation")
        .config("directionalShading", "3D Surface Shading", pl::modmenu::ConfigType::Toggle, boolStr(cfg.directionalShading))
        .config("showOrientationGuides", "Directional Guide", pl::modmenu::ConfigType::Toggle, boolStr(cfg.showOrientationGuides))
        .config("subtleBreathingPulse", "Subtle Breathing Pulse", pl::modmenu::ConfigType::Toggle, boolStr(cfg.subtleBreathingPulse))
        .config("hideOnInteractiveWithoutSneak", "Hide on Chests / Tables Unless Sneaking", pl::modmenu::ConfigType::Toggle, boolStr(cfg.hideOnInteractiveWithoutSneak))
        .config("resetDefaults", "Reset All Settings to Defaults", pl::modmenu::ConfigType::Button, "true");

    if (!builder.registerModule()) {
        uiLog().warn("Failed to register ModMenu module '{}'", kModuleId);
        return false;
    }

    pl::modmenu::setModuleEnabled(kModuleId, cfg.enabled);
    refreshSchema();
    return true;
}

void refreshSchema() {
    const ModConfig cfg = ConfigManager::instance().snapshot();
    pl::modmenu::setConfigSchemaJson(kModuleId, buildSchemaJson(cfg));
}

void unregisterMenu() {
    dismissLiveColorPreview();
    pl::modmenu::clearConfigSchema(kModuleId);
    pl::modmenu::unregisterModule(kModuleId);
}

} // namespace placepreview::ui
