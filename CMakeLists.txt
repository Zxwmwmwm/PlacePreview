cmake_minimum_required(VERSION 3.22)
project(BuildGhost VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_GHOST_CORE_SOURCES
    src/Config.cpp
    src/MaterialPalette.cpp
    src/StateInspector.cpp
    src/PlacementResolver.cpp
    src/GhostMesher.cpp
)

if(ANDROID)
    set(PRELOADER_INCLUDE_DIR "" CACHE PATH "Path to LiteLDev/preloader-android include directory")
    set(PRELOADER_LIBRARY "" CACHE FILEPATH "Path to libpreloader.so import library")
    set(FMT_INCLUDE_DIR "" CACHE PATH "Optional path to fmt 11.x include directory")

    include(FetchContent)

    if(NOT PRELOADER_INCLUDE_DIR)
        FetchContent_Declare(
            preloader_headers
            GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
            GIT_TAG main
        )
        FetchContent_Populate(preloader_headers)
        set(PRELOADER_INCLUDE_DIR "${preloader_headers_SOURCE_DIR}/include")
    endif()

    if(NOT FMT_INCLUDE_DIR)
        FetchContent_Declare(
            fmt_headers
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 11.2.0
        )
        FetchContent_Populate(fmt_headers)
        set(FMT_INCLUDE_DIR "${fmt_headers_SOURCE_DIR}/include")
    endif()

    if(NOT PRELOADER_LIBRARY)
        message(FATAL_ERROR "Set -DPRELOADER_LIBRARY=/path/to/libpreloader.so")
    endif()

    add_library(BuildGhost SHARED
        ${BUILD_GHOST_CORE_SOURCES}
        src/EngineBridge.cpp
        src/ModMenuUi.cpp
        src/main.cpp
    )

    target_include_directories(BuildGhost
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE
            "${PRELOADER_INCLUDE_DIR}"
            "${FMT_INCLUDE_DIR}"
    )

    target_compile_definitions(BuildGhost PRIVATE FMT_HEADER_ONLY=1)

    target_compile_options(BuildGhost PRIVATE
        -O2
        -fPIC
        -ffunction-sections
        -fdata-sections
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -Wall
        -Wextra
        -Wno-missing-field-initializers
    )

    target_link_options(BuildGhost PRIVATE
        -Wl,--gc-sections
        -Wl,--hash-style=gnu
        -Wl,-z,max-page-size=16384
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
    )

    find_library(ANDROID_LOG_LIB log REQUIRED)

    target_link_libraries(BuildGhost PRIVATE
        "${PRELOADER_LIBRARY}"
        "${ANDROID_LOG_LIB}"
        dl
        m
        c
    )
else()
    enable_testing()
    add_executable(test_build_ghost
        ${BUILD_GHOST_CORE_SOURCES}
        tests/test_build_ghost.cpp
    )
    target_include_directories(test_build_ghost PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_compile_options(test_build_ghost PRIVATE
        -O2
        -g
        -Wall
        -Wextra
        -Werror
    )
    add_test(NAME BuildGhostUnitSuite COMMAND test_build_ghost)
endif()
