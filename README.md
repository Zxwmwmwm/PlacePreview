# BuildGhost

Real-time translucent 3D placement preview mod (`preload-native`, `arm64-v8a`, 16 KB page-size aligned) for **Minecraft: Bedrock Edition `1.26.52.03`** on Android via **LeviLauncher / Preloader**.

## Features

- **Exact Target Position & Replacement Resolution**: Evaluates `Item::_calculatePlacePos` (`Item::vtable[129]`) live on the render thread each frame so tall grass, ferns, snow layers, water, lava, and neighbor face offsets resolve to the exact world block cell where the block will land.
- **Immediate Validation & Instant Update**: Runs read-only `BlockSource::mayPlace` (`BlockSource::vtable[57]`) with live entity/player collision checks, Adventure-mode `can_place_on` validation, Spectator detection, and multi-block support checks. The preview **disappears on the exact frame** placement becomes invalid (sky/miss, out of reach, entity obstruction, unsupported torch/door/bed, world height limit) and **updates on the exact frame** any block coordinate, clicked face, sub-block hit point, player rotation, or held item slot changes.
- **True 3D Orientation & Multi-Box Geometry**:
  - **Stairs**: Exact upright and upside-down orientation (`subClick.y > 0.5`), 4-way yaw direction, and live outer/inner corner morphing extracted directly from `BlockType::addAABBs` (`BlockType::vtable[7]`), with coplanar interior seam culling so translucent faces never double-blend.
  - **Slabs & Slab Merges**: Upper/lower half-slab previews plus **smart half-slab merge diffing** — aiming a matching single slab at an existing bottom/top slab previews ONLY the missing upper/lower half slotting cleanly onto the existing slab.
  - **Logs & Pillars**: Decodes `pillar_axis` (`Y`, `X`, `Z`) directly from the placed `Block` bitfield and draws 3D octagonal end-grain rings on both cut ends plus bark grain flutes along the log axis.
  - **6-Way Redstone & Utility Blocks**: Displays active output/front aperture frames and 3D directional arrows across all 6 faces (`Up`, `Down`, `North`, `South`, `West`, `East`) for Observers, Pistons, Sticky Pistons, Dispensers, Droppers, Hoppers, Barrels, and Rods.
  - **2-Block Doors & Beds**: Full 2-cell validation (`[y, y+1]` for Doors with left/right hinge calculation; `[foot, head]` for Beds with 4 corner legs, raised head pillow, and exact 16-color dye tinting).
  - **Water-Surface & Offhand Blocks**: Supports liquid-surface raycasts (`HitResult::mLiquid`) for Lily Pads and optional automatic fallback to offhand blocks when holding tools in mainhand.
- **In-Game Mod Menu & Persistent Config**: Full Mod Menu v2 panel with 5 built-in presets (`Classic Cyan`, `Material Tint`, `Crisp Wireframe`, `Subtle Minimal`, `High Contrast`), render style selector, custom RGB or automatic per-material tinting, fill/wireframe opacity sliders, and persistent `buildghost.json` storage.

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
