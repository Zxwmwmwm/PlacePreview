#include "placepreview/ModMenuUi.hpp"

#include "placepreview/Config.hpp"
#include "placepreview/Version.hpp"

#include <pl/Logger.hpp>
#include <pl/ModMenu.hpp>

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
    const ModConfig cfg = ConfigManager::instance().snapshot();

    // Register directly via Preloader's native ModuleBuilder (without overriding via V2 schema) so:
    //   1. `ConfigType::Color` renders inline on the module page with the live color preview swatch
    //      and direct A / R / G / B sliders side-by-side.
    //   2. Every entry initializes from the saved `placepreview.json` values (`cfg.*`) and updates
    //      `mod.configs[i].current_value` in Preloader + `placepreview.json` on disk immediately,
    //      persisting reliably across closing/reopening Mod Menu AND restarting the game.
    pl::modmenu::ModuleBuilder builder(kModuleId, std::string(version::Name));
    builder.description(std::string(version::Description))
        .modId(ownerModId.empty() ? std::string(version::Id) : std::string(ownerModId))
        .defaultEnabled(cfg.enabled)
        .hideInHudEditor(true)
        .onToggle([](std::string_view, bool enabled) {
            ConfigManager::instance().setEnabled(enabled);
        })
        .onConfigChanged([](std::string_view, std::string_view key, std::string_view value) {
            ConfigManager::instance().applyKeyValue(key, value);
        });

    builder.config(
        "previewColor",
        "Preview Color",
        pl::modmenu::ConfigType::Color,
        cfg.previewColorHex());

    builder.config(
        "colorMode",
        "Color Mode",
        pl::modmenu::ConfigType::Radio,
        cfg.colorMode,
        "Custom Color,Material Adaptive");

    builder.config(
        "renderMode",
        "Render Style",
        pl::modmenu::ConfigType::Radio,
        cfg.renderMode,
        "Shaded + Wireframe,Wireframe Only,Translucent Fill");

    builder.config(
        "fillOpacity",
        "Fill Opacity",
        pl::modmenu::ConfigType::SliderFloat,
        fmtFloat(cfg.fillOpacity),
        "0.00",
        "0.90");

    builder.config(
        "wireframeOpacity",
        "Outline Opacity",
        pl::modmenu::ConfigType::SliderFloat,
        fmtFloat(cfg.wireframeOpacity),
        "0.05",
        "1.00");

    builder.config(
        "smoothAnimation",
        "Smooth Transition",
        pl::modmenu::ConfigType::Toggle,
        boolStr(cfg.smoothAnimation));

    builder.config(
        "animationSpeed",
        "Transition Speed",
        pl::modmenu::ConfigType::SliderFloat,
        fmtFloat(cfg.animationSpeed),
        "4.00",
        "40.00",
        "smoothAnimation");

    builder.config(
        "directionalShading",
        "3D Surface Shading",
        pl::modmenu::ConfigType::Toggle,
        boolStr(cfg.directionalShading));

    builder.config(
        "showOrientationGuides",
        "Directional Guide",
        pl::modmenu::ConfigType::Toggle,
        boolStr(cfg.showOrientationGuides));

    builder.config(
        "subtleBreathingPulse",
        "Breathing Pulse",
        pl::modmenu::ConfigType::Toggle,
        boolStr(cfg.subtleBreathingPulse));

    builder.config(
        "hideOnInteractiveWithoutSneak",
        "Hide on Chests Unless Sneaking",
        pl::modmenu::ConfigType::Toggle,
        boolStr(cfg.hideOnInteractiveWithoutSneak));

    if (!builder.registerModule()) {
        uiLog().warn("Failed to register ModMenu module '{}'", kModuleId);
        return false;
    }

    pl::modmenu::setModuleEnabled(kModuleId, cfg.enabled);
    return true;
}

void refreshSchema() {
    // Module controls use Preloader's persistent in-memory `current_value` table directly.
}

void unregisterMenu() {
    pl::modmenu::unregisterModule(kModuleId);
}

} // namespace placepreview::ui
