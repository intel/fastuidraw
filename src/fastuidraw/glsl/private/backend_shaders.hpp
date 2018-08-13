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
#include <fastuidraw/glsl/painter_composite_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

enum fastuidraw::PainterCompositeShader::shader_type
shader_composite_type(enum fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_type_t in_value);

/* Values for render pass for stroke shading */
enum uber_stroke_render_pass_t
  {
    /*
     * uber_stroke_non_aa must be 0 because when we make a special shader
     * with only supporting this render pass, that pass takes no bits,
     * which means bit-extract always returns 0
     */
    uber_stroke_non_aa,

    uber_stroke_aa_pass1,
    uber_stroke_aa_pass2,

    uber_number_passes
  };

class CompositeShaderSetCreator
{
public:
  explicit
  CompositeShaderSetCreator(enum PainterCompositeShader::shader_type tp);

  PainterCompositeShaderSet
  create_composite_shaders(void);

private:
  void
  add_composite_shader(PainterCompositeShaderSet &out,
                       enum PainterEnums::composite_mode_t md,
                       const BlendMode &single_md,
                       const std::string &dual_src_file,
                       const BlendMode &dual_md,
                       const std::string &framebuffer_fetch_src_file);
  void
  add_composite_shader(PainterCompositeShaderSet &out,
                       enum PainterEnums::composite_mode_t md,
                       const std::string &dual_src_file,
                       const BlendMode &dual_md,
                       const std::string &framebuffer_fetch_src_file);
  void
  add_composite_shader(PainterCompositeShaderSet &out,
                       enum PainterEnums::composite_mode_t md,
                       const std::string &framebuffer_fetch_src_file);

  void
  add_single_src_composite_shader(PainterCompositeShaderSet &out,
                                  enum PainterEnums::composite_mode_t md,
                                  const BlendMode &single_md);

  void
  add_dual_src_composite_shader(PainterCompositeShaderSet &out,
                                enum PainterEnums::composite_mode_t md,
                                const std::string &dual_src_file,
                                const BlendMode &dual_md);

  void
  add_fbf_composite_shader(PainterCompositeShaderSet &out,
                           enum PainterEnums::composite_mode_t md,
                           const std::string &framebuffer_fetch_src_file);

  enum PainterCompositeShader::shader_type m_type;
  reference_counted_ptr<PainterCompositeShaderGLSL> m_single_src_composite_shader_code;
};

class ShaderSetCreatorStrokingConstants
{
public:
  ShaderSetCreatorStrokingConstants(void);

protected:
  uint32_t m_stroke_render_pass_num_bits, m_stroke_dash_style_num_bits;
  uint32_t m_stroke_render_pass_bit0, m_stroke_dash_style_bit0;
  ShaderSource::MacroSet m_subshader_constants;
  ShaderSource::MacroSet m_subshader_constants_non_aa_only;
  ShaderSource::MacroSet m_stroke_constants;
  ShaderSource::MacroSet m_arc_stroke_constants;

private:
  void
  create_macro_set(ShaderSource::MacroSet &dst,
                   bool render_pass_varies) const;
};

class ShaderSetCreator:
  private ShaderSetCreatorStrokingConstants,
  private CompositeShaderSetCreator
{
public:
  explicit
  ShaderSetCreator(enum PainterCompositeShader::shader_type composite_tp,
                   enum PainterStrokeShader::type_t stroke_tp,
                   const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass1,
                   const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass2);

  PainterShaderSet
  create_shader_set(void);

private:
  enum
    {
      arc_shader = 1,
      dashed_shader = 2,
      only_supports_non_aa = 4,
    };

  PainterItemShaderGLSL*
  build_uber_stroke_shader(uint32_t flags, unsigned int num_shaders) const;

  varying_list
  build_uber_stroke_varyings(uint32_t flags) const;

  ShaderSource
  build_uber_stroke_source(uint32_t flags, bool is_vertex_shader) const;

  reference_counted_ptr<PainterItemShader>
  create_glyph_item_shader(const std::string &vert_src,
                           const std::string &frag_src,
                           const varying_list &varyings);

  PainterGlyphShader
  create_glyph_shader(bool anisotropic);

  /*
   * stroke_dash_style having value number_cap_styles means
   * to not have dashed stroking.
   */
  PainterStrokeShader
  create_stroke_shader(enum PainterEnums::cap_style stroke_dash_style,
                       const reference_counted_ptr<const StrokingDataSelectorBase> &stroke_data_selector);

  PainterDashedStrokeShaderSet
  create_dashed_stroke_shader_set(void);

  reference_counted_ptr<PainterItemShader>
  create_stroke_item_shader(bool arc_shader,
                            enum PainterEnums::cap_style stroke_dash_style,
                            enum uber_stroke_render_pass_t render_pass_macro);

  PainterFillShader
  create_fill_shader(void);

  reference_counted_ptr<PainterBlendShader>
  create_blend_shader(const std::string &framebuffer_fetch_src_file);

  PainterBlendShaderSet
  create_blend_shaders(void);

  enum PainterStrokeShader::type_t m_stroke_tp;

  reference_counted_ptr<PainterItemShaderGLSL> m_uber_stroke_shader, m_uber_dashed_stroke_shader;
  reference_counted_ptr<PainterItemShaderGLSL> m_dashed_discard_stroke_shader;
  reference_counted_ptr<PainterItemShaderGLSL> m_uber_arc_stroke_shader, m_uber_arc_dashed_stroke_shader;
  reference_counted_ptr<PainterItemShaderGLSL> m_arc_discard_stroke_shader;
  reference_counted_ptr<PainterItemShaderGLSL> m_dashed_arc_discard_stroke_shader;

  reference_counted_ptr<const PainterDraw::Action> m_stroke_action_pass1;
  reference_counted_ptr<const PainterDraw::Action> m_stroke_action_pass2;
};

}}}
