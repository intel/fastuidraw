#pragma once

#include <fastuidraw/painter/painter_shader_set.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail { namespace backend_shaders {

/*
  Values for render pass for stroke shading
*/
enum uber_stroke_render_pass_t
  {
    uber_stroke_opaque_pass,
    uber_stroke_aa_pass,
    uber_stroke_non_aa,

    uber_number_passes
  };

class BlendShaderSetCreator
{
public:
  BlendShaderSetCreator(void);

  PainterBlendShaderSet
  create_blend_shaders(void);

private:
  reference_counted_ptr<PainterBlendShaderGLSL>
  create_blend_shader(const BlendMode &single_md,
                      const std::string &dual_src_file,
                      const BlendMode &dual_md,
                      const std::string &framebuffer_fetch_src_file);

  reference_counted_ptr<BlendShaderSourceCode> m_single_src_blend_shader_code;
};

class ShaderSetCreatorConstants
{
public:
  ShaderSetCreatorConstants(void);

  uint32_t m_stroke_render_pass_num_bits, m_stroke_dash_num_bits;
  uint32_t m_stroke_width_pixels_bit0, m_stroke_render_pass_bit0, m_stroke_dash_style_bit0;
};

class ShaderSetCreator:
  public ShaderSetCreatorConstants,
  public BlendShaderSetCreator
{
public:
  ShaderSetCreator(void);

  reference_counted_ptr<PainterItemShader>
  create_glyph_item_shader(const std::string &vert_src,
                           const std::string &frag_src,
                           const varying_list &varyings);

  PainterGlyphShader
  create_glyph_shader(bool anisotropic);

  /*
    stroke_dash_style having value number_dashed_cap_styles means
    to not have dashed stroking.
    */
  PainterStrokeShader
  create_stroke_shader(enum PainterEnums::dashed_cap_style stroke_dash_style,
                       bool pixel_width_stroking);

  PainterDashedStrokeShaderSet
  create_dashed_stroke_shader_set(bool pixel_width_stroking);

  reference_counted_ptr<PainterItemShader>
  create_stroke_item_shader(enum PainterEnums::dashed_cap_style stroke_dash_style,
                            bool pixel_width_stroking,
                            enum uber_stroke_render_pass_t render_pass_macro);

  reference_counted_ptr<PainterItemShader>
  create_fill_shader(void);

  PainterShaderSet
  create_shader_set(void);

  reference_counted_ptr<PainterItemShader> m_uber_stroke_shader;
};

}}}}
