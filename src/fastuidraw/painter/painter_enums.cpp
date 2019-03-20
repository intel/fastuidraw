/*!
 * \file painter_enums.cpp
 * \brief file painter_enums.cpp
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


#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter.hpp>

enum fastuidraw::PainterEnums::fill_rule_t
fastuidraw::PainterEnums::
complement_fill_rule(enum fill_rule_t f)
{
  switch(f)
    {
    case odd_even_fill_rule:
      return complement_odd_even_fill_rule;

    case complement_odd_even_fill_rule:
      return odd_even_fill_rule;

    case nonzero_fill_rule:
      return complement_nonzero_fill_rule;

    case complement_nonzero_fill_rule:
      return nonzero_fill_rule;

    default:
      return f;
    }
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum screen_orientation v)
{
  static const c_string labels[number_screen_orientation] =
    {
      [y_increases_downwards] = "y_increases_downwards",
      [y_increases_upwards] = "y_increases_upwards",
    };
  return (v < number_screen_orientation) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum rotation_orientation_t v)
{
  static const c_string labels[number_rotation_orientation] =
    {
      [clockwise] = "clockwise",
      [counter_clockwise] = "counter_clockwise",
    };
  return (v < number_rotation_orientation) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum glyph_layout_type v)
{
  static const c_string labels[number_glyph_layout] =
    {
      [glyph_layout_horizontal] = "glyph_layout_horizontal",
      [glyph_layout_vertical] = "glyph_layout_vertical",
    };
  return (v < number_glyph_layout) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum cap_style v)
{
  static const c_string labels[number_cap_styles] =
    {
      [flat_caps] = "flat_caps",
      [rounded_caps] = "rounded_caps",
      [square_caps] = "square_caps",
    };
  return (v < number_cap_styles) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum join_style v)
{
  static const c_string labels[number_join_styles] =
    {
      [no_joins] = "no_joins",
      [rounded_joins] = "rounded_joins",
      [bevel_joins] = "bevel_joins",
      [miter_clip_joins] = "miter_clip_joins",
      [miter_bevel_joins] = "miter_bevel_joins",
      [miter_joins] = "miter_joins",
    };
  return (v < number_join_styles) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum fill_rule_t v)
{
  static const c_string labels[number_fill_rule] =
    {
      [odd_even_fill_rule] = "odd_even_fill_rule",
      [complement_odd_even_fill_rule] = "complement_odd_even_fill_rule",
      [nonzero_fill_rule] = "nonzero_fill_rule",
      [complement_nonzero_fill_rule] = "complement_nonzero_fill_rule",
    };
  return (v < number_fill_rule) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum immediate_coverage_support_t v)
{
  static const c_string labels[number_immediate_coverage_support] =
    {
      [immediate_coverage_no_support] = "immediate_coverage_no_support",
      [immediate_coverage_slow] = "immediate_coverage_slow",
      [immediate_coverage_fast] = "immediate_coverage_fast",
    };
  return (v < number_immediate_coverage_support) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum shader_anti_alias_t v)
{
  static const c_string labels[number_shader_anti_alias] =
    {
      [shader_anti_alias_none] = "shader_anti_alias_none",
      [shader_anti_alias_immediate_coverage] = "shader_anti_alias_immediate_coverage",
      [shader_anti_alias_deferred_coverage] = "shader_anti_alias_deferred_coverage",
      [shader_anti_alias_adaptive] = "shader_anti_alias_adaptive",
      [shader_anti_alias_fastest] = "shader_anti_alias_fastest",
    };
  return (v < number_shader_anti_alias) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum stroking_method_t v)
{
  static const c_string labels[number_stroking_methods] =
    {
      [stroking_method_linear] = "stroking_method_linear",
      [stroking_method_arc] = "stroking_method_arc",
      [stroking_method_fastest] = "stroking_method_fastest",
    };
  return (v < number_stroking_methods) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::PainterEnums::
label(enum composite_mode_t v)
{
  static const c_string labels[number_composite_mode] =
    {
      [composite_porter_duff_clear] = "composite_porter_duff_clear",
      [composite_porter_duff_src] = "composite_porter_duff_src",
      [composite_porter_duff_dst] = "composite_porter_duff_dst",
      [composite_porter_duff_src_over] = "composite_porter_duff_src_over",
      [composite_porter_duff_dst_over] = "composite_porter_duff_dst_over",
      [composite_porter_duff_src_in] = "composite_porter_duff_src_in",
      [composite_porter_duff_dst_in] = "composite_porter_duff_dst_in",
      [composite_porter_duff_src_out] = "composite_porter_duff_src_out",
      [composite_porter_duff_dst_out] = "composite_porter_duff_dst_out",
      [composite_porter_duff_src_atop] = "composite_porter_duff_src_atop",
      [composite_porter_duff_dst_atop] = "composite_porter_duff_dst_atop",
      [composite_porter_duff_xor] = "composite_porter_duff_xor",
      [composite_porter_duff_plus] = "composite_porter_duff_plus",
      [composite_porter_duff_modulate] = "composite_porter_duff_modulate",

      [blend_w3c_multiply] = "blend_w3c_multiply",
      [blend_w3c_screen] = "blend_w3c_screen",
      [blend_w3c_overlay] = "blend_w3c_overlay",
      [blend_w3c_darken] = "blend_w3c_darken",
      [blend_w3c_lighten] = "blend_w3c_lighten",
      [blend_w3c_color_dodge] = "blend_w3c_color_dodge",
      [blend_w3c_color_burn] = "blend_w3c_color_burn",
      [blend_w3c_hardlight] = "blend_w3c_hardlight",
      [blend_w3c_softlight] = "blend_w3c_softlight",
      [blend_w3c_difference] = "blend_w3c_difference",
      [blend_w3c_exclusion] = "blend_w3c_exclusion",
      [blend_w3c_hue] = "blend_w3c_hue",
      [blend_w3c_saturation] = "blend_w3c_saturation",
      [blend_w3c_color] = "blend_w3c_color",
      [blend_w3c_luminosity] = "blend_w3c_luminosity",
    };
  return (v < number_composite_mode) ? labels[v] : "InvalidEnum";
}
