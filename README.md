# Place Preview

Smooth, accurate real-time 3D block placement preview mod (`preload-native`, `arm64-v8a`, 16 KB page-size aligned) for **Minecraft: Bedrock Edition `1.26.52.03`** on Android via **LeviLauncher / Preloader**.

## Features

- **Exact 3D Block & Item Shape Accuracy Across All Vanilla & Addon Blocks**:
  - Resolves placed `BlockType` and state properties across all three native Bedrock systems (`ItemStackBase::mBlock`, `BlockItem::mBlockType`, `BlockPlanterItemComponent`, `BlockTypeRegistry`, classic `mStates` RB-trees, and 1.21–1.26 `BlockTrait` state vectors), querying `BlockType::getPlacementBlock`, `BlockType::getOutline`, `Block::getRandomOffset`, and `FlowerBedSelectionBox::getOutline` directly in real time while passing cleanly through water, lava, and bubble columns underwater.
  - Full outline fidelity across **all flowers & flora** (per-cell `RandomOffset` single flowers, tulips, orchids, eyeblossoms, golden dandelions, torchflowers, bushes, cactus flowers, pale hanging moss, **2-block tall flowers & plants** with upper-cell clearance validation, and **1–4 quadrant-stacking `pink_petals` / `wildflowers` / `leaf_litter`**), **8-part directional 3D Copper Golem Statues** across all oxidation & waxed variants, **wall shelves (`3/16` depth)**, **copper bars**, **16-angle standing signs**, **ceiling & wall hanging signs**, **banners**, **armor stands**, **16-angle skulls & heads**, **angled wall torches**, **slanted lecterns**, **item frames**, **paintings**, **sea pickles / turtle eggs / candles (`1–4` clusters)**, **stairs**, **slabs & merges**, **doors**, **beds**, **hoppers**, **anvils**, **cauldrons**, **campfires**, and **lanterns**.
- **Smooth 3D Glide & Held-Block Shape Morphing**:
  - Smoothly glides from block to block AND morphs 3D shapes and rotations continuously whenever switching held items in the hotbar, fading in and out smoothly when aiming at valid surfaces vs open sky.
- **Schema V2 Mod Menu + Live Floating Color Swatch Preview (No Need to Close Mod Menu)**:
  - Categorized **Schema V2** UI featuring a **live floating color swatch pill** that floats right above both the Mod Menu and the `Preview Color` `RGBA` popup in real time as you adjust **Color Swatch** chips, **Red / Green / Blue / Alpha** sliders, **Preview Color (`#AARRGGBB`)**, **Fill Opacity**, or **Outline Opacity**—so you see the exact color, outline stroke, and hexadecimal value live **without ever closing the Mod Menu**, with automatic persistence across sessions and restarts.
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
