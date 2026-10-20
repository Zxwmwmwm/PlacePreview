#pragma once

#include <string_view>

namespace buildghost::version {

inline constexpr std::string_view Name = "BuildGhost";
inline constexpr std::string_view Id = "zxwmwmwm.build_ghost";
inline constexpr std::string_view Version = "1.0.0";
inline constexpr int Major = 1;
inline constexpr int Minor = 0;
inline constexpr int Patch = 0;
inline constexpr std::string_view Author = "Zxwmwmwm";
inline constexpr std::string_view TargetGameVersion = "1.26.52.03";
inline constexpr std::string_view Description =
    "Real-time translucent 3D placement preview showing exact block target position, orientation, slab merges, stairs, pillars, doors, and beds.";

} // namespace buildghost::version
