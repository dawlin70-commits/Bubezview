cmake_minimum_required(VERSION 3.16)

# ==============================================================================
# BubezView — Modern Modular Document & Media Viewer
# Root Build System Configuration
# ==============================================================================
project(bubezview VERSION 0.4.7 LANGUAGES CXX)
set(BUBEZVIEW_VERSION_STRING "0.4.7")
add_compile_definitions(BUBEZVIEW_VERSION="${BUBEZVIEW_VERSION_STRING}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(QT_NO_DEBUG_OUTPUT)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-alloc-size-larger-than -ftls-model=global-dynamic)
    add_link_options(-Wno-alloc-size-larger-than)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Output all runtime executables directly into the root build directory (build_deb/bubezview)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")

# -----------------------------------------------------------------------------
# 1. Build Configuration Options
# -----------------------------------------------------------------------------
option(STATIC_PDF "Statically embed and optimize PDF engine (Poppler)" OFF)
option(STATIC_ARCHIVE "Statically embed and optimize Archive engine (LibArchive)" OFF)
option(STATIC_OFFICE "Embed internal Office document parser" OFF)
option(ENABLE_INTERNAL_OFFICE "Enable internal Office document parser (librevenge)" OFF)
option(OPTIMIZE_FOR_NATIVE_CPU "Enable native CPU microarchitecture tuning (-march=native)" ON)

# -----------------------------------------------------------------------------
# 2. Optimization Level & CPU Instruction Tuning
# -----------------------------------------------------------------------------
set(OPTIMIZATION_LEVEL "O2" CACHE STRING "Compiler optimization: O0, O1, O2, or O3")
set(CUSTOM_CXX_FLAGS "" CACHE STRING "Custom compiler flags applied uniformly across all targets")

if(NOT MSVC)
    if(OPTIMIZATION_LEVEL STREQUAL "O3")
        add_compile_options(-O3)
        message(STATUS "Optimization Level: -O3 (Aggressive vectorization & loop unrolling)")
    elseif(OPTIMIZATION_LEVEL STREQUAL "O1")
        add_compile_options(-O1 -g)
        message(STATUS "Optimization Level: -O1 (Fast compilation for active development)")
    elseif(OPTIMIZATION_LEVEL STREQUAL "O0")
        add_compile_options(-O0 -g -fno-omit-frame-pointer)
        message(STATUS "Optimization Level: -O0 (No optimization, maximum debuggability)")
    else()
        add_compile_options(-O2)
        message(STATUS "Optimization Level: -O2 (Balanced)")
    endif()
    add_compile_definitions(BUBEZ_OPT_LEVEL="${OPTIMIZATION_LEVEL}")

    if(CUSTOM_CXX_FLAGS AND NOT CUSTOM_CXX_FLAGS STREQUAL "")
        if(OPTIMIZE_FOR_NATIVE_CPU AND NOT CUSTOM_CXX_FLAGS MATCHES "-march=")
            add_compile_options(-march=native)
        endif()
        separate_arguments(_CUSTOM_FLAGS_LIST NATIVE_COMMAND "${CUSTOM_CXX_FLAGS}")
        if(_CUSTOM_FLAGS_LIST)
            add_compile_options(${_CUSTOM_FLAGS_LIST})
            message(STATUS "Custom Compiler Flags: ${CUSTOM_CXX_FLAGS}")
        endif()
    elseif(OPTIMIZE_FOR_NATIVE_CPU AND (OPTIMIZATION_LEVEL STREQUAL "O2" OR OPTIMIZATION_LEVEL STREQUAL "O3"))
        add_compile_options(-march=native -mcrc32 -funroll-loops -falign-loops=32 -fomit-frame-pointer -pipe)
        message(STATUS "CPU Instruction Tuning: -march=native -mcrc32 -funroll-loops -falign-loops=32 -fomit-frame-pointer -pipe")
    endif()
endif()

# Link-Time Optimization (LTO): only enable on O3 when requested or default
if(OPTIMIZATION_LEVEL STREQUAL "O3" AND (NOT CUSTOM_CXX_FLAGS OR CUSTOM_CXX_FLAGS MATCHES "-flto"))
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "IPO / Link-Time Optimization (LTO): ENABLED")
    endif()
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
endif()

# -----------------------------------------------------------------------------
# 3. Dependencies Discovery
# -----------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml DBus PrintSupport Test Network OpenGLWidgets Svg Multimedia MultimediaWidgets)
find_package(PkgConfig REQUIRED)

# Prioritize user-compiled libraries in ~/.local/lib/pkgconfig if present
if(EXISTS "$ENV{HOME}/.local/lib/pkgconfig")
    set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/.local/lib/pkgconfig:$ENV{HOME}/.local/lib/x86_64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# Core & optional engine packages
pkg_check_modules(LIBARCHIVE libarchive)
if(LIBARCHIVE_FOUND)
    add_compile_definitions(HAVE_LIBARCHIVE)
endif()

if(ENABLE_INTERNAL_OFFICE)
    message(STATUS "Office Engine: Internal fast parser ENABLED (librevenge)")
    pkg_check_modules(LIBREVENGE REQUIRED librevenge-0.0 librevenge-generators-0.0 librevenge-stream-0.0)
    add_compile_definitions(ENABLE_INTERNAL_OFFICE)
else()
    set(LIBREVENGE_INCLUDE_DIRS "")
    set(LIBREVENGE_LIBRARIES "")
    set(LIBREVENGE_CFLAGS_OTHER "")
endif()

pkg_check_modules(LIBQRENCODE REQUIRED libqrencode)

# -----------------------------------------------------------------------------
# 4. Translations (.ts -> .qm) and Global Resources
# -----------------------------------------------------------------------------
find_program(LRELEASE_EXECUTABLE NAMES lrelease lrelease-qt6 PATHS /usr/lib/qt6/bin /usr/bin)
if(LRELEASE_EXECUTABLE)
    file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts")
    set(QM_FILES)
    foreach(TS_FILE ${TS_FILES})
        get_filename_component(TS_NAME ${TS_FILE} NAME_WE)
        set(QM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/translations/${TS_NAME}.qm")
        list(APPEND QM_FILES ${QM_FILE})
        add_custom_command(
            OUTPUT ${QM_FILE}
            COMMAND ${LRELEASE_EXECUTABLE} ${TS_FILE} -qm ${QM_FILE}
            DEPENDS ${TS_FILE}
            COMMENT "Compiling translation: ${TS_NAME}.ts -> ${TS_NAME}.qm"
            VERBATIM
        )
    endforeach()
    add_custom_target(compile_translations ALL DEPENDS ${QM_FILES})
endif()

set(BUBEZVIEW_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons.qrc
    CACHE INTERNAL "Global application Qt resources"
)

# -----------------------------------------------------------------------------
# 5. Tree Subdirectories (Core, Plugins, Tests)
# -----------------------------------------------------------------------------
enable_testing()
add_subdirectory(src)
add_subdirectory(tests)

# -----------------------------------------------------------------------------
# 6. Global Desktop Integration Installation Rules
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

# Application Icon (PNG 256x256)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/bubezview.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        RENAME "bubezview.png")

# Desktop Entry
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/bubezview.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

# Nemo Action Context Menu
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/bubezview.nemo_action"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/nemo/actions")
