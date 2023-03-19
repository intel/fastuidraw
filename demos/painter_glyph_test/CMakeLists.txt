
project(painter_glyph_test)

add_executable(${PROJECT_NAME} 
    glyph_finder.cpp
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC demos_common)
