# Place Preview

Smooth, accurate real-time 3D block placement preview mod (`preload-native`, `arm64-v8a`, 16 KB page-size aligned) for **Minecraft: Bedrock Edition `1.26.52.03`** on Android via **LeviLauncher / Preloader**.

## Features

- **Exact 3D Block & Item Shape Accuracy (Above Ground & Underwater)**:
  - Reads live engine block states and outlines (`ItemStackBase::mBlock`, `BlockTypeRegistry`, `BlockType::getPlacementBlock`, and `BlockType::getOutline`) across vanilla and custom addon blocks while passing cleanly through water, lava, and bubble columns underwater and hiding immediately when holding non-placeable tools, weapons, or items.
  - True 3D oriented geometry (`OrientedBox`) for **16-angle standing signs** (wooden post + full-width sign board rotated to your exact yaw angle), **hanging signs** (ceiling dual chains + board & horizontal wall side-arm bracket), **standing & wall banners**, **armor stands**, **16-angle skulls & heads**, **angled wall torches**, and **slanted lecterns**.
  - Multi-part 3D geometry with coplanar interior seam culling for **item frames & glow item frames**, **paintings**, **sea pickles (`1–4` clusters)**, **turtle eggs (`1–4` clusters)**, **candles (`1–4` clusters)**, **conduits**, **heavy cores**, **end rods & lightning rods**, **stairs** (upright, upside-down, outer & inner corners), **slabs & half-slab merges**, **2-block doors** (exact hinge side), **2-block beds** (legs + pillow + 16 dye colors), **walls / fences / panes / iron bars** (live neighbor connections), **fence gates**, **hoppers**, **anvils**, **cauldrons**, **campfires**, **lanterns**, **pots**, **coral & fans**, and **water-surface blocks**.
- **Smooth 3D Glide & Held-Block Shape Morphing**:
  - Smoothly glides from block to block AND morphs 3D shapes and rotations continuously whenever switching held items in the hotbar, fading in and out smoothly when aiming at valid surfaces vs open sky.
- **Mod Menu Schema V2, Presets, Adaptive Tinting & True Hardware Opacity**:
  - Categorized **Schema V2** UI (`Color & Style` and `Motion & Behavior`) featuring one-tap **Quick Presets** (`Frosted White`, `Classic Cyan`, `Material Adaptive`, `Crisp Wireframe`, `Subtle Minimal`, `High Contrast`), **Render Style** (`Shaded + Wireframe`, `Wireframe Only`, `Translucent Fill`), **Material Adaptive** block color matching, **Preview Color** (`#AARRGGBB`), and independent hardware alpha-blended **Fill Opacity** & **Outline Opacity** (`0.00–1.00`) that persist reliably across menu sessions and game restarts.
- **60–120 FPS Center-Crosshair Tracking Across All Gamemodes**:
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
