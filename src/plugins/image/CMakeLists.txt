# ==============================================================================
# BubezView Image Studio Plugin (bubez_image)
# Modular photo editing controls: adjustments, presets, auto-enhance & effects
# ==============================================================================

message(STATUS "Image Studio Engine: Building bubez_image module")

set(IMAGE_PLUGIN_SOURCES
    image_plugin.cpp
    image_studio_widget.cpp
)

add_library(bubez_image MODULE ${IMAGE_PLUGIN_SOURCES})

target_include_directories(bubez_image PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/interfaces
    ${CMAKE_SOURCE_DIR}/src/core/services
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bubez_image PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(bubez_image PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)

if(TARGET compile_translations)
    add_dependencies(bubez_image compile_translations)
endif()

install(TARGETS bubez_image DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
