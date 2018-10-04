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

#include <string>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_glyphs.hpp>
#include <fastuidraw/painter/stroked_point.hpp>
#include <fastuidraw/painter/arc_stroked_point.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include <fastuidraw/text/glyph_attribute.hpp>
#include "backend_shaders.hpp"

namespace fastuidraw { namespace glsl { namespace detail {

/////////////////////////////////////
// CompositeShaderSetCreator methods
CompositeShaderSetCreator::
CompositeShaderSetCreator(enum PainterCompositeShader::shader_type tp):
  m_type(tp)
{
  if (m_type == PainterCompositeShader::single_src)
    {
      m_single_src_composite_shader_code =
        FASTUIDRAWnew PainterCompositeShaderGLSL(PainterCompositeShader::single_src,
                                             ShaderSource()
                                             .add_source("fastuidraw_fall_through.glsl.resource_string",
                                                         ShaderSource::from_resource));
    }
}

void
CompositeShaderSetCreator::
add_single_src_composite_shader(PainterCompositeShaderSet &out,
                            enum PainterEnums::composite_mode_t md,
                            const BlendMode &single_md)
{
  FASTUIDRAWassert(m_type == PainterCompositeShader::single_src);
  out.shader(md, single_md, m_single_src_composite_shader_code);
}

void
CompositeShaderSetCreator::
add_dual_src_composite_shader(PainterCompositeShaderSet &out,
                          enum PainterEnums::composite_mode_t md,
                          const std::string &dual_src_file,
                          const BlendMode &dual_md)
{
  FASTUIDRAWassert(m_type == PainterCompositeShader::dual_src);

  reference_counted_ptr<PainterCompositeShader> p;
  ShaderSource src;

  src.add_source(dual_src_file.c_str(), ShaderSource::from_resource);
  p = FASTUIDRAWnew PainterCompositeShaderGLSL(m_type, src);
  out.shader(md, dual_md, p);
}

void
CompositeShaderSetCreator::
add_fbf_composite_shader(PainterCompositeShaderSet &out,
                     enum PainterEnums::composite_mode_t md,
                     const std::string &framebuffer_fetch_src_file)
{
  FASTUIDRAWassert(m_type == PainterCompositeShader::framebuffer_fetch);

  reference_counted_ptr<PainterCompositeShader> p;
  ShaderSource src;

  src.add_source(framebuffer_fetch_src_file.c_str(), ShaderSource::from_resource);
  p = FASTUIDRAWnew PainterCompositeShaderGLSL(m_type, src);
  out.shader(md, BlendMode().blending_on(false), p);
}

void
CompositeShaderSetCreator::
add_composite_shader(PainterCompositeShaderSet &out,
                 enum PainterEnums::composite_mode_t md,
                 const BlendMode &single_md,
                 const std::string &dual_src_file,
                 const BlendMode &dual_md,
                 const std::string &framebuffer_fetch_src_file)
{
  switch(m_type)
    {
    case PainterCompositeShader::single_src:
      add_single_src_composite_shader(out, md, single_md);
      break;

    case PainterCompositeShader::dual_src:
      add_dual_src_composite_shader(out, md, dual_src_file, dual_md);
      break;

    case PainterCompositeShader::framebuffer_fetch:
      add_fbf_composite_shader(out, md, framebuffer_fetch_src_file);
      break;

    default:
      FASTUIDRAWassert(!"Bad m_type");
    }
}

void
CompositeShaderSetCreator::
add_composite_shader(PainterCompositeShaderSet &out,
                 enum PainterEnums::composite_mode_t md,
                 const std::string &dual_src_file,
                 const BlendMode &dual_md,
                 const std::string &framebuffer_fetch_src_file)
{
  switch(m_type)
    {
    case PainterCompositeShader::single_src:
      break;

    case PainterCompositeShader::dual_src:
      add_dual_src_composite_shader(out, md, dual_src_file, dual_md);
      break;

    case PainterCompositeShader::framebuffer_fetch:
      add_fbf_composite_shader(out, md, framebuffer_fetch_src_file);
      break;

    default:
      FASTUIDRAWassert(!"Bad m_type");
    }
}

void
CompositeShaderSetCreator::
add_composite_shader(PainterCompositeShaderSet &out,
                 enum PainterEnums::composite_mode_t md,
                 const std::string &framebuffer_fetch_src_file)
{
  switch(m_type)
    {
    case PainterCompositeShader::single_src:
      break;

    case PainterCompositeShader::dual_src:
      break;

    case PainterCompositeShader::framebuffer_fetch:
      add_fbf_composite_shader(out, md, framebuffer_fetch_src_file);
      break;

    default:
      FASTUIDRAWassert(!"Bad m_type");
    }
}

PainterCompositeShaderSet
CompositeShaderSetCreator::
create_composite_shaders(void)
{
  using namespace fastuidraw::PainterEnums;
  /* try to use as few composite modes as possible so that
   * we have fewer draw call breaks. The convention is as
   * follows:
   * - src0 is GL_ONE and the GLSL code handles the multiply
   * - src1 is computed by the GLSL code as needed
   * This is fine for those modes that do not need DST values
   */
  BlendMode one_src1, dst_alpha_src1, one_minus_dst_alpha_src1;
  PainterCompositeShaderSet shaders;

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

  add_composite_shader(shaders, composite_porter_duff_src_over,
                   BlendMode().func(BlendMode::ONE, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_src_over.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_src_over.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_dst_over,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE),
                   "fastuidraw_porter_duff_dst_over.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_dst_over.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_clear,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ZERO),
                   "fastuidraw_porter_duff_clear.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_clear.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_src,
                   BlendMode().func(BlendMode::ONE, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_src.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_dst,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ONE),
                   "fastuidraw_porter_duff_dst.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_src_in,
                   BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src_in.glsl.resource_string", dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_in.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_dst_in,
                   BlendMode().func(BlendMode::ZERO, BlendMode::SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_in.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst_in.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_src_out,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ZERO),
                   "fastuidraw_porter_duff_src_out.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_out.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_dst_out,
                   BlendMode().func(BlendMode::ZERO, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_out.glsl.resource_string", one_src1,
                   "fastuidraw_fbf_porter_duff_dst_out.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_src_atop,
                   BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_src_atop.glsl.resource_string", dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_src_atop.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_dst_atop,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::SRC_ALPHA),
                   "fastuidraw_porter_duff_dst_atop.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string");

  add_composite_shader(shaders, composite_porter_duff_xor,
                   BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                   "fastuidraw_porter_duff_xor.glsl.resource_string", one_minus_dst_alpha_src1,
                   "fastuidraw_fbf_porter_duff_xor.glsl.resource_string");

  return shaders;
}

///////////////////////////////////////////////
// ShaderSetCreatorStrokingConstants methods
ShaderSetCreatorStrokingConstants::
ShaderSetCreatorStrokingConstants(void)
{
  using namespace fastuidraw::PainterEnums;

  m_stroke_render_pass_num_bits = number_bits_required(uber_number_passes);
  m_stroke_dash_style_num_bits = number_bits_required(number_cap_styles);
  FASTUIDRAWassert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_render_pass_num_bits) >= uber_number_passes);
  FASTUIDRAWassert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_dash_style_num_bits) >= number_cap_styles);
  FASTUIDRAWassert(m_stroke_render_pass_num_bits + m_stroke_dash_style_num_bits + 1u <= 32u);

  m_stroke_render_pass_bit0 = 0;
  m_stroke_dash_style_bit0 = m_stroke_render_pass_bit0 + m_stroke_render_pass_num_bits;

  create_macro_set(m_subshader_constants, true);
  create_macro_set(m_subshader_constants_non_aa_only, false);

  m_stroke_constants
    /* offset types of StrokedPoint */
    .add_macro("fastuidraw_stroke_offset_sub_edge", uint32_t(StrokedPoint::offset_sub_edge))
    .add_macro("fastuidraw_stroke_offset_shared_with_edge", uint32_t(StrokedPoint::offset_shared_with_edge))
    .add_macro("fastuidraw_stroke_offset_rounded_join", uint32_t(StrokedPoint::offset_rounded_join))

    .add_macro("fastuidraw_stroke_offset_miter_bevel_join", uint32_t(StrokedPoint::offset_miter_bevel_join))
    .add_macro("fastuidraw_stroke_offset_miter_join", uint32_t(StrokedPoint::offset_miter_join))
    .add_macro("fastuidraw_stroke_offset_miter_clip_join", uint32_t(StrokedPoint::offset_miter_clip_join))

    .add_macro("fastuidraw_stroke_offset_rounded_cap", uint32_t(StrokedPoint::offset_rounded_cap))
    .add_macro("fastuidraw_stroke_offset_square_cap", uint32_t(StrokedPoint::offset_square_cap))
    .add_macro("fastuidraw_stroke_offset_adjustable_cap", uint32_t(StrokedPoint::offset_adjustable_cap))

    /* bit masks for StrokedPoint::m_packed_data */
    .add_macro("fastuidraw_stroke_sin_sign_mask", uint32_t(StrokedPoint::sin_sign_mask))
    .add_macro("fastuidraw_stroke_normal0_y_sign_mask", uint32_t(StrokedPoint::normal0_y_sign_mask))
    .add_macro("fastuidraw_stroke_normal1_y_sign_mask", uint32_t(StrokedPoint::normal1_y_sign_mask))
    .add_macro("fastuidraw_stroke_lambda_negated_mask", uint32_t(StrokedPoint::lambda_negated_mask))
    .add_macro("fastuidraw_stroke_boundary_bit", uint32_t(StrokedPoint::boundary_bit))
    .add_macro("fastuidraw_stroke_join_mask", uint32_t(StrokedPoint::join_mask))
    .add_macro("fastuidraw_stroke_bevel_edge_mask", uint32_t(StrokedPoint::bevel_edge_mask))
    .add_macro("fastuidraw_stroke_end_sub_edge_mask", uint32_t(StrokedPoint::end_sub_edge_mask))
    .add_macro("fastuidraw_stroke_adjustable_cap_ending_mask", uint32_t(StrokedPoint::adjustable_cap_ending_mask))
    .add_macro("fastuidraw_stroke_adjustable_cap_end_contour_mask", uint32_t(StrokedPoint::adjustable_cap_is_end_contour_mask))
    .add_macro("fastuidraw_stroke_depth_bit0", uint32_t(StrokedPoint::depth_bit0))
    .add_macro("fastuidraw_stroke_depth_num_bits", uint32_t(StrokedPoint::depth_num_bits))
    .add_macro("fastuidraw_stroke_offset_type_bit0", uint32_t(StrokedPoint::offset_type_bit0))
    .add_macro("fastuidraw_stroke_offset_type_num_bits", uint32_t(StrokedPoint::offset_type_num_bits));

  m_arc_stroke_constants
    /* offset types of ArcStrokedPoint */
    .add_macro("fastuidraw_arc_stroke_arc_point", uint32_t(ArcStrokedPoint::offset_arc_point))
    .add_macro("fastuidraw_arc_stroke_line_segment", uint32_t(ArcStrokedPoint::offset_line_segment))
    .add_macro("fastuidraw_arc_stroke_dashed_capper", uint32_t(ArcStrokedPoint::offset_arc_point_dashed_capper))

    /* bit masks for ArcStrokedPoint::m_packed_data */
    .add_macro("fastuidraw_arc_stroke_extend_mask", uint32_t(ArcStrokedPoint::extend_mask))
    .add_macro("fastuidraw_arc_stroke_join_mask", uint32_t(ArcStrokedPoint::join_mask))
    .add_macro("fastuidraw_arc_stroke_distance_constant_on_primitive_mask", uint32_t(ArcStrokedPoint::distance_constant_on_primitive_mask))
    .add_macro("fastuidraw_arc_stroke_beyond_boundary_mask", uint32_t(ArcStrokedPoint::beyond_boundary_mask))
    .add_macro("fastuidraw_arc_stroke_inner_stroking_mask", uint32_t(ArcStrokedPoint::inner_stroking_mask))
    .add_macro("fastuidraw_arc_stroke_move_to_arc_center_mask", uint32_t(ArcStrokedPoint::move_to_arc_center_mask))
    .add_macro("fastuidraw_arc_stroke_end_segment_mask", uint32_t(ArcStrokedPoint::end_segment_mask))
    .add_macro("fastuidraw_arc_stroke_boundary_bit", uint32_t(ArcStrokedPoint::boundary_bit))
    .add_macro("fastuidraw_arc_stroke_boundary_mask", uint32_t(ArcStrokedPoint::boundary_mask))
    .add_macro("fastuidraw_arc_stroke_depth_bit0", uint32_t(ArcStrokedPoint::depth_bit0))
    .add_macro("fastuidraw_arc_stroke_depth_num_bits", uint32_t(ArcStrokedPoint::depth_num_bits))
    .add_macro("fastuidraw_arc_stroke_offset_type_bit0", uint32_t(ArcStrokedPoint::offset_type_bit0))
    .add_macro("fastuidraw_arc_stroke_offset_type_num_bits", uint32_t(ArcStrokedPoint::offset_type_num_bits));
}

void
ShaderSetCreatorStrokingConstants::
create_macro_set(ShaderSource::MacroSet &dst, bool render_pass_varies) const
{
  unsigned int stroke_dash_style_bit0(m_stroke_dash_style_bit0);
  unsigned int stroke_render_pass_num_bits(m_stroke_render_pass_num_bits);

  if (!render_pass_varies)
    {
      stroke_dash_style_bit0 -= m_stroke_render_pass_num_bits;
      stroke_render_pass_num_bits -= m_stroke_render_pass_num_bits;
    }
  else
    {
      dst
        .add_macro("fastuidraw_stroke_sub_shader_render_pass_bit0", uint32_t(m_stroke_render_pass_bit0))
        .add_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits", uint32_t(stroke_render_pass_num_bits));
    }

  dst
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_bit0", uint32_t(stroke_dash_style_bit0))
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_num_bits", uint32_t(m_stroke_dash_style_num_bits))
    .add_macro("fastuidraw_stroke_dashed_flat_caps", uint32_t(PainterEnums::flat_caps))
    .add_macro("fastuidraw_stroke_dashed_rounded_caps", uint32_t(PainterEnums::rounded_caps))
    .add_macro("fastuidraw_stroke_dashed_square_caps", uint32_t(PainterEnums::square_caps))
    .add_macro("fastuidraw_stroke_aa_pass1", uint32_t(uber_stroke_aa_pass1))
    .add_macro("fastuidraw_stroke_aa_pass2", uint32_t(uber_stroke_aa_pass2))
    .add_macro("fastuidraw_stroke_non_aa", uint32_t(uber_stroke_non_aa));
}

//////////////////////////////////////////
//  ShaderSetCreator methods
ShaderSetCreator::
ShaderSetCreator(enum PainterCompositeShader::shader_type composite_tp,
                 enum PainterStrokeShader::type_t stroke_tp,
                 const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass1,
                 const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass2):
  CompositeShaderSetCreator(composite_tp),
  m_stroke_tp(stroke_tp),
  m_stroke_action_pass1(stroke_action_pass1),
  m_stroke_action_pass2(stroke_action_pass2)
{
  unsigned int num_undashed_sub_shaders, num_dashed_sub_shaders;

  num_undashed_sub_shaders = 1u << (m_stroke_render_pass_num_bits + 1);
  num_dashed_sub_shaders = 1u << (m_stroke_render_pass_num_bits + m_stroke_dash_style_num_bits + 1u);

  m_uber_stroke_shader = build_uber_stroke_shader(0u, num_undashed_sub_shaders);
  m_uber_dashed_stroke_shader = build_uber_stroke_shader(dashed_shader, num_dashed_sub_shaders);

  m_uber_arc_stroke_shader = build_uber_stroke_shader(arc_shader, num_undashed_sub_shaders);
  m_uber_arc_dashed_stroke_shader = build_uber_stroke_shader(arc_shader | dashed_shader, num_dashed_sub_shaders);

  if (stroke_tp != PainterStrokeShader::draws_solid_then_fuzz)
    {
      /* we need a special stroke non-aa dashed shader that does discard */
      unsigned int num_dashed_sub_shaders_non_aa;

      num_dashed_sub_shaders_non_aa = 1u << (1 + m_stroke_dash_style_num_bits);
      m_dashed_discard_stroke_shader = build_uber_stroke_shader(dashed_shader | only_supports_non_aa, num_dashed_sub_shaders_non_aa);
      FASTUIDRAWassert(m_dashed_discard_stroke_shader->uses_discard());

      /* we also need a shaders for non-aa arc-shading as they will do discard too */
      m_arc_discard_stroke_shader = build_uber_stroke_shader(only_supports_non_aa | arc_shader, 1);
      m_dashed_arc_discard_stroke_shader = build_uber_stroke_shader(only_supports_non_aa | dashed_shader | arc_shader, num_dashed_sub_shaders_non_aa);
      FASTUIDRAWassert(m_arc_discard_stroke_shader->uses_discard());
      FASTUIDRAWassert(m_dashed_arc_discard_stroke_shader->uses_discard());
      FASTUIDRAWassert(!m_uber_arc_stroke_shader->uses_discard());
      FASTUIDRAWassert(!m_uber_arc_dashed_stroke_shader->uses_discard());
    }
  else
    {
      FASTUIDRAWassert(m_uber_arc_stroke_shader->uses_discard());
      FASTUIDRAWassert(m_uber_arc_dashed_stroke_shader->uses_discard());
    }

  m_common_glyph_attribute_macros
    .add_macro("FASTUIDRAW_GLYPH_RECT_WIDTH_NUMBITS", uint32_t(GlyphAttribute::rect_width_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_HEIGHT_NUMBITS", uint32_t(GlyphAttribute::rect_height_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_X_NUMBITS", uint32_t(GlyphAttribute::rect_x_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_Y_NUMBITS", uint32_t(GlyphAttribute::rect_y_num_bits))
    .add_macro("FASTUIDRAW_GLYPH_RECT_WIDTH_BIT0", uint32_t(GlyphAttribute::rect_width_bit0))
    .add_macro("FASTUIDRAW_GLYPH_RECT_HEIGHT_BIT0", uint32_t(GlyphAttribute::rect_height_bit0))
    .add_macro("FASTUIDRAW_GLYPH_RECT_X_BIT0", uint32_t(GlyphAttribute::rect_x_bit0))
    .add_macro("FASTUIDRAW_GLYPH_RECT_Y_BIT0", uint32_t(GlyphAttribute::rect_y_bit0));

  m_glyph_restricted_rays_macros
    .add_macro("HIERARCHY_NODE_BIT", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_is_node_bit))
    .add_macro("HIERARCHY_NODE_MASK", uint32_t(FASTUIDRAW_MASK(GlyphRenderDataRestrictedRays::hierarchy_is_node_bit, 1u)))
    .add_macro("HIERARCHY_SPLIT_COORD_BIT", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_splitting_coordinate_bit))
    .add_macro("HIERARCHY_CHILD0_BIT", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_child0_offset_bit0))
    .add_macro("HIERARCHY_CHILD1_BIT", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_child1_offset_bit0))
    .add_macro("HIERARCHY_CHILD_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_child_offset_numbits))
    .add_macro("HIERARCHY_CURVE_LIST_BIT0", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_bit0))
    .add_macro("HIERARCHY_CURVE_LIST_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_numbits))
    .add_macro("HIERARCHY_CURVE_LIST_SIZE_BIT0", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_size_bit0))
    .add_macro("HIERARCHY_CURVE_LIST_SIZE_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_size_numbits))
    .add_macro("POINT_COORDINATE_BIAS", uint32_t(GlyphRenderDataRestrictedRays::point_coordinate_bias))
    .add_macro("POINT_COORDINATE_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::point_coordinate_numbits))
    .add_macro("POINT_X_COORDINATE_BIT0", uint32_t(GlyphRenderDataRestrictedRays::point_x_coordinate_bit0))
    .add_macro("POINT_Y_COORDINATE_BIT0", uint32_t(GlyphRenderDataRestrictedRays::point_y_coordinate_bit0))
    .add_macro("RIGHT_CORNER_MASK", uint32_t(GlyphAttribute::right_corner_mask))
    .add_macro("TOP_CORNER_MASK", uint32_t(GlyphAttribute::top_corner_mask))
    .add_macro("WINDING_VALUE_BIAS", uint32_t(GlyphRenderDataRestrictedRays::winding_bias))
    .add_macro("WINDING_VALUE_BIT0", uint32_t(GlyphRenderDataRestrictedRays::winding_value_bit0))
    .add_macro("WINDING_VALUE_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::winding_value_numbits))
    .add_macro("POSITION_DELTA_BIAS", uint32_t(GlyphRenderDataRestrictedRays::delta_bias))
    .add_macro("POSITION_DELTA_DIVIDE", uint32_t(GlyphRenderDataRestrictedRays::delta_div_factor))
    .add_macro("POSITION_DELTA_X_BIT0", uint32_t(GlyphRenderDataRestrictedRays::delta_x_bit0))
    .add_macro("POSITION_DELTA_Y_BIT0", uint32_t(GlyphRenderDataRestrictedRays::delta_y_bit0))
    .add_macro("POSITION_DELTA_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::delta_numbits))
    .add_macro("CURVE_ENTRY_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::curve_numbits))
    .add_macro("CURVE_ENTRY0_BIT0", uint32_t(GlyphRenderDataRestrictedRays::curve_entry0_bit0))
    .add_macro("CURVE_ENTRY1_BIT0", uint32_t(GlyphRenderDataRestrictedRays::curve_entry1_bit0))
    .add_macro("CURVE_IS_QUADRATIC_BIT", uint32_t(GlyphRenderDataRestrictedRays::curve_is_quadratic_bit))
    .add_macro("CURVE_IS_QUADRATIC_MASK", uint32_t(FASTUIDRAW_MASK(GlyphRenderDataRestrictedRays::curve_is_quadratic_bit, 1u)))
    .add_macro("CURVE_BIT0", uint32_t(GlyphRenderDataRestrictedRays::curve_location_bit0))
    .add_macro("CURVE_NUM_BITS", uint32_t(GlyphRenderDataRestrictedRays::curve_location_numbits));
}

varying_list
ShaderSetCreator::
build_uber_stroke_varyings(uint32_t flags) const
{
  varying_list return_value;

  if (flags & arc_shader)
    {
      return_value
        .add_float_varying("fastuidraw_arc_stroking_relative_to_center_x")
        .add_float_varying("fastuidraw_arc_stroking_relative_to_center_y")
        .add_float_varying("fastuidraw_arc_stroking_arc_radius")
        .add_float_varying("fastuidraw_arc_stroking_stroke_radius");

      if (flags & dashed_shader)
        {
          return_value
            .add_float_varying("fastuidraw_arc_stroking_distance_sub_edge_start")
            .add_float_varying("fastuidraw_arc_stroking_distance_sub_edge_end")
            .add_float_varying("fastuidraw_arc_stroking_distance")
            .add_uint_varying("fastuidraw_arc_stroking_dash_bits");
        }
    }
  else
    {
      return_value
        .add_float_varying("fastuidraw_stroking_on_boundary");

      if (flags & dashed_shader)
        {
          return_value
            .add_float_varying("fastuidraw_stroking_distance")
            .add_float_varying("fastuidraw_stroking_distance_sub_edge_start")
            .add_float_varying("fastuidraw_stroking_distance_sub_edge_end")
            .add_uint_varying("fastuidraw_stroking_dash_bits");
        }
    }

  return return_value;
}

ShaderSource
ShaderSetCreator::
build_uber_stroke_source(uint32_t flags, bool is_vertex_shader) const
{
  ShaderSource return_value;
  const ShaderSource::MacroSet *subpass_constants_ptr;
  const ShaderSource::MacroSet *stroke_constants_ptr;
  c_string extra_macro, src, src_util(nullptr);

  if (m_stroke_tp == PainterStrokeShader::draws_solid_then_fuzz
      || (flags & only_supports_non_aa) != 0)
    {
      extra_macro = "FASTUIDRAW_STROKE_SOLID_THEN_FUZZ";
    }
  else
    {
      extra_macro = "FASTUIDRAW_STROKE_COVER_THEN_DRAW";
    }

  if (flags & dashed_shader)
    {
      return_value.add_macro("FASTUIDRAW_STROKE_DASHED");
    }

  if (flags & only_supports_non_aa)
    {
      return_value.add_macro("FASTUIDRAW_STROKE_ONLY_SUPPORT_NON_AA");
      subpass_constants_ptr = &m_subshader_constants_non_aa_only;
    }
  else
    {
      subpass_constants_ptr = &m_subshader_constants;
    }

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

  return_value
    .add_macro(extra_macro)
    .add_macros(*subpass_constants_ptr)
    .add_macros(*stroke_constants_ptr);
  if (src_util)
    {
      return_value.add_source(src_util, ShaderSource::from_resource);
    }
  return_value
    .add_source(src, ShaderSource::from_resource)
    .remove_macros(*stroke_constants_ptr)
    .remove_macros(*subpass_constants_ptr)
    .remove_macro(extra_macro);

  if (flags & only_supports_non_aa)
    {
      return_value.remove_macro("FASTUIDRAW_STROKE_ONLY_SUPPORT_NON_AA");
    }

  if (flags & dashed_shader)
    {
      return_value.remove_macro("FASTUIDRAW_STROKE_DASHED");
    }

  return return_value;
}

PainterItemShaderGLSL*
ShaderSetCreator::
build_uber_stroke_shader(uint32_t flags, unsigned int num_sub_shaders) const
{
  bool uses_discard;

  uses_discard = (flags & only_supports_non_aa) != 0
    || (m_stroke_tp == PainterStrokeShader::draws_solid_then_fuzz && (flags & (arc_shader | dashed_shader)) != 0);

  return FASTUIDRAWnew PainterItemShaderGLSL(uses_discard,
                                             build_uber_stroke_source(flags, true),
                                             build_uber_stroke_source(flags, false),
                                             build_uber_stroke_varyings(flags),
                                             num_sub_shaders);
}

reference_counted_ptr<PainterItemShader>
ShaderSetCreator::
create_glyph_item_shader(const std::string &vert_src,
                         const std::string &frag_src,
                         const varying_list &varyings,
                         const ShaderSource::MacroSet &frag_macros)
{
  reference_counted_ptr<PainterItemShader> shader;
  shader = FASTUIDRAWnew PainterItemShaderGLSL(false,
                                               ShaderSource()
                                               .add_macros(m_common_glyph_attribute_macros)
                                               .add_source(vert_src.c_str(), ShaderSource::from_resource)
                                               .remove_macros(m_common_glyph_attribute_macros),
                                               ShaderSource()
                                               .add_macros(frag_macros)
                                               .add_source(frag_src.c_str(), ShaderSource::from_resource)
                                               .remove_macros(frag_macros),
                                               varyings);
  return shader;
}

PainterGlyphShader
ShaderSetCreator::
create_glyph_shader(void)
{
  PainterGlyphShader return_value;
  varying_list coverage_varyings, distance_varyings;
  varying_list restricted_rays_varyings;

  distance_varyings
    .add_float_varying("fastuidraw_glyph_coord_x")
    .add_float_varying("fastuidraw_glyph_coord_y")
    .add_float_varying("fastuidraw_glyph_width")
    .add_float_varying("fastuidraw_glyph_height")
    .add_uint_varying("fastuidraw_glyph_data_location");

  coverage_varyings
    .add_float_varying("fastuidraw_glyph_coord_x")
    .add_float_varying("fastuidraw_glyph_coord_y")
    .add_float_varying("fastuidraw_glyph_width")
    .add_float_varying("fastuidraw_glyph_height")
    .add_uint_varying("fastuidraw_glyph_data_location");

  restricted_rays_varyings
    .add_float_varying("fastuidraw_glyph_coord_x")
    .add_float_varying("fastuidraw_glyph_coord_y")
    .add_float_varying("fastuidraw_glyph_max_x")
    .add_float_varying("fastuidraw_glyph_max_y")
    .add_uint_varying("fastuidraw_glyph_data_location");

  return_value
    .shader(coverage_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_coverage.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_coverage.frag.glsl.resource_string",
                                     coverage_varyings, ShaderSource::MacroSet()));

  return_value
    .shader(restricted_rays_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_restricted_rays.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_restricted_rays.frag.glsl.resource_string",
                                     restricted_rays_varyings, m_glyph_restricted_rays_macros));
  return_value
    .shader(distance_field_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string",
                                     distance_varyings, ShaderSource::MacroSet()));

  return return_value;
}

reference_counted_ptr<PainterItemShader>
ShaderSetCreator::
create_stroke_item_shader(bool arc_shader,
                          enum PainterEnums::cap_style stroke_dash_style,
                          enum uber_stroke_render_pass_t render_pass)
{
  reference_counted_ptr<PainterItemShader> shader, return_value;
  uint32_t sub_shader;

  if (m_stroke_tp == PainterStrokeShader::draws_solid_then_fuzz || render_pass != uber_stroke_non_aa)
    {
      if (stroke_dash_style == fastuidraw::PainterEnums::number_cap_styles)
        {
          sub_shader = render_pass << m_stroke_render_pass_bit0;
          shader = (arc_shader) ?
            m_uber_arc_stroke_shader :
            m_uber_stroke_shader;
        }
      else
        {
          sub_shader = (stroke_dash_style << m_stroke_dash_style_bit0)
            | (render_pass << m_stroke_render_pass_bit0);
          shader = (arc_shader) ?
            m_uber_arc_dashed_stroke_shader :
            m_uber_dashed_stroke_shader;
        }
    }
  else
    {
      if (stroke_dash_style == fastuidraw::PainterEnums::number_cap_styles)
        {
          shader = (arc_shader) ?
            m_arc_discard_stroke_shader :
            m_uber_stroke_shader;
          sub_shader = (arc_shader) ? 0u : render_pass << m_stroke_render_pass_bit0;
        }
      else
        {
          shader = (arc_shader) ?
            m_dashed_arc_discard_stroke_shader :
            m_dashed_discard_stroke_shader;
          sub_shader = stroke_dash_style << (m_stroke_dash_style_bit0 - m_stroke_render_pass_num_bits);
        }
    }

  return_value = FASTUIDRAWnew PainterItemShader(sub_shader, shader);
  return return_value;
}

PainterStrokeShader
ShaderSetCreator::
create_stroke_shader(enum PainterEnums::cap_style stroke_style,
                     const reference_counted_ptr<const StrokingDataSelectorBase> &stroke_data_selector)
{
  using namespace fastuidraw::PainterEnums;
  PainterStrokeShader return_value;

  return_value
    .aa_type(m_stroke_tp)
    .stroking_data_selector(stroke_data_selector)
    .aa_action_pass1(m_stroke_action_pass1)
    .aa_action_pass2(m_stroke_action_pass2)
    .aa_shader_pass1(create_stroke_item_shader(false, stroke_style, uber_stroke_aa_pass1))
    .aa_shader_pass2(create_stroke_item_shader(false, stroke_style, uber_stroke_aa_pass2))
    .non_aa_shader(create_stroke_item_shader(false, stroke_style, uber_stroke_non_aa))
    .arc_aa_shader_pass1(create_stroke_item_shader(true, stroke_style, uber_stroke_aa_pass1))
    .arc_aa_shader_pass2(create_stroke_item_shader(true, stroke_style, uber_stroke_aa_pass2))
    .arc_non_aa_shader(create_stroke_item_shader(true, stroke_style, uber_stroke_non_aa));
  return return_value;
}

PainterDashedStrokeShaderSet
ShaderSetCreator::
create_dashed_stroke_shader_set(void)
{
  using namespace fastuidraw::PainterEnums;
  PainterDashedStrokeShaderSet return_value;
  reference_counted_ptr<const StrokingDataSelectorBase> se;

  se = PainterDashedStrokeParams::stroking_data_selector();
  return_value
    .shader(flat_caps, create_stroke_shader(flat_caps, se))
    .shader(rounded_caps, create_stroke_shader(rounded_caps, se))
    .shader(square_caps, create_stroke_shader(square_caps, se));
  return return_value;
}

PainterFillShader
ShaderSetCreator::
create_fill_shader(void)
{
  PainterFillShader fill_shader;

  fill_shader
    .item_shader(FASTUIDRAWnew PainterItemShaderGLSL(false,
                                                     ShaderSource()
                                                     .add_source("fastuidraw_painter_fill.vert.glsl.resource_string",
                                                                 ShaderSource::from_resource),
                                                     ShaderSource()
                                                     .add_source("fastuidraw_painter_fill.frag.glsl.resource_string",
                                                                 ShaderSource::from_resource),
                                                     varying_list()))
    .aa_fuzz_shader(FASTUIDRAWnew PainterItemShaderGLSL(false,
                                                        ShaderSource()
                                                        .add_source("fastuidraw_painter_fill_aa_fuzz.vert.glsl.resource_string",
                                                                    ShaderSource::from_resource),
                                                        ShaderSource()
                                                        .add_source("fastuidraw_painter_fill_aa_fuzz.frag.glsl.resource_string",
                                                                    ShaderSource::from_resource),
                                                        varying_list().add_float_varying("fastuidraw_aa_fuzz")));

  return fill_shader;
}

reference_counted_ptr<PainterBlendShader>
ShaderSetCreator::
create_blend_shader(const std::string &framebuffer_fetch_src_file)
{
  ShaderSource src;

  src.add_source(framebuffer_fetch_src_file.c_str(), ShaderSource::from_resource);
  return FASTUIDRAWnew PainterBlendShaderGLSL(src);
}

PainterBlendShaderSet
ShaderSetCreator::
create_blend_shaders(void)
{
  PainterBlendShaderSet return_value;

  return_value
    .shader(PainterEnums::blend_w3c_normal,
            create_blend_shader("fastuidraw_fbf_w3c_normal.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_multiply,
            create_blend_shader("fastuidraw_fbf_w3c_multiply.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_screen,
            create_blend_shader("fastuidraw_fbf_w3c_screen.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_overlay,
            create_blend_shader("fastuidraw_fbf_w3c_overlay.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_darken,
            create_blend_shader("fastuidraw_fbf_w3c_darken.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_lighten,
            create_blend_shader("fastuidraw_fbf_w3c_lighten.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_color_dodge,
            create_blend_shader("fastuidraw_fbf_w3c_color_dodge.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_color_burn,
            create_blend_shader("fastuidraw_fbf_w3c_color_burn.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_hardlight,
            create_blend_shader("fastuidraw_fbf_w3c_hardlight.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_softlight,
            create_blend_shader("fastuidraw_fbf_w3c_softlight.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_difference,
            create_blend_shader("fastuidraw_fbf_w3c_difference.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_exclusion,
            create_blend_shader("fastuidraw_fbf_w3c_exclusion.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_hue,
            create_blend_shader("fastuidraw_fbf_w3c_hue.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_saturation,
            create_blend_shader("fastuidraw_fbf_w3c_saturation.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_color,
            create_blend_shader("fastuidraw_fbf_w3c_color.glsl.resource_string"))
    .shader(PainterEnums::blend_w3c_luminosity,
            create_blend_shader("fastuidraw_fbf_w3c_luminosity.glsl.resource_string"));

  return return_value;
}

PainterShaderSet
ShaderSetCreator::
create_shader_set(void)
{
  using namespace fastuidraw::PainterEnums;
  PainterShaderSet return_value;
  reference_counted_ptr<const StrokingDataSelectorBase> se;

  se = PainterStrokeParams::stroking_data_selector();

  return_value
    .glyph_shader(create_glyph_shader())
    .stroke_shader(create_stroke_shader(number_cap_styles, se))
    .dashed_stroke_shader(create_dashed_stroke_shader_set())
    .fill_shader(create_fill_shader())
    .composite_shaders(create_composite_shaders())
    .blend_shaders(create_blend_shaders());
  return return_value;
}

enum fastuidraw::PainterCompositeShader::shader_type
shader_composite_type(enum fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_type_t in_value)
{
  switch(in_value)
    {
    case PainterShaderRegistrarGLSL::compositing_single_src:
      return PainterCompositeShader::single_src;

    case PainterShaderRegistrarGLSL::compositing_dual_src:
      return PainterCompositeShader::dual_src;

    case PainterShaderRegistrarGLSL::compositing_framebuffer_fetch:
    case PainterShaderRegistrarGLSL::compositing_interlock:
      return PainterCompositeShader::framebuffer_fetch;
    }

  FASTUIDRAWassert(!"Bad compositing_type_t value");
  return PainterCompositeShader::single_src;
}

}}}
