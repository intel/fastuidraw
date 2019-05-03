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
#include <fastuidraw/glsl/painter_brush_shader_glsl.hpp>
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
  vecN<uvec2, varying_list::interpolator_number_types> m_varying_start;
};

class UberShaderVaryings:fastuidraw::noncopyable
{
public:
  typedef bool (*filter_varying_t)(c_string name);

  static
  bool
  accept_all_varyings(c_string)
  {
    return true;
  }

  static
  bool
  accept_all_varying_alias(c_string, c_string)
  {
    return true;
  }

  void
  add_varyings(c_string label,
               const varying_list &p,
               AliasVaryingLocation *datum)
  {
    vecN<size_t, varying_list::interpolator_number_types> counts;

    for (unsigned int i = 0; i < varying_list::interpolator_number_types; ++i)
      {
        enum varying_list::interpolator_type_t tp;

        tp = static_cast<enum varying_list::interpolator_type_t>(i);
        counts[i] = p.varyings(tp).size();
      }

    add_varyings(label, counts, datum);
  }

  void
  add_varyings(c_string label,
               c_array<const size_t> counts,
               AliasVaryingLocation *datum);

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

  /* Sighs. GLSL mandates that in's of a fragment shader are
   * read-only. The bad news is that then when one's chains
   * shaders, it is not possible to directly modify the
   * varyings. So, instead we need to within the fragment shader
   * -copy- the in values to global values and in fragment shader
   * use those copy global values instead of the original in's.
   */
  void
  stream_varying_rw_copies(ShaderSource &dst) const;

  /*!
   * Add or remove aliases that have elements of p
   * refer to varying declared by a UberShaderVaryings
   * \param shader ShaderSource to which to stream
   * \param p how many varyings of each type along with their names
   * \param add_aliases if true add the aliases by add_macro(),
   *                    if false remove the aliases by remove_macro()
   */
  void
  stream_alias_varyings(bool use_rw_copies,
                        ShaderSource &shader, const varying_list &p,
                        bool add_aliases,
                        const AliasVaryingLocation &datum,
                        filter_varying_t filter_varying = &accept_all_varyings) const;
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

  void
  declare_varyings(std::ostringstream &str,
                   c_string varying_qualifier,
                   c_string interface_name = nullptr,
                   c_string instance_name = nullptr) const;

  uvec2
  add_varyings_impl_type(std::vector<per_varying> &varyings,
                         unsigned int cnt,
                         c_string qualifier,
                         vecN<c_string, 4> types,
                         c_string name, bool is_flat);

  void
  declare_varyings_impl(std::ostringstream &str,
                        const std::vector<per_varying> &varyings,
                        c_string varying_qualifier,
                        unsigned int &slot) const;

  void
  stream_alias_varyings_impl(bool use_rw_copies,
                             const std::vector<per_varying> &varyings_to_use,
                             ShaderSource &shader,
                             c_array<const c_string> p,
                             bool add_aliases, uvec2 start,
                             filter_varying_t filter_varying) const;

  vecN<std::vector<per_varying>, varying_list::interpolator_number_types> m_varyings;
};

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
stream_uber_vert_shader(bool use_switch, ShaderSource &vert,
                        c_array<const reference_counted_ptr<PainterItemCoverageShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum);

void
stream_uber_frag_shader(bool use_switch, ShaderSource &frag,
                        c_array<const reference_counted_ptr<PainterItemCoverageShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum);

void
stream_uber_brush_vert_shader(bool use_switch, ShaderSource &vert,
                              c_array<const reference_counted_ptr<PainterBrushShaderGLSL> > brush_shaders,
                              const UberShaderVaryings &declare_varyings,
                              const AliasVaryingLocation &datum);

void
stream_uber_brush_frag_shader(bool use_switch, ShaderSource &frag,
                              c_array<const reference_counted_ptr<PainterBrushShaderGLSL> > brush_shaders,
                              const UberShaderVaryings &declare_varyings,
                              const AliasVaryingLocation &datum);

void
stream_uber_blend_shader(bool use_switch, ShaderSource &frag,
                             c_array<const reference_counted_ptr<PainterBlendShaderGLSL> > blend_shaders,
                             enum PainterBlendShader::shader_type tp);


}}}
