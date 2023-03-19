
project(FastUIDraw)

find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options

option(FASTUIDRAW_GL_BACKEND "Use the GL backend" ON)
option(FASTUIDRAW_GLES_BACKEND "Use the GLES backend" OFF)
option(FASTUIDRAW_NEGL "Build NEGL" OFF)

# FIXME: figure out why EQUAL doesn't work
if("${FASTUIDRAW_GL_BACKEND}" STREQUAL "${FASTUIDRAW_GLES_BACKEND}")
    message(FATAL_ERROR "You have to select either the GL or the GLES backend!")
endif()

set(GL_INCLUDEPATH "${OPENGL_INCLUDE_DIR}" CACHE STRING "Directory where GL/GLES headers are located")

set(GL_DEFAULT_RAW_HEADER_FILES
    GL/glcorearb.h
)

set(GL_RAW_HEADER_FILES "${GL_DEFAULT_RAW_HEADER_FILES}" CACHE STRING "GL header files")

if(FASTUIDRAW_NEGL)

    if(NOT OpenGL_EGL_FOUND)
        message(FATAL_ERROR "EGL requested, but it's not available on the system.")
    endif()

    set(EGL_INCLUDEPATH "${OPENGL_EGL_INCLUDE_DIRS}" CACHE STRING "Directory where EGL headers are located")

    set(EGL_DEFAULT_RAW_HEADER_FILES
        EGL/egl.h
        EGL/eglext.h
    )

    set(EGL_RAW_HEADER_FILES "${EGL_DEFAULT_RAW_HEADER_FILES}" CACHE STRING "EGL header files")

endif()

if(FASTUIDRAW_GLES_BACKEND)

    set(GLES_DEFAULT_RAW_HEADER_FILES
        GLES3/gl3platform.h
        GLES3/gl3.h
        GLES3/gl31.h
        GLES3/gl32.h
        GLES2/gl2ext.h
    )

    set(GLES_RAW_HEADER_FILES "${GLES_DEFAULT_RAW_HEADER_FILES}" CACHE STRING "GLES header files")

endif()

add_subdirectory(ngl_generator)

# Sources

if(FASTUIDRAW_GL_BACKEND)

    set(NGL_GL_CPP ${FASTUIDRAW_ROOT_DIR}/src/fastuidraw/gl_backend/ngl_gl.cpp)
    set(NGL_GL_HPP ${FASTUIDRAW_ROOT_DIR}/inc/fastuidraw/gl_backend/ngl_gl.hpp)

    set(GL_BACKEND_NGL_SOURCES ${NGL_GL_HPP} ${NGL_GL_CPP})

    prepend_path(GL_HEADER_FILES ${GL_INCLUDEPATH} ${GL_RAW_HEADER_FILES})

    add_custom_command(
        OUTPUT ${GL_BACKEND_NGL_SOURCES}
        COMMAND filter ${GL_HEADER_FILES} > gl_filter_output.txt
        COMMAND extractor macro_prefix=FASTUIDRAWgl namespace=fastuidraw::gl_binding path=${GL_INCLUDEPATH} output_cpp=${NGL_GL_CPP} output_hpp=${NGL_GL_HPP} ${GL_RAW_HEADER_FILES} < gl_filter_output.txt
        WORKING_DIRECTORY ${FASTUIDRAW_ROOT_DIR}
        COMMENT "Generating NGL sources for GL"
        VERBATIM
    )

    add_custom_target(ngl_generate_gl DEPENDS ${GL_BACKEND_NGL_SOURCES})

elseif(FASTUIDRAW_GLES_BACKEND)

    set(NGL_GLES_CPP ${FASTUIDRAW_ROOT_DIR}/src/fastuidraw/gl_backend/ngl_gles3.cpp)
    set(NGL_GLES_HPP ${FASTUIDRAW_ROOT_DIR}/inc/fastuidraw/gl_backend/ngl_gles3.hpp)

    set(GL_BACKEND_NGLES_SOURCES ${NGL_GLES_HPP} ${NGL_GLES_CPP})

    prepend_path(GLES_HEADER_FILES ${GL_INCLUDEPATH} ${GLES_RAW_HEADER_FILES})

    add_custom_command(
        OUTPUT ${GL_BACKEND_NGLES_SOURCES}
        COMMAND filter ${GLES_HEADER_FILES} > gles_filter_output.txt
        COMMAND extractor macro_prefix=FASTUIDRAWgl namespace=fastuidraw::gl_binding path=${GL_INCLUDEPATH} output_cpp=${NGL_GLES_CPP} output_hpp=${NGL_GLES_HPP} ${GLES_RAW_HEADER_FILES} < gles_filter_output.txt
        WORKING_DIRECTORY ${FASTUIDRAW_ROOT_DIR}
        COMMENT "Generating NGL sources for GLES"
        VERBATIM
    )

    add_custom_target(ngl_generate_gles DEPENDS ${GL_BACKEND_NGLES_SOURCES})

endif()

# gl_backend

set(GL_BACKEND_NGL_COMMON_SOURCES
    gl_backend/gl_binding.cpp
)

set(GL_BACKEND_PRIVATE_SOURCES
    gl_backend/private/tex_buffer.cpp
    gl_backend/private/texture_gl.cpp
    gl_backend/private/texture_view.cpp
)

set(GL_BACKEND_RESOURCE_STRINGS
    gl_backend/shaders/fastuidraw_painter_gles_precision.glsl.resource_string
)

set(GL_BACKEND_SOURCES
    ${GL_BACKEND_NGL_COMMON_SOURCES}
    ${GL_BACKEND_PRIVATE_SOURCES}
    ${GL_BACKEND_RESOURCE_STRINGS}

    gl_backend/gl_get.cpp
    gl_backend/opengl_trait.cpp
    gl_backend/gluniform_implement.cpp
    gl_backend/gl_program.cpp
    gl_backend/gl_context_properties.cpp
    gl_backend/image_gl.cpp
    gl_backend/colorstop_atlas_gl.cpp
    gl_backend/glyph_atlas_gl.cpp
    gl_backend/painter_backend_gl.cpp
)

if(FASTUIDRAW_GL_BACKEND)
    list(APPEND GL_BACKEND_SOURCES ${GL_BACKEND_NGL_SOURCES})
elseif(FASTUIDRAW_GLES_BACKEND)
    list(APPEND GL_BACKEND_SOURCES ${GL_BACKEND_NGLES_SOURCES})
endif()

# NEGL

if(FASTUIDRAW_NEGL)

    set(NGL_EGL_CPP ${FASTUIDRAW_ROOT_DIR}/src/fastuidraw/ngl_egl.cpp)
    set(NGL_EGL_HPP ${FASTUIDRAW_ROOT_DIR}/inc/fastuidraw/ngl_egl.hpp)

    set(NEGL_SOURCES ${NGL_EGL_HPP} ${NGL_EGL_CPP})

    prepend_path(EGL_HEADER_FILES ${EGL_INCLUDEPATH} ${EGL_RAW_HEADER_FILES})

    add_custom_command(
        OUTPUT ${NEGL_SOURCES}
        COMMAND filter ${EGL_HEADER_FILES} > egl_filter_output.txt
        COMMAND extractor macro_prefix=FASTUIDRAWegl namespace=fastuidraw::egl_binding path=${EGL_INCLUDEPATH} output_cpp=${NGL_EGL_CPP} output_hpp=${NGL_EGL_HPP} ${EGL_RAW_HEADER_FILES} < egl_filter_output.txt
        WORKING_DIRECTORY ${FASTUIDRAW_ROOT_DIR}
        COMMENT "Generating NGL sources for EGL"
        VERBATIM
    )

    add_custom_target(ngl_generate_egl DEPENDS ${NEGL_SOURCES})

    list(APPEND NEGL_SOURCES egl_binding.cpp)

endif()

# glsl

set(GLSL_PRIVATE_SOURCES
    glsl/private/backend_shaders.cpp
    glsl/private/uber_shader_builder.cpp
)

set(GLSL_PAINTER_BLEND_DUAL_SRC_RESOURCE_STRINGS
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_clear.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_dst_out.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_src_in.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_dst_atop.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_dst_over.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_src_out.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_dst.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_src_atop.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_src_over.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_dst_in.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_src.glsl.resource_string
    glsl/shaders/painter/blend/dual_src/fastuidraw_porter_duff_xor.glsl.resource_string
)

set(GLSL_PAINTER_BLEND_FRAMEBUFFER_FETCH_RESOURCE_STRINGS
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_clear.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_dst_out.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_src_in.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_dst_over.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_src_out.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_dst.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_src_atop.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_src_over.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_dst_in.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_src.glsl.resource_string
    glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_fbf_porter_duff_xor.glsl.resource_string
)

set(GLSL_PAINTER_BLEND_SINGLE_SRC_RESOURCE_STRINGS
    glsl/shaders/painter/blend/single_src/fastuidraw_fall_through.glsl.resource_string
)

set(GLSL_PAINTER_BLEND_RESOURCE_STRINGS
    ${GLSL_PAINTER_BLEND_DUAL_SRC_RESOURCE_STRINGS}
    ${GLSL_PAINTER_BLEND_FRAMEBUFFER_FETCH_RESOURCE_STRINGS}
    ${GLSL_PAINTER_BLEND_SINGLE_SRC_RESOURCE_STRINGS}
)

set(GLSL_PAINTER_BRUSH_RESOURCE_STRINGS
    glsl/shaders/painter/brush/fastuidraw_painter_brush.vert.glsl.resource_string
    glsl/shaders/painter/brush/fastuidraw_painter_brush_types.glsl.resource_string
    glsl/shaders/painter/brush/fastuidraw_painter_brush_unpack.glsl.resource_string
    glsl/shaders/painter/brush/fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string
    glsl/shaders/painter/brush/fastuidraw_painter_brush_macros.glsl.resource_string
    glsl/shaders/painter/brush/fastuidraw_painter_brush.frag.glsl.resource_string
)

set(GLSL_PAINTER_GLYPH_RESOURCE_STRINGS
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_coverage.vert.glsl.resource_string
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_coverage.frag.glsl.resource_string
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_distance_field_anisotropic.frag.glsl.resource_string
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_curve_pair.vert.glsl.resource_string
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_curve_pair.frag.glsl.resource_string
    glsl/shaders/painter/glyph/fastuidraw_painter_glyph_curve_pair_anisotropic.frag.glsl.resource_string
)

set(GLSL_PAINTER_RESOURCE_STRINGS
    ${GLSL_PAINTER_BLEND_RESOURCE_STRINGS}
    ${GLSL_PAINTER_BRUSH_RESOURCE_STRINGS}
    ${GLSL_PAINTER_GLYPH_RESOURCE_STRINGS}

    glsl/shaders/painter/fastuidraw_painter_main.vert.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_types.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_forward_declares.vert.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_uniforms.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_forward_declares.frag.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_main.frag.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_stroke.vert.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_stroke.frag.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_fill.vert.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_fill.frag.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_fill_aa_fuzz.vert.glsl.resource_string
    glsl/shaders/painter/fastuidraw_painter_fill_aa_fuzz.frag.glsl.resource_string
)

set(GLSL_RESOURCE_STRINGS
    ${GLSL_PAINTER_RESOURCE_STRINGS}

    glsl/shaders/fastuidraw_atlas_image_fetch.glsl.resource_string
    glsl/shaders/fastuidraw_curvepair_glyph.frag.glsl.resource_string
    glsl/shaders/fastuidraw_curvepair_glyph_derivative.frag.glsl.resource_string
    glsl/shaders/fastuidraw_circular_interpolate.glsl.resource_string
    glsl/shaders/fastuidraw_anisotropic.frag.glsl.resource_string
    glsl/shaders/fastuidraw_unpack_unit_vector.glsl.resource_string
    glsl/shaders/fastuidraw_align.vert.glsl.resource_string
    glsl/shaders/fastuidraw_compute_local_distance_from_pixel_distance.glsl.resource_string
)

set(GLSL_SOURCES
    ${GLSL_PRIVATE_SOURCES}
    ${GLSL_RESOURCE_STRINGS}

    glsl/shader_source.cpp
    glsl/shader_code.cpp
    glsl/painter_item_shader_glsl.cpp
    glsl/painter_blend_shader_glsl.cpp
    glsl/painter_backend_glsl.cpp
)

# private

set(GLU_TESS_SOURCES
    ../3rd_party/glu-tess/dict.cpp
    ../3rd_party/glu-tess/geom.cpp
    ../3rd_party/glu-tess/memalloc.cpp
    ../3rd_party/glu-tess/mesh.cpp
    ../3rd_party/glu-tess/priorityq.cpp
    ../3rd_party/glu-tess/render.cpp
    ../3rd_party/glu-tess/sweep.cpp
    ../3rd_party/glu-tess/tess.cpp
    ../3rd_party/glu-tess/tessmono.cpp
)

set(PRIVATE_SOURCES
    ${GLU_TESS_SOURCES}

    private/interval_allocator.cpp
    private/path_util_private.cpp
    private/clip.cpp
    private/int_path.cpp
)

# painter

set(PAINTER_PACKING_SOURCES
    painter/packing/painter_backend.cpp
    painter/packing/painter_draw.cpp
    painter/packing/painter_packer.cpp
)

set(PAINTER_SOURCES
    ${PAINTER_PACKING_SOURCES}

    painter/fill_rule.cpp
    painter/painter_attribute_data.cpp
    painter/painter_attribute_data_filler_glyphs.cpp
    painter/painter_brush.cpp
    painter/painter_stroke_params.cpp
    painter/painter_dashed_stroke_params.cpp
    painter/painter.cpp
    painter/painter_enums.cpp
    painter/painter_shader_data.cpp
    painter/painter_clip_equations.cpp
    painter/painter_item_matrix.cpp
    painter/painter_header.cpp
    painter/painter_shader.cpp
    painter/painter_shader_set.cpp
    painter/painter_dashed_stroke_shader_set.cpp
    painter/painter_stroke_shader.cpp
    painter/painter_glyph_shader.cpp
    painter/painter_blend_shader_set.cpp
    painter/painter_fill_shader.cpp
    painter/stroked_path.cpp
    painter/filled_path.cpp
)

# text

set(TEXT_PRIVATE_SOURCES
    text/private/rect_atlas.cpp
)

set(TEXT_SOURCES
    ${TEXT_PRIVATE_SOURCES}

    text/glyph_atlas.cpp
    text/glyph_render_data.cpp
    text/glyph_render_data_curve_pair.cpp
    text/glyph_render_data_distance_field.cpp
    text/glyph_render_data_coverage.cpp
    text/glyph_cache.cpp 
    text/glyph_selector.cpp
    text/freetype_face.cpp 
    text/freetype_lib.cpp
    text/font_freetype.cpp 
    text/font_properties.cpp
)

# util

set(UTIL_SOURCES
    util/static_resource.cpp
    util/fastuidraw_memory.cpp
    util/util.cpp
    util/blend_mode.cpp
    util/reference_count_mutex.cpp
    util/reference_count_atomic.cpp
    util/pixel_distance_math.cpp
    util/data_buffer.cpp
    util/api_callback.cpp
)

# resource strings

set(RESOURCE_STRINGS
    ${GLSL_RESOURCE_STRINGS}
    ${GL_BACKEND_RESOURCE_STRINGS}
)

assemble_resource_string_sources("${RESOURCE_STRINGS}" RESOURCE_STRING_SOURCES)
add_custom_target(generate_fastuidraw_resource_strings DEPENDS ${RESOURCE_STRING_SOURCES})

# all together

set(SOURCES
    ${GL_BACKEND_SOURCES}
    ${GLSL_SOURCES}
    ${PAINTER_SOURCES}
    ${PRIVATE_SOURCES}
    ${TEXT_SOURCES}
    ${UTIL_SOURCES}
    ${RESOURCE_STRING_SOURCES}

    colorstop_atlas.cpp
    colorstop.cpp
    image.cpp
    path.cpp
    tessellated_path.cpp
)

if(FASTUIDRAW_NEGL)
    list(APPEND SOURCES ${NEGL_SOURCES})
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES}) # TODO: static library if requested - it doesn't work with the static resource system currently (gets optimized away?)
target_include_directories(${PROJECT_NAME} PUBLIC ${FASTUIDRAW_ROOT_DIR}/inc)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE GL_DEBUG)
endif()

# Dependencies

find_package(Freetype REQUIRED)

target_include_directories(${PROJECT_NAME} PUBLIC ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${FREETYPE_LIBRARIES})

# OpenGL binding dependencies

if(FASTUIDRAW_GL_BACKEND)

    add_dependencies(${PROJECT_NAME} ngl_generate_gl)

    target_link_libraries(${PROJECT_NAME} PUBLIC ${OPENGL_opengl_LIBRARY})

elseif(FASTUIDRAW_GLES_BACKEND)

    add_dependencies(${PROJECT_NAME} ngl_generate_gles)

    target_compile_definitions(${PROJECT_NAME} PUBLIC FASTUIDRAW_GL_USE_GLES)
    target_link_libraries(${PROJECT_NAME} PUBLIC GLESv2)

endif()

if(FASTUIDRAW_NEGL)

    add_dependencies(${PROJECT_NAME} ngl_generate_egl)

    target_include_directories(${PROJECT_NAME} PUBLIC ${OPENGL_EGL_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${OPENGL_egl_LIBRARY})

endif()

add_dependencies(${PROJECT_NAME} generate_fastuidraw_resource_strings)
