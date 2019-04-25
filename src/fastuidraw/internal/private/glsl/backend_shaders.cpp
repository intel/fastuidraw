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

#include <iostream>
#include <string>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/filled_path.hpp>
#include <fastuidraw/text/glyph_attribute.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include <fastuidraw/text/glyph_render_data_banded_rays.hpp>
#include <private/glsl/backend_shaders.hpp>
#include <fastuidraw/glsl/unpack_source_generator.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

/////////////////////////////////////
// BlendShaderSetCreator methods
BlendShaderSetCreator::
BlendShaderSetCreator(enum PainterBlendShader::shader_type preferred_blending_type,
                      enum PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_type):
  m_preferred_type(preferred_blending_type),
  m_fbf_type(fbf_type)
{
  if (m_preferred_type == PainterBlendShader::single_src)
    {
      m_fall_through_shader =
        FASTUIDRAWnew PainterBlendShaderGLSL(PainterBlendShader::single_src,
                                             ShaderSource()
                                             .add_source("fastuidraw_fall_through.glsl.resource_string",
                                                         ShaderSource::from_resource));
    }
}

void
BlendShaderSetCreator::
add_single_src_blend_shader(PainterBlendShaderSet &out,
                            enum PainterEnums::blend_mode_t md,
                            const BlendMode &single_md)
{
  out.shader(md, single_md, m_fall_through_shader);
}

void
BlendShaderSetCreator::
add_dual_src_blend_shader(PainterBlendShaderSet &out,
                          enum PainterEnums::blend_mode_t md,
                          const std::string &dual_src_file,
                          const BlendMode &dual_md)
{
  reference_counted_ptr<PainterBlendShader> p;
  ShaderSource src;

  src.add_source(dual_src_file.c_str(), ShaderSource::from_resource);
  p = FASTUIDRAWnew PainterBlendShaderGLSL(PainterBlendShader::dual_src, src);
  out.shader(md, dual_md, p);
}

void
BlendShaderSetCreator::
add_fbf_blend_shader(PainterBlendShaderSet &out,
                     enum PainterEnums::blend_mode_t md,
                     const std::string &framebuffer_fetch_src_file)
{
  FASTUIDRAWassert(m_fbf_type != PainterShaderRegistrarGLSL::fbf_blending_not_supported);

  reference_counted_ptr<PainterBlendShader> p;
  ShaderSource src;

  src.add_source(framebuffer_fetch_src_file.c_str(), ShaderSource::from_resource);
  p = FASTUIDRAWnew PainterBlendShaderGLSL(PainterBlendShader::framebuffer_fetch, src);
  out.shader(md, BlendMode().blending_on(false), p);
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
  switch(m_preferred_type)
    {
    case PainterBlendShader::single_src:
      add_single_src_blend_shader(out, md, single_md);
      break;

    case PainterBlendShader::dual_src:
      add_dual_src_blend_shader(out, md, dual_src_file, dual_md);
      break;

    case PainterBlendShader::framebuffer_fetch:
      add_fbf_blend_shader(out, md, framebuffer_fetch_src_file);
      break;

    default:
      FASTUIDRAWassert(!"Bad m_preferred_type");
    }
}

PainterBlendShaderSet
BlendShaderSetCreator::
create_blend_shaders(void)
{
  using namespace fastuidraw;
  /* try to use as few blend modes as possible so that
   * we have fewer draw call breaks. The convention is as
   * follows:
   * - src0 is GL_ONE and the GLSL code handles the multiply
   * - src1 is computed by the GLSL code as needed
   * This is fine for those modes that do not need DST values
   */
  BlendMode one_src1, dst_alpha_src1, one_minus_dst_alpha_src1;
  PainterBlendShaderSet shaders;

  one_src1
    .equation(BlendMode::ADD)
    .func_src(BlendMode::ONE)
    .func_dst_rgb(BlendMode::SRC1_COLOR)
    .func_dst_alpha(BlendMode::SRC1_ALPHA);

  dst_alpha_src1
    .equation(BlendMode::ADD)
    .func_src(BlendMode::DST_ALPHA)
    .func_dst_rgb(BlendMode::SRC1_COLOR)
    .func_dst_alpha(BlendMode::SRC1_ALPHA);

  one_minus_dst_alpha_src1
    .equation(BlendMode::ADD)
    .func_src(BlendMode::ONE_MINUS_DST_ALPHA)
    .func_dst_rgb(BlendMode::SRC1_COLOR)
    .func_dst_alpha(BlendMode::SRC1_ALPHA);

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_src_over,
                   BlendMode().func(BlendMode::ONE, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_src_over.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_src_over.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_dst_over,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE),
                   "fastuidraw_porter_duff_dst_over.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_dst_over.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_clear,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ZERO),
                   "fastuidraw_porter_duff_clear.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_clear.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_src,
                   BlendMode().func(BlendMode::ONE, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_src.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_dst,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ONE),
                   "fastuidraw_porter_duff_dst.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_src_in,
                   BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src_in.glsl.resource_string", dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_in.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_dst_in,
                   BlendMode().func(BlendMode::ZERO, BlendMode::SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_in.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst_in.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_src_out,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src_out.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_out.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_dst_out,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_out.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst_out.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_src_atop,
                   BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_src_atop.glsl.resource_string", dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_atop.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_dst_atop,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_atop.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_xor,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_xor.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_xor.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_plus,
                   BlendMode().func(BlendMode::ONE, BlendMode::ONE),
                   "fastuidraw_porter_duff_plus.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_plus.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_porter_duff_modulate,
                   BlendMode()
                   .func_src_rgb(BlendMode::DST_COLOR)
                   .func_src_alpha(BlendMode::DST_ALPHA)
                   .func_dst(BlendMode::ZERO),
                   "fastuidraw_porter_duff_modulate.glsl.resource_string", dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_modulate.glsl.resource_string");

  add_blend_shader(shaders, PainterEnums::blend_w3c_screen,
                   BlendMode()
                   .func_src(BlendMode::ONE)
                   .func_dst_rgb(BlendMode::ONE_MINUS_SRC_COLOR)
                   .func_dst_alpha(BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_w3c_screen.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_w3c_screen.glsl.resource_string");

  if (m_fbf_type != PainterShaderRegistrarGLSL::fbf_blending_not_supported)
    {
      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_softlight,
                           "fastuidraw_fbf_w3c_softlight.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_overlay,
                           "fastuidraw_fbf_w3c_overlay.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_darken,
                           "fastuidraw_fbf_w3c_darken.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_lighten,
                           "fastuidraw_fbf_w3c_lighten.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_color_dodge,
                           "fastuidraw_fbf_w3c_color_dodge.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_color_burn,
                           "fastuidraw_fbf_w3c_color_burn.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_hardlight,
                           "fastuidraw_fbf_w3c_hardlight.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_difference,
                           "fastuidraw_fbf_w3c_difference.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_exclusion,
                           "fastuidraw_fbf_w3c_exclusion.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_multiply,
                           "fastuidraw_fbf_w3c_multiply.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_hue,
                           "fastuidraw_fbf_w3c_hue.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_saturation,
                           "fastuidraw_fbf_w3c_saturation.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_color,
                           "fastuidraw_fbf_w3c_color.glsl.resource_string");

      add_fbf_blend_shader(shaders, PainterEnums::blend_w3c_luminosity,
                           "fastuidraw_fbf_w3c_luminosity.glsl.resource_string");
    }

  return shaders;
}

///////////////////////////////////////////////
// ShaderSetCreatorStrokingConstants methods
ShaderSetCreatorStrokingConstants::
ShaderSetCreatorStrokingConstants(void)
{
  using namespace fastuidraw;

  m_subshader_constants
    .add_macro_u32("fastuidraw_stroke_dashed_flat_caps", sub_shader(PainterEnums::flat_caps))
    .add_macro_u32("fastuidraw_stroke_dashed_rounded_caps", sub_shader(PainterEnums::rounded_caps))
    .add_macro_u32("fastuidraw_stroke_dashed_square_caps", sub_shader(PainterEnums::square_caps))
    .add_macro_u32("fastuidraw_stroke_not_dashed", sub_shader(PainterEnums::number_cap_styles));

  m_common_stroke_constants
    .add_macro_u32("fastuidraw_stroke_pixel_units", PainterStrokeParams::pixel_stroking_units)
    .add_macro_u32("fastuidraw_stroke_path_units", PainterStrokeParams::pixel_stroking_units)
    .add_macro_u32("fastuidraw_stroke_gauranteed_to_be_covered_mask", 1u)
    .add_macro_u32("fastuidraw_stroke_skip_dash_interval_lookup_mask", 2u)
    .add_macro_u32("fastuidraw_stroke_distance_constant", 4u)
    .add_macro_u32("fastuidraw_arc_stroke_negative_arc_angle_mask", 8u)
    .add_macro_u32("fastuidraw_arc_stroke_inverted_inner_region_mask", 16u);

  m_stroke_constants
    /* offset types of StrokedPoint */
    .add_macro_u32("fastuidraw_stroke_offset_sub_edge", StrokedPoint::offset_sub_edge)
    .add_macro_u32("fastuidraw_stroke_offset_shared_with_edge", StrokedPoint::offset_shared_with_edge)
    .add_macro_u32("fastuidraw_stroke_offset_rounded_join", StrokedPoint::offset_rounded_join)

    .add_macro_u32("fastuidraw_stroke_offset_miter_bevel_join", StrokedPoint::offset_miter_bevel_join)
    .add_macro_u32("fastuidraw_stroke_offset_miter_join", StrokedPoint::offset_miter_join)
    .add_macro_u32("fastuidraw_stroke_offset_miter_clip_join", StrokedPoint::offset_miter_clip_join)

    .add_macro_u32("fastuidraw_stroke_offset_rounded_cap", StrokedPoint::offset_rounded_cap)
    .add_macro_u32("fastuidraw_stroke_offset_square_cap", StrokedPoint::offset_square_cap)
    .add_macro_u32("fastuidraw_stroke_offset_adjustable_cap", StrokedPoint::offset_adjustable_cap)

    /* bit masks for StrokedPoint::m_packed_data */
    .add_macro_u32("fastuidraw_stroke_sin_sign_mask", StrokedPoint::sin_sign_mask)
    .add_macro_u32("fastuidraw_stroke_normal0_y_sign_mask", StrokedPoint::normal0_y_sign_mask)
    .add_macro_u32("fastuidraw_stroke_normal1_y_sign_mask", StrokedPoint::normal1_y_sign_mask)
    .add_macro_u32("fastuidraw_stroke_lambda_negated_mask", StrokedPoint::lambda_negated_mask)
    .add_macro_u32("fastuidraw_stroke_boundary_bit", StrokedPoint::boundary_bit)
    .add_macro_u32("fastuidraw_stroke_join_mask", StrokedPoint::join_mask)
    .add_macro_u32("fastuidraw_stroke_bevel_edge_mask", StrokedPoint::bevel_edge_mask)
    .add_macro_u32("fastuidraw_stroke_end_sub_edge_mask", StrokedPoint::end_sub_edge_mask)
    .add_macro_u32("fastuidraw_stroke_adjustable_cap_ending_mask", StrokedPoint::adjustable_cap_ending_mask)
    .add_macro_u32("fastuidraw_stroke_adjustable_cap_end_contour_mask", StrokedPoint::adjustable_cap_is_end_contour_mask)
    .add_macro_u32("fastuidraw_stroke_depth_bit0", StrokedPoint::depth_bit0)
    .add_macro_u32("fastuidraw_stroke_depth_num_bits", StrokedPoint::depth_num_bits)
    .add_macro_u32("fastuidraw_stroke_offset_type_bit0", StrokedPoint::offset_type_bit0)
    .add_macro_u32("fastuidraw_stroke_offset_type_num_bits", StrokedPoint::offset_type_num_bits);

  m_arc_stroke_constants
    /* offset types of ArcStrokedPoint */
    .add_macro_u32("fastuidraw_arc_stroke_arc_point", ArcStrokedPoint::offset_arc_point)
    .add_macro_u32("fastuidraw_arc_stroke_line_segment", ArcStrokedPoint::offset_line_segment)
    .add_macro_u32("fastuidraw_arc_stroke_dashed_capper", ArcStrokedPoint::offset_arc_point_dashed_capper)

    /* bit masks for ArcStrokedPoint::m_packed_data */
    .add_macro_u32("fastuidraw_arc_stroke_extend_mask", ArcStrokedPoint::extend_mask)
    .add_macro_u32("fastuidraw_arc_stroke_join_mask", ArcStrokedPoint::join_mask)
    .add_macro_u32("fastuidraw_arc_stroke_distance_constant_on_primitive_mask", ArcStrokedPoint::distance_constant_on_primitive_mask)
    .add_macro_u32("fastuidraw_arc_stroke_beyond_boundary_mask", ArcStrokedPoint::beyond_boundary_mask)
    .add_macro_u32("fastuidraw_arc_stroke_inner_stroking_mask", ArcStrokedPoint::inner_stroking_mask)
    .add_macro_u32("fastuidraw_arc_stroke_move_to_arc_center_mask", ArcStrokedPoint::move_to_arc_center_mask)
    .add_macro_u32("fastuidraw_arc_stroke_end_segment_mask", ArcStrokedPoint::end_segment_mask)
    .add_macro_u32("fastuidraw_arc_stroke_boundary_bit", ArcStrokedPoint::boundary_bit)
    .add_macro_u32("fastuidraw_arc_stroke_boundary_mask", ArcStrokedPoint::boundary_mask)
    .add_macro_u32("fastuidraw_arc_stroke_depth_bit0", ArcStrokedPoint::depth_bit0)
    .add_macro_u32("fastuidraw_arc_stroke_depth_num_bits", ArcStrokedPoint::depth_num_bits)
    .add_macro_u32("fastuidraw_arc_stroke_offset_type_bit0", ArcStrokedPoint::offset_type_bit0)
    .add_macro_u32("fastuidraw_arc_stroke_offset_type_num_bits", ArcStrokedPoint::offset_type_num_bits);
}

uint32_t
ShaderSetCreatorStrokingConstants::
sub_shader(enum PainterEnums::cap_style stroke_dash_style)
{
  /* We need to make that having no dashes is sub-shader = 0,
   * this is why the value is not just the enumeration.
   */
  return (stroke_dash_style == PainterEnums::number_cap_styles) ?
    0 :
    stroke_dash_style + 1;
}

/////////////////////////////////
// StrokeShaderCreator methods
StrokeShaderCreator::
StrokeShaderCreator(void)
{
  //for non-aa non-dashed linear stroking
  m_non_aa_shader[0] = build_uber_stroke_shader(0u, 1u);
  FASTUIDRAWassert(!m_non_aa_shader[0]->uses_discard());

  //for non-aa dashed linear stroking
  m_non_aa_shader[discard_shader] = build_uber_stroke_shader(discard_shader, PainterEnums::number_cap_styles + 1);
  FASTUIDRAWassert(m_non_aa_shader[discard_shader]->uses_discard());

  // for dashed and non-dashed arc-strokgin.
  m_non_aa_shader[arc_shader | discard_shader] = build_uber_stroke_shader(arc_shader | discard_shader, PainterEnums::number_cap_styles + 1);
  FASTUIDRAWassert(m_non_aa_shader[arc_shader | discard_shader]->uses_discard());

  // shaders that draw to coverae buffer
  m_coverage_shaders[0] = build_uber_stroke_coverage_shader(coverage_shader, PainterEnums::number_cap_styles + 1u);
  m_coverage_shaders[arc_shader] = build_uber_stroke_coverage_shader(coverage_shader | arc_shader, PainterEnums::number_cap_styles + 1u);

  // shaders that draw to color buffer reading from coverage buffer
  m_post_coverage_shaders[0] = build_uber_stroke_shader(coverage_shader, PainterEnums::number_cap_styles + 1u);
  m_post_coverage_shaders[arc_shader] = build_uber_stroke_shader(coverage_shader | arc_shader, PainterEnums::number_cap_styles + 1u);
}

reference_counted_ptr<PainterItemShader>
StrokeShaderCreator::
create_stroke_non_aa_item_shader(enum PainterEnums::cap_style stroke_dash_style,
                                 enum PainterEnums::stroking_method_t tp)
{
  reference_counted_ptr<PainterItemShader> return_value;
  uint32_t shader_choice(0u), sub_shader_id(0u);

  if (tp == PainterEnums::stroking_method_arc)
    {
      shader_choice |= arc_shader;
      shader_choice |= discard_shader;
    }

  if (stroke_dash_style != PainterEnums::number_cap_styles)
    {
      shader_choice |= discard_shader;
    }

  if (shader_choice == 0)
    {
      /* linear stroking without dashing */
      return_value = m_non_aa_shader[0];
    }
  else
    {
      sub_shader_id = sub_shader(stroke_dash_style);
      FASTUIDRAWassert(m_non_aa_shader[shader_choice]);
      return_value = FASTUIDRAWnew PainterItemShader(m_non_aa_shader[shader_choice], sub_shader_id);
    }
  return return_value;
}

reference_counted_ptr<PainterItemShader>
StrokeShaderCreator::
create_stroke_aa_item_shader(enum PainterEnums::cap_style stroke_dash_style,
                             enum PainterEnums::stroking_method_t tp)
{
  reference_counted_ptr<PainterItemShader> return_value;
  reference_counted_ptr<PainterItemCoverageShader> cvg_shader;
  uint32_t shader_choice(0u);

  if (tp == PainterEnums::stroking_method_arc)
    {
      shader_choice |= arc_shader;
    }

  cvg_shader = FASTUIDRAWnew PainterItemCoverageShader(m_coverage_shaders[shader_choice], sub_shader(stroke_dash_style));
  return_value = FASTUIDRAWnew PainterItemShader(m_post_coverage_shaders[shader_choice], sub_shader(stroke_dash_style), cvg_shader);
  return return_value;
}

varying_list
StrokeShaderCreator::
build_uber_stroke_varyings(uint32_t flags) const
{
  varying_list return_value;

  if (flags & arc_shader)
    {
      return_value
        .add_float("fastuidraw_arc_stroking_relative_to_center_x")
        .add_float("fastuidraw_arc_stroking_relative_to_center_y")
        .add_float("fastuidraw_arc_stroking_arc_radius")
        .add_float("fastuidraw_arc_stroking_stroke_radius")
        .add_float("fastuidraw_arc_stroking_distance_sub_edge_start")
        .add_float("fastuidraw_arc_stroking_distance_sub_edge_end")
        .add_float("fastuidraw_arc_stroking_distance")
        .add_uint("fastuidraw_arc_stroking_dash_bits");
    }
  else
    {
      return_value
        .add_float("fastuidraw_stroking_on_boundary")
        .add_float("fastuidraw_stroking_distance")
        .add_float("fastuidraw_stroking_distance_sub_edge_start")
        .add_float("fastuidraw_stroking_distance_sub_edge_end")
        .add_uint("fastuidraw_stroking_dash_bits");
    }

  return return_value;
}

ShaderSource
StrokeShaderCreator::
build_uber_stroke_source(uint32_t flags, bool is_vertex_shader) const
{
  ShaderSource return_value;
  const ShaderSource::MacroSet *stroke_constants_ptr;
  c_string src, dash_util_src(nullptr), src_util(nullptr);

  if (flags & arc_shader)
    {
      stroke_constants_ptr = &m_arc_stroke_constants;
      src = (is_vertex_shader) ?
        "fastuidraw_painter_arc_stroke.vert.glsl.resource_string":
        "fastuidraw_painter_arc_stroke.frag.glsl.resource_string";
    }
  else
    {
      stroke_constants_ptr = &m_stroke_constants;
      src = (is_vertex_shader) ?
        "fastuidraw_painter_stroke.vert.glsl.resource_string":
        "fastuidraw_painter_stroke.frag.glsl.resource_string";

      src_util = (is_vertex_shader) ?
        "fastuidraw_painter_stroke_compute_offset.vert.glsl.resource_string":
        nullptr;
    }

  dash_util_src = (is_vertex_shader) ?
    "fastuidraw_painter_stroke_util.vert.glsl.resource_string" :
    "fastuidraw_painter_stroke_util.frag.glsl.resource_string";

  return_value
    .add_macros(m_subshader_constants)
    .add_macros(m_common_stroke_constants)
    .add_macros(*stroke_constants_ptr);

  if (flags & coverage_shader)
    {
      return_value.add_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE");
    }

  if (src_util)
    {
      return_value.add_source(src_util, ShaderSource::from_resource);
    }

  return_value
    .add_source(dash_util_src, ShaderSource::from_resource)
    .add_source(src, ShaderSource::from_resource)
    .remove_macros(*stroke_constants_ptr)
    .remove_macros(m_common_stroke_constants)
    .remove_macros(m_subshader_constants);

  if (flags & coverage_shader)
    {
      return_value.remove_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE");
    }

  return return_value;
}

PainterItemCoverageShaderGLSL*
StrokeShaderCreator::
build_uber_stroke_coverage_shader(uint32_t flags, unsigned int num_sub_shaders) const
{
  flags |= coverage_shader;
  return FASTUIDRAWnew PainterItemCoverageShaderGLSL(build_uber_stroke_source(flags, true),
                                                     build_uber_stroke_source(flags, false),
                                                     build_uber_stroke_varyings(flags),
                                                     num_sub_shaders);
}

PainterItemShaderGLSL*
StrokeShaderCreator::
build_uber_stroke_shader(uint32_t flags, unsigned int num_sub_shaders) const
{
  return FASTUIDRAWnew PainterItemShaderGLSL(flags & discard_shader,
                                             build_uber_stroke_source(flags, true),
                                             build_uber_stroke_source(flags, false),
                                             build_uber_stroke_varyings(flags),
                                             num_sub_shaders);
}

//////////////////////////////////////////
// ShaderSetCreator methods
ShaderSetCreator::
ShaderSetCreator(enum PainterBlendShader::shader_type preferred_blending_type,
                 enum PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_type):
  BlendShaderSetCreator(preferred_blending_type, fbf_type),
  StrokeShaderCreator()
{
  m_fill_macros
    .add_macro("fastuidraw_aa_fuzz_type_on_path", uint32_t(FilledPath::Subset::aa_fuzz_type_on_path))
    .add_macro("fastuidraw_aa_fuzz_type_on_boundary", uint32_t(FilledPath::Subset::aa_fuzz_type_on_boundary))
    .add_macro("fastuidraw_aa_fuzz_type_on_boundary_miter", uint32_t(FilledPath::Subset::aa_fuzz_type_on_boundary_miter));

  m_common_glyph_attribute_macros
    .add_macro_float("fastuidraw_restricted_rays_glyph_coord_value", GlyphRenderDataRestrictedRays::glyph_coord_value)
    .add_macro_float("fastuidraw_banded_rays_glyph_coord_value", GlyphRenderDataBandedRays::glyph_coord_value)
    .add_macro("FASTUIDRAW_GLYPH_RECT_WIDTH_NUMBITS", uint32_t(GlyphAttribute::rect_width_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_HEIGHT_NUMBITS", uint32_t(GlyphAttribute::rect_height_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_X_NUMBITS", uint32_t(GlyphAttribute::rect_x_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_Y_NUMBITS", uint32_t(GlyphAttribute::rect_y_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_WIDTH_BIT0", uint32_t(GlyphAttribute::rect_width_bit0))
    .add_macro("FASTUIDRAW_GLYPH_RECT_HEIGHT_BIT0", uint32_t(GlyphAttribute::rect_height_bit0))
    .add_macro("FASTUIDRAW_GLYPH_RECT_X_BIT0", uint32_t(GlyphAttribute::rect_x_bit0))
    .add_macro("FASTUIDRAW_GLYPH_RECT_Y_BIT0", uint32_t(GlyphAttribute::rect_y_bit0));
}

reference_counted_ptr<PainterItemShader>
ShaderSetCreator::
create_glyph_item_shader(c_string vert_src,
                         c_string frag_src,
                         const varying_list &varyings)
{
  ShaderSource vert, frag;
  reference_counted_ptr<PainterItemShader> shader;

  vert
    .add_macros(m_common_glyph_attribute_macros)
    .add_source(vert_src, ShaderSource::from_resource)
    .remove_macros(m_common_glyph_attribute_macros);

  frag
    .add_source(frag_src, ShaderSource::from_resource);

  shader = FASTUIDRAWnew PainterItemShaderGLSL(false, vert, frag, varyings);
  return shader;
}

PainterGlyphShader
ShaderSetCreator::
create_glyph_shader(void)
{
  PainterGlyphShader return_value;
  varying_list coverage_varyings, distance_varyings;
  varying_list restricted_rays_varyings;
  varying_list banded_rays_varyings;

  distance_varyings
    .add_float("fastuidraw_glyph_coord_x")
    .add_float("fastuidraw_glyph_coord_y")
    .add_float("fastuidraw_glyph_width")
    .add_float("fastuidraw_glyph_height")
    .add_uint("fastuidraw_glyph_data_location");

  coverage_varyings
    .add_float("fastuidraw_glyph_coord_x")
    .add_float("fastuidraw_glyph_coord_y")
    .add_float("fastuidraw_glyph_width")
    .add_float("fastuidraw_glyph_height")
    .add_uint("fastuidraw_glyph_data_location");

  restricted_rays_varyings
    .add_float("fastuidraw_glyph_coord_x")
    .add_float("fastuidraw_glyph_coord_y")
    .add_uint("fastuidraw_glyph_data_location");

  banded_rays_varyings
    .add_float("fastuidraw_glyph_coord_x")
    .add_float("fastuidraw_glyph_coord_y")
    .add_uint("fastuidraw_glyph_data_num_vertical_bands")
    .add_uint("fastuidraw_glyph_data_num_horizontal_bands")
    .add_uint("fastuidraw_glyph_data_location");

  return_value
    .shader(coverage_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_coverage_distance_field.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_coverage.frag.glsl.resource_string",
                                     coverage_varyings));

  return_value
    .shader(restricted_rays_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_restricted_rays.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_restricted_rays.frag.glsl.resource_string",
                                     restricted_rays_varyings));
  return_value
    .shader(distance_field_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_coverage_distance_field.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string",
                                     distance_varyings));

  return_value
    .shader(banded_rays_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_banded_rays.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_banded_rays.frag.glsl.resource_string",
                                     banded_rays_varyings));

  return return_value;
}

PainterStrokeShader
ShaderSetCreator::
create_stroke_shader(enum PainterEnums::cap_style cap_style,
                     const reference_counted_ptr<const StrokingDataSelectorBase> &stroke_data_selector)
{
  using namespace fastuidraw;
  PainterStrokeShader return_value;

  return_value
    .stroking_data_selector(stroke_data_selector);

  for (unsigned int tp = 0; tp < PainterEnums::stroking_method_number_precise_choices; ++tp)
    {
      enum PainterEnums::stroking_method_t e_tp;

      e_tp = static_cast<enum PainterEnums::stroking_method_t>(tp);
      return_value
        .shader(e_tp, PainterStrokeShader::non_aa_shader, create_stroke_non_aa_item_shader(cap_style, e_tp))
        .shader(e_tp, PainterStrokeShader::aa_shader, create_stroke_aa_item_shader(cap_style, e_tp));
    }

  if (cap_style == PainterEnums::number_cap_styles)
    {
      return_value
        .fastest_non_anti_aliased_stroking_method(PainterEnums::stroking_method_linear);
    }
  else
    {
      return_value
        .fastest_non_anti_aliased_stroking_method(PainterEnums::stroking_method_arc);
    }

  return_value
    .fastest_anti_aliased_stroking_method(PainterEnums::stroking_method_arc);

  return return_value;
}

PainterDashedStrokeShaderSet
ShaderSetCreator::
create_dashed_stroke_shader_set(void)
{
  using namespace fastuidraw;
  PainterDashedStrokeShaderSet return_value;
  reference_counted_ptr<const StrokingDataSelectorBase> se;

  se = PainterDashedStrokeParams::stroking_data_selector(false);
  return_value
    .shader(PainterEnums::flat_caps, create_stroke_shader(PainterEnums::flat_caps, se))
    .shader(PainterEnums::rounded_caps, create_stroke_shader(PainterEnums::rounded_caps, se))
    .shader(PainterEnums::square_caps, create_stroke_shader(PainterEnums::square_caps, se));
  return return_value;
}

PainterFillShader
ShaderSetCreator::
create_fill_shader(void)
{
  PainterFillShader fill_shader;
  reference_counted_ptr<PainterItemShader> item_shader;
  reference_counted_ptr<PainterItemShader> uber_fuzz_shader;
  reference_counted_ptr<PainterItemShader> aa_fuzz_deferred;
  reference_counted_ptr<PainterItemCoverageShaderGLSL> aa_fuzz_deferred_coverage;

  item_shader = FASTUIDRAWnew PainterItemShaderGLSL(false,
                                                    ShaderSource()
                                                    .add_source("fastuidraw_painter_fill.vert.glsl.resource_string",
                                                                ShaderSource::from_resource),
                                                    ShaderSource()
                                                    .add_source("fastuidraw_painter_fill.frag.glsl.resource_string",
                                                                ShaderSource::from_resource),
                                                    varying_list());

  /* the aa-fuzz shader via deferred coverage is not a part of the uber-fuzz shader */
  aa_fuzz_deferred_coverage =
    FASTUIDRAWnew PainterItemCoverageShaderGLSL(ShaderSource()
                                                .add_macros(m_fill_macros)
                                                .add_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                                .add_source("fastuidraw_painter_fill_aa_fuzz.vert.glsl.resource_string",
                                                            ShaderSource::from_resource)
                                                .remove_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                                .remove_macros(m_fill_macros),
                                                ShaderSource()
                                                .add_macros(m_fill_macros)
                                                .add_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                                .add_source("fastuidraw_painter_fill_aa_fuzz.frag.glsl.resource_string",
                                                            ShaderSource::from_resource)
                                                .remove_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                                .remove_macros(m_fill_macros),
                                                varying_list().add_float("fastuidraw_aa_fuzz"));
  aa_fuzz_deferred =
    FASTUIDRAWnew PainterItemShaderGLSL(false,
                                        ShaderSource()
                                        .add_macros(m_fill_macros)
                                        .add_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                        .add_source("fastuidraw_painter_fill_aa_fuzz.vert.glsl.resource_string",
                                                    ShaderSource::from_resource)
                                        .remove_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                        .remove_macros(m_fill_macros),
                                        ShaderSource()
                                        .add_macros(m_fill_macros)
                                        .add_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                        .add_source("fastuidraw_painter_fill_aa_fuzz.frag.glsl.resource_string",
                                                    ShaderSource::from_resource)
                                        .remove_macro("FASTUIDRAW_STROKING_USE_DEFFERRED_COVERAGE")
                                        .remove_macros(m_fill_macros),
                                        varying_list().add_float("fastuidraw_aa_fuzz"),
                                        aa_fuzz_deferred_coverage);


  fill_shader
    .item_shader(item_shader)
    .aa_fuzz_shader(aa_fuzz_deferred);

  return fill_shader;
}

PainterShaderSet
ShaderSetCreator::
create_shader_set(void)
{
  PainterShaderSet return_value;
  reference_counted_ptr<const StrokingDataSelectorBase> se;
  reference_counted_ptr<PainterBrushShader> br;
  ShaderSource::MacroSet brush_macros;
  varying_list brush_varyings;
  ShaderSource str;

  se = PainterStrokeParams::stroking_data_selector(false);

  /* TODO: localize all these functions with FASTUIDRAW_LOCAL
   * and localize the routines in the .glsl.resource_string
   * files as well.
   */
  UnpackSourceGenerator("fastuidraw_brush_header")
    .set(PainterBrush::features_offset, ".features", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::header_red_green_offset, ".red_green", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::header_blue_alpha_offset, ".blue_alpha", UnpackSourceGenerator::uint_type)
    .stream_unpack_function(str, "fastuidraw_read_brush_header");

  UnpackSourceGenerator("fastuidraw_brush_repeat_window")
    .set(PainterBrush::repeat_window_x_offset, ".xy.x")
    .set(PainterBrush::repeat_window_y_offset, ".xy.y")
    .set(PainterBrush::repeat_window_width_offset, ".wh.x")
    .set(PainterBrush::repeat_window_height_offset, ".wh.y")
    .stream_unpack_function(str, "fastuidraw_read_brush_repeat_window");

  UnpackSourceGenerator("fastuidraw_brush_image_data_raw")
    .set(PainterBrush::image_atlas_location_xyz_offset, ".image_atlas_location_xyz", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::image_size_xy_offset, ".image_size_xy", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::image_start_xy_offset, ".image_start_xy", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::image_number_lookups_offset, ".image_number_lookups", UnpackSourceGenerator::uint_type)
    .stream_unpack_function(str, "fastuidraw_read_brush_image_raw_data");

  UnpackSourceGenerator("fastuidraw_brush_gradient_raw")
    .set(PainterBrush::gradient_p0_x_offset, ".p0.x")
    .set(PainterBrush::gradient_p0_y_offset, ".p0.y")
    .set(PainterBrush::gradient_p1_x_offset, ".p1.x")
    .set(PainterBrush::gradient_p1_y_offset, ".p1.y")
    .set(PainterBrush::gradient_color_stop_xy_offset, ".color_stop_sequence_xy", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::gradient_color_stop_length_offset, ".color_stop_sequence_length", UnpackSourceGenerator::uint_type)
    .stream_unpack_function(str, "fastuidraw_read_brush_linear_or_sweep_gradient_data");

  UnpackSourceGenerator("fastuidraw_brush_gradient_raw")
    .set(PainterBrush::gradient_p0_x_offset, ".p0.x")
    .set(PainterBrush::gradient_p0_y_offset, ".p0.y")
    .set(PainterBrush::gradient_p1_x_offset, ".p1.x")
    .set(PainterBrush::gradient_p1_y_offset, ".p1.y")
    .set(PainterBrush::gradient_color_stop_xy_offset, ".color_stop_sequence_xy", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::gradient_color_stop_length_offset, ".color_stop_sequence_length", UnpackSourceGenerator::uint_type)
    .set(PainterBrush::gradient_start_radius_offset, ".r0")
    .set(PainterBrush::gradient_end_radius_offset, ".r1")
    .stream_unpack_function(str, "fastuidraw_read_brush_radial_gradient_data");

  brush_varyings
    .add_float("fastuidraw_brush_p_x")
    .add_float("fastuidraw_brush_p_y")
    .add_float("fastuidraw_brush_repeat_window_x", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_repeat_window_y", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_repeat_window_w", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_repeat_window_h", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_gradient_p0_x", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_gradient_p0_y", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_gradient_p1_x", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_gradient_p1_y", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_gradient_r0", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_gradient_r1", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_image_x", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_image_y", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_image_size_x", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_image_size_y", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_image_texel_size_on_master_index_tile", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_recip_image_texel_size_on_master_index_tile", varying_list::interpolation_flat)
    .add_uint("fastuidraw_brush_image_layer")
    .add_uint("fastuidraw_brush_image_number_index_lookups")
    .add_float("fastuidraw_brush_color_stop_x", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_color_stop_y", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_color_stop_length", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_color_x", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_color_y", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_color_z", varying_list::interpolation_flat)
    .add_float("fastuidraw_brush_color_w", varying_list::interpolation_flat)
    .add_uint("fastuidraw_brush_features")
    .add_alias("fastuidraw_brush_image_layer", "fastuidraw_brush_image_bindless_low_handle")
    .add_alias("fastuidraw_brush_image_number_index_lookups", "fastuidraw_brush_image_bindless_high_handle")
    .add_alias("fastuidraw_brush_gradient_p0_x", "fastuidraw_brush_gradient_sweep_point_x")
    .add_alias("fastuidraw_brush_gradient_p0_y", "fastuidraw_brush_gradient_sweep_point_y")
    .add_alias("fastuidraw_brush_gradient_p1_x", "fastuidraw_brush_gradient_sweep_angle")
    .add_alias("fastuidraw_brush_gradient_p1_y", "fastuidraw_brush_gradient_sweep_sign_factor");

  brush_macros
    .add_macro_u32("fastuidraw_brush_image_mask", PainterBrush::image_mask)
    .add_macro_u32("fastuidraw_brush_image_filter_bit0", PainterBrush::image_filter_bit0)
    .add_macro_u32("fastuidraw_brush_image_filter_num_bits", PainterBrush::image_filter_num_bits)
    .add_macro_u32("fastuidraw_brush_image_filter_nearest", PainterBrush::image_filter_nearest)
    .add_macro_u32("fastuidraw_brush_image_filter_linear", PainterBrush::image_filter_linear)
    .add_macro_u32("fastuidraw_brush_image_filter_cubic", PainterBrush::image_filter_cubic)

    .add_macro_u32("fastuidraw_brush_image_type_mask", PainterBrush::image_type_mask)
    .add_macro_u32("fastuidraw_brush_image_type_bit0", PainterBrush::image_type_bit0)
    .add_macro_u32("fastuidraw_brush_image_type_num_bits", PainterBrush::image_type_num_bits)
    .add_macro_u32("fastuidraw_brush_image_type_on_atlas", Image::on_atlas)
    .add_macro_u32("fastuidraw_brush_image_type_bindless_texture2d", Image::bindless_texture2d)
    .add_macro_u32("fastuidraw_brush_image_type_context_texture2d", Image::context_texture2d)

    .add_macro_u32("fastuidraw_brush_image_format_mask", PainterBrush::image_format_mask)
    .add_macro_u32("fastuidraw_brush_image_format_bit0", PainterBrush::image_format_bit0)
    .add_macro_u32("fastuidraw_brush_image_format_num_bits", PainterBrush::image_format_num_bits)
    .add_macro_u32("fastuidraw_brush_image_format_rgba", Image::rgba_format)
    .add_macro_u32("fastuidraw_brush_image_format_premultipied_rgba", Image::premultipied_rgba_format)

    .add_macro_u32("fastuidraw_brush_image_mipmap_mask", PainterBrush::image_mipmap_mask)
    .add_macro_u32("fastuidraw_brush_image_mipmap_bit0", PainterBrush::image_mipmap_bit0)
    .add_macro_u32("fastuidraw_brush_image_mipmap_num_bits", PainterBrush::image_mipmap_num_bits)

    .add_macro_u32("fastuidraw_brush_gradient_type_bit0", PainterBrush::gradient_type_bit0)
    .add_macro_u32("fastuidraw_brush_gradient_type_num_bits", PainterBrush::gradient_type_num_bits)
    .add_macro_u32("fastuidraw_brush_no_gradient_type", PainterBrush::no_gradient_type)
    .add_macro_u32("fastuidraw_brush_linear_gradient_type", PainterBrush::linear_gradient_type)
    .add_macro_u32("fastuidraw_brush_radial_gradient_type", PainterBrush::radial_gradient_type)
    .add_macro_u32("fastuidraw_brush_sweep_gradient_type", PainterBrush::sweep_gradient_type)

    .add_macro_u32("fastuidraw_brush_gradient_spread_type_bit0", PainterBrush::gradient_spread_type_bit0)

    .add_macro_u32("fastuidraw_brush_spread_type_num_bits", PainterBrush::spread_type_num_bits)
    .add_macro_u32("fastuidraw_brush_spread_clamp", PainterBrush::spread_clamp)
    .add_macro_u32("fastuidraw_brush_spread_repeat", PainterBrush::spread_repeat)
    .add_macro_u32("fastuidraw_brush_spread_mirror_repeat", PainterBrush::spread_mirror_repeat)
    .add_macro_u32("fastuidraw_brush_spread_mirror", PainterBrush::spread_mirror)

    .add_macro_u32("fastuidraw_brush_repeat_window_mask", PainterBrush::repeat_window_mask)
    .add_macro_u32("fastuidraw_brush_repeat_window_x_spread_type_bit0", PainterBrush::repeat_window_x_spread_type_bit0)
    .add_macro_u32("fastuidraw_brush_repeat_window_y_spread_type_bit0", PainterBrush::repeat_window_y_spread_type_bit0)

    .add_macro_u32("fastuidraw_brush_transformation_translation_mask", PainterBrush::transformation_translation_mask)
    .add_macro_u32("fastuidraw_brush_transformation_matrix_mask", PainterBrush::transformation_matrix_mask)
    .add_macro_u32("fastuidraw_brush_image_master_index_x_bit0",     PainterBrush::image_atlas_location_x_bit0)
    .add_macro_u32("fastuidraw_brush_image_master_index_x_num_bits", PainterBrush::image_atlas_location_x_num_bits)
    .add_macro_u32("fastuidraw_brush_image_master_index_y_bit0",     PainterBrush::image_atlas_location_y_bit0)
    .add_macro_u32("fastuidraw_brush_image_master_index_y_num_bits", PainterBrush::image_atlas_location_y_num_bits)
    .add_macro_u32("fastuidraw_brush_image_master_index_z_bit0",     PainterBrush::image_atlas_location_z_bit0)
    .add_macro_u32("fastuidraw_brush_image_master_index_z_num_bits", PainterBrush::image_atlas_location_z_num_bits)
    .add_macro_u32("fastuidraw_brush_image_size_x_bit0",     PainterBrush::image_size_x_bit0)
    .add_macro_u32("fastuidraw_brush_image_size_x_num_bits", PainterBrush::image_size_x_num_bits)
    .add_macro_u32("fastuidraw_brush_image_size_y_bit0",     PainterBrush::image_size_y_bit0)
    .add_macro_u32("fastuidraw_brush_image_size_y_num_bits", PainterBrush::image_size_y_num_bits)
    .add_macro_u32("fastuidraw_brush_colorstop_x_bit0",     PainterBrush::gradient_color_stop_x_bit0)
    .add_macro_u32("fastuidraw_brush_colorstop_x_num_bits", PainterBrush::gradient_color_stop_x_num_bits)
    .add_macro_u32("fastuidraw_brush_colorstop_y_bit0",     PainterBrush::gradient_color_stop_y_bit0)
    .add_macro_u32("fastuidraw_brush_colorstop_y_num_bits", PainterBrush::gradient_color_stop_y_num_bits);

  br = FASTUIDRAWnew PainterBrushShaderGLSL(1,
                                            ShaderSource()
                                            .add_macros(brush_macros)
                                            .add_source("fastuidraw_brush_utils.glsl.resource_string", ShaderSource::from_resource)
                                            .add_source("fastuidraw_painter_brush.vert.glsl.resource_string", ShaderSource::from_resource)
                                            .add_source(str)
                                            .remove_macros(brush_macros),
                                            ShaderSource()
                                            .add_macros(brush_macros)
                                            .add_source("fastuidraw_painter_brush.frag.glsl.resource_string", ShaderSource::from_resource)
                                            .remove_macros(brush_macros),
                                            brush_varyings);

  return_value
    .glyph_shader(create_glyph_shader())
    .stroke_shader(create_stroke_shader(PainterEnums::number_cap_styles, se))
    .dashed_stroke_shader(create_dashed_stroke_shader_set())
    .fill_shader(create_fill_shader())
    .blend_shaders(create_blend_shaders())
    .brush_shader(br);
  return return_value;
}

}}}
