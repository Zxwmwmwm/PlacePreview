#pragma once

#include "placepreview/Types.hpp"

#include <string_view>

namespace placepreview {

class MaterialPalette {
public:
    [[nodiscard]] static RgbaColor colorForBlock(std::string_view blockIdentifier, int auxData = 0) noexcept;
    [[nodiscard]] static RgbaColor colorForDyeIndex(int auxData) noexcept;
};

} // namespace placepreview
