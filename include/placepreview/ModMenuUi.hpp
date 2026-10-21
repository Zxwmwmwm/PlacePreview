#pragma once

#include <string_view>

namespace placepreview::ui {

[[nodiscard]] bool registerMenu(std::string_view ownerModId);
void refreshSchema();
void unregisterMenu();

} // namespace placepreview::ui
