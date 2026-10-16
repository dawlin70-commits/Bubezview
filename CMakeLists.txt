cmake_minimum_required(VERSION 3.16)
project(bubezview VERSION 0.4.1 LANGUAGES CXX)
set(BUBEZVIEW_VERSION_STRING "0.4.1")
add_compile_definitions(BUBEZVIEW_VERSION="${BUBEZVIEW_VERSION_STRING}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(QT_NO_DEBUG_OUTPUT)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-alloc-size-larger-than -ftls-model=global-dynamic)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -----------------------------------------------------------------------------
# Build Configuration Options
# Granular control over engines:
# - STATIC_PDF: Statically embed & optimize PDF engine (Poppler)
# - STATIC_ARCHIVE: Statically embed & optimize Archive engine (LibArchive)
# - OPTIMIZE_FOR_NATIVE_CPU: Compile with -O3 -march=native for Ryzen 9 Zen2 AVX2
# -----------------------------------------------------------------------------
option(STATIC_PDF "Statically embed and optimize PDF engine (Poppler)" OFF)
option(STATIC_ARCHIVE "Statically embed and optimize Archive engine (LibArchive)" OFF)
option(OPTIMIZE_FOR_NATIVE_CPU "Enable native CPU microarchitecture instruction tuning (-march=native -O3)" ON)
option(ENABLE_INTERNAL_OFFICE "Enable internal Office document parser (librevenge)" OFF)
option(STATIC_BUILD "Legacy shortcut to enable all static options at once" OFF)

if(STATIC_BUILD)
    set(STATIC_PDF ON)
    set(STATIC_ARCHIVE ON)
endif()

# -----------------------------------------------------------------------------
# Global Optimization Level & CPU Architecture Tuning
# Applied uniformly to the core app, all plugins, and static engines (Poppler, etc.)
# -----------------------------------------------------------------------------
set(OPTIMIZATION_LEVEL "O2" CACHE STRING "Compiler optimization level: O0 (no opt/fastest build), O1 (fast dev build), O2 (balanced), or O3 (aggressive/compute)")
set(CUSTOM_CXX_FLAGS "" CACHE STRING "Custom compiler flags applied uniformly across all targets")

# Sanitize CMake default flags and custom flags to prevent duplicate or conflicting -O levels
string(REGEX REPLACE "-O[0-3s]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REGEX REPLACE "-O[0-3s]" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REGEX REPLACE "-O[0-3s]" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REGEX REPLACE "-O[0-3s]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

if(NOT MSVC)
    if(OPTIMIZATION_LEVEL STREQUAL "O3")
        add_compile_options(-O3)
        message(STATUS "Optimization Level: -O3 (Aggressive vectorization & loop unrolling across all targets)")
    elseif(OPTIMIZATION_LEVEL STREQUAL "O1")
        add_compile_options(-O1 -g)
        message(STATUS "Optimization Level: -O1 (Fast compilation for active development with debug symbols)")
    elseif(OPTIMIZATION_LEVEL STREQUAL "O0")
        add_compile_options(-O0 -g -fno-omit-frame-pointer)
        message(STATUS "Optimization Level: -O0 (No optimization, maximum debuggability, frame pointers & debug symbols)")
    else()
        add_compile_options(-O2)
        message(STATUS "Optimization Level: -O2 (Balanced & battery friendly across all targets)")
    endif()
    add_compile_definitions(BUBEZ_OPT_LEVEL="${OPTIMIZATION_LEVEL}")

    if(CUSTOM_CXX_FLAGS AND NOT CUSTOM_CXX_FLAGS STREQUAL "")
        string(REGEX REPLACE "-O[0-3s]" "" _CLEANED_CUSTOM_FLAGS "${CUSTOM_CXX_FLAGS}")
        if(OPTIMIZE_FOR_NATIVE_CPU AND NOT _CLEANED_CUSTOM_FLAGS MATCHES "-march=")
            add_compile_options(-march=native)
        endif()
        separate_arguments(_CUSTOM_FLAGS_LIST NATIVE_COMMAND "${_CLEANED_CUSTOM_FLAGS}")
        if(_CUSTOM_FLAGS_LIST)
            add_compile_options(${_CUSTOM_FLAGS_LIST})
            message(STATUS "Custom Compiler Flags: ${_CLEANED_CUSTOM_FLAGS} (applied across all targets)")
        endif()
    elseif(OPTIMIZE_FOR_NATIVE_CPU AND (OPTIMIZATION_LEVEL STREQUAL "O2" OR OPTIMIZATION_LEVEL STREQUAL "O3"))
        add_compile_options(-march=native -mcrc32 -funroll-loops -falign-loops=32 -fomit-frame-pointer -pipe)
        message(STATUS "CPU Instruction Tuning: Native microarchitecture (-march=native -mcrc32 -funroll-loops -falign-loops=32 -fomit-frame-pointer -pipe across all targets)")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml DBus PrintSupport Test Network OpenGLWidgets Svg SvgWidgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MPV mpv)

# Link-Time Optimization (LTO): only enable on O3 when not explicitly disabled in CUSTOM_CXX_FLAGS
set(_ALLOW_LTO FALSE)
if(OPTIMIZATION_LEVEL STREQUAL "O3")
    if(NOT CUSTOM_CXX_FLAGS OR CUSTOM_CXX_FLAGS STREQUAL "" OR CUSTOM_CXX_FLAGS MATCHES "-flto")
        set(_ALLOW_LTO TRUE)
    endif()
endif()
if(_ALLOW_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "IPO / Link-Time Optimization (LTO) ENABLED")
    endif()
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    message(STATUS "IPO / Link-Time Optimization (LTO) DISABLED (Fast link mode)")
endif()

# Prioritize user-compiled libraries in ~/.local/lib/pkgconfig if present
if(EXISTS "$ENV{HOME}/.local/lib/pkgconfig")
    set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/.local/lib/pkgconfig:$ENV{HOME}/.local/lib/x86_64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# 1. PDF Engine Discovery (Plugin bubez_pdf links dynamically to ~/.local or system poppler-qt6)
pkg_check_modules(POPPLER_QT6 poppler-qt6)
if(POPPLER_QT6_FOUND)
    message(STATUS ">>> PDF Plugin Engine: Detected poppler-qt6 (${POPPLER_QT6_VERSION}) <<<")
else()
    message(STATUS ">>> PDF Plugin Engine: poppler-qt6 not detected (PDF plugin will be skipped) <<<")
endif()

# 2. Archive Engine Discovery (Plugin bubez_archive links dynamically to ~/.local or system libarchive)
pkg_check_modules(LIBARCHIVE libarchive)
if(LIBARCHIVE_FOUND)
    message(STATUS ">>> Archive Plugin Engine: Detected libarchive (${LIBARCHIVE_VERSION}) <<<")
    add_compile_definitions(HAVE_LIBARCHIVE)
else()
    message(STATUS ">>> Archive Plugin Engine: libarchive not detected (Archive plugin will be skipped) <<<")
endif()

# 3. Optional QPDF Engine (Advanced PDF Optimization, Linearization & Merge)
pkg_check_modules(QPDF libqpdf)
if(QPDF_FOUND)
    message(STATUS ">>> QPDF Engine: Detected on host (Advanced PDF operations ENABLED) <<<")
    add_compile_definitions(HAVE_LIBQPDF)
else()
    message(STATUS ">>> QPDF Engine: Not detected (Fallback to Poppler standard tools) <<<")
    set(QPDF_LIBRARIES "")
    set(QPDF_INCLUDE_DIRS "")
endif()

# 4. Office & QR (librevenge is optional for internal office reader)
if(ENABLE_INTERNAL_OFFICE)
    message(STATUS ">>> Office Engine: Internal fast parser ENABLED (librevenge) <<<")
    pkg_check_modules(LIBREVENGE REQUIRED librevenge-0.0 librevenge-generators-0.0 librevenge-stream-0.0)
    add_compile_definitions(ENABLE_INTERNAL_OFFICE)
else()
    message(STATUS ">>> Office Engine: External LibreOffice headless conversion mode (librevenge DISABLED) <<<")
    set(LIBREVENGE_INCLUDE_DIRS "")
    set(LIBREVENGE_LIBRARIES "")
    set(LIBREVENGE_CFLAGS_OTHER "")
endif()
pkg_check_modules(LIBQRENCODE REQUIRED libqrencode)

set(APP_CORE_SOURCES
    src/ui/base_skin.cpp
    src/ui/action_registry.cpp
    src/ui/mainwindow.cpp
    src/ui/share_dialog.cpp
    src/ui/share_manager.cpp
    src/ui/goto_folder_dialog.cpp
    src/ui/settings_dialog.cpp
    src/ui/build_console_dialog.cpp
    src/ui/fallback_notice_dialog.cpp
    src/ui/build_options_dialog.cpp
    src/ui/search_bar_widget.cpp
    src/ui/open_with_dialog.cpp
    src/ui/configure_toolbar_dialog.cpp
    src/ui/flippable_toolbar.cpp
    src/ui/engine_prompt_card.cpp
    src/core/app_launcher.cpp
    src/core/theme_manager.cpp
    src/core/build_config.cpp
    src/core/package_manager_helper.cpp
    src/core/desktop_integrator.cpp
    src/core/dbus_previewer.cpp
    src/core/plugin_manager.cpp
    src/core/preload_cache_manager.cpp
    src/viewers/image/image_viewer.cpp
    src/viewers/text/text_viewer.cpp
    src/viewers/text/line_number_area.cpp
    src/viewers/text/syntax_highlighter.cpp
    src/viewers/office/office_viewer.cpp
    src/viewers/folder/folder_viewer.cpp
    src/ui/filmstrip_widget.cpp
    src/ui/update_dialog.cpp
    src/ui/first_run_wizard.cpp
    src/ui/compiler_wizard_dialog.cpp
    src/ui/ocr_dialog.cpp
    src/ui/ocr_language_dialog.cpp
    src/ui/ocr_snipper_overlay.cpp
    src/ui/pdf_image_extract_dialog.cpp
    src/ui/pdf_merge_dialog.cpp
    src/ui/pdf_repair_dialog.cpp
    src/core/update_manager.cpp
    src/core/cpu_info.cpp
    src/core/ocr_manager.cpp
    src/ui/extension_feature_model.cpp
    src/ui/accordion_extension_card.cpp
    src/ui/extension_row_widget.cpp
    src/ui/custom_source_dialog.cpp
    src/ui/settings/settings_appearance_tab.cpp
    src/ui/settings/settings_integration_tab.cpp
    src/ui/settings/settings_performance_tab.cpp
    src/ui/settings/settings_extensions_tab.cpp
    src/ui/settings/settings_extra_tab.cpp
)

set(PDF_VIEWER_SOURCES
    src/viewers/pdf/pdf_viewer.cpp
    src/viewers/pdf/pdf_page_widget.cpp
    src/viewers/pdf/pdf_document_ops.cpp
    src/viewers/pdf/pdf_canvas_tools.cpp
    src/viewers/pdf/pdf_transform_box.cpp
    src/ui/pdf_typewriter_dialog.cpp
    src/ui/pdf_signature_dialog.cpp
    src/ui/pdf_crypto_signature_dialog.cpp
    src/ui/pdf_image_dialog.cpp
    src/ui/pdf_shape_dialog.cpp
    src/ui/pdf_watermark_dialog.cpp
    src/ui/pdf_tools_dialog.cpp
    src/ui/pdf_page_organizer_dialog.cpp
)
set(PDF_PLUGIN_SOURCES
    src/plugins/pdf/pdf_plugin.cpp
    ${PDF_VIEWER_SOURCES}
)

set(ARCHIVE_VIEWER_SOURCES
    src/viewers/archive/archive_viewer.cpp
)
set(ARCHIVE_PLUGIN_SOURCES
    src/plugins/archive/archive_plugin.cpp
    ${ARCHIVE_VIEWER_SOURCES}
)

set(MEDIA_VIEWER_SOURCES
    src/viewers/media/mpv_video_widget.cpp
    src/viewers/media/media_viewer.cpp
    src/viewers/media/stereo_oscilloscope_widget.cpp
    src/core/audio_processor.cpp
    src/core/equalizer_manager.cpp
    src/ui/audio_trim_dialog.cpp
    src/ui/audio_tag_dialog.cpp
    src/ui/cover_selection_dialog.cpp
    src/ui/equalizer_widget.cpp
)
set(MEDIA_PLUGIN_SOURCES
    src/plugins/media/media_plugin.cpp
    ${MEDIA_VIEWER_SOURCES}
)

set(APP_COMMON_SOURCES
    ${APP_CORE_SOURCES}
)

set(PDF_PLUGIN_HEADERS
    src/ui/pdf_page_organizer_dialog.h
    src/ui/pdf_typewriter_dialog.h
    src/ui/pdf_signature_dialog.h
    src/ui/pdf_crypto_signature_dialog.h
    src/ui/pdf_image_dialog.h
    src/ui/pdf_shape_dialog.h
    src/ui/pdf_watermark_dialog.h
    src/ui/pdf_tools_dialog.h
    src/viewers/pdf/pdf_transform_box.h
    src/viewers/pdf/pdf_viewer.h
    src/viewers/pdf/pdf_page_widget.h
    src/viewers/pdf/pdf_document_ops.h
    src/viewers/pdf/pdf_canvas_tools.h
)

set(ARCHIVE_PLUGIN_HEADERS
    src/viewers/archive/archive_viewer.h
)

set(MEDIA_PLUGIN_HEADERS
    src/viewers/media/mpv_video_widget.h
    src/viewers/media/media_viewer.h
    src/viewers/media/stereo_oscilloscope_widget.h
    src/core/audio_processor.h
    src/ui/audio_trim_dialog.h
    src/ui/audio_tag_dialog.h
    src/ui/cover_selection_dialog.h
    src/core/equalizer_manager.h
    src/ui/equalizer_widget.h
)

set(APP_CORE_HEADERS
    src/core/interfaces/i_viewer_widget.h
    src/core/interfaces/i_pdf_viewer.h
    src/core/interfaces/i_archive_viewer.h
    src/core/interfaces/i_media_viewer.h
    src/core/interfaces/viewer_plugin_interface.h
    src/ui/base_skin.h
    src/ui/action_registry.h
    src/ui/engine_prompt_card.h
    src/ui/extension_feature_model.h
    src/ui/accordion_extension_card.h
    src/ui/extension_row_widget.h
    src/ui/custom_source_dialog.h
    src/ui/mainwindow.h
    src/ui/share_dialog.h
    src/ui/share_manager.h
    src/ui/goto_folder_dialog.h
    src/ui/configure_toolbar_dialog.h
    src/ui/settings_dialog.h
    src/ui/settings/settings_appearance_tab.h
    src/ui/settings/settings_integration_tab.h
    src/ui/settings/settings_performance_tab.h
    src/ui/settings/settings_extensions_tab.h
    src/ui/settings/settings_extra_tab.h
    src/ui/build_console_dialog.h
    src/ui/build_options_dialog.h
    src/ui/compiler_wizard_dialog.h
    src/ui/first_run_wizard.h
    src/ui/ocr_dialog.h
    src/ui/ocr_language_dialog.h
    src/ui/ocr_snipper_overlay.h
    src/ui/pdf_image_extract_dialog.h
    src/ui/pdf_merge_dialog.h
    src/ui/pdf_repair_dialog.h
    src/ui/search_bar_widget.h
    src/ui/filmstrip_widget.h
    src/ui/open_with_dialog.h
    src/ui/vector_icons.h
    src/ui/update_dialog.h
    src/core/app_launcher.h
    src/core/theme_manager.h
    src/core/build_config.h
    src/core/cpu_info.h
    src/core/desktop_integrator.h
    src/core/dbus_previewer.h
    src/core/plugin_manager.h
    src/core/preload_cache_manager.h
    src/core/update_manager.h
    src/core/ocr_manager.h
    src/viewers/image/image_viewer.h
    src/viewers/text/text_viewer.h
    src/viewers/text/line_number_area.h
    src/viewers/text/syntax_highlighter.h
    src/viewers/office/office_viewer.h
    src/viewers/folder/folder_viewer.h
)


# Automated Translation Compilation (.ts -> .qm)
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

set(RESOURCES
    resources/translations.qrc
    resources/icons.qrc
)

# -----------------------------------------------------------------------------
# Modular Plugins
# -----------------------------------------------------------------------------
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")

# PDF Plugin
if(POPPLER_QT6_FOUND OR STATIC_PDF)
    add_library(bubez_pdf MODULE ${PDF_PLUGIN_SOURCES} ${PDF_PLUGIN_HEADERS} ${RESOURCES})
    target_include_directories(bubez_pdf PRIVATE
        src
        src/ui
        src/core
        src/viewers/pdf
        ${POPPLER_QT6_INCLUDE_DIRS}
        ${QPDF_INCLUDE_DIRS}
    )
    target_link_libraries(bubez_pdf PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        ${POPPLER_QT6_LIBRARIES}
        ${QPDF_LIBRARIES}
    )
    set_target_properties(bubez_pdf PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        PREFIX ""
    )
    if(TARGET compile_translations)
        add_dependencies(bubez_pdf compile_translations)
    endif()
endif()

# Archive Plugin
if(LIBARCHIVE_FOUND OR STATIC_ARCHIVE)
    add_library(bubez_archive MODULE ${ARCHIVE_PLUGIN_SOURCES} ${ARCHIVE_PLUGIN_HEADERS} ${RESOURCES})
    target_include_directories(bubez_archive PRIVATE
        src
        src/ui
        src/core
        src/viewers/archive
        ${LIBARCHIVE_INCLUDE_DIRS}
    )
    target_link_libraries(bubez_archive PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${LIBARCHIVE_LIBRARIES}
    )
    set_target_properties(bubez_archive PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        PREFIX ""
    )
    if(TARGET compile_translations)
        add_dependencies(bubez_archive compile_translations)
    endif()
endif()

# Media Plugin
if(MPV_FOUND)
    add_library(bubez_media MODULE ${MEDIA_PLUGIN_SOURCES} ${MEDIA_PLUGIN_HEADERS} ${RESOURCES})
    target_include_directories(bubez_media PRIVATE
        src
        src/ui
        src/core
        src/viewers/media
        ${MPV_INCLUDE_DIRS}
    )
    target_link_libraries(bubez_media PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Network
        Qt6::DBus
        ${MPV_LIBRARIES}
    )
    set_target_properties(bubez_media PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        PREFIX ""
    )
    if(TARGET compile_translations)
        add_dependencies(bubez_media compile_translations)
    endif()
endif()

# -----------------------------------------------------------------------------
# Main Application Executable (Lean Host)
# -----------------------------------------------------------------------------
add_executable(bubezview src/main.cpp ${APP_CORE_SOURCES} ${APP_CORE_HEADERS} ${RESOURCES})

if(TARGET compile_translations)
    add_dependencies(bubezview compile_translations)
endif()
if(TARGET pdfimages)
    add_dependencies(bubezview pdfimages)
    add_custom_command(
        TARGET bubezview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:pdfimages> ${CMAKE_BINARY_DIR}/pdfimages
        COMMENT "Copying compiled pdfimages to build root"
    )
endif()

target_include_directories(bubezview PRIVATE
    src
    src/ui
    src/ui/settings
    src/core
    src/viewers/image
    src/viewers/text
    src/viewers/office
    src/viewers/folder
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${LIBREVENGE_INCLUDE_DIRS}
    ${LIBQRENCODE_INCLUDE_DIRS}
    ${QPDF_INCLUDE_DIRS}
)

target_link_libraries(bubezview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    Qt6::DBus
    Qt6::OpenGLWidgets
    Qt6::PrintSupport
    Qt6::Network
    Qt6::Svg
    Qt6::SvgWidgets
    ${LIBARCHIVE_LIBRARIES}
    ${LIBREVENGE_LIBRARIES}
    ${LIBQRENCODE_LIBRARIES}
    ${QPDF_LIBRARIES}
)

set_target_properties(bubezview PROPERTIES ENABLE_EXPORTS ON)

target_compile_options(bubezview PRIVATE ${LIBARCHIVE_CFLAGS_OTHER} ${LIBREVENGE_CFLAGS_OTHER} ${LIBQRENCODE_CFLAGS_OTHER})

# Compiler warnings for main binary
if(MSVC)
    target_compile_options(bubezview PRIVATE /W4)
else()
    target_compile_options(bubezview PRIVATE -Wall -Wextra)
endif()

# Automated Interactive GUI & Exploration Test Target (excluded from default build)
enable_testing()
add_executable(test_interactive EXCLUDE_FROM_ALL
    tests/test_interactive.cpp
    ${APP_CORE_SOURCES}
    ${PDF_VIEWER_SOURCES}
    ${ARCHIVE_VIEWER_SOURCES}
    ${MEDIA_VIEWER_SOURCES}
    ${APP_CORE_HEADERS}
    ${PDF_PLUGIN_HEADERS}
    ${ARCHIVE_PLUGIN_HEADERS}
    ${MEDIA_PLUGIN_HEADERS}
    ${RESOURCES}
)
target_compile_definitions(test_interactive PRIVATE PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(test_interactive PRIVATE
    src
    src/ui
    src/ui/settings
    src/core
    src/viewers/image
    src/viewers/text
    src/viewers/pdf
    src/viewers/archive
    src/viewers/media
    src/viewers/office
    src/viewers/folder
    ${POPPLER_QT6_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${LIBREVENGE_INCLUDE_DIRS}
    ${LIBQRENCODE_INCLUDE_DIRS}
    ${MPV_INCLUDE_DIRS}
    ${QPDF_INCLUDE_DIRS}
)
target_link_libraries(test_interactive PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    Qt6::DBus
    Qt6::OpenGLWidgets
    Qt6::PrintSupport
    Qt6::Network
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::Test
    ${POPPLER_QT6_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${LIBREVENGE_LIBRARIES}
    ${LIBQRENCODE_LIBRARIES}
    ${MPV_LIBRARIES}
    ${QPDF_LIBRARIES}
)
set_target_properties(test_interactive PROPERTIES ENABLE_EXPORTS ON)

# Note: test_interactive contains modal dialogues and manual checks; excluded from automated ctest per Rule 2
# add_test(NAME TestInteractive COMMAND test_interactive)

add_executable(test_share
    tests/test_share.cpp
    src/ui/share_manager.cpp
    src/ui/share_dialog.cpp
    src/ui/share_manager.h
    src/ui/share_dialog.h
)
target_include_directories(test_share PRIVATE
    src
    src/ui
    ${LIBQRENCODE_INCLUDE_DIRS}
)
target_link_libraries(test_share PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
    ${LIBQRENCODE_LIBRARIES}
)
add_test(NAME TestShare COMMAND test_share)

# Installation rules
include(GNUInstallDirs)
install(TARGETS bubezview DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET bubez_pdf)
    install(TARGETS bubez_pdf DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
endif()
if(TARGET bubez_archive)
    install(TARGETS bubez_archive DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
endif()
if(TARGET bubez_media)
    install(TARGETS bubez_media DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
endif()

# Icons (PNG 256x256)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/bubezview.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        RENAME "bubezview.png")

# Desktop entry for Application Menu
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/bubezview.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

# Nemo Action for right-click context menu
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/bubezview.nemo_action"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/nemo/actions")

