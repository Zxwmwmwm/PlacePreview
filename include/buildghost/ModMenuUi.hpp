#pragma once

#include <string_view>

namespace buildghost::ui {

[[nodiscard]] bool registerMenu(std::string_view ownerModId);
void refreshSchema();
void unregisterMenu();

} // namespace buildghost::ui
