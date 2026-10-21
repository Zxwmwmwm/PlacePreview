# BuildGhost

Real-time translucent 3D placement preview mod (`preload-native`, `arm64-v8a`, 16 KB page-size aligned) for **Minecraft: Bedrock Edition `1.26.52.03`** on Android via **LeviLauncher / Preloader**.

## Features

- **Continuous 60–120 FPS Center-Crosshair Tracking (Touch & Split Controls)**: Traces the live sub-tick interpolated render camera ray (`ScreenContext` camera eye position + live `pitch/yaw` look vector) through an exact 3D Amanatides-Woo voxel + sub-AABB raycaster across Creative (`11.5m`) and Survival/Adventure (`5.5m`) reaches — so the preview follows the exact center crosshair smoothly every frame on both Android Touch Mode and Split Controls without needing to tap first.
- **Immediate Validation & Instant Update Across All Gamemodes**: Evaluates read-only chunk/block states (`LevelChunk::getBlock`), live entity/player hitbox collision checks, Adventure-mode permissions, Spectator detection (`GameType == 6`), and multi-block support rules. Switching freely between Creative, Survival, Adventure, and Spectator updates immediately on the exact frame with zero crashes.
- **Clean Frosted-White 3D Geometry & Orientation**:
  - **Camera-Facing Exterior Shading**: Renders clean frosted-white translucent exterior faces with natural directional shading and deduplicated silhouette/corner edges — zero internal hidden-line clutter or back-wall double-blending.
  - **Stairs**: Exact upright and upside-down orientation (`subClick.y > 0.5`), 4-way yaw direction, and live outer/inner corner morphing with coplanar interior seam culling.
  - **Slabs & Slab Merges**: Upper/lower half-slab previews plus **smart half-slab merge diffing** — aiming a matching single slab at an existing bottom/top slab previews ONLY the missing upper/lower half slotting cleanly onto the existing slab.
  - **Logs, Pillars, 6-Way Utility Blocks, 2-Block Doors & Beds, Lily Pads & Offhand**: Full 3D geometry across pillars (`Y/X/Z`), 6-way directional blocks, 2-block doors (hinge side + ceiling check), 2-block beds (legs + head pillow + 16-color dye tinting), liquid-surface blocks, and optional offhand block fallback.
- **In-Game Mod Menu & Persistent Config**: Full Mod Menu v2 panel with 6 built-in presets (`Frosted White` default, `Classic Cyan`, `Material Tint`, `Crisp Wireframe`, `Subtle Minimal`, `High Contrast`), render style selector, custom RGB or automatic per-material tinting, fill/wireframe opacity sliders, optional 3D schematic orientation guides, and persistent `buildghost.json` storage.

## Building from Source

### Host Unit & Sanitizer Suite (Linux C++20 + ASan/UBSan)

```bash
cmake -S . -B build-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-host -j"$(nproc)"
ctest --test-dir build-host --output-on-failure
```

### Android `arm64-v8a` LeviPack

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r29
export PRELOADER_INCLUDE_DIR=/path/to/preloader-android/include
export PRELOADER_LIBRARY=/path/to/libpreloader.so
export FMT_INCLUDE_DIR=/path/to/fmt/include
bash ./scripts/build.sh
```

Outputs `artifacts/libBuildGhost.so` and `artifacts/BuildGhost.levipack`.

## Installation

1. Download **`BuildGhost.levipack`** from [Releases](https://github.com/Zxwmwmwm/BuildGhost/releases).
2. Import `BuildGhost.levipack` in **LeviLauncher** (`Mods -> Import`).
3. Launch Minecraft `1.26.52.03` and hold any placeable block to see its live 3D placement preview.

## License

Licensed under the [MIT License](LICENSE) — Copyright (c) 2026 **Zxwmwmwm**.
