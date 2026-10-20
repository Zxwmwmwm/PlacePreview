#pragma once

#include "buildghost/Types.hpp"

#include <string_view>

namespace buildghost {

class MaterialPalette {
public:
    // Resolves a clean, natural RGB tint for any vanilla or custom block identifier
    // and optional aux/dye value (e.g. bed/wool/concrete/glass color 0..15).
    [[nodiscard]] static RgbaColor colorForBlock(std::string_view blockIdentifier, int auxData = 0) noexcept;

    // Resolves standard Minecraft Bedrock 0..15 dye RGB color (BedItem / dyed blocks).
    [[nodiscard]] static RgbaColor colorForDyeIndex(int auxData) noexcept;
};

} // namespace buildghost
