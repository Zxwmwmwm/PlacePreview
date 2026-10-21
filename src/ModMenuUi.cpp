#include "placepreview/ModMenuUi.hpp"

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
    return buf;
}

std::string boolStr(bool v) {
    return v ? "true" : "false";
}

} // namespace

bool registerMenu(std::string_view ownerModId) {
    const ModConfig def{};
    const ModConfig cfg = ConfigManager::instance().snapshot();

    pl::modmenu::ModuleInfo info{};
    info.moduleId = kModuleId;
    info.displayName = std::string(version::Name);
    info.description = std::string(version::Description);
    info.modId = ownerModId.empty() ? std::string(version::Id) : std::string(ownerModId);
    info.defaultEnabled = cfg.enabled;
    info.hideInHudEditor = true;

    info.configs = {
        {"preset", "Quick Preset", pl::modmenu::ConfigType::Radio, def.preset, "", "", ""},
        {"previewColor", "Preview Color", pl::modmenu::ConfigType::Color, def.previewColorHex(), "", "", ""},
        {"colorMode", "Color Mode", pl::modmenu::ConfigType::Radio, def.colorMode, "", "", ""},
        {"fillOpacity", "Fill Opacity", pl::modmenu::ConfigType::SliderFloat, fmtFloat(def.fillOpacity), "0.05", "1.00", ""},
        {"wireframeOpacity", "Outline Opacity", pl::modmenu::ConfigType::SliderFloat, fmtFloat(def.wireframeOpacity), "0.05", "1.00", ""},
        {"smoothAnimation", "Smooth Block Transition", pl::modmenu::ConfigType::Toggle, boolStr(def.smoothAnimation), "", "", ""},
        {"animationSpeed", "Transition Speed", pl::modmenu::ConfigType::SliderFloat, fmtFloat(def.animationSpeed), "4.00", "40.00", "smoothAnimation"},
        {"renderMode", "Render Style", pl::modmenu::ConfigType::Radio, def.renderMode, "", "", ""},
        {"directionalShading", "3D Surface Shading", pl::modmenu::ConfigType::Toggle, boolStr(def.directionalShading), "", "", ""},
        {"showOrientationGuides", "Directional Guide", pl::modmenu::ConfigType::Toggle, boolStr(def.showOrientationGuides), "", "", ""},
        {"subtleBreathingPulse", "Subtle Breathing Pulse", pl::modmenu::ConfigType::Toggle, boolStr(def.subtleBreathingPulse), "", "", ""},
        {"hideOnInteractiveWithoutSneak", "Hide on Interactive Blocks", pl::modmenu::ConfigType::Toggle, boolStr(def.hideOnInteractiveWithoutSneak), "", "", ""},
        {"resetDefaults", "Reset All Settings to Defaults", pl::modmenu::ConfigType::Button, "true", "", "", ""},
    };

    info.onToggle = [](std::string_view, bool enabled) {
        ConfigManager::instance().setEnabled(enabled);
    };

    info.onConfigChanged = [](std::string_view, std::string_view key, std::string_view value) {
        ConfigManager::instance().applyKeyValue(key, value);
        // Refresh V2 UI widgets immediately ONLY when applying a Preset or Reset Defaults so live
        // color-picker & slider drags stay silky smooth without recreating the active popup!
        if (key == "preset" || key == "resetDefaults") {
            refreshSchema();
        }
    };

    if (!pl::modmenu::registerModule(info)) {
        uiLog().warn("Failed to register ModMenu module '{}'", kModuleId);
        return false;
    }

    pl::modmenu::setModuleEnabled(kModuleId, cfg.enabled);
    refreshSchema();
    return true;
}

void refreshSchema() {
    const ModConfig def{};
    const ModConfig cfg = ConfigManager::instance().snapshot();

    pl::modmenu::ConfigSchemaBuilder builder;
    builder
        .defaultCategory("color")
        .category("color", "Color & Opacity", "Interactive color slider with live swatch preview, presets, and opacity controls")
        .category("motion", "Animation & Style", "Smooth block-to-block glide speed, render style, and behavior");

    builder.node({
        .id = "preset",
        .key = "preset",
        .category = "color",
        .title = "Quick Preset",
        .description = "Apply a curated color and opacity profile with one tap.",
        .section = "Color & Presets",
        .type = pl::modmenu::ConfigControlTypeV2::Choice,
        .choiceStyle = pl::modmenu::ConfigChoiceStyleV2::Chips,
        .defaultValue = def.preset,
        .currentValue = cfg.preset,
        .hasCurrentValue = true,
        .options = {
            {"Frosted White", "Frosted White", "Clean bright white glass fill + crisp outline"},
            {"Classic Cyan", "Classic Cyan", "Translucent cyan preview with bright edges"},
            {"Material Adaptive", "Material Adaptive", "Automatically tints to match wood, stone, copper, wool, or glass"},
            {"Crisp Wireframe", "Crisp Wireframe", "Pure clean block outline"},
            {"Subtle Minimal", "Subtle Minimal", "Soft low-opacity preview"},
            {"High Contrast", "High Contrast", "Vivid golden-amber preview for bright biomes"},
        },
    });

    builder.node({
        .id = "previewColor",
        .key = "previewColor",
        .category = "color",
        .title = "Preview Color",
        .description = "Slide to pick any custom color & intensity with live visible color preview.",
        .section = "Color & Presets",
        .type = pl::modmenu::ConfigControlTypeV2::Color,
        .defaultValue = def.previewColorHex(),
        .currentValue = cfg.previewColorHex(),
        .hasCurrentValue = true,
        .colorAlpha = true,
    });

    builder.node({
        .id = "colorMode",
        .key = "colorMode",
        .category = "color",
        .title = "Color Source",
        .description = "Use your chosen Preview Color or automatically tint by block material.",
        .section = "Color & Presets",
        .type = pl::modmenu::ConfigControlTypeV2::Choice,
        .choiceStyle = pl::modmenu::ConfigChoiceStyleV2::Segmented,
        .defaultValue = def.colorMode,
        .currentValue = cfg.colorMode,
        .hasCurrentValue = true,
        .options = {
            {"Custom Accent", "Preview Color", "Use your selected custom color picker value"},
            {"Material Adaptive", "Material Adaptive", "Match held block material automatically"},
        },
    });

    builder.node({
        .id = "fillOpacity",
        .key = "fillOpacity",
        .category = "color",
        .title = "Fill Opacity",
        .description = "Opacity of translucent 3D block faces.",
        .section = "Opacity",
        .type = pl::modmenu::ConfigControlTypeV2::SliderFloat,
        .defaultValue = fmtFloat(def.fillOpacity),
        .currentValue = fmtFloat(cfg.fillOpacity),
        .hasCurrentValue = true,
        .minValue = "0.05",
        .maxValue = "1.00",
        .step = "0.02",
    });

    builder.node({
        .id = "wireframeOpacity",
        .key = "wireframeOpacity",
        .category = "color",
        .title = "Outline Opacity",
        .description = "Opacity of crisp outer 3D block edges.",
        .section = "Opacity",
        .type = pl::modmenu::ConfigControlTypeV2::SliderFloat,
        .defaultValue = fmtFloat(def.wireframeOpacity),
        .currentValue = fmtFloat(cfg.wireframeOpacity),
        .hasCurrentValue = true,
        .minValue = "0.05",
        .maxValue = "1.00",
        .step = "0.02",
    });

    builder.node({
        .id = "smoothAnimation",
        .key = "smoothAnimation",
        .category = "motion",
        .title = "Smooth Block Transition",
        .description = "Smoothly glide and morph the preview from block to block instead of popping out of nowhere.",
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
        .description = "Higher values glide faster across blocks; lower values float more gently.",
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
        .id = "renderMode",
        .key = "renderMode",
        .category = "motion",
        .title = "Render Style",
        .description = "Choose shaded 3D fill + crisp wireframe, pure wireframe, or translucent fill.",
        .section = "Appearance",
        .type = pl::modmenu::ConfigControlTypeV2::Choice,
        .choiceStyle = pl::modmenu::ConfigChoiceStyleV2::Segmented,
        .defaultValue = def.renderMode,
        .currentValue = cfg.renderMode,
        .hasCurrentValue = true,
        .options = {
            {"Shaded + Wireframe", "Shaded + Wireframe", "Translucent shaded 3D faces with crisp edges"},
            {"Wireframe Only", "Wireframe Only", "Clean outer block wireframe"},
            {"Translucent Fill", "Translucent Fill", "Shaded translucent faces without edge lines"},
        },
    });

    builder.node({
        .id = "directionalShading",
        .key = "directionalShading",
        .category = "motion",
        .title = "3D Surface Shading",
        .description = "Subtle top/north-south/east-west face lighting for clear 3D depth perception.",
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
        .description = "Subtle orientation cue on directional blocks (stairs, observers, pillars, doors).",
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
        .description = "Gentle opacity pulse while holding position.",
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
        .description = "Hide placement preview when aiming at chests, crafting tables, or furnaces while standing.",
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
        .description = "Restore factory default color, animation, and opacity settings.",
        .section = "Maintenance",
        .type = pl::modmenu::ConfigControlTypeV2::Button,
        .actionValue = "true",
    });

    pl::modmenu::setConfigSchemaJson(kModuleId, builder.toJson());
}

void unregisterMenu() {
    pl::modmenu::clearConfigSchema(kModuleId);
    pl::modmenu::unregisterModule(kModuleId);
}

} // namespace placepreview::ui
