# Place Preview

Smooth, accurate real-time 3D block placement preview mod (`preload-native`, `arm64-v8a`, 16 KB page-size aligned) for **Minecraft: Bedrock Edition `1.26.52.03`** on Android via **LeviLauncher / Preloader**.

## Features

- **Exact 3D Block & Item Shape Accuracy**:
  - Reads live engine block states and outlines (`ItemStackBase::mBlock`, `BlockTypeRegistry`, `BlockType::getPlacementBlock`, and `BlockType::getOutline`) across vanilla and custom addon blocks while immediately hiding when holding non-placeable tools, weapons, food, or items.
  - True 3D oriented geometry (`OrientedBox`) for **16-angle standing signs** (wooden post + full-width sign board rotated to your exact yaw angle), **hanging signs** (ceiling dual chains + board & horizontal wall side-arm bracket), **standing & wall banners**, **16-angle skulls & heads**, **angled wall torches**, and **slanted lecterns**.
  - Multi-part 3D geometry with coplanar interior seam culling for **stairs** (upright, upside-down, outer & inner corners), **slabs & half-slab merges**, **2-block doors** (exact hinge side), **2-block beds** (legs + pillow + 16 dye colors), **walls / fences / panes / iron bars** (live neighbor connections), **fence gates**, **hoppers**, **anvils**, **cauldrons**, **campfires**, **lanterns**, **1–4 candle clusters**, **pots**, **frames**, and **water-surface blocks**.
- **Smooth 3D Block-to-Block Transition Animation**:
  - Glides smoothly from block to block instead of popping instantaneously out of nowhere, morphs sub-block slab/rotation changes cleanly, and gently fades in and out when aiming at valid surfaces vs sky/out-of-reach targets.
- **Interactive Slider Color Changer with Live Swatch Preview**:
  - Native Mod Menu `#AARRGGBB` slider & spectrum color picker with live visible color preview, quick style presets (`Frosted White`, `Classic Cyan`, `Material Adaptive`, `Crisp Wireframe`, `Subtle Minimal`, `High Contrast`), opacity controls, and clean Bedrock-only settings.
- **60–120 FPS Center-Crosshair Tracking In both survival and creative Gamemodes**:
  - Works seamlessly on both Android Touch Mode and Split Controls across Creative (`11.5m`), Survival (`5.5m`), and Adventure (`5.5m`), hiding cleanly in Spectator mode.

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

Outputs `artifacts/libPlacePreview.so` and `artifacts/PlacePreview.levipack`.

## Installation

1. Download **`PlacePreview.levipack`** from [Releases](https://github.com/Zxwmwmwm/PlacePreview/releases).
2. Import `PlacePreview.levipack` in **LeviLauncher** (`Mods -> Import`).
3. Launch Minecraft `1.26.52.03` and hold any placeable block to see its live 3D placement preview.

## License

Licensed under the [MIT License](LICENSE) — Copyright (c) 2026 **Zxwmwmwm**.
