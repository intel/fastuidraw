/*!
 * \file backend_shaders.cpp
 * \brief file backend_shaders.cpp
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

#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_path_fill.hpp>
#include "backend_shaders.hpp"

#include <string>

namespace fastuidraw { namespace glsl { namespace detail {

/////////////////////////////////////
// BlendShaderSetCreator methods
BlendShaderSetCreator::
BlendShaderSetCreator(enum PainterBlendShader::shader_type tp):
  m_type(tp)
{
  if(m_type == PainterBlendShader::single_src)
    {
      m_single_src_blend_shader_code =
        FASTUIDRAWnew PainterBlendShaderGLSL(PainterBlendShader::single_src,
                                             ShaderSource()
                                             .add_source("fastuidraw_fall_through.glsl.resource_string",
                                                         ShaderSource::from_resource));
    }
}

void
BlendShaderSetCreator::
add_blend_shader(PainterBlendShaderSet &out,
                 enum PainterEnums::blend_mode_t md,
                 const BlendMode &single_md,
                 const std::string &dual_src_file,
                 const BlendMode &dual_md,
                 const std::string &framebuffer_fetch_src_file)
{
  switch(m_type)
    {
    case PainterBlendShader::single_src:
      {
        out.shader(md, single_md, m_single_src_blend_shader_code);
      }
      break;

    case PainterBlendShader::dual_src:
      {
        reference_counted_ptr<PainterBlendShader> p;
        p = FASTUIDRAWnew PainterBlendShaderGLSL(m_type,
                                                 ShaderSource()
                                                 .add_source(dual_src_file.c_str(), ShaderSource::from_resource));
        out.shader(md, dual_md, p);
      }
      break;

    case PainterBlendShader::framebuffer_fetch:
      {
        reference_counted_ptr<PainterBlendShader> p;
        p = FASTUIDRAWnew PainterBlendShaderGLSL(m_type,
                                                 ShaderSource()
                                                 .add_source(framebuffer_fetch_src_file.c_str(), ShaderSource::from_resource));
        out.shader(md, BlendMode().blending_on(false), p);
      }
      break;

    default:
      assert(!"Bad m_type");
    }
}

PainterBlendShaderSet
BlendShaderSetCreator::
create_blend_shaders(void)
{
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

  add_blend_shader(shaders, blend_porter_duff_src_over,
                   BlendMode().func(BlendMode::ONE, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_src_over.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_src_over.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_dst_over,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE),
                   "fastuidraw_porter_duff_dst_over.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_dst_over.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_clear,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ZERO),
                   "fastuidraw_porter_duff_clear.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_clear.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_src,
                   BlendMode().func(BlendMode::ONE, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_src.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_dst,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ONE),
                   "fastuidraw_porter_duff_dst.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_src_in,
                   BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src_in.glsl.resource_string", dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_in.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_dst_in,
                   BlendMode().func(BlendMode::ZERO, BlendMode::SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_in.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst_in.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_src_out,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src_out.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_out.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_dst_out,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_out.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst_out.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_src_atop,
                   BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_src_atop.glsl.resource_string", dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_atop.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_dst_atop,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_atop.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string");

  add_blend_shader(shaders, blend_porter_duff_xor,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_xor.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string");

  return shaders;
}

///////////////////////////////////////////////
// ShaderSetCreatorConstants methods
ShaderSetCreatorConstants::
ShaderSetCreatorConstants(void)
{
  using namespace fastuidraw::PainterEnums;

  m_stroke_render_pass_num_bits = number_bits_required(uber_number_passes);
  m_stroke_dash_style_num_bits = number_bits_required(number_cap_styles);
  assert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_render_pass_num_bits) >= uber_number_passes);
  assert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_dash_style_num_bits) >= number_cap_styles);
  assert(m_stroke_render_pass_num_bits + m_stroke_dash_style_num_bits + 1u <= 32u);

  m_stroke_render_pass_bit0 = 0;
  m_stroke_width_pixels_bit0 = m_stroke_render_pass_bit0 + m_stroke_render_pass_num_bits;
  m_stroke_dash_style_bit0 = m_stroke_width_pixels_bit0 + 1u;
}

void
ShaderSetCreatorConstants::
add_constants(ShaderSource &src)
{
  src
    .add_macro("fastuidraw_stroke_sub_shader_width_pixels_bit0", m_stroke_width_pixels_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_width_pixels_num_bits", 1)
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_bit0", m_stroke_render_pass_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits", m_stroke_render_pass_num_bits)
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_bit0", m_stroke_dash_style_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_num_bits", m_stroke_dash_style_num_bits)
    .add_macro("fastuidraw_stroke_opaque_pass", uber_stroke_opaque_pass)
    .add_macro("fastuidraw_stroke_aa_pass", uber_stroke_aa_pass)
    .add_macro("fastuidraw_stroke_non_aa", uber_stroke_non_aa);
}

//////////////////////////////////////////
//  ShaderSetCreator methods
ShaderSetCreator::
ShaderSetCreator(enum PainterBlendShader::shader_type tp,
                 bool non_dashed_stroke_shader_uses_discard):
  BlendShaderSetCreator(tp)
{
  unsigned int num_undashed_sub_shaders, num_dashed_sub_shaders;
  const char *extra_macro;

  if(non_dashed_stroke_shader_uses_discard)
    {
      extra_macro = "FASTUIDRAW_STROKE_USE_DISCARD";
    }
  else
    {
      extra_macro = "FASTUIDRAW_STROKE_DOES_NOT_USE_DISCARD";
    }

  num_undashed_sub_shaders = 1u << (m_stroke_render_pass_num_bits + 1);
  m_uber_stroke_shader =
    FASTUIDRAWnew PainterItemShaderGLSL(non_dashed_stroke_shader_uses_discard,
                                        ShaderSource()
                                        .add_macro(extra_macro)
                                        .add_source("fastuidraw_painter_stroke.vert.glsl.resource_string",
                                                    ShaderSource::from_resource)
                                        .remove_macro(extra_macro),

                                        ShaderSource()
                                        .add_macro(extra_macro)
                                        .add_source("fastuidraw_painter_stroke.frag.glsl.resource_string",
                                                    ShaderSource::from_resource)
                                        .remove_macro(extra_macro),

                                        varying_list()
                                        .add_float_varying("fastuidraw_stroking_on_boundary"),
                                        num_undashed_sub_shaders
                                        );

  num_dashed_sub_shaders = 1u << (m_stroke_render_pass_num_bits + m_stroke_dash_style_num_bits + 1u);

  m_uber_dashed_stroke_shader =
    FASTUIDRAWnew PainterItemShaderGLSL(true,
                                        ShaderSource()
                                        .add_macro("FASTUIDRAW_STROKE_DASHED")
                                        .add_macro("FASTUIDRAW_STROKE_USE_DISCARD")
                                        .add_source("fastuidraw_painter_stroke.vert.glsl.resource_string",
                                                    ShaderSource::from_resource)
                                        .remove_macro("FASTUIDRAW_STROKE_USE_DISCARD")
                                        .remove_macro("FASTUIDRAW_STROKE_DASHED"),

                                        ShaderSource()
                                        .add_macro("FASTUIDRAW_STROKE_DASHED")
                                        .add_macro("FASTUIDRAW_STROKE_USE_DISCARD")
                                        .add_source("fastuidraw_painter_stroke.frag.glsl.resource_string",
                                                    ShaderSource::from_resource)
                                        .remove_macro("FASTUIDRAW_STROKE_USE_DISCARD")
                                        .remove_macro("FASTUIDRAW_STROKE_DASHED"),

                                        varying_list()
                                        .add_float_varying("fastuidraw_stroking_on_boundary")
                                        .add_float_varying("fastuidraw_stroking_distance")
                                        .add_float_varying("fastuidraw_stroking_distance_sub_edge_start")
                                        .add_float_varying("fastuidraw_stroking_distance_sub_edge_end")
                                        .add_uint_varying("fastuidraw_stroking_dash_bits"),
                                        num_dashed_sub_shaders
                                        );
}

reference_counted_ptr<PainterItemShader>
ShaderSetCreator::
create_glyph_item_shader(const std::string &vert_src,
                         const std::string &frag_src,
                         const varying_list &varyings)
{
  reference_counted_ptr<PainterItemShader> shader;
  shader = FASTUIDRAWnew PainterItemShaderGLSL(false,
                                               ShaderSource()
                                               .add_source(vert_src.c_str(), ShaderSource::from_resource),
                                               ShaderSource()
                                               .add_source(frag_src.c_str(), ShaderSource::from_resource),
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
create_stroke_item_shader(enum PainterEnums::cap_style stroke_dash_style,
                          bool pixel_width_stroking,
                          enum uber_stroke_render_pass_t render_pass)
{
  reference_counted_ptr<PainterItemShader> shader;
  uint32_t sub_shader;

  if(stroke_dash_style == fastuidraw::PainterEnums::number_cap_styles)
    {
      sub_shader = (render_pass << m_stroke_render_pass_bit0)
        | (uint32_t(pixel_width_stroking) << m_stroke_width_pixels_bit0);
      shader = FASTUIDRAWnew PainterItemShader(sub_shader, m_uber_stroke_shader);
    }
  else
    {
      sub_shader = (stroke_dash_style << m_stroke_dash_style_bit0)
        | (render_pass << m_stroke_render_pass_bit0)
        | (uint32_t(pixel_width_stroking) << m_stroke_width_pixels_bit0);
      shader = FASTUIDRAWnew PainterItemShader(sub_shader, m_uber_dashed_stroke_shader);
    }
  return shader;
}



PainterStrokeShader
ShaderSetCreator::
create_stroke_shader(enum PainterEnums::cap_style stroke_style,
                     bool pixel_width_stroking,
                     const reference_counted_ptr<const StrokingDataSelectorBase> &stroke_data_selector)
{
  using namespace fastuidraw::PainterEnums;
  PainterStrokeShader return_value;

  return_value
    .stroking_data_selector(stroke_data_selector)
    .aa_shader_pass1(create_stroke_item_shader(stroke_style, pixel_width_stroking, uber_stroke_opaque_pass))
    .aa_shader_pass2(create_stroke_item_shader(stroke_style, pixel_width_stroking, uber_stroke_aa_pass))
    .non_aa_shader(create_stroke_item_shader(stroke_style, pixel_width_stroking, uber_stroke_non_aa));
  return return_value;
}

PainterDashedStrokeShaderSet
ShaderSetCreator::
create_dashed_stroke_shader_set(bool pixel_width_stroking)
{
  using namespace fastuidraw::PainterEnums;
  PainterDashedStrokeShaderSet return_value;
  reference_counted_ptr<const DashEvaluatorBase> de;
  reference_counted_ptr<const StrokingDataSelectorBase> se;

  se = PainterDashedStrokeParams::stroking_data_selector(pixel_width_stroking);
  de = PainterDashedStrokeParams::dash_evaluator(pixel_width_stroking);
  return_value
    .dash_evaluator(de)
    .shader(flat_caps, create_stroke_shader(flat_caps, pixel_width_stroking, se))
    .shader(rounded_caps, create_stroke_shader(rounded_caps, pixel_width_stroking, se))
    .shader(square_caps, create_stroke_shader(square_caps, pixel_width_stroking, se));
  return return_value;
}

PainterFillShader
ShaderSetCreator::
create_fill_shader(void)
{
  PainterFillShader fill_shader;
  varying_list varyings;

  varyings.add_float_varying("fastuidraw_stroking_on_boundary");
  fill_shader
    .chunk_selector(PainterAttributeDataFillerPathFill::chunk_selector())
    .item_shader(FASTUIDRAWnew PainterItemShaderGLSL(false,
                                                     ShaderSource()
                                                     .add_source("fastuidraw_painter_fill.vert.glsl.resource_string",
                                                                 ShaderSource::from_resource),
                                                     ShaderSource()
                                                     .add_source("fastuidraw_painter_fill.frag.glsl.resource_string",
                                                                 ShaderSource::from_resource),
                                                     varyings));;
  return fill_shader;
}

PainterShaderSet
ShaderSetCreator::
create_shader_set(void)
{
  using namespace fastuidraw::PainterEnums;
  PainterShaderSet return_value;
  reference_counted_ptr<const StrokingDataSelectorBase> se, se_pixel;

  se = PainterStrokeParams::stroking_data_selector(false);
  se_pixel = PainterStrokeParams::stroking_data_selector(true);

  return_value
    .glyph_shader(create_glyph_shader(false))
    .glyph_shader_anisotropic(create_glyph_shader(true))
    .stroke_shader(create_stroke_shader(number_cap_styles, false, se))
    .pixel_width_stroke_shader(create_stroke_shader(number_cap_styles, true, se_pixel))
    .dashed_stroke_shader(create_dashed_stroke_shader_set(false))
    .pixel_width_dashed_stroke_shader(create_dashed_stroke_shader_set(true))
    .fill_shader(create_fill_shader())
    .blend_shaders(create_blend_shaders());
  return return_value;
}

}}}
