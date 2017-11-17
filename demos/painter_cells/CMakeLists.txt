
project(painter_cells)

add_executable(${PROJECT_NAME} 
    cell_group.cpp
    cell.cpp
    main.cpp
    table.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC demos_common)
