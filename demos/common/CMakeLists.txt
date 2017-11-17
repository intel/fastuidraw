
project(demos_common)

add_library(${PROJECT_NAME}
    generic_command_line.cpp
    colorstop_command_line.cpp
    sdl_benchmark.cpp
    sdl_demo.cpp
    sdl_painter_demo.cpp
    PanZoomTracker.cpp
    ImageLoader.cpp
    read_colorstops.cpp
    read_path.cpp
    text_helper.cpp
    PainterWidget.cpp
    cycle_value.cpp
    random.cpp
    read_dash_pattern.cpp
    egl_helper.cpp
)

# SDL2

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

target_include_directories(${PROJECT_NAME} PUBLIC ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})

# EGL (fixme: why is it required for the demos?)

find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::OpenGL OpenGL::EGL)

# FastUIDraw
target_include_directories(${PROJECT_NAME} PUBLIC FastUIDraw)
target_link_libraries(${PROJECT_NAME} PUBLIC FastUIDraw)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
