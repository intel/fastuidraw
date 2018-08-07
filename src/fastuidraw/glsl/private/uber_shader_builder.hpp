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
#include <fastuidraw/glsl/painter_composite_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

class UberShaderVaryings;
class AliasVaryingLocation
{
private:
  friend class UberShaderVaryings;

  /* What varying in UberShaderVaryings to which to
   * start aliasing: which varying and which component
   */
  std::string m_label;
  uvec2 m_uint_varying_start;
  uvec2 m_int_varying_start;
  vecN<uvec2, varying_list::interpolation_number_types> m_float_varying_start;
};

class UberShaderVaryings:fastuidraw::noncopyable
{
public:
  void
  add_varyings(c_string label,
               const varying_list &p,
               AliasVaryingLocation *datum)
  {
    add_varyings(label,
                 p.uints().size(),
                 p.ints().size(),
                 p.float_counts(),
                 datum);
  }

  void
  add_varyings(c_string label,
               size_t uint_count,
               size_t int_count,
               c_array<const size_t> float_counts,
               AliasVaryingLocation *datum);

  void
  declare_varyings(std::ostringstream &str,
                   c_string varying_qualifier,
                   c_string interface_name = nullptr,
                   c_string instance_name = nullptr) const;

  std::string
  declare_varyings(c_string varying_qualifier,
                   c_string interface_name = nullptr,
                   c_string instance_name = nullptr) const
  {
    std::ostringstream str;
    declare_varyings(str, varying_qualifier,
                     interface_name, instance_name);
    return str.str();
  }

  /*!
   * Add or remove aliases that have elements of p
   * refer to varying declared by a UberShaderVaryings
   * \param shader ShaderSource to which to stream
   * \param p how many varyings of each type along with their names
   * \param add_aliases if true add the aliases by add_macro(),
   *                    if false remove the aliases by remove_macro()
   */
  void
  stream_alias_varyings(ShaderSource &shader, const varying_list &p,
                        bool add_aliases,
                        const AliasVaryingLocation &datum) const;
private:
  class per_varying
  {
  public:
    bool m_is_flat;
    std::string m_type;
    std::string m_name;
    std::string m_qualifier;
    unsigned int m_num_components;
  };

  uvec2
  add_varyings_impl_type(std::vector<per_varying> &varyings,
                         unsigned int cnt,
                         c_string qualifier, c_string types[],
                         c_string name, bool is_flat);

  void
  declare_varyings_impl(std::ostringstream &str,
                        const std::vector<per_varying> &varyings,
                        c_string varying_qualifier,
                        unsigned int &slot) const;

  void
  stream_alias_varyings_impl(const std::vector<per_varying> &varyings_to_use,
                             ShaderSource &shader,
                             c_array<const c_string> p,
                             bool add_aliases, uvec2 start) const;

  std::vector<per_varying> m_uint_varyings;
  std::vector<per_varying> m_int_varyings;
  vecN<std::vector<per_varying>, varying_list::interpolation_number_types> m_float_varyings;
};

void
stream_as_local_variables(ShaderSource &shader, const varying_list &p);

void
stream_uber_vert_shader(bool use_switch, ShaderSource &vert,
                        c_array<const reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum);

void
stream_uber_frag_shader(bool use_switch, ShaderSource &frag,
                        c_array<const reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum);

void
stream_uber_composite_shader(bool use_switch, ShaderSource &frag,
                         c_array<const reference_counted_ptr<PainterCompositeShaderGLSL> > composite_shaders,
                         enum PainterCompositeShader::shader_type tp);


}}}
