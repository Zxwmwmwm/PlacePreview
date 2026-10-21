#include "buildghost/ModMenuUi.hpp"

#include "buildghost/Config.hpp"
#include "buildghost/EngineBridge.hpp"
#include "buildghost/Version.hpp"

#include <pl/ModMenu.hpp>
#include <pl/ModMenuConfig.hpp>

#include <cstdio>
#include <string>

namespace buildghost::ui {

namespace {

std::string fmt2(float value) {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(value));
    return std::string(buf);
}

std::string buildSchemaJson(const ModConfig& cfg) {
    using pl::modmenu::ConfigChoiceStyleV2;
    using pl::modmenu::ConfigControlTypeV2;
    using pl::modmenu::ConfigNodeV2;
    using pl::modmenu::ConfigOptionV2;
    using pl::modmenu::ConfigSchemaBuilder;

    ConfigSchemaBuilder schema;
    schema.defaultCategory("appearance")
          .category("appearance", "Ghost Appearance",
                    "Translucent 3D preview style, opacity, directional shading, and material tinting.")
          .category("orientation", "Orientation & Behavior",
                    "3D log axis rings, 6-way redstone/hopper arrows, slab merge previews, and offhand support.")
          .category("maintenance", "Presets & Reset",
                    "Instant visual presets and factory reset controls.");

    schema.node(ConfigNodeV2{
        .id = "node_preset",
        .key = "preset",
        .category = "appearance",
        .title = "Visual Preset",
        .description = "Switch instantly between clean builder themes.",
        .section = "Theme",
        .type = ConfigControlTypeV2::Choice,
        .choiceStyle = ConfigChoiceStyleV2::Chips,
        .defaultValue = "Frosted White",
        .currentValue = cfg.preset,
        .hasCurrentValue = true,
        .options = {
            ConfigOptionV2{.value = "Frosted White", .label = "Frosted White"},
            ConfigOptionV2{.value = "Classic Cyan", .label = "Classic Cyan"},
            ConfigOptionV2{.value = "Material Tint", .label = "Material Tint"},
            ConfigOptionV2{.value = "Crisp Wireframe", .label = "Crisp Wireframe"},
            ConfigOptionV2{.value = "Subtle Minimal", .label = "Subtle Minimal"},
            ConfigOptionV2{.value = "High Contrast", .label = "High Contrast"},
            ConfigOptionV2{.value = "Custom", .label = "Custom"},
        },
    });

    schema.node(ConfigNodeV2{
        .id = "node_render_mode",
        .key = "renderMode",
        .category = "appearance",
        .title = "Render Style",
        .description = "Choose shaded translucent faces + edges, fill only, or wireframe only.",
        .section = "Theme",
        .type = ConfigControlTypeV2::Choice,
        .choiceStyle = ConfigChoiceStyleV2::Chips,
        .defaultValue = "Shaded + Wireframe",
        .currentValue = cfg.renderMode,
        .hasCurrentValue = true,
        .options = {
            ConfigOptionV2{.value = "Shaded + Wireframe", .label = "Shaded + Wireframe"},
            ConfigOptionV2{.value = "Translucent Fill", .label = "Translucent Fill"},
            ConfigOptionV2{.value = "Wireframe Only", .label = "Wireframe Only"},
        },
    });

    schema.node(ConfigNodeV2{
        .id = "node_color_mode",
        .key = "colorMode",
        .category = "appearance",
        .title = "Color Source",
        .description = "Match held block material color automatically or use custom accent RGB.",
        .section = "Color",
        .type = ConfigControlTypeV2::Choice,
        .choiceStyle = ConfigChoiceStyleV2::Chips,
        .defaultValue = "Custom Accent",
        .currentValue = cfg.colorMode,
        .hasCurrentValue = true,
        .options = {
            ConfigOptionV2{.value = "Custom Accent", .label = "Custom Accent"},
            ConfigOptionV2{.value = "Material Adaptive", .label = "Material Adaptive"},
        },
    });

    schema.node(ConfigNodeV2{
        .id = "node_fill_opacity",
        .key = "fillOpacity",
        .category = "appearance",
        .title = "Translucent Fill Opacity",
        .description = "Alpha opacity of the 3D ghost faces.",
        .section = "Opacity",
        .type = ConfigControlTypeV2::SliderFloat,
        .defaultValue = "0.36",
        .currentValue = fmt2(cfg.fillOpacity),
        .hasCurrentValue = true,
        .minValue = "0.05",
        .maxValue = "0.85",
        .step = "0.02",
    });

    schema.node(ConfigNodeV2{
        .id = "node_wire_opacity",
        .key = "wireframeOpacity",
        .category = "appearance",
        .title = "Edge & Guide Opacity",
        .description = "Alpha opacity of silhouette edges and orientation indicators.",
        .section = "Opacity",
        .type = ConfigControlTypeV2::SliderFloat,
        .defaultValue = "0.62",
        .currentValue = fmt2(cfg.wireframeOpacity),
        .hasCurrentValue = true,
        .minValue = "0.10",
        .maxValue = "1.00",
        .step = "0.02",
    });

    schema.node(ConfigNodeV2{
        .id = "node_accent_r",
        .key = "accentRed",
        .category = "appearance",
        .title = "Accent Red (0-255)",
        .description = "Red channel when Custom Accent mode is selected.",
        .section = "Custom Accent RGB",
        .type = ConfigControlTypeV2::SliderInt,
        .defaultValue = "255",
        .currentValue = std::to_string(cfg.accentRed),
        .hasCurrentValue = true,
        .minValue = "0",
        .maxValue = "255",
        .step = "1",
    });

    schema.node(ConfigNodeV2{
        .id = "node_accent_g",
        .key = "accentGreen",
        .category = "appearance",
        .title = "Accent Green (0-255)",
        .description = "Green channel when Custom Accent mode is selected.",
        .section = "Custom Accent RGB",
        .type = ConfigControlTypeV2::SliderInt,
        .defaultValue = "255",
        .currentValue = std::to_string(cfg.accentGreen),
        .hasCurrentValue = true,
        .minValue = "0",
        .maxValue = "255",
        .step = "1",
    });

    schema.node(ConfigNodeV2{
        .id = "node_accent_b",
        .key = "accentBlue",
        .category = "appearance",
        .title = "Accent Blue (0-255)",
        .description = "Blue channel when Custom Accent mode is selected.",
        .section = "Custom Accent RGB",
        .type = ConfigControlTypeV2::SliderInt,
        .defaultValue = "255",
        .currentValue = std::to_string(cfg.accentBlue),
        .hasCurrentValue = true,
        .minValue = "0",
        .maxValue = "255",
        .step = "1",
    });

    schema.node(ConfigNodeV2{
        .id = "node_shading",
        .key = "directionalShading",
        .category = "orientation",
        .title = "Directional 3D Face Shading",
        .description = "Applies natural top/side/bottom block light contrast for instant stair and slab depth.",
        .section = "3D Cues",
        .type = ConfigControlTypeV2::Toggle,
        .defaultValue = "true",
        .currentValue = cfg.directionalShading ? "true" : "false",
        .hasCurrentValue = true,
    });

    schema.node(ConfigNodeV2{
        .id = "node_guides",
        .key = "showOrientationGuides",
        .category = "orientation",
        .title = "Show 3D Orientation Guides",
        .description = "Renders log end-grain rings, 6-way active aperture arrows, and horizontal front indicators.",
        .section = "3D Cues",
        .type = ConfigControlTypeV2::Toggle,
        .defaultValue = "false",
        .currentValue = cfg.showOrientationGuides ? "true" : "false",
        .hasCurrentValue = true,
    });

    schema.node(ConfigNodeV2{
        .id = "node_subpip",
        .key = "showSubTargetPip",
        .category = "orientation",
        .title = "Show Sub-Block Target Pip",
        .description = "Marks exact sub-block hit point for upper/lower slab and stair placement.",
        .section = "3D Cues",
        .type = ConfigControlTypeV2::Toggle,
        .defaultValue = "false",
        .currentValue = cfg.showSubTargetPip ? "true" : "false",
        .hasCurrentValue = true,
    });

    schema.node(ConfigNodeV2{
        .id = "node_pulse",
        .key = "subtleBreathingPulse",
        .category = "orientation",
        .title = "Subtle Breathing Pulse",
        .description = "Gently modulates preview opacity so it stays distinct from real world blocks.",
        .section = "3D Cues",
        .type = ConfigControlTypeV2::Toggle,
        .defaultValue = "false",
        .currentValue = cfg.subtleBreathingPulse ? "true" : "false",
        .hasCurrentValue = true,
    });

    schema.node(ConfigNodeV2{
        .id = "node_offhand",
        .key = "enableOffhandPreview",
        .category = "orientation",
        .title = "Preview Offhand Blocks",
        .description = "Shows ghost preview for offhand blocks when holding tools or weapons in mainhand.",
        .section = "Gameplay",
        .type = ConfigControlTypeV2::Toggle,
        .defaultValue = "true",
        .currentValue = cfg.enableOffhandPreview ? "true" : "false",
        .hasCurrentValue = true,
    });

    schema.node(ConfigNodeV2{
        .id = "node_interactive_hide",
        .key = "hideOnInteractiveWithoutSneak",
        .category = "orientation",
        .title = "Hide on Containers Unless Sneaking",
        .description = "Hides preview when aiming at chests, furnaces, or doors without crouching.",
        .section = "Gameplay",
        .type = ConfigControlTypeV2::Toggle,
        .defaultValue = "false",
        .currentValue = cfg.hideOnInteractiveWithoutSneak ? "true" : "false",
        .hasCurrentValue = true,
    });

    schema.node(ConfigNodeV2{
        .id = "node_reset_defaults",
        .key = "resetDefaults",
        .category = "maintenance",
        .title = "Reset Configuration Defaults",
        .description = "Restores all BuildGhost settings back to Classic Cyan defaults.",
        .section = "Reset",
        .type = ConfigControlTypeV2::Button,
    });

    return schema.toJson();
}

void onModuleToggle(std::string_view /*moduleId*/, bool enabled) {
    ConfigManager::instance().setEnabled(enabled);
    EngineBridge::instance().notifyConfigUpdated(ConfigManager::instance().snapshot());
}

void onConfigChanged(std::string_view /*moduleId*/, std::string_view key, std::string_view value) {
    if (key == "resetDefaults") {
        ConfigManager::instance().resetDefaults();
    } else {
        ConfigManager::instance().applyKeyValue(key, value);
    }
    EngineBridge::instance().notifyConfigUpdated(ConfigManager::instance().snapshot());
    refreshSchema();
}

} // namespace

void refreshSchema() {
    const ModConfig cfg = ConfigManager::instance().snapshot();
    pl::modmenu::setConfigSchemaJson(version::Name, buildSchemaJson(cfg));
}

bool registerMenu(std::string_view ownerModId) {
    const ModConfig cfg = ConfigManager::instance().snapshot();

    pl::modmenu::ModuleBuilder builder{std::string(version::Name), std::string(version::Name)};
    if (!ownerModId.empty()) {
        builder.modId(std::string(ownerModId));
    }

    builder.description(std::string(version::Description))
           .defaultEnabled(cfg.enabled)
           .hideInHudEditor(true)
           .onToggle(onModuleToggle)
           .onConfigChanged(onConfigChanged)
           .config("preset", "Visual Preset", pl::modmenu::ConfigType::Radio, cfg.preset,
                   "Classic Cyan,Material Tint,Crisp Wireframe,Subtle Minimal,High Contrast,Custom")
           .config("renderMode", "Render Style", pl::modmenu::ConfigType::Radio, cfg.renderMode,
                   "Shaded + Wireframe,Translucent Fill,Wireframe Only")
           .config("colorMode", "Color Source", pl::modmenu::ConfigType::Radio, cfg.colorMode,
                   "Custom Accent,Material Adaptive")
           .config("fillOpacity", "Fill Opacity", pl::modmenu::ConfigType::SliderFloat,
                   fmt2(cfg.fillOpacity), "0.05", "0.85")
           .config("wireframeOpacity", "Wireframe Opacity", pl::modmenu::ConfigType::SliderFloat,
                   fmt2(cfg.wireframeOpacity), "0.10", "1.00")
           .config("accentRed", "Accent Red", pl::modmenu::ConfigType::SliderInt,
                   std::to_string(cfg.accentRed), "0", "255")
           .config("accentGreen", "Accent Green", pl::modmenu::ConfigType::SliderInt,
                   std::to_string(cfg.accentGreen), "0", "255")
           .config("accentBlue", "Accent Blue", pl::modmenu::ConfigType::SliderInt,
                   std::to_string(cfg.accentBlue), "0", "255")
           .config("directionalShading", "Directional Shading", pl::modmenu::ConfigType::Toggle,
                   cfg.directionalShading ? "true" : "false")
           .config("showOrientationGuides", "Orientation Guides", pl::modmenu::ConfigType::Toggle,
                   cfg.showOrientationGuides ? "true" : "false")
           .config("showSubTargetPip", "Sub-Block Target Pip", pl::modmenu::ConfigType::Toggle,
                   cfg.showSubTargetPip ? "true" : "false")
           .config("subtleBreathingPulse", "Subtle Breathing Pulse", pl::modmenu::ConfigType::Toggle,
                   cfg.subtleBreathingPulse ? "true" : "false")
           .config("enableOffhandPreview", "Offhand Block Preview", pl::modmenu::ConfigType::Toggle,
                   cfg.enableOffhandPreview ? "true" : "false")
           .config("hideOnInteractiveWithoutSneak", "Hide on Containers", pl::modmenu::ConfigType::Toggle,
                   cfg.hideOnInteractiveWithoutSneak ? "true" : "false")
           .config("resetDefaults", "Reset Defaults", pl::modmenu::ConfigType::Button, "false");

    if (!builder.registerModule()) {
        return false;
    }

    refreshSchema();
    return true;
}

void unregisterMenu() {
    pl::modmenu::clearConfigSchema(version::Name);
    pl::modmenu::unregisterModule(version::Name);
}

} // namespace buildghost::ui
