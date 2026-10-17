# ==============================================================================
# BubezView Media Plugin (bubez_media)
# Hardware-accelerated audio/video playback and audio tools based on libmpv
# ==============================================================================

pkg_check_modules(MPV mpv)

if(MPV_FOUND)
    message(STATUS "Media Engine: Building bubez_media module with libmpv (${MPV_VERSION})")

    set(MEDIA_PLUGIN_SOURCES
        media_plugin.cpp
        ${CMAKE_SOURCE_DIR}/src/core/audio_processor.cpp
        ${CMAKE_SOURCE_DIR}/src/core/equalizer_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/audio_tag_dialog.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/audio_trim_dialog.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/cover_selection_dialog.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/equalizer_widget.cpp
        ${CMAKE_SOURCE_DIR}/src/viewers/media/media_viewer.cpp
        ${CMAKE_SOURCE_DIR}/src/viewers/media/mpv_video_widget.cpp
        ${CMAKE_SOURCE_DIR}/src/viewers/media/stereo_oscilloscope_widget.cpp
        ${BUBEZVIEW_RESOURCES}
    )

    add_library(bubez_media MODULE ${MEDIA_PLUGIN_SOURCES})

    target_include_directories(bubez_media PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/core/interfaces
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/viewers/media
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

    install(TARGETS bubez_media DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
else()
    message(STATUS "Media Engine: libmpv not found, skipping bubez_media plugin")
endif()
