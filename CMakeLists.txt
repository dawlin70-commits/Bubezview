cmake_minimum_required(VERSION 3.16)
project(bubezview VERSION 0.3.1 LANGUAGES CXX)
add_compile_definitions(BUBEZVIEW_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(QT_NO_DEBUG_OUTPUT)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-alloc-size-larger-than)
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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml DBus PrintSupport Test Network OpenGLWidgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MPV REQUIRED mpv)

# Link-Time Optimization (LTO) when any static engine or CPU optimization is active
if(STATIC_PDF OR STATIC_ARCHIVE OR (CMAKE_BUILD_TYPE STREQUAL "Release"))
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "IPO / Link-Time Optimization (LTO) ENABLED")
    endif()
endif()

include(FetchContent)

# 1. PDF Engine Configuration (Compiled/Built-in vs System Library)
if(STATIC_PDF)
    if(NOT DEFINED POPPLER_VERSION OR POPPLER_VERSION STREQUAL "")
        set(POPPLER_VERSION "24.08.0" CACHE STRING "Poppler source version for static build")
    endif()
    message(STATUS ">>> PDF Engine: FETCHING & COMPILING POPPLER ${POPPLER_VERSION} (BUILT-IN STATIC) <<<")

    set(ENABLE_QT6 ON CACHE BOOL "" FORCE)
    set(ENABLE_QT5 OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLIB OFF CACHE BOOL "" FORCE)
    set(ENABLE_CPP OFF CACHE BOOL "" FORCE)
    set(ENABLE_UTILS OFF CACHE BOOL "" FORCE)
    set(BUILD_QT6_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_CPP_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_MANUAL_TESTS OFF CACHE BOOL "" FORCE)
    set(ENABLE_BOOST OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ENABLE_UNSTABLE_API_ABI_HEADERS ON CACHE BOOL "" FORCE)
    set(ENABLE_NSS3 OFF CACHE BOOL "" FORCE)
    set(ENABLE_GPGME OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        poppler
        URL https://poppler.freedesktop.org/poppler-${POPPLER_VERSION}.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(poppler)
    if(NOT poppler_POPULATED)
        FetchContent_Populate(poppler)
        if(EXISTS "${poppler_SOURCE_DIR}/CMakeLists.txt")
            file(READ "${poppler_SOURCE_DIR}/CMakeLists.txt" _POPPLER_CML)
            string(REPLACE "\${CMAKE_SOURCE_DIR}/poppler/libpoppler.map.in" "\${CMAKE_CURRENT_SOURCE_DIR}/poppler/libpoppler.map.in" _POPPLER_CML "${_POPPLER_CML}")
            file(WRITE "${poppler_SOURCE_DIR}/CMakeLists.txt" "${_POPPLER_CML}")
        endif()
        if(EXISTS "${poppler_SOURCE_DIR}/cmake/modules")
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
            if(IS_SYMLINK "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
                file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
                file(CREATE_LINK "${poppler_SOURCE_DIR}/cmake/modules" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" SYMBOLIC)
            endif()
        endif()
        if(EXISTS "${poppler_SOURCE_DIR}/poppler")
            if(IS_SYMLINK "${CMAKE_CURRENT_SOURCE_DIR}/poppler" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/poppler")
                file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/poppler")
                file(CREATE_LINK "${poppler_SOURCE_DIR}/poppler" "${CMAKE_CURRENT_SOURCE_DIR}/poppler" SYMBOLIC)
            endif()
        endif()
        add_subdirectory(${poppler_SOURCE_DIR} ${poppler_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    set(POPPLER_QT6_LIBRARIES poppler-qt6 poppler)
    set(POPPLER_QT6_INCLUDE_DIRS 
        ${poppler_SOURCE_DIR}/qt6/src 
        ${poppler_BINARY_DIR}/qt6/src
        ${poppler_SOURCE_DIR}/poppler
        ${poppler_BINARY_DIR}/poppler
        ${poppler_SOURCE_DIR}
        ${poppler_BINARY_DIR}
    )
    add_compile_definitions(POPPLER_BUILTIN_STATIC)
else()
    message(STATUS ">>> PDF Engine: Dynamic System Library <<<")
    pkg_check_modules(POPPLER_QT6 REQUIRED poppler-qt6)
endif()

# 2. Archive Engine Configuration (Compiled/Built-in vs System Library)
if(STATIC_ARCHIVE)
    message(STATUS ">>> Archive Engine: FETCHING & COMPILING LIBARCHIVE (BUILT-IN STATIC) <<<")
    set(ENABLE_TEST OFF CACHE BOOL "" FORCE)
    set(ENABLE_TAR OFF CACHE BOOL "" FORCE)
    set(ENABLE_CPIO OFF CACHE BOOL "" FORCE)
    set(ENABLE_CAT OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        libarchive
        URL https://github.com/libarchive/libarchive/releases/download/v3.8.9/libarchive-3.8.9.tar.xz
    )
    FetchContent_MakeAvailable(libarchive)

    set(LIBARCHIVE_LIBRARIES archive_static)
    set(LIBARCHIVE_INCLUDE_DIRS ${libarchive_SOURCE_DIR}/libarchive ${libarchive_BINARY_DIR})
    add_compile_definitions(LIBARCHIVE_BUILTIN_STATIC)
else()
    message(STATUS ">>> Archive Engine: Dynamic System Library <<<")
    pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
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

set(APP_COMMON_SOURCES
    src/ui/base_skin.cpp
    src/ui/action_registry.cpp
    src/ui/mainwindow.cpp
    src/ui/share_dialog.cpp
    src/ui/share_manager.cpp
    src/ui/goto_folder_dialog.cpp
    src/ui/settings_dialog.cpp
    src/ui/build_console_dialog.cpp
    src/ui/build_options_dialog.cpp
    src/ui/search_bar_widget.cpp
    src/ui/open_with_dialog.cpp
    src/ui/configure_toolbar_dialog.cpp
    src/ui/flippable_toolbar.cpp
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
    src/viewers/pdf/pdf_viewer.cpp
    src/viewers/pdf/pdf_page_widget.cpp
    src/viewers/pdf/pdf_document_ops.cpp
    src/viewers/pdf/pdf_canvas_tools.cpp
    src/viewers/archive/archive_viewer.cpp
    src/viewers/media/mpv_video_widget.cpp
    src/viewers/media/media_viewer.cpp
    src/viewers/media/stereo_oscilloscope_widget.cpp
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
    src/ui/pdf_page_organizer_dialog.cpp
    src/ui/pdf_typewriter_dialog.cpp
    src/ui/pdf_signature_dialog.cpp
    src/ui/pdf_image_dialog.cpp
    src/ui/pdf_shape_dialog.cpp
    src/ui/pdf_watermark_dialog.cpp
    src/ui/pdf_tools_dialog.cpp
    src/viewers/pdf/pdf_transform_box.cpp
    src/core/update_manager.cpp
    src/core/cpu_info.cpp
    src/core/ocr_manager.cpp
    src/core/audio_processor.cpp
    src/ui/audio_trim_dialog.cpp
    src/ui/audio_tag_dialog.cpp
    src/ui/cover_selection_dialog.cpp
    src/core/equalizer_manager.cpp
    src/ui/equalizer_widget.cpp
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

set(SOURCES
    src/main.cpp
    ${APP_COMMON_SOURCES}
)

set(HEADERS
    src/ui/base_skin.h
    src/ui/action_registry.h
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
    src/ui/pdf_page_organizer_dialog.h
    src/ui/pdf_typewriter_dialog.h
    src/ui/pdf_signature_dialog.h
    src/ui/pdf_image_dialog.h
    src/ui/pdf_shape_dialog.h
    src/ui/pdf_watermark_dialog.h
    src/ui/pdf_tools_dialog.h
    src/viewers/pdf/pdf_transform_box.h
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
    src/viewers/pdf/pdf_viewer.h
    src/viewers/pdf/pdf_page_widget.h
    src/viewers/pdf/pdf_document_ops.h
    src/viewers/pdf/pdf_canvas_tools.h
    src/viewers/archive/archive_viewer.h
    src/viewers/media/mpv_video_widget.h
    src/viewers/media/media_viewer.h
    src/viewers/media/stereo_oscilloscope_widget.h
    src/viewers/office/office_viewer.h
    src/viewers/folder/folder_viewer.h
    src/core/audio_processor.h
    src/ui/audio_trim_dialog.h
    src/ui/audio_tag_dialog.h
    src/ui/cover_selection_dialog.h
    src/core/equalizer_manager.h
    src/ui/equalizer_widget.h
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

add_executable(bubezview ${SOURCES} ${HEADERS} ${RESOURCES})

if(TARGET compile_translations)
    add_dependencies(bubezview compile_translations)
endif()

target_include_directories(bubezview PRIVATE
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

target_link_libraries(bubezview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    Qt6::DBus
    Qt6::OpenGLWidgets
    Qt6::PrintSupport
    Qt6::Network
    ${POPPLER_QT6_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${LIBREVENGE_LIBRARIES}
    ${LIBQRENCODE_LIBRARIES}
    ${MPV_LIBRARIES}
    ${QPDF_LIBRARIES}
)

target_compile_options(bubezview PRIVATE ${POPPLER_QT6_CFLAGS_OTHER} ${LIBARCHIVE_CFLAGS_OTHER} ${LIBREVENGE_CFLAGS_OTHER} ${LIBQRENCODE_CFLAGS_OTHER})

# Optimization level setting (defaults to O2 if unspecified, or O3 if requested)
set(OPTIMIZATION_LEVEL "O2" CACHE STRING "Compiler optimization level: O2 (balanced/battery) or O3 (aggressive/compute)")

# Compiler warnings and optimizations
if(MSVC)
    target_compile_options(bubezview PRIVATE /W4)
else()
    target_compile_options(bubezview PRIVATE -Wall -Wextra)
    if(OPTIMIZATION_LEVEL STREQUAL "O3")
        target_compile_options(bubezview PRIVATE -O3)
        message(STATUS "Optimization Level: -O3 (Aggressive vectorization & loop unrolling)")
    else()
        target_compile_options(bubezview PRIVATE -O2)
        message(STATUS "Optimization Level: -O2 (Balanced & battery friendly)")
    endif()

    if(OPTIMIZE_FOR_NATIVE_CPU)
        target_compile_options(bubezview PRIVATE -march=native -pipe)
        message(STATUS "CPU Instruction Tuning: Native microarchitecture (-march=native)")
    endif()
endif()

# Automated Interactive GUI & Exploration Test Target
enable_testing()
add_executable(test_interactive
    tests/test_interactive.cpp
    ${APP_COMMON_SOURCES}
    ${HEADERS}
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
    Qt6::Test
    ${POPPLER_QT6_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${LIBREVENGE_LIBRARIES}
    ${LIBQRENCODE_LIBRARIES}
    ${MPV_LIBRARIES}
)
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

# CPack DEB generation
set(CPACK_PACKAGE_NAME "bubezview")
set(CPACK_PACKAGE_VENDOR "Davide / Bubez")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Visualizzatore universale di file e anteprima rapida nativa Qt6 C++")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Davide <bubez@local>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libreoffice")
include(CPack)
