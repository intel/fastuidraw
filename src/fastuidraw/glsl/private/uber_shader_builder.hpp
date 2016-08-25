#pragma once

#include <list>
#include <map>
#include <sstream>
#include <vector>

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/stroked_path.hpp>
#include <fastuidraw/painter/packing/painter_backend.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_value.hpp>
#include <fastuidraw/painter/painter_stroke_value.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>
#include <fastuidraw/painter/packing/painter_packing_brush.hpp>
#include <fastuidraw/glsl/shader_source.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {


void
add_enums(unsigned int alignment, ShaderSource &src);

void
add_texture_size_constants(ShaderSource &src,
                           const GlyphAtlas *glyph_atlas,
                           const ImageAtlas *image_atlas,
                           const ColorStopAtlas *colorstop_atlas);

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


}}}
