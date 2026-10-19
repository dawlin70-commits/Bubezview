# ==============================================================================
# BubezView PDF Plugin (bubez_pdf)
# High-performance PDF renderer and annotation/signing engine based on Poppler-Qt6
# ==============================================================================

pkg_check_modules(POPPLER_QT6 poppler-qt6)

if(POPPLER_QT6_FOUND OR STATIC_PDF)
    message(STATUS "PDF Engine: Building bubez_pdf module with poppler-qt6 (${POPPLER_QT6_VERSION})")

    set(PDF_PLUGIN_SOURCES
        pdf_plugin.cpp
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
        ${BUBEZVIEW_RESOURCES}
    )

    add_library(bubez_pdf MODULE ${PDF_PLUGIN_SOURCES})

    target_include_directories(bubez_pdf PRIVATE
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
        ${CMAKE_SOURCE_DIR}/src/viewers/pdf
        ${POPPLER_QT6_INCLUDE_DIRS}
    )

    target_link_libraries(bubez_pdf PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        ${POPPLER_QT6_LIBRARIES}
    )

    set_target_properties(bubez_pdf PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        PREFIX ""
    )

    if(TARGET compile_translations)
        add_dependencies(bubez_pdf compile_translations)
    endif()

    install(TARGETS bubez_pdf DESTINATION "${CMAKE_INSTALL_LIBDIR}/bubezview/plugins")
else()
    message(STATUS "PDF Engine: poppler-qt6 not found, skipping bubez_pdf plugin")
endif()
