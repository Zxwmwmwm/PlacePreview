#include "buildghost/Config.hpp"
#include "buildghost/EngineBridge.hpp"
#include "buildghost/ModMenuUi.hpp"
#include "buildghost/Version.hpp"

#include <pl/Mod.hpp>

namespace {

class BuildGhostMod {
public:
    static BuildGhostMod& instance() noexcept {
        static BuildGhostMod mod;
        return mod;
    }

    bool load(pl::mod::ModContext& context) {
        buildghost::ConfigManager::instance().init(context.configDir());
        context.logger().info(
            "{} v{} loaded (config: {})",
            buildghost::version::Name,
            buildghost::version::Version,
            context.configDir().string());
        return true;
    }

    bool enable(pl::mod::ModContext& context) {
        if (!buildghost::EngineBridge::instance().installHooks()) {
            context.logger().error(
                "Failed to install native placement preview hook for {}",
                buildghost::version::Name);
            return false;
        }

        if (!buildghost::ui::registerMenu(context.info().id)) {
            context.logger().warn(
                "Mod Menu UI registration non-fatal failure; JSON config remains active");
        }

        context.logger().info(
            "{} v{} enabled",
            buildghost::version::Name,
            buildghost::version::Version);
        return true;
    }

    bool disable(pl::mod::ModContext& context) {
        buildghost::ui::unregisterMenu();
        buildghost::EngineBridge::instance().uninstallHooks();
        context.logger().info("{} disabled", buildghost::version::Name);
        return true;
    }

    bool unload(pl::mod::ModContext& context) {
        buildghost::ui::unregisterMenu();
        buildghost::EngineBridge::instance().uninstallHooks();
        context.logger().info("{} unloaded", buildghost::version::Name);
        return true;
    }
};

} // namespace

PL_REGISTER_MOD(BuildGhostMod, BuildGhostMod::instance())
