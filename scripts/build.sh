#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NDK_ROOT="${ANDROID_NDK_HOME:-/var/tmp/android-ndk-r29}"
PRELOADER_INC="${PRELOADER_INCLUDE_DIR:-/var/tmp/ws_preloader/include}"
PRELOADER_LIB="${PRELOADER_LIBRARY:-/var/tmp/ws_preloader/libpreloader.so}"
FMT_INC="${FMT_INCLUDE_DIR:-/var/tmp/fmt-11.2.0/include}"

BUILD_DIR="${BUILD_DIR:-/tmp/placepreview-android-arm64}"
OUT_DIR="${REPO_ROOT}/artifacts"
mkdir -p "${BUILD_DIR}" "${OUT_DIR}"

if command -v cmake >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
    cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="${NDK_ROOT}/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-28 \
        -DCMAKE_BUILD_TYPE=Release \
        -DPRELOADER_INCLUDE_DIR="${PRELOADER_INC}" \
        -DPRELOADER_LIBRARY="${PRELOADER_LIB}" \
        -DFMT_INCLUDE_DIR="${FMT_INC}"
    cmake --build "${BUILD_DIR}" --target PlacePreview -j"$(nproc)"
    cp -f "${BUILD_DIR}/libPlacePreview.so" "${OUT_DIR}/libPlacePreview.so"
    "${NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-unneeded "${OUT_DIR}/libPlacePreview.so"
else
    CXX="${NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang++"
    STRIP="${NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
    "${CXX}" -std=c++20 -O2 -fPIC -shared \
        -ffunction-sections -fdata-sections \
        -fvisibility=hidden -fvisibility-inlines-hidden \
        -Wall -Wextra -Wno-missing-field-initializers \
        -DFMT_HEADER_ONLY=1 \
        -I"${REPO_ROOT}/include" \
        -I"${PRELOADER_INC}" \
        -I"${FMT_INC}" \
        "${REPO_ROOT}/src/Config.cpp" \
        "${REPO_ROOT}/src/MaterialPalette.cpp" \
        "${REPO_ROOT}/src/StateInspector.cpp" \
        "${REPO_ROOT}/src/PlacementResolver.cpp" \
        "${REPO_ROOT}/src/PreviewAnimator.cpp" \
        "${REPO_ROOT}/src/GhostMesher.cpp" \
        "${REPO_ROOT}/src/EngineBridge.cpp" \
        "${REPO_ROOT}/src/ModMenuUi.cpp" \
        "${REPO_ROOT}/src/main.cpp" \
        "${PRELOADER_LIB}" \
        -llog -ldl -lm -lc \
        -Wl,--gc-sections \
        -Wl,--hash-style=gnu \
        -Wl,-z,max-page-size=16384 \
        "-Wl,--version-script=${REPO_ROOT}/exports.map" \
        -o "${OUT_DIR}/libPlacePreview.so"
    "${STRIP}" --strip-unneeded "${OUT_DIR}/libPlacePreview.so"
fi

python3 "${REPO_ROOT}/scripts/package_levipack.py" \
    "${REPO_ROOT}" \
    "${OUT_DIR}/libPlacePreview.so" \
    "${OUT_DIR}/PlacePreview.levipack"
