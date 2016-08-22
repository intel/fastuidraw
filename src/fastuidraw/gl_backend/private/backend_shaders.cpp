#include "backend_shaders.hpp"

namespace fastuidraw { namespace gl { namespace detail { namespace backend_shaders {

/////////////////////////////////////
// BlendShaderSetCreator methods
BlendShaderSetCreator::
BlendShaderSetCreator(void)
{
  m_single_src_blend_shader_code =
    FASTUIDRAWnew BlendShaderSourceCode(glsl::ShaderSource()
                                        .add_source("fastuidraw_fall_through.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource));
}

reference_counted_ptr<PainterBlendShaderGL>
BlendShaderSetCreator::
create_blend_shader(const BlendMode &single_md,
                    const std::string &dual_src_file,
                    const BlendMode &dual_md,
                    const std::string &framebuffer_fetch_src_file)
{
  return FASTUIDRAWnew PainterBlendShaderGL(SingleSourceBlenderShader(single_md, m_single_src_blend_shader_code),
                                            DualSourceBlenderShader(dual_md,
                                                                    glsl::ShaderSource()
                                                                    .add_source(dual_src_file.c_str(),
                                                                                glsl::ShaderSource::from_resource)),
                                            FramebufferFetchBlendShader(glsl::ShaderSource()
                                                                        .add_source(framebuffer_fetch_src_file.c_str(),
                                                                                    glsl::ShaderSource::from_resource)));
}

PainterBlendShaderSet
BlendShaderSetCreator::
create_blend_shaders(void)
{
  using namespace fastuidraw::gl;
  using namespace fastuidraw::PainterEnums;
  /* try to use as few blend modes as possible so that
     we have fewer draw call breaks. The convention is as
     follows:
     - src0 is GL_ONE and the GLSL code handles the multiply
     - src1 is computed by the GLSL code as needed
     This is fine for those modes that do not need DST values
  */
  BlendMode one_src1, dst_alpha_src1, one_minus_dst_alpha_src1;
  PainterBlendShaderSet shaders;

  one_src1
    .equation(BlendMode::ADD)
    .func_src(BlendMode::ONE)
    .func_dst(BlendMode::SRC1_COLOR);

  dst_alpha_src1
    .equation(BlendMode::ADD)
    .func_src(BlendMode::DST_ALPHA)
    .func_dst(BlendMode::SRC1_COLOR);

  one_minus_dst_alpha_src1
    .equation(BlendMode::ADD)
    .func_src(BlendMode::ONE_MINUS_DST_ALPHA)
    .func_dst(BlendMode::SRC1_COLOR);

  shaders.shader(blend_porter_duff_src_over,
                 create_blend_shader(BlendMode().func(BlendMode::ONE, BlendMode::ONE_MINUS_SRC_ALPHA),
                                     "fastuidraw_porter_duff_src_over.glsl.resource_string", one_src1,
                                     "fastuidraw_fbf_porter_duff_src_over.glsl.resource_string"));

  shaders.shader(blend_porter_duff_dst_over,
                 create_blend_shader(BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE),
                                     "fastuidraw_porter_duff_dst_over.glsl.resource_string", one_minus_dst_alpha_src1,
                                     "fastuidraw_fbf_porter_duff_dst_over.glsl.resource_string"));

  shaders.shader(blend_porter_duff_clear,
                 create_blend_shader(BlendMode().func(BlendMode::ZERO, BlendMode::ZERO),
                                     "fastuidraw_porter_duff_clear.glsl.resource_string", one_src1,
                                     "fastuidraw_fbf_porter_duff_clear.glsl.resource_string"));

  shaders.shader(blend_porter_duff_src,
                 create_blend_shader(BlendMode().func(BlendMode::ONE, BlendMode::ZERO),
                                     "fastuidraw_porter_duff_src.glsl.resource_string", one_src1,
                                     "fastuidraw_fbf_porter_duff_src.glsl.resource_string"));

  shaders.shader(blend_porter_duff_dst,
                 create_blend_shader(BlendMode().func(BlendMode::ZERO, BlendMode::ONE),
                                     "fastuidraw_porter_duff_dst.glsl.resource_string", one_src1,
                                     "fastuidraw_fbf_porter_duff_dst.glsl.resource_string"));

  shaders.shader(blend_porter_duff_src_in,
                 create_blend_shader(BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ZERO),
                                     "fastuidraw_porter_duff_src_in.glsl.resource_string", dst_alpha_src1,
                                     "fastuidraw_fbf_porter_duff_src_in.glsl.resource_string"));

  shaders.shader(blend_porter_duff_dst_in,
                 create_blend_shader(BlendMode().func(BlendMode::ZERO, BlendMode::SRC_ALPHA),
                                     "fastuidraw_porter_duff_dst_in.glsl.resource_string", one_src1,
                                     "fastuidraw_fbf_porter_duff_dst_in.glsl.resource_string"));

  shaders.shader(blend_porter_duff_src_out,
                 create_blend_shader(BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ZERO),
                                     "fastuidraw_porter_duff_src_out.glsl.resource_string", one_minus_dst_alpha_src1,
                                     "fastuidraw_fbf_porter_duff_src_out.glsl.resource_string"));

  shaders.shader(blend_porter_duff_dst_out,
                 create_blend_shader(BlendMode().func(BlendMode::ZERO, BlendMode::ONE_MINUS_SRC_ALPHA),
                                     "fastuidraw_porter_duff_dst_out.glsl.resource_string", one_src1,
                                     "fastuidraw_fbf_porter_duff_dst_out.glsl.resource_string"));

  shaders.shader(blend_porter_duff_src_atop,
                 create_blend_shader(BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                                     "fastuidraw_porter_duff_src_atop.glsl.resource_string", dst_alpha_src1,
                                     "fastuidraw_fbf_porter_duff_src_atop.glsl.resource_string"));

  shaders.shader(blend_porter_duff_dst_atop,
                 create_blend_shader(BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::SRC_ALPHA),
                                     "fastuidraw_porter_duff_dst_atop.glsl.resource_string", one_minus_dst_alpha_src1,
                                     "fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string"));

  shaders.shader(blend_porter_duff_xor,
                 create_blend_shader(BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                                     "fastuidraw_porter_duff_xor.glsl.resource_string", one_minus_dst_alpha_src1,
                                     "fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string"));

  return shaders;
}

///////////////////////////////////////////////
// ShaderSetCreatorConstants methods
ShaderSetCreatorConstants::
ShaderSetCreatorConstants(void)
{
  using namespace fastuidraw::PainterEnums;

  m_stroke_render_pass_num_bits = number_bits_required(uber_number_passes - 1);
  m_stroke_dash_num_bits = number_bits_required(number_dashed_cap_styles);
  assert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_render_pass_num_bits) >= uber_number_passes - 1);
  assert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_dash_num_bits) >= number_dashed_cap_styles);

  m_stroke_width_pixels_bit0 = 0;
  m_stroke_render_pass_bit0 = m_stroke_width_pixels_bit0 + 1;
  m_stroke_dash_style_bit0 = m_stroke_render_pass_bit0 + m_stroke_render_pass_num_bits;
}

//////////////////////////////////////////
//  ShaderSetCreator methods
ShaderSetCreator::
ShaderSetCreator(void)
{
  unsigned int num_sub_shaders;

  num_sub_shaders = 1u << (m_stroke_render_pass_num_bits + m_stroke_dash_num_bits + 1u);
  m_uber_stroke_shader = FASTUIDRAWnew PainterItemShaderGL(num_sub_shaders,
                                                           glsl::ShaderSource()
                                                           .add_macro("fastuidraw_stroke_sub_shader_width_pixels_bit0", m_stroke_width_pixels_bit0)
                                                           .add_macro("fastuidraw_stroke_sub_shader_render_pass_bit0", m_stroke_render_pass_bit0)
                                                           .add_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits", m_stroke_render_pass_num_bits)
                                                           .add_macro("fastuidraw_stroke_sub_shader_dash_style_bit0", m_stroke_dash_style_bit0)
                                                           .add_macro("fastuidraw_stroke_sub_shader_dash_num_bits", m_stroke_dash_num_bits)
                                                           .add_macro("fastuidraw_stroke_opaque_pass", uber_stroke_opaque_pass)
                                                           .add_macro("fastuidraw_stroke_aa_pass", uber_stroke_aa_pass)
                                                           .add_macro("fastuidraw_stroke_non_aa", uber_stroke_non_aa)
                                                           .add_source("fastuidraw_painter_stroke.vert.glsl.resource_string",
                                                                       glsl::ShaderSource::from_resource),
                                                           glsl::ShaderSource()
                                                           .add_macro("fastuidraw_stroke_sub_shader_width_pixels_bit0", m_stroke_width_pixels_bit0)
                                                           .add_macro("fastuidraw_stroke_sub_shader_render_pass_bit0", m_stroke_render_pass_bit0)
                                                           .add_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits", m_stroke_render_pass_num_bits)
                                                           .add_macro("fastuidraw_stroke_sub_shader_dash_style_bit0", m_stroke_dash_style_bit0)
                                                           .add_macro("fastuidraw_stroke_sub_shader_dash_num_bits", m_stroke_dash_num_bits)
                                                           .add_macro("fastuidraw_stroke_opaque_pass", uber_stroke_opaque_pass)
                                                           .add_macro("fastuidraw_stroke_aa_pass", uber_stroke_aa_pass)
                                                           .add_macro("fastuidraw_stroke_non_aa", uber_stroke_non_aa)
                                                           .add_source("fastuidraw_painter_stroke.frag.glsl.resource_string",
                                                                       glsl::ShaderSource::from_resource),
                                                           varying_list()
                                                           .add_float_varying("fastuidraw_stroking_on_boundary")
                                                           .add_float_varying("fastuidraw_stroking_distance"));
}

reference_counted_ptr<PainterItemShader>
ShaderSetCreator::
create_glyph_item_shader(const std::string &vert_src,
                         const std::string &frag_src,
                         const varying_list &varyings)
{
  reference_counted_ptr<PainterItemShader> shader;
  shader = FASTUIDRAWnew PainterItemShaderGL(glsl::ShaderSource()
                                             .add_source(vert_src.c_str(), glsl::ShaderSource::from_resource),
                                             glsl::ShaderSource()
                                             .add_source(frag_src.c_str(), glsl::ShaderSource::from_resource),
                                             varyings);
  return shader;
}

PainterGlyphShader
ShaderSetCreator::
create_glyph_shader(bool anisotropic)
{
  PainterGlyphShader return_value;
  varying_list varyings;

  varyings
    .add_float_varying("fastuidraw_glyph_tex_coord_x")
    .add_float_varying("fastuidraw_glyph_tex_coord_y")
    .add_float_varying("fastuidraw_glyph_secondary_tex_coord_x")
    .add_float_varying("fastuidraw_glyph_secondary_tex_coord_y")
    .add_uint_varying("fastuidraw_glyph_tex_coord_layer")
    .add_uint_varying("fastuidraw_glyph_secondary_tex_coord_layer")
    .add_uint_varying("fastuidraw_glyph_geometry_data_location");

  return_value
    .shader(coverage_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_coverage.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_coverage.frag.glsl.resource_string",
                                     varyings));

  if(anisotropic)
    {
      return_value
        .shader(distance_field_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_distance_field_anisotropic.frag.glsl.resource_string",
                                         varyings))
        .shader(curve_pair_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_curve_pair.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_curve_pair_anisotropic.frag.glsl.resource_string",
                                         varyings));
    }
  else
    {
      return_value
        .shader(distance_field_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string",
                                         varyings))
        .shader(curve_pair_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_curve_pair.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_curve_pair.frag.glsl.resource_string",
                                         varyings));
    }

  return return_value;
}

reference_counted_ptr<PainterItemShader>
ShaderSetCreator::
create_stroke_item_shader(enum PainterEnums::dashed_cap_style stroke_dash_style,
                          bool pixel_width_stroking,
                          enum uber_stroke_render_pass_t render_pass)
{
  reference_counted_ptr<PainterItemShader> shader;
  uint32_t sub_shader;

  sub_shader = (stroke_dash_style << m_stroke_dash_style_bit0)
    | (render_pass << m_stroke_render_pass_bit0)
    | (uint32_t(pixel_width_stroking) << m_stroke_width_pixels_bit0);
  shader = FASTUIDRAWnew PainterItemShader(sub_shader, m_uber_stroke_shader);

  return shader;
}



PainterStrokeShader
ShaderSetCreator::
create_stroke_shader(enum PainterEnums::dashed_cap_style stroke_dash_style,
                     bool pixel_width_stroking)
{
  using namespace fastuidraw::PainterEnums;

  PainterStrokeShader return_value;
  return_value
    .aa_shader_pass1(create_stroke_item_shader(stroke_dash_style, pixel_width_stroking, uber_stroke_opaque_pass))
    .aa_shader_pass2(create_stroke_item_shader(stroke_dash_style, pixel_width_stroking, uber_stroke_aa_pass))
    .non_aa_shader(create_stroke_item_shader(stroke_dash_style, pixel_width_stroking, uber_stroke_non_aa));
  return return_value;
}

PainterDashedStrokeShaderSet
ShaderSetCreator::
create_dashed_stroke_shader_set(bool pixel_width_stroking)
{
  using namespace fastuidraw::PainterEnums;
  PainterDashedStrokeShaderSet return_value;

  return_value
    .shader(dashed_no_caps_closed, create_stroke_shader(dashed_no_caps_closed, pixel_width_stroking))
    .shader(dashed_rounded_caps_closed, create_stroke_shader(dashed_rounded_caps_closed, pixel_width_stroking))
    .shader(dashed_square_caps_closed, create_stroke_shader(dashed_square_caps_closed, pixel_width_stroking))
    .shader(dashed_no_caps, create_stroke_shader(dashed_no_caps, pixel_width_stroking))
    .shader(dashed_rounded_caps, create_stroke_shader(dashed_rounded_caps, pixel_width_stroking))
    .shader(dashed_square_caps, create_stroke_shader(dashed_square_caps, pixel_width_stroking));
  return return_value;
}

reference_counted_ptr<PainterItemShader>
ShaderSetCreator::
create_fill_shader(void)
{
  reference_counted_ptr<PainterItemShader> shader;
  varying_list varyings;

  varyings.add_float_varying("fastuidraw_stroking_on_boundary");
  shader = FASTUIDRAWnew PainterItemShaderGL(glsl::ShaderSource()
                                             .add_source("fastuidraw_painter_fill.vert.glsl.resource_string",
                                                         glsl::ShaderSource::from_resource),
                                             glsl::ShaderSource()
                                             .add_source("fastuidraw_painter_fill.frag.glsl.resource_string",
                                                         glsl::ShaderSource::from_resource),
                                             varyings);
  return shader;
}

PainterShaderSet
ShaderSetCreator::
create_shader_set(void)
{
  using namespace fastuidraw::PainterEnums;
  PainterShaderSet return_value;

  return_value
    .glyph_shader(create_glyph_shader(false))
    .glyph_shader_anisotropic(create_glyph_shader(true))
    .stroke_shader(create_stroke_shader(number_dashed_cap_styles, false))
    .pixel_width_stroke_shader(create_stroke_shader(number_dashed_cap_styles, true))
    .dashed_stroke_shader(create_dashed_stroke_shader_set(false))
    .pixel_width_dashed_stroke_shader(create_dashed_stroke_shader_set(true))
    .fill_shader(create_fill_shader())
    .blend_shaders(create_blend_shaders());
  return return_value;
}

}}}}
