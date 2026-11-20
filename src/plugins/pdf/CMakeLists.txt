# ==============================================================================
# BubezView PDF Plugin (bubez_pdf)
# High-performance PDF renderer and annotation/signing engine based on Poppler-Qt6
# ==============================================================================

pkg_check_modules(POPPLER_QT6 poppler-qt6)

if(POPPLER_QT6_FOUND OR STATIC_PDF)
    message(STATUS "PDF Engine: Building bubez_pdf module with poppler-qt6 (${POPPLER_QT6_VERSION})")

    # Core symbols (FramelessDialog, BaseStudioPanel, PdfCanvasTools, PdfDocumentOps,
    # PdfHistoryWidget, PdfImageExtractDialog, PdfRepairDialog, PackageManagerHelper,
    # FontResolutionHelper, GpuAccelerationManager, and Qt resources) are already exported
    # by the bubezview executable (ENABLE_EXPORTS ON), avoiding duplicate compilation.
    set(PDF_PLUGIN_SOURCES
        pdf_plugin.cpp
        pdf_studio_panel.cpp
        pdf_crypto_signature_dialog.cpp
        pdf_page_organizer_dialog.cpp
        pdf_signature_dialog.cpp
        pdf_typewriter_dialog.cpp
        pdf_watermark_dialog.cpp
        pdf_merge_dialog.cpp
        pdf_page_widget.cpp
        pdf_transform_box.cpp
        pdf_viewer.cpp
    )

    add_library(bubez_pdf MODULE ${PDF_PLUGIN_SOURCES})

    target_include_directories(bubez_pdf PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/core/interfaces
        ${CMAKE_SOURCE_DIR}/src/core/system
        ${CMAKE_SOURCE_DIR}/src/core/services
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/ui/pdf
        ${CMAKE_SOURCE_DIR}/src/ui/dialogs
        ${CMAKE_SOURCE_DIR}/src/ui/extensions
        ${CMAKE_SOURCE_DIR}/src/viewers/pdf
        ${POPPLER_QT6_INCLUDE_DIRS}
    )

    target_link_libraries(bubez_pdf PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::PrintSupport
        Qt6::Network
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
