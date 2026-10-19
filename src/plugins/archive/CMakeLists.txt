# ==============================================================================
# BubezView Archive Plugin (bubez_archive)
# Multi-format archive viewer and extractor based on LibArchive
# ==============================================================================

pkg_check_modules(LIBARCHIVE libarchive)

if(LIBARCHIVE_FOUND OR STATIC_ARCHIVE)
    message(STATUS "Archive Engine: Building bubez_archive module with libarchive (${LIBARCHIVE_VERSION})")
    add_compile_definitions(HAVE_LIBARCHIVE)

    set(ARCHIVE_PLUGIN_SOURCES
        archive_plugin.cpp
        ${CMAKE_SOURCE_DIR}/src/viewers/archive/archive_viewer.cpp
        ${BUBEZVIEW_RESOURCES}
    )

    add_library(bubez_archive MODULE ${ARCHIVE_PLUGIN_SOURCES})

    target_include_directories(bubez_archive PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/core/interfaces
        ${CMAKE_SOURCE_DIR}/src/core/system
        ${CMAKE_SOURCE_DIR}/src/core/services
        ${CMAKE_SOURCE_DIR}/src/core/audio
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/ui/pdf
        ${CMAKE_SOURCE_DIR}/src/ui/audio
        ${CMAKE_SOURCE_DIR}/src/ui/ocr
        ${CMAKE_SOURCE_DIR}/src/ui/extensions
        ${CMAKE_SOURCE_DIR}/src/ui/dialogs
        ${CMAKE_SOURCE_DIR}/src/ui/settings
        ${CMAKE_SOURCE_DIR}/src/viewers/archive
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

    install(TARGETS bubez_archive DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
else()
    message(STATUS "Archive Engine: libarchive not found, skipping bubez_archive plugin")
endif()
