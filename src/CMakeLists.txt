# ==============================================================================
# BubezView Core Application (bubezview)
# ==============================================================================

set(APP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/system/app_launcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/system/build_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/system/cpu_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/system/dbus_previewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/system/desktop_integrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/services/ocr_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/system/package_manager_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/services/plugin_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/services/preload_cache_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/services/theme_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/services/update_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/accordion_extension_card.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/action_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/base_skin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/build_console_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/build_options_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/compiler_wizard_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/configure_toolbar_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/custom_source_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/engine_prompt_card.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/extension_feature_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/extension_row_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/fallback_notice_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/filmstrip_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions/first_run_wizard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/flippable_toolbar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/goto_folder_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ocr/ocr_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ocr/ocr_language_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ocr/ocr_snipper_overlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/open_with_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/pdf/pdf_image_extract_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/pdf/pdf_merge_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/pdf/pdf_repair_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/search_bar_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings/settings_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings/settings_appearance_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings/settings_extensions_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings/settings_extra_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings/settings_integration_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings/settings_performance_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/share_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/share_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/update_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/folder/folder_viewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/image/image_viewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/office/office_viewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/text/line_number_area.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/text/syntax_highlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/text/text_viewer.cpp
    CACHE INTERNAL "BubezView core application source files"
)

# Common include directories for core and viewers
set(APP_COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/core/system
    ${CMAKE_CURRENT_SOURCE_DIR}/core/services
    ${CMAKE_CURRENT_SOURCE_DIR}/core/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/pdf
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ocr
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/image
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/text
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/office
    ${CMAKE_CURRENT_SOURCE_DIR}/viewers/folder
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${LIBREVENGE_INCLUDE_DIRS}
    ${LIBQRENCODE_INCLUDE_DIRS}
    CACHE INTERNAL "BubezView common include paths"
)

add_executable(bubezview
    main.cpp
    ${APP_CORE_SOURCES}
    ${BUBEZVIEW_RESOURCES}
)

if(TARGET compile_translations)
    add_dependencies(bubezview compile_translations)
endif()

target_include_directories(bubezview PRIVATE ${APP_COMMON_INCLUDES})

target_link_libraries(bubezview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    Qt6::DBus
    Qt6::PrintSupport
    Qt6::Network
    Qt6::Svg
    ${LIBARCHIVE_LIBRARIES}
    ${LIBREVENGE_LIBRARIES}
    ${LIBQRENCODE_LIBRARIES}
)

set_target_properties(bubezview PROPERTIES ENABLE_EXPORTS ON)
target_compile_options(bubezview PRIVATE ${LIBARCHIVE_CFLAGS_OTHER} ${LIBREVENGE_CFLAGS_OTHER} ${LIBQRENCODE_CFLAGS_OTHER})

install(TARGETS bubezview DESTINATION ${CMAKE_INSTALL_BINDIR})

# Build modular plugins
add_subdirectory(plugins)
