
project(gradient_test)

set(GRADIENT_TEST_RESOURCE_STRINGS
    shaders/linear_gradient.vert.glsl.resource_string
    shaders/linear_gradient.frag.glsl.resource_string
    shaders/draw_pt.vert.glsl.resource_string
    shaders/draw_pt.frag.glsl.resource_string
)

assemble_resource_string_sources("${GRADIENT_TEST_RESOURCE_STRINGS}" GRADIENT_TEST_RESOURCE_STRING_SOURCES)
add_custom_target(generate_gradient_test_resource_strings DEPENDS ${GRADIENT_TEST_RESOURCE_STRING_SOURCES})

add_executable(${PROJECT_NAME}
    ${GRADIENT_TEST_RESOURCE_STRING_SOURCES}
    main.cpp
)

add_dependencies(${PROJECT_NAME} generate_gradient_test_resource_strings)

target_link_libraries(${PROJECT_NAME} PUBLIC demos_common)
