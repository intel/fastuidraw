/*!
 * \file uber_shader_builder.hpp
 * \brief file uber_shader_builder.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#pragma once

#include <vector>
#include <string>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/util.hpp>
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

class DeclareVaryings:fastuidraw::noncopyable
{
public:
  void
  add_varyings(c_string suffix,
               size_t uint_count,
               size_t int_count,
               c_array<const size_t> float_counts,
               DeclareVaryingsStringDatum *datum);

  std::string
  declare_varyings(c_string varying_qualifier,
                   c_string interface_name = nullptr,
                   c_string instance_name = nullptr) const;
  
private:
  class per_varying
  {
  public:
    std::string m_type;
    std::string m_name;
    std::string m_qualifier;
    unsigned int m_slot;
  };

  void
  add_varyings_impl(c_string suffix,
                    size_t uint_count,
                    size_t int_count,
                    c_array<const size_t> float_counts);

  void
  add_varyings_impl_type(c_string suffix, unsigned int cnt,
                         c_string qualifier, c_string types[],
                         c_string name);

  std::vector<per_varying> m_varyings;
};

void
stream_alias_varyings(c_string suffix,
                      ShaderSource &shader,
                      const varying_list &p,
                      bool define,
                      const DeclareVaryingsStringDatum &datum);

void
stream_varyings_as_local_variables(ShaderSource &shader, const varying_list &p);

void
stream_uber_vert_shader(bool use_switch, ShaderSource &vert,
                        c_array<const reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const DeclareVaryingsStringDatum &datum);

void
stream_uber_frag_shader(bool use_switch, ShaderSource &frag,
                        c_array<const reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const DeclareVaryingsStringDatum &datum);


void
stream_uber_blend_shader(bool use_switch, ShaderSource &frag,
                         c_array<const reference_counted_ptr<PainterBlendShaderGLSL> > blend_shaders,
                         enum PainterBlendShader::shader_type tp);


}}}
