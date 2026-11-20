# ==============================================================================
# BubezView Folder Studio Plugin (bubez_folder)
# Multi-format deduplication, PhotoRec/TestDisk recovery, tree organizer & batch tools
# ==============================================================================

message(STATUS "Folder Studio Engine: Building bubez_folder module")

set(FOLDER_PLUGIN_SOURCES
    folder_plugin.cpp
    folder_studio_engine.cpp
    folder_studio_widget.cpp
)

add_library(bubez_folder MODULE ${FOLDER_PLUGIN_SOURCES})

target_include_directories(bubez_folder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/interfaces
    ${CMAKE_SOURCE_DIR}/src/core/system
    ${CMAKE_SOURCE_DIR}/src/core/services
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${POPPLER_QT6_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
)

target_link_libraries(bubez_folder PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${POPPLER_QT6_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
)

if(POPPLER_QT6_FOUND)
    target_compile_definitions(bubez_folder PRIVATE HAVE_POPPLER_QT6)
endif()
if(LIBARCHIVE_FOUND OR STATIC_ARCHIVE)
    target_compile_definitions(bubez_folder PRIVATE HAVE_LIBARCHIVE)
endif()

set_target_properties(bubez_folder PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)

if(TARGET compile_translations)
    add_dependencies(bubez_folder compile_translations)
endif()

install(TARGETS bubez_folder DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
