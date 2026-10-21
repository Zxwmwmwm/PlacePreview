#pragma once

#include "placepreview/Config.hpp"

namespace placepreview::ui {

// Displays / updates a non-focusable, touch-passthrough live swatch card at the top center
// above the open Schema V2 Mod Menu on Android's UI thread while adjusting color or opacity.
void showLiveColorPreview(const ModConfig& cfg) noexcept;

// Cleanly dismisses and releases the live swatch overlay when the mod is disabled/unloaded.
void dismissLiveColorPreview() noexcept;

} // namespace placepreview::ui
