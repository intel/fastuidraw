/*!
 * \file backend_shaders.hpp
 * \brief file backend_shaders.hpp
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

#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_brush_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

class BlendShaderSetCreator
{
public:
  explicit
  BlendShaderSetCreator(enum PainterBlendShader::shader_type preferred_blending_type,
                        enum PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_type);

  PainterBlendShaderSet
  create_blend_shaders(void);

private:
  void
  add_blend_shader(PainterBlendShaderSet &out,
                   enum PainterEnums::blend_mode_t md,
                   const BlendMode &single_md,
                   const std::string &dual_src_file,
                   const BlendMode &dual_md,
                   const std::string &framebuffer_fetch_src_file);

  void
  add_single_src_blend_shader(PainterBlendShaderSet &out,
                              enum PainterEnums::blend_mode_t md,
                              const BlendMode &single_md);

  void
  add_dual_src_blend_shader(PainterBlendShaderSet &out,
                            enum PainterEnums::blend_mode_t md,
                            const std::string &dual_src_file,
                            const BlendMode &dual_md);

  void
  add_fbf_blend_shader(PainterBlendShaderSet &out,
                       enum PainterEnums::blend_mode_t md,
                       const std::string &framebuffer_fetch_src_file);

  enum PainterBlendShader::shader_type m_preferred_type;
  enum PainterShaderRegistrarGLSL::fbf_blending_type_t m_fbf_type;
  reference_counted_ptr<PainterBlendShaderGLSL> m_fall_through_shader;
};

class ShaderSetCreatorStrokingConstants
{
public:
  ShaderSetCreatorStrokingConstants(void);

protected:
  ShaderSource::MacroSet m_subshader_constants;
  ShaderSource::MacroSet m_common_stroke_constants;
  ShaderSource::MacroSet m_stroke_constants;
  ShaderSource::MacroSet m_arc_stroke_constants;

  static
  uint32_t
  sub_shader(enum PainterEnums::cap_style stroke_dash_style);
};

class StrokeShaderCreator:private ShaderSetCreatorStrokingConstants
{
public:
  StrokeShaderCreator(void);

  reference_counted_ptr<PainterItemShader>
  create_stroke_non_aa_item_shader(enum PainterEnums::cap_style stroke_dash_style,
                                   enum PainterEnums::stroking_method_t tp);

  reference_counted_ptr<PainterItemShader>
  create_stroke_aa_item_shader(enum PainterEnums::cap_style stroke_dash_style,
                               enum PainterEnums::stroking_method_t tp);
private:
  enum
    {
      arc_shader = 1,
      discard_shader = 2,
      coverage_shader = 4,
    };

  PainterItemShaderGLSL*
  build_uber_stroke_shader(uint32_t flags, unsigned int num_shaders) const;

  PainterItemCoverageShaderGLSL*
  build_uber_stroke_coverage_shader(uint32_t flags, unsigned int num_shaders) const;

  varying_list
  build_uber_stroke_varyings(uint32_t flags) const;

  ShaderSource
  build_uber_stroke_source(uint32_t flags, bool is_vertex_shader) const;

  vecN<reference_counted_ptr<PainterItemShaderGLSL>, 4> m_non_aa_shader;
  vecN<reference_counted_ptr<PainterItemCoverageShaderGLSL>, 2> m_coverage_shaders;
  vecN<reference_counted_ptr<PainterItemShaderGLSL>, 2> m_post_coverage_shaders;
};

class ShaderSetCreator:
  private BlendShaderSetCreator,
  private StrokeShaderCreator
{
public:
  explicit
  ShaderSetCreator(enum PainterBlendShader::shader_type preferred_blending_type,
                   enum PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_type);

  PainterShaderSet
  create_shader_set(void);

private:
  reference_counted_ptr<PainterItemShader>
  create_glyph_item_shader(c_string vert_src,
                           c_string frag_src,
                           const varying_list &varyings);

  PainterGlyphShader
  create_glyph_shader(void);

  /*
   * stroke_dash_style having value number_cap_styles means
   * to not have dashed stroking.
   */
  PainterStrokeShader
  create_stroke_shader(enum PainterEnums::cap_style stroke_dash_style,
                       const reference_counted_ptr<const StrokingDataSelectorBase> &stroke_data_selector);

  PainterDashedStrokeShaderSet
  create_dashed_stroke_shader_set(void);

  PainterFillShader
  create_fill_shader(void);

  ShaderSource::MacroSet m_fill_macros;
  ShaderSource::MacroSet m_common_glyph_attribute_macros;
};

}}}
