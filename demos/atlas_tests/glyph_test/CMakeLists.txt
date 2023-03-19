
project(glyph_test)

set(GLYPH_TEST_RESOURCE_STRINGS
    shaders/glyph.vert.glsl.resource_string
    shaders/coverage_glyph.frag.glsl.resource_string
    shaders/distance_glyph.frag.glsl.resource_string
    shaders/curvepair_glyph.frag.glsl.resource_string
    shaders/glyph_atlas.vert.glsl.resource_string
    shaders/glyph_atlas.frag.glsl.resource_string
    shaders/perform_aa.frag.glsl.resource_string
    shaders/gles_prec.frag.glsl.resource_string
)

assemble_resource_string_sources("${GLYPH_TEST_RESOURCE_STRINGS}" GLYPH_TEST_RESOURCE_STRING_SOURCES)
add_custom_target(generate_glyph_test_resource_strings DEPENDS ${GLYPH_TEST_RESOURCE_STRING_SOURCES})

add_executable(${PROJECT_NAME}
    ${GLYPH_TEST_RESOURCE_STRING_SOURCES}
    main.cpp
)

add_dependencies(${PROJECT_NAME} generate_glyph_test_resource_strings)

target_link_libraries(${PROJECT_NAME} PUBLIC demos_common)
