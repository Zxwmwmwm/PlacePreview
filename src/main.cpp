#include "placepreview/Config.hpp"
#include "placepreview/EngineBridge.hpp"
#include "placepreview/ModMenuUi.hpp"
#include "placepreview/Version.hpp"

#include <pl/Mod.hpp>

namespace {

class PlacePreviewMod {
public:
    static PlacePreviewMod& instance() noexcept {
        static PlacePreviewMod mod;
        return mod;
    }

    bool load(pl::mod::ModContext& context) {
        placepreview::ConfigManager::instance().init(context.configDir());
        context.logger().info(
            "{} v{} loaded (config: {})",
            placepreview::version::Name,
            placepreview::version::Version,
            context.configDir().string());
        return true;
    }

    bool enable(pl::mod::ModContext& context) {
        if (!placepreview::EngineBridge::instance().installHooks()) {
            context.logger().error(
                "Failed to install native placement preview hook for {}",
                placepreview::version::Name);
            return false;
        }

        if (!placepreview::ui::registerMenu(context.info().id)) {
            context.logger().warn(
                "Mod Menu UI registration non-fatal failure; JSON config remains active");
        }

        context.logger().info(
            "{} v{} enabled",
            placepreview::version::Name,
            placepreview::version::Version);
        return true;
    }

    bool disable(pl::mod::ModContext& context) {
        placepreview::ui::unregisterMenu();
        placepreview::EngineBridge::instance().uninstallHooks();
        context.logger().info("{} disabled", placepreview::version::Name);
        return true;
    }

    bool unload(pl::mod::ModContext& context) {
        placepreview::ui::unregisterMenu();
        placepreview::EngineBridge::instance().uninstallHooks();
        context.logger().info("{} unloaded", placepreview::version::Name);
        return true;
    }
};

} // namespace

PL_REGISTER_MOD(PlacePreviewMod, PlacePreviewMod::instance())
