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
#include <fastuidraw/glsl/painter_backend_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

class DeclareVaryingsStringDatum
{
public:
  unsigned int m_uint_special_index;
  unsigned int m_int_special_index;
  vecN<unsigned int, varying_list::interpolation_number_types> m_float_special_index;
};

std::string
declare_varyings_string(const char *append_to_name,
                        size_t uint_count,
                        size_t int_count,
                        const_c_array<size_t> float_counts,
                        unsigned int *slot,
                        DeclareVaryingsStringDatum *datum);
void
stream_alias_varyings(const char *append_to_name,
                      ShaderSource &shader,
                      const varying_list &p,
                      bool define,
                      const DeclareVaryingsStringDatum &datum);

void
stream_varyings_as_local_variables(ShaderSource &shader, const varying_list &p);

void
stream_uber_vert_shader(bool use_switch, ShaderSource &vert,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const DeclareVaryingsStringDatum &datum);

void
stream_uber_frag_shader(bool use_switch, ShaderSource &frag,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const DeclareVaryingsStringDatum &datum);


void
stream_uber_blend_shader(bool use_switch, ShaderSource &frag,
                         const_c_array<reference_counted_ptr<PainterBlendShaderGLSL> > blend_shaders,
                         enum PainterBlendShader::shader_type tp);


}}}
