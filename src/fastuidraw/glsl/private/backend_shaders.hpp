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

#include <fastuidraw/painter/painter_shader_set.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

/* Values for render pass for stroke shading */
enum uber_stroke_render_pass_t
  {
    uber_stroke_aa_pass1,
    uber_stroke_aa_pass2,
    uber_stroke_non_aa,

    uber_number_passes
  };

class BlendShaderSetCreator
{
public:
  explicit
  BlendShaderSetCreator(enum PainterBlendShader::shader_type tp);

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

  enum PainterBlendShader::shader_type m_type;
  reference_counted_ptr<PainterBlendShaderGLSL> m_single_src_blend_shader_code;
};

class ShaderSetCreatorConstants
{
public:
  ShaderSetCreatorConstants(void);

  void
  add_constants(ShaderSource &src);

  uint32_t m_stroke_render_pass_num_bits, m_stroke_dash_style_num_bits;
  uint32_t m_stroke_width_pixels_bit0, m_stroke_render_pass_bit0, m_stroke_dash_style_bit0;
};

class ShaderSetCreator:
  public ShaderSetCreatorConstants,
  public BlendShaderSetCreator
{
public:
  explicit
  ShaderSetCreator(enum PainterBlendShader::shader_type blend_tp,
                   enum PainterStrokeShader::type_t stroke_tp,
                   const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass1,
                   const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass2);

  PainterShaderSet
  create_shader_set(void);

private:
  reference_counted_ptr<PainterItemShader>
  create_glyph_item_shader(const std::string &vert_src,
                           const std::string &frag_src,
                           const varying_list &varyings);

  PainterGlyphShader
  create_glyph_shader(bool anisotropic);

  /*
    stroke_dash_style having value number_cap_styles means
    to not have dashed stroking.
    */
  PainterStrokeShader
  create_stroke_shader(enum PainterEnums::cap_style stroke_dash_style,
                       bool pixel_width_stroking,
                       const reference_counted_ptr<const StrokingDataSelectorBase> &stroke_data_selector);

  PainterDashedStrokeShaderSet
  create_dashed_stroke_shader_set(bool pixel_width_stroking);

  reference_counted_ptr<PainterItemShader>
  create_stroke_item_shader(enum PainterEnums::cap_style stroke_dash_style,
                            bool pixel_width_stroking,
                            enum uber_stroke_render_pass_t render_pass_macro);

  PainterFillShader
  create_fill_shader(void);

  enum PainterStrokeShader::type_t m_stroke_tp;
  reference_counted_ptr<PainterItemShader> m_uber_stroke_shader, m_uber_dashed_stroke_shader;
  reference_counted_ptr<const PainterDraw::Action> m_stroke_action_pass1;
  reference_counted_ptr<const PainterDraw::Action> m_stroke_action_pass2;
};

}}}
