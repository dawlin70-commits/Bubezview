# ==============================================================================
# BubezView Video Studio Plugin (bubez_video)
# Modular video editing tools: trim/cut, audio extract, snapshot & transforms
# ==============================================================================

message(STATUS "Video Studio Engine: Building bubez_video module")

set(VIDEO_PLUGIN_SOURCES
    video_plugin.cpp
    video_studio_widget.cpp
)

add_library(bubez_video MODULE ${VIDEO_PLUGIN_SOURCES})

target_include_directories(bubez_video PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/interfaces
    ${CMAKE_SOURCE_DIR}/src/core/services
    ${CMAKE_SOURCE_DIR}/src/core/system
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bubez_video PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(bubez_video PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)

if(TARGET compile_translations)
    add_dependencies(bubez_video compile_translations)
endif()

install(TARGETS bubez_video DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
