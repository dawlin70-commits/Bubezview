# ==============================================================================
# BubezView Modular Plugins Directory
# ==============================================================================

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")

add_subdirectory(pdf)
add_subdirectory(archive)
add_subdirectory(media)
