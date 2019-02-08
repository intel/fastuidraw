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
#include <fastuidraw/painter/stroked_point.hpp>
#include <fastuidraw/painter/arc_stroked_point.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include <fastuidraw/text/glyph_attribute.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include <fastuidraw/text/glyph_render_data_banded_rays.hpp>
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
  using namespace fastuidraw;
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

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_src_over,
                       BlendMode().func(BlendMode::ONE, BlendMode::ONE_MINUS_SRC_ALPHA),
                       "fastuidraw_porter_duff_src_over.glsl.resource_string", one_src1,
                       "fastuidraw_fbf_porter_duff_src_over.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_dst_over,
                       BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE),
                       "fastuidraw_porter_duff_dst_over.glsl.resource_string", one_minus_dst_alpha_src1,
                       "fastuidraw_fbf_porter_duff_dst_over.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_clear,
                       BlendMode().func(BlendMode::ZERO, BlendMode::ZERO),
                       "fastuidraw_porter_duff_clear.glsl.resource_string", one_src1,
                       "fastuidraw_fbf_porter_duff_clear.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_src,
                       BlendMode().func(BlendMode::ONE, BlendMode::ZERO),
                       "fastuidraw_porter_duff_src.glsl.resource_string", one_src1,
                       "fastuidraw_fbf_porter_duff_src.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_dst,
                       BlendMode().func(BlendMode::ZERO, BlendMode::ONE),
                       "fastuidraw_porter_duff_dst.glsl.resource_string", one_src1,
                       "fastuidraw_fbf_porter_duff_dst.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_src_in,
                       BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ZERO),
                       "fastuidraw_porter_duff_src_in.glsl.resource_string", dst_alpha_src1,
                       "fastuidraw_fbf_porter_duff_src_in.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_dst_in,
                       BlendMode().func(BlendMode::ZERO, BlendMode::SRC_ALPHA),
                       "fastuidraw_porter_duff_dst_in.glsl.resource_string", one_src1,
                       "fastuidraw_fbf_porter_duff_dst_in.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_src_out,
                       BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ZERO),
                       "fastuidraw_porter_duff_src_out.glsl.resource_string", one_minus_dst_alpha_src1,
                       "fastuidraw_fbf_porter_duff_src_out.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_dst_out,
                       BlendMode().func(BlendMode::ZERO, BlendMode::ONE_MINUS_SRC_ALPHA),
                       "fastuidraw_porter_duff_dst_out.glsl.resource_string", one_src1,
                       "fastuidraw_fbf_porter_duff_dst_out.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_src_atop,
                       BlendMode().func(BlendMode::DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                       "fastuidraw_porter_duff_src_atop.glsl.resource_string", dst_alpha_src1,
                       "fastuidraw_fbf_porter_duff_src_atop.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_dst_atop,
                       BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::SRC_ALPHA),
                       "fastuidraw_porter_duff_dst_atop.glsl.resource_string", one_minus_dst_alpha_src1,
                       "fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_xor,
                       BlendMode().func(BlendMode::ONE_MINUS_DST_ALPHA, BlendMode::ONE_MINUS_SRC_ALPHA),
                       "fastuidraw_porter_duff_xor.glsl.resource_string", one_minus_dst_alpha_src1,
                       "fastuidraw_fbf_porter_duff_xor.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_plus,
                       BlendMode().func(BlendMode::ONE, BlendMode::ONE),
                       "fastuidraw_porter_duff_plus.glsl.resource_string", one_src1,
                       "fastuidraw_fbf_porter_duff_plus.glsl.resource_string");

  add_composite_shader(shaders, PainterEnums::composite_porter_duff_modulate,
                       BlendMode()
                       .func_src_rgb(BlendMode::DST_COLOR)
                       .func_src_alpha(BlendMode::DST_ALPHA)
                       .func_dst(BlendMode::ZERO),
                       "fastuidraw_porter_duff_modulate.glsl.resource_string", dst_alpha_src1,
                       "fastuidraw_fbf_porter_duff_modulate.glsl.resource_string");

  return shaders;
}

///////////////////////////////////////////////
// ShaderSetCreatorStrokingConstants methods
ShaderSetCreatorStrokingConstants::
ShaderSetCreatorStrokingConstants(void)
{
  using namespace fastuidraw;

  m_stroke_render_pass_num_bits = number_bits_required(number_render_passes);
  m_stroke_dash_style_num_bits = number_bits_required(PainterEnums::number_cap_styles);
  FASTUIDRAWassert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_render_pass_num_bits) >= number_render_passes);
  FASTUIDRAWassert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_dash_style_num_bits) >= PainterEnums::number_cap_styles);
  FASTUIDRAWassert(m_stroke_render_pass_num_bits + m_stroke_dash_style_num_bits + 2u <= 32u);

  m_stroke_render_pass_bit0 = 0;
  m_stroke_dash_style_bit0 = m_stroke_render_pass_bit0 + m_stroke_render_pass_num_bits;
  m_stroke_aa_method_bit = m_stroke_dash_style_bit0 + m_stroke_dash_style_num_bits;

  create_macro_set(m_subshader_constants);

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
create_macro_set(ShaderSource::MacroSet &dst) const
{
  dst
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_bit0", uint32_t(m_stroke_render_pass_bit0))
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits", uint32_t(m_stroke_render_pass_num_bits))
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_bit0", uint32_t(m_stroke_dash_style_bit0))
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_num_bits", uint32_t(m_stroke_dash_style_num_bits))
    .add_macro("fastuidraw_stroke_sub_shader_aa_method_bit0", uint32_t(m_stroke_aa_method_bit))
    .add_macro("fastuidraw_stroke_dashed_flat_caps", uint32_t(PainterEnums::flat_caps))
    .add_macro("fastuidraw_stroke_dashed_rounded_caps", uint32_t(PainterEnums::rounded_caps))
    .add_macro("fastuidraw_stroke_dashed_square_caps", uint32_t(PainterEnums::square_caps))
    .add_macro("fastuidraw_stroke_not_dashed", uint32_t(PainterEnums::number_cap_styles))
    .add_macro("fastuidraw_stroke_aa_pass1", uint32_t(render_aa_pass1))
    .add_macro("fastuidraw_stroke_aa_pass2", uint32_t(render_aa_pass2))
    .add_macro("fastuidraw_stroke_non_aa", uint32_t(render_non_aa_pass))
    .add_macro("fastuidraw_stroke_solid_then_fuzz", uint32_t(0u))
    .add_macro("fastuidraw_stroke_cover_then_draw", uint32_t(1u));
}

uint32_t
ShaderSetCreatorStrokingConstants::
compute_sub_shader(bool is_hq,
                   enum PainterEnums::cap_style dash_style,
                   enum render_pass_t render_pass)
{
  uint32_t return_value(0u);

  FASTUIDRAWassert(!is_hq || render_pass != render_non_aa_pass);

  return_value |= pack_bits(m_stroke_dash_style_bit0,
                            m_stroke_dash_style_num_bits,
                            dash_style);

  return_value |= pack_bits(m_stroke_render_pass_bit0,
                            m_stroke_render_pass_num_bits,
                            render_pass);

  return_value |= pack_bits(m_stroke_aa_method_bit, 1u,
                            is_hq ? 1u : 0u);

  return return_value;
}

/////////////////////////////////
// StrokeShaderCreator methods
StrokeShaderCreator::
StrokeShaderCreator(void)
{
  unsigned int num_sub_shaders;

  num_sub_shaders = 1u << (m_stroke_render_pass_num_bits + m_stroke_dash_style_num_bits + 1u);

  m_shaders[0] = build_uber_stroke_shader(0u, num_sub_shaders);
  m_shaders[discard_shader] = build_uber_stroke_shader(discard_shader, num_sub_shaders);
  FASTUIDRAWassert(!m_shaders[0]->uses_discard());
  FASTUIDRAWassert(m_shaders[discard_shader]->uses_discard());

  m_shaders[arc_shader] = build_uber_stroke_shader(arc_shader, num_sub_shaders);
  m_shaders[arc_shader | discard_shader] = build_uber_stroke_shader(arc_shader | discard_shader, num_sub_shaders);
  FASTUIDRAWassert(!m_shaders[arc_shader]->uses_discard());
  FASTUIDRAWassert(m_shaders[arc_shader | discard_shader]->uses_discard());
}

reference_counted_ptr<PainterItemShader>
StrokeShaderCreator::
create_stroke_item_shader(enum PainterEnums::cap_style stroke_dash_style,
                          enum PainterStrokeShader::stroke_type_t tp,
                          enum PainterStrokeShader::shader_type_t pass)
{
  reference_counted_ptr<PainterItemShader> shader, return_value;
  uint32_t shader_choice(0u), sub_shader(0u);
  enum render_pass_t render_pass;
  bool is_hq_shader;

  if (tp == PainterStrokeShader::arc_stroke_type)
    {
      shader_choice |= arc_shader;
    }

  switch (pass)
    {
    default:
      FASTUIDRAWassert(!"Bad pass value!!");
      //fall through

    case PainterStrokeShader::non_aa_shader:
      render_pass = render_non_aa_pass;
      is_hq_shader = false;
      break;

    case PainterStrokeShader::aa_shader_pass1:
      render_pass = render_aa_pass1;
      is_hq_shader = false;
      break;

    case PainterStrokeShader::aa_shader_pass2:
      render_pass = render_aa_pass2;
      is_hq_shader = false;
      break;

    case PainterStrokeShader::hq_aa_shader_pass1:
      render_pass = render_aa_pass1;
      is_hq_shader = true;
      break;

    case PainterStrokeShader::hq_aa_shader_pass2:
      render_pass = render_aa_pass2;
      is_hq_shader = true;
      break;
    }

  if (!is_hq_shader &&
      (tp == PainterStrokeShader::arc_stroke_type || stroke_dash_style != fastuidraw::PainterEnums::number_cap_styles))
    {
      shader_choice |= discard_shader;
    }

  sub_shader = compute_sub_shader(is_hq_shader, stroke_dash_style, render_pass);
  return_value = FASTUIDRAWnew PainterItemShader(sub_shader, m_shaders[shader_choice]);
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
        .add_float_varying("fastuidraw_arc_stroking_relative_to_center_x")
        .add_float_varying("fastuidraw_arc_stroking_relative_to_center_y")
        .add_float_varying("fastuidraw_arc_stroking_arc_radius")
        .add_float_varying("fastuidraw_arc_stroking_stroke_radius")
        .add_float_varying("fastuidraw_arc_stroking_distance_sub_edge_start")
        .add_float_varying("fastuidraw_arc_stroking_distance_sub_edge_end")
        .add_float_varying("fastuidraw_arc_stroking_distance")
        .add_uint_varying("fastuidraw_arc_stroking_dash_bits");
    }
  else
    {
      return_value
        .add_float_varying("fastuidraw_stroking_on_boundary")
        .add_float_varying("fastuidraw_stroking_distance")
        .add_float_varying("fastuidraw_stroking_distance_sub_edge_start")
        .add_float_varying("fastuidraw_stroking_distance_sub_edge_end")
        .add_uint_varying("fastuidraw_stroking_dash_bits");
    }

  return return_value;
}

ShaderSource
StrokeShaderCreator::
build_uber_stroke_source(uint32_t flags, bool is_vertex_shader) const
{
  ShaderSource return_value;
  const ShaderSource::MacroSet *stroke_constants_ptr;
  c_string src, src_util(nullptr);

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
    .add_macros(m_subshader_constants)
    .add_macros(*stroke_constants_ptr);

  if (src_util)
    {
      return_value.add_source(src_util, ShaderSource::from_resource);
    }

  return_value
    .add_source(src, ShaderSource::from_resource)
    .remove_macros(*stroke_constants_ptr)
    .remove_macros(m_subshader_constants);

  return return_value;
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
ShaderSetCreator(bool has_auxiliary_coverage_buffer,
                 enum PainterCompositeShader::shader_type composite_tp,
                 const reference_counted_ptr<const PainterDraw::Action> &flush_auxiliary_buffer_between_draws):
  CompositeShaderSetCreator(composite_tp),
  StrokeShaderCreator(),
  m_has_auxiliary_coverage_buffer(has_auxiliary_coverage_buffer),
  m_flush_auxiliary_buffer_between_draws(flush_auxiliary_buffer_between_draws)
{
  if (!m_has_auxiliary_coverage_buffer)
    {
      m_hq_support = PainterEnums::hq_anti_alias_no_support;
    }
  else if (m_flush_auxiliary_buffer_between_draws)
    {
      m_hq_support = PainterEnums::hq_anti_alias_slow;
    }
  else
    {
      m_hq_support = PainterEnums::hq_anti_alias_fast;
    }

  m_fill_macros
    .add_macro("fastuidraw_aa_fuzz_type_on_path", uint32_t(FilledPath::Subset::aa_fuzz_type_on_path))
    .add_macro("fastuidraw_aa_fuzz_type_on_boundary", uint32_t(FilledPath::Subset::aa_fuzz_type_on_boundary))
    .add_macro("fastuidraw_aa_fuzz_type_on_boundary_miter", uint32_t(FilledPath::Subset::aa_fuzz_type_on_boundary_miter))
    .add_macro("fastuidraw_aa_fuzz_direct_pass", uint32_t(fill_aa_fuzz_direct_pass))
    .add_macro("fastuidraw_aa_fuzz_hq_pass1", uint32_t(fill_aa_fuzz_hq_pass1))
    .add_macro("fastuidraw_aa_fuzz_hq_pass2", uint32_t(fill_aa_fuzz_hq_pass2));

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
    .add_uint_varying("fastuidraw_glyph_data_location");

  banded_rays_varyings
    .add_float_varying("fastuidraw_glyph_coord_x")
    .add_float_varying("fastuidraw_glyph_coord_y")
    .add_uint_varying("fastuidraw_glyph_data_num_vertical_bands")
    .add_uint_varying("fastuidraw_glyph_data_num_horizontal_bands")
    .add_uint_varying("fastuidraw_glyph_data_location");

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
    .hq_anti_alias_support(m_hq_support)
    .stroking_data_selector(stroke_data_selector)
    .hq_aa_action_pass1(m_flush_auxiliary_buffer_between_draws)
    .hq_aa_action_pass2(m_flush_auxiliary_buffer_between_draws)
    .arc_stroking_is_fast(PainterEnums::shader_anti_alias_none, false) //because of discard
    .arc_stroking_is_fast(PainterEnums::shader_anti_alias_simple, false) //because of discard
    .arc_stroking_is_fast(PainterEnums::shader_anti_alias_high_quality,
                          m_hq_support == PainterEnums::hq_anti_alias_fast)
    .arc_stroking_is_fast(PainterEnums::shader_anti_alias_auto,
                          m_hq_support == PainterEnums::hq_anti_alias_fast)
    .arc_stroking_is_fast(PainterEnums::shader_anti_alias_fastest,
                          m_hq_support == PainterEnums::hq_anti_alias_fast
                          && cap_style != PainterEnums::number_cap_styles);

  for (unsigned int tp = 0; tp < PainterStrokeShader::number_stroke_types; ++tp)
    {
      enum PainterStrokeShader::stroke_type_t e_tp;

      e_tp = static_cast<enum PainterStrokeShader::stroke_type_t>(tp);

      /* If hq is fast (i.e. no actions to call), then it will be
       * faster than the simple whenever the simple would do discard;
       * simple does discard on arc-stroking and dashed-stroking.
       */
      if (m_hq_support == PainterEnums::hq_anti_alias_fast &&
          (e_tp == PainterStrokeShader::arc_stroke_type
           || cap_style != PainterEnums::number_cap_styles))
        {
          return_value.fastest_anti_alias_mode(e_tp, PainterEnums::shader_anti_alias_high_quality);
        }
      else
        {
          return_value.fastest_anti_alias_mode(e_tp, PainterEnums::shader_anti_alias_simple);
        }

      for (unsigned int sh = 0; sh < PainterStrokeShader::number_shader_types; ++sh)
        {
          enum PainterStrokeShader::shader_type_t e_sh;
          bool is_hq_pass;

          e_sh = static_cast<enum PainterStrokeShader::shader_type_t>(sh);
          is_hq_pass = (e_sh == PainterStrokeShader::hq_aa_shader_pass1
                        || e_sh == PainterStrokeShader::hq_aa_shader_pass2);

          if (!is_hq_pass || m_has_auxiliary_coverage_buffer)
            {
              return_value.shader(e_tp, e_sh, create_stroke_item_shader(cap_style, e_tp, e_sh));
            }
        }
    }

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
  reference_counted_ptr<PainterItemShader> aa_fuzz_direct_shader;

  item_shader = FASTUIDRAWnew PainterItemShaderGLSL(false,
                                                    ShaderSource()
                                                    .add_source("fastuidraw_painter_fill.vert.glsl.resource_string",
                                                                ShaderSource::from_resource),
                                                    ShaderSource()
                                                    .add_source("fastuidraw_painter_fill.frag.glsl.resource_string",
                                                                ShaderSource::from_resource),
                                                    varying_list());

  uber_fuzz_shader = FASTUIDRAWnew PainterItemShaderGLSL(false,
                                                         ShaderSource()
                                                         .add_macros(m_fill_macros)
                                                         .add_source("fastuidraw_painter_fill_aa_fuzz.vert.glsl.resource_string",
                                                                     ShaderSource::from_resource)
                                                         .remove_macros(m_fill_macros),
                                                         ShaderSource()
                                                         .add_macros(m_fill_macros)
                                                         .add_source("fastuidraw_painter_fill_aa_fuzz.frag.glsl.resource_string",
                                                                     ShaderSource::from_resource)
                                                         .remove_macros(m_fill_macros),
                                                         varying_list().add_float_varying("fastuidraw_aa_fuzz"),
                                                         fill_aa_fuzz_number_passes);

  aa_fuzz_direct_shader = FASTUIDRAWnew PainterItemShader(fill_aa_fuzz_direct_pass, uber_fuzz_shader);
  fill_shader
    .hq_anti_alias_support(m_hq_support)
    .fastest_anti_alias_mode(PainterEnums::shader_anti_alias_simple)
    .item_shader(item_shader)
    .aa_fuzz_shader(aa_fuzz_direct_shader);

  if (m_has_auxiliary_coverage_buffer)
    {
      reference_counted_ptr<PainterItemShader> hq1, hq2;

      hq1 = FASTUIDRAWnew PainterItemShader(fill_aa_fuzz_hq_pass1, uber_fuzz_shader);
      hq2 = FASTUIDRAWnew PainterItemShader(fill_aa_fuzz_hq_pass2, uber_fuzz_shader);

      fill_shader
        .aa_fuzz_hq_shader_pass1(hq1)
        .aa_fuzz_hq_shader_pass2(hq2)
        .aa_fuzz_hq_action_pass1(m_flush_auxiliary_buffer_between_draws)
        .aa_fuzz_hq_action_pass2(m_flush_auxiliary_buffer_between_draws);
    }

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
  using namespace fastuidraw;
  PainterShaderSet return_value;
  reference_counted_ptr<const StrokingDataSelectorBase> se;

  se = PainterStrokeParams::stroking_data_selector(false);

  return_value
    .glyph_shader(create_glyph_shader())
    .stroke_shader(create_stroke_shader(PainterEnums::number_cap_styles, se))
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
