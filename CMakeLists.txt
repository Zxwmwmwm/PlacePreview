cmake_minimum_required(VERSION 3.22)

project(PlacePreview VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(PlacePreviewCore STATIC
    src/Config.cpp
    src/MaterialPalette.cpp
    src/StateInspector.cpp
    src/PlacementResolver.cpp
    src/PreviewAnimator.cpp
    src/GhostMesher.cpp
    src/EngineBridge.cpp
)

target_include_directories(PlacePreviewCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_options(PlacePreviewCore PRIVATE
    -Wall
    -Wextra
    -Wno-missing-field-initializers
)

if(ANDROID)
    set(PRELOADER_INCLUDE_DIR "" CACHE PATH "Path to Levi Preloader include directory")
    set(PRELOADER_LIBRARY "" CACHE FILEPATH "Path to libpreloader.so")
    set(FMT_INCLUDE_DIR "" CACHE PATH "Path to fmt include directory")

    target_include_directories(PlacePreviewCore PUBLIC
        "${PRELOADER_INCLUDE_DIR}"
        "${FMT_INCLUDE_DIR}"
    )
    target_compile_definitions(PlacePreviewCore PUBLIC FMT_HEADER_ONLY=1)

    add_library(PlacePreview SHARED
        src/ColorPreviewOverlay.cpp
        src/ModMenuUi.cpp
        src/main.cpp
    )

    target_include_directories(PlacePreview PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${PRELOADER_INCLUDE_DIR}"
        "${FMT_INCLUDE_DIR}"
    )

    target_compile_definitions(PlacePreview PRIVATE FMT_HEADER_ONLY=1)

    target_compile_options(PlacePreview PRIVATE
        -O2
        -ffunction-sections
        -fdata-sections
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -Wall
        -Wextra
        -Wno-missing-field-initializers
    )

    target_link_libraries(PlacePreview PRIVATE
        PlacePreviewCore
        "${PRELOADER_LIBRARY}"
        log
        dl
        m
        c
    )

    target_link_options(PlacePreview PRIVATE
        -Wl,--gc-sections
        -Wl,--hash-style=gnu
        -Wl,-z,max-page-size=16384
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
    )
else()
    enable_testing()

    add_executable(test_place_preview
        tests/test_place_preview.cpp
    )

    target_link_libraries(test_place_preview PRIVATE
        PlacePreviewCore
    )

    add_test(NAME PlacePreviewSuite COMMAND test_place_preview)
endif()
