
project(image_test)

set(IMAGE_TEST_RESOURCE_STRINGS
    shaders/layer_texture_blit.vert.glsl.resource_string
    shaders/layer_texture_blit.frag.glsl.resource_string
    shaders/atlas_image_blit.frag.glsl.resource_string
    shaders/atlas_image_blit.vert.glsl.resource_string
    shaders/detect_boundary.glsl.resource_string
)

assemble_resource_string_sources("${IMAGE_TEST_RESOURCE_STRINGS}" IMAGE_TEST_RESOURCE_STRING_SOURCES)
add_custom_target(generate_image_test_resource_strings DEPENDS ${IMAGE_TEST_RESOURCE_STRING_SOURCES})

add_executable(${PROJECT_NAME}
    ${IMAGE_TEST_RESOURCE_STRING_SOURCES}
    main.cpp
)

add_dependencies(${PROJECT_NAME} generate_image_test_resource_strings)

target_link_libraries(${PROJECT_NAME} PUBLIC demos_common)
