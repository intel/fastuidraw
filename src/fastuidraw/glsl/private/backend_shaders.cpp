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
#include <fastuidraw/painter/arc_stroked_point.hpp>
#include "backend_shaders.hpp"

namespace fastuidraw { namespace glsl { namespace detail {

/////////////////////////////////////
// BlendShaderSetCreator methods
BlendShaderSetCreator::
BlendShaderSetCreator(enum PainterBlendShader::shader_type tp):
  m_type(tp)
{
  if (m_type == PainterBlendShader::single_src)
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
      FASTUIDRAWassert(!"Bad m_type");
    }
}

PainterBlendShaderSet
BlendShaderSetCreator::
create_blend_shaders(void)
{
  using namespace fastuidraw::PainterEnums;
  /* try to use as few blend modes as possible so that
   *  we have fewer draw call breaks. The convention is as
   *  follows:
   *  - src0 is GL_ONE and the GLSL code handles the multiply
   *  - src1 is computed by the GLSL code as needed
   *  This is fine for those modes that do not need DST values
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
                   "fastuidraw_fbf_porter_duff_xor.glsl.resource_string");

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
  FASTUIDRAWassert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_render_pass_num_bits) >= uber_number_passes);
  FASTUIDRAWassert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_dash_style_num_bits) >= number_cap_styles);
  FASTUIDRAWassert(m_stroke_render_pass_num_bits + m_stroke_dash_style_num_bits + 1u <= 32u);

  m_stroke_render_pass_bit0 = 0;
  m_stroke_dash_style_bit0 = m_stroke_render_pass_bit0 + m_stroke_render_pass_num_bits;
}

ShaderSource&
ShaderSetCreatorConstants::
add_constants(ShaderSource &src, bool render_pass_varies) const
{
  unsigned int stroke_dash_style_bit0(m_stroke_dash_style_bit0);
  unsigned int stroke_render_pass_num_bits(m_stroke_render_pass_num_bits);

  if (!render_pass_varies)
    {
      stroke_dash_style_bit0 -= m_stroke_render_pass_num_bits;
      stroke_render_pass_num_bits -= m_stroke_render_pass_num_bits;
    }

  src
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_bit0", m_stroke_render_pass_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits", stroke_render_pass_num_bits)
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_bit0", stroke_dash_style_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_num_bits", m_stroke_dash_style_num_bits)
    .add_macro("fastuidraw_stroke_aa_pass1", uber_stroke_aa_pass1)
    .add_macro("fastuidraw_stroke_aa_pass2", uber_stroke_aa_pass2)
    .add_macro("fastuidraw_stroke_non_aa", uber_stroke_non_aa);

  return src;
}

ShaderSource&
ShaderSetCreatorConstants::
remove_constants(ShaderSource &src) const
{
  src
    .remove_macro("fastuidraw_stroke_sub_shader_width_pixels_bit0")
    .remove_macro("fastuidraw_stroke_sub_shader_width_pixels_num_bits")
    .remove_macro("fastuidraw_stroke_sub_shader_render_pass_bit0")
    .remove_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits")
    .remove_macro("fastuidraw_stroke_sub_shader_dash_style_bit0")
    .remove_macro("fastuidraw_stroke_sub_shader_dash_style_num_bits")
    .remove_macro("fastuidraw_stroke_aa_pass1")
    .remove_macro("fastuidraw_stroke_aa_pass2")
    .remove_macro("fastuidraw_stroke_non_aa");

  return src;
}

//////////////////////////////////////////
//  ShaderSetCreator methods
ShaderSetCreator::
ShaderSetCreator(enum PainterBlendShader::shader_type blend_tp,
                 enum PainterStrokeShader::type_t stroke_tp,
                 const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass1,
                 const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass2):
  BlendShaderSetCreator(blend_tp),
  m_stroke_tp(stroke_tp),
  m_stroke_action_pass1(stroke_action_pass1),
  m_stroke_action_pass2(stroke_action_pass2)
{
  unsigned int num_undashed_sub_shaders, num_dashed_sub_shaders;

  // prepare the uber-stroke shader builds.
  add_constants(m_add_constants, true);
  add_constants(m_add_constants_non_aa_only, false);
  remove_constants(m_remove_constants);

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

      /* we also need a shaders for non-aa arc-shading as they will do discard too */
    }
}

varying_list
ShaderSetCreator::
build_uber_stroke_varyings(uint32_t flags) const
{
  varying_list return_value;

  if (flags & arc_shader)
    {
      return_value
        .add_float_varying("fastuidraw_arc_stroke_relative_to_center_x")
        .add_float_varying("fastuidraw_arc_stroke_relative_to_center_y")
        .add_float_varying("fastuidraw_arc_stroke_arc_radius")
        .add_float_varying("fastuidraw_arc_stroke_stroke_radius");

      if (flags & dashed_shader)
        {
          return_value
            .add_float_varying("fastuidraw_arc_stroke_arc_angle")
            .add_float_varying("fastuidraw_stroking_distance")
            .add_float_varying("fastuidraw_stroking_distance_sub_edge_start")
            .add_float_varying("fastuidraw_stroking_distance_sub_edge_end");
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
build_uber_stroke_source(uint32_t flags, fastuidraw::c_string src) const
{
  ShaderSource return_value;
  const ShaderSource *add_constants_ptr;
  c_string extra_macro;

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
      add_constants_ptr = &m_add_constants_non_aa_only;
    }
  else
    {
      add_constants_ptr = &m_add_constants;
    }

  return_value
    .add_macro(extra_macro)
    .add_source(*add_constants_ptr)
    .add_source(src, ShaderSource::from_resource)
    .add_source(m_remove_constants)
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

PainterItemShader*
ShaderSetCreator::
build_uber_stroke_shader(uint32_t flags, unsigned int num_sub_shaders) const
{
  c_string vert, frag;
  bool uses_discard;

  if (flags & arc_shader)
    {
      vert = "fastuidraw_painter_arc_stroke.vert.glsl.resource_string";
      frag = "fastuidraw_painter_arc_stroke.frag.glsl.resource_string";
    }
  else
    {
      vert = "fastuidraw_painter_stroke.vert.glsl.resource_string";
      frag = "fastuidraw_painter_stroke.frag.glsl.resource_string";
    }

  uses_discard = (flags & only_supports_non_aa) != 0
    || (m_stroke_tp == PainterStrokeShader::draws_solid_then_fuzz && (flags & (arc_shader | dashed_shader)) != 0);

  return FASTUIDRAWnew PainterItemShaderGLSL(uses_discard,
                                             build_uber_stroke_source(flags, vert),
                                             build_uber_stroke_source(flags, frag),
                                             build_uber_stroke_varyings(flags),
                                             num_sub_shaders);
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

  if (anisotropic)
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
create_stroke_item_shader(bool arc_shader,
                          enum PainterEnums::cap_style stroke_dash_style,
                          enum uber_stroke_render_pass_t render_pass)
{
  reference_counted_ptr<PainterItemShader> shader;
  uint32_t sub_shader;

  if (stroke_dash_style == fastuidraw::PainterEnums::number_cap_styles)
    {
      sub_shader = (render_pass << m_stroke_render_pass_bit0);
      shader = FASTUIDRAWnew PainterItemShader(sub_shader,
                                               (arc_shader) ?
                                               m_uber_arc_stroke_shader :
                                               m_uber_stroke_shader);
    }
  else
    {
      if (!arc_shader && render_pass == uber_stroke_non_aa && m_dashed_discard_stroke_shader)
        {
          sub_shader = (stroke_dash_style << (m_stroke_dash_style_bit0 - m_stroke_render_pass_num_bits));
          shader = FASTUIDRAWnew PainterItemShader(sub_shader, m_dashed_discard_stroke_shader);
        }
      else
        {
          sub_shader = (stroke_dash_style << m_stroke_dash_style_bit0)
            | (render_pass << m_stroke_render_pass_bit0);
          shader = FASTUIDRAWnew PainterItemShader(sub_shader,
                                                   (arc_shader) ?
                                                   m_uber_arc_dashed_stroke_shader :
                                                   m_uber_dashed_stroke_shader);
        }
    }
  return shader;
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

PainterShaderSet
ShaderSetCreator::
create_shader_set(void)
{
  using namespace fastuidraw::PainterEnums;
  PainterShaderSet return_value;
  reference_counted_ptr<const StrokingDataSelectorBase> se;

  se = PainterStrokeParams::stroking_data_selector();

  return_value
    .glyph_shader(create_glyph_shader(false))
    .glyph_shader_anisotropic(create_glyph_shader(true))
    .stroke_shader(create_stroke_shader(number_cap_styles, se))
    .dashed_stroke_shader(create_dashed_stroke_shader_set())
    .fill_shader(create_fill_shader())
    .blend_shaders(create_blend_shaders());
  return return_value;
}

}}}
