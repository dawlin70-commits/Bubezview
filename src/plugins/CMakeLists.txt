# ==============================================================================
# BubezView Modular Edit Studio Plugins Directory
# Each .so module is optional and built on demand (or during updates if active).
# ==============================================================================

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")

if(BUILD_PLUGIN_PDF OR BUILD_ALL_PLUGINS)
    add_subdirectory(pdf)
endif()

if(BUILD_PLUGIN_AUDIO OR BUILD_PLUGIN_MEDIA OR BUILD_ALL_PLUGINS)
    add_subdirectory(audio)
endif()

if(BUILD_PLUGIN_VIDEO OR BUILD_ALL_PLUGINS)
    add_subdirectory(video)
endif()

if(BUILD_PLUGIN_FOLDER OR BUILD_ALL_PLUGINS)
    add_subdirectory(folder)
endif()

if(BUILD_PLUGIN_IMAGE OR BUILD_ALL_PLUGINS)
    add_subdirectory(image)
endif()
