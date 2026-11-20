# ==============================================================================
# BubezView Audio Studio Plugin (bubez_audio)
# Hardware-accelerated audio playback & full Audio Studio based on libmpv
# ==============================================================================

pkg_check_modules(MPV mpv)

if(MPV_FOUND)
    message(STATUS "Audio Studio Engine: Building bubez_audio module with libmpv (${MPV_VERSION})")

    # Core symbols (FramelessDialog, AudioProcessor, EqualizerManager, VinylSleeveWidget,
    # SmoothWaveformScrubber, and Qt resources) are already exported by bubezview
    # (ENABLE_EXPORTS ON), avoiding duplicate compilation inside bubez_audio.so.
    set(AUDIO_PLUGIN_SOURCES
        media_plugin.cpp
        audio_tag_dialog.cpp
        audio_trim_dialog.cpp
        cover_selection_dialog.cpp
        equalizer_widget.cpp
        media_viewer.cpp
        mpv_video_widget.cpp
        stereo_oscilloscope_widget.cpp
    )

    add_library(bubez_audio MODULE ${AUDIO_PLUGIN_SOURCES})

    target_include_directories(bubez_audio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/core/interfaces
        ${CMAKE_SOURCE_DIR}/src/core/system
        ${CMAKE_SOURCE_DIR}/src/core/services
        ${CMAKE_SOURCE_DIR}/src/core/audio
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/ui/dialogs
        ${CMAKE_SOURCE_DIR}/src/viewers/media
        ${MPV_INCLUDE_DIRS}
    )

    target_link_libraries(bubez_audio PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Network
        Qt6::DBus
        ${MPV_LIBRARIES}
    )

    set_target_properties(bubez_audio PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        PREFIX ""
    )

    if(TARGET compile_translations)
        add_dependencies(bubez_audio compile_translations)
    endif()

    install(TARGETS bubez_audio DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
else()
    message(STATUS "Audio Studio Engine: libmpv not found, skipping bubez_audio plugin")
endif()
