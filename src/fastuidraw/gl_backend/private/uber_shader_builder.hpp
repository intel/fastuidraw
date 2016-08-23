#pragma once

#include <list>
#include <map>
#include <sstream>
#include <vector>

#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>

#include <fastuidraw/stroked_path.hpp>
#include <fastuidraw/painter/painter_shader_set.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>
#include <fastuidraw/painter/painter_stroke_value.hpp>

namespace fastuidraw { namespace glsl { namespace detail { namespace shader_builder {


void
add_enums(unsigned int alignment, ShaderSource &src);

void
add_texture_size_constants(ShaderSource &src,
                           const gl::PainterBackendGL::params &P);

void
stream_declare_varyings(std::ostream &str,
                        size_t uint_count, size_t int_count,
                        const_c_array<size_t> float_counts);

void
stream_unpack_code(unsigned int alignment, ShaderSource &str);

void
stream_uber_vert_shader(bool use_switch, ShaderSource &vert,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders);

void
stream_uber_frag_shader(bool use_switch, ShaderSource &frag,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders);


void
stream_uber_blend_shader(bool use_switch, ShaderSource &frag,
                         const_c_array<reference_counted_ptr<PainterBlendShaderGLSL> > blend_shaders,
                         enum PainterBlendShader::shader_type tp);


}}}}
