
project(demos)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # For FindSDL2_image.cmake

add_subdirectory(common)

add_subdirectory(atlas_tests/glyph_test)
add_subdirectory(atlas_tests/gradient_test)
add_subdirectory(atlas_tests/image_test)

add_subdirectory(painter_cells)
add_subdirectory(painter_clippath_test)
add_subdirectory(painter_cliprect_test)
add_subdirectory(painter_glyph_test)
add_subdirectory(painter_path_test)
add_subdirectory(painter_test)
