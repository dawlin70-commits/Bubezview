# ==============================================================================
# BubezView Test Suite
# ==============================================================================

enable_testing()

# Automated fast test (executed during ctest)
add_executable(test_share
    test_share.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/share_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/share_dialog.cpp
)

target_include_directories(test_share PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs
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

# Automated GPU acceleration manager test
add_executable(test_gpu
    test_gpu.cpp
    ${CMAKE_SOURCE_DIR}/src/core/system/gpu_acceleration_manager.cpp
)

target_include_directories(test_gpu PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/system
)

target_link_libraries(test_gpu PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Test
)

add_test(NAME TestGpu COMMAND test_gpu)

# Automated image viewer, adjustments, and transparency test
add_executable(test_image
    test_image.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/image/image_viewer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/theme_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/system/gpu_acceleration_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/image_processor.cpp
)

target_include_directories(test_image PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/services
    ${CMAKE_SOURCE_DIR}/src/core/system
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs
    ${CMAKE_SOURCE_DIR}/src/viewers/image
)

target_link_libraries(test_image PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Test
)

add_test(NAME TestImage COMMAND test_image)

# Automated EXIF metadata and GPS parsing test
add_executable(test_exif
    test_exif.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/exif_parser.cpp
)

target_include_directories(test_exif PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/services
)

target_link_libraries(test_exif PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME TestExif COMMAND test_exif)

# Automated native system media playback test
add_executable(test_media_system
    test_media_system.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/media/system_media_viewer.cpp
)

target_include_directories(test_media_system PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/interfaces
    ${CMAKE_SOURCE_DIR}/src/viewers/media
)

target_link_libraries(test_media_system PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Test
)

add_test(NAME TestMediaSystem COMMAND test_media_system)

# Interactive manual test target (excluded from default build and ctest per Rule 2)
add_executable(test_interactive EXCLUDE_FROM_ALL
    test_interactive.cpp
    ${APP_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/ui/pdf/pdf_crypto_signature_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pdf/pdf_image_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pdf/pdf_page_organizer_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pdf/pdf_signature_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pdf/pdf_typewriter_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pdf/pdf_watermark_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/pdf/pdf_canvas_tools.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/pdf/pdf_document_ops.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/pdf/pdf_page_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/pdf/pdf_transform_box.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/pdf/pdf_viewer.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/archive/archive_viewer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audio/audio_processor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audio/equalizer_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/audio/audio_tag_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/audio/audio_trim_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/audio/cover_selection_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/audio/equalizer_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/media/media_viewer.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/media/mpv_video_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/viewers/media/stereo_oscilloscope_widget.cpp
    ${BUBEZVIEW_RESOURCES}
)

target_compile_definitions(test_interactive PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

target_include_directories(test_interactive PRIVATE
    ${APP_COMMON_INCLUDES}
    ${CMAKE_SOURCE_DIR}/src/viewers/pdf
    ${CMAKE_SOURCE_DIR}/src/viewers/archive
    ${CMAKE_SOURCE_DIR}/src/viewers/media
    ${POPPLER_QT6_INCLUDE_DIRS}
    ${MPV_INCLUDE_DIRS}
)

target_link_libraries(test_interactive PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::DBus Qt6::OpenGLWidgets
    Qt6::PrintSupport Qt6::Network Qt6::Svg Qt6::Test
    ${POPPLER_QT6_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    ${LIBREVENGE_LIBRARIES}
    ${LIBQRENCODE_LIBRARIES}
    ${MPV_LIBRARIES}
)

set_target_properties(test_interactive PROPERTIES ENABLE_EXPORTS ON)
