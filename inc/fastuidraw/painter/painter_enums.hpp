/*!
 * \file painter_enums.hpp
 * \brief file painter_enums.hpp
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

#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief Class to encapsulate enumerations used in Painter
   *        interface, part of the main library libFastUIDraw.
   */
  class PainterEnums
  {
  public:
    /*!
     * \brief
     * Enumeration to indicate in what direction the
     * y-coordinate increases
     */
    enum screen_orientation
      {
        y_increases_downwards, /*!< y-coordinate increases downwards */
        y_increases_upwards, /*!< y-coordinate increases upwards */

        number_screen_orientation,
      };

    /*!
     * \brief
     * Enumeration to specify orientation of a rotation
     */
    enum rotation_orientation_t
      {
        clockwise, /*!< indicates clockwise */
        counter_clockwise, /*!< indicates counter-clockwise */

        number_rotation_orientation
      };

    /*!
     * \brief
     * Enumeration to indicate if glyph layout is horizontal
     * or vertical
     */
    enum glyph_layout_type
      {
        /*!
         * Glyphs are layed out horizontally, thus will use
         * \ref GlyphMetrics::horizontal_layout_offset()
         * to offset the glyphs.
         */
        glyph_layout_horizontal,

        /*!
         * Glyphs are layed out vertically, thus will use
         * \ref GlyphMetrics::vertical_layout_offset()
         * to offset the glyphs.
         */
        glyph_layout_vertical,

        number_glyph_layout
      };

    /*!
     * \brief
     * Enumeration specifying if and how to draw caps when stroking.
     */
    enum cap_style
      {
        flat_caps,      /*!< indicates to have flat (i.e. no) caps when stroking */
        rounded_caps,   /*!< indicates to have rounded caps when stroking */
        square_caps,    /*!< indicates to have square caps when stroking */

        number_cap_styles /*!< number of cap styles */
      };

    /*!
     * \brief
     * Enumeration specifying if and how to draw joins when stroking
     */
    enum join_style
      {
        /*!
         * indicates to stroke without joins
         */
        no_joins,

        /*!
         * indicates to stroke with rounded joins
         */
        rounded_joins,

        /*!
         * indicates to stroke with bevel joins
         */
        bevel_joins,

        /*!
         * indicates to stroke with miter joins where if miter distance
         * is exceeded then the miter join is clipped to the miter
         * distance.
         */
        miter_clip_joins,

        /*!
         * indicates to stroke with miter joins where if miter distance
         * is exceeded then the miter join is drawn as a bevel join.
         */
        miter_bevel_joins,

        /*!
         * indicates to stroke with miter joins where if miter distance
         * is exceeded then the miter-tip is truncated to the miter
         * distance.
         */
        miter_joins,

        number_join_styles, /*!< number of join styles */
      };

    /*!
     * \brief
     * Enumerations specifying common fill rules.
     */
    enum fill_rule_t
      {
        odd_even_fill_rule, /*!< indicates to use odd-even fill rule */
        complement_odd_even_fill_rule, /*!< indicates to give the complement of the odd-even fill rule */
        nonzero_fill_rule, /*!< indicates to use the non-zero fill rule */
        complement_nonzero_fill_rule, /*!< indicates to give the complement of the non-zero fill rule */

        number_fill_rule /*!< count of enums */
      };

    /*!
     * Enumeration to describe immediate coverage buffer
     * support.
     */
    enum immediate_coverage_support_t
      {
        /*!
         * Indicates that immediate coverage buffer is
         * NOT supported.
         */
        immediate_coverage_no_support,

        /*!
         * Indicates that immediate coverage buffer is
         * supported with performance impact (typically
         * of the form of draw breaks between passes
         * to issue a memory barrier command).
         */
        immediate_coverage_slow,

        /*!
         * Indicates that immediate coverage buffer is
         * supported with no or minimal performance impact
         * (i.e. no draw breaks).
         */
        immediate_coverage_fast,

        number_immediate_coverage_support
      };

    /*!
     * Enumeration to specify the anti-aliasing quality
     * to apply when performing path fills or strokes.
     */
    enum shader_anti_alias_t
      {
        /*!
         * Do not apply any shader based anti-aliasing
         * to the fill.
         */
        shader_anti_alias_none,

        /*!
         * Appliesanti-aliasing shading. The shader works
         * in two passes: the first pass renders to the
         * IMMEDIATE coverage buffer and the 2nd pass reads
         * from it and uses that as the coverage value.
         */
        shader_anti_alias_immediate_coverage,

        /*!
         * Applies anti-aliasing shading. The shader works
         * in two passes: the first pass renders to the
         * DEFERRED coverage buffer and the 2nd pass reads
         * from it and uses that as the coverage value.
         */
        shader_anti_alias_deferred_coverage,

        /*!
         * Represents to use either \ref shader_anti_alias_immediate_coverage
         * or \ref shader_anti_alias_deferred_coverage decided from
         * which modes are supported and the size of deferred coverage
         * buffer needed.
         */
        shader_anti_alias_adaptive,

        /* make the modes that indicate for Painter to choose
         * to come after the modes that precisely specify a
         * choice.
         */

        /*!
         * Represents to use the fastest anti-alias mode
         * (which is not shader_anti_alias_none).
         */
        shader_anti_alias_fastest,

        /*!
         * Number of shader-anti-alias enums present.
         */
        number_shader_anti_alias,
      };

    /*!
     * Enumeration to specify how to stroke
     */
    enum stroking_method_t
      {
        /*!
         * Use linear stroking taken directly from the
         * Path. Thus the passed \ref StrokedPath only
         * consists of line segments.
         */
        stroking_method_linear,

        /*!
         * Use arc-stroking, i.e. the passed \ref
         * StrokedPath has both arc-segments and
         * line segments. This results in fewer
         * vertices with the fragment shader
         * computing per-pixel coverage.
         */
        stroking_method_arc,

        /*!
         *
         */
        stroking_method_number_precise_choices,

        /* make the modes that indicate to choose to come
         * after the modes that precisely specify a value.
         */

        /*!
         * Choose for optimal performance.
         */
        stroking_method_fastest = stroking_method_number_precise_choices,

        /*!
         * Number of stroking enums present.
         */
        number_stroking_methods,
      };

    /*!
     * \brief
     * Enumeration specifying composite modes. The following function-formulas
     * are used in a number of the blend modes:
     * \code
     * UndoAlpha(C.rgba) = (0, 0, 0) if Ca = 0
     *                     C.rgb / C.a otherwise
     * MinColorChannel(C.rgb) = min(C.r, C.g, C.b)
     * MaxColorChannel(C.rgb) = max(C.r, C.g, C.b)
     * Luminosity(C.rgb) = dot(C.rgb, vec3(0.30, 0.59, 0.11))
     * Saturation(C.rgb) = MaxColorChannel(C.rgb) - MinColorChannel(C.rgb)
     * \endcode
     * The next set of functions are a little messier and written in GLSL
     * \code
     * vec3 ClipColor(in vec3 C)
     * {
     *    float L = Luminosity(C);
     *    float MinC = MinColorChannel(C);
     *    float MaxC = MaxColorChannel(C);
     *    if (MinC < 0.0)
     *       C = vec3(L) + (C - vec3(L)) * (L / (L - MinC));
     *    if (MaxC > 1.0)
     *       C = vec3(L) + (C - vec3(L)) * ((1 - L) / (MaxC - L));
     *    return C;
     * }
     *
     * vec3 OverrideLuminosity(vec3 C, vec3 L)
     * {
     *    float Clum = Luminosity(C);
     *    float Llum = Luminosity(L);
     *    float Delta = Llum - Clum;
     *    return ClipColor(C + vec3(Delta));
     * }
     *
     * vec3 OverrideLuminosityAndSaturation(vec3 C, vec3 S, vec3 L)
     * {
     *    float Cmin = MinColorChannel(C);
     *    float Csat = Saturation(C);
     *    float Ssat = Saturation(S);
     *    if (Csat > 0.0)
     *      {
     *         C = (C - Cmin) * Ssat / Csat;
     *      }
     *    else
     *      {
     *         C = vec3(0.0);
     *      }
     *    return OverrideLuminosity(C, L);
     * }
     * \endcode
     */
    enum composite_mode_t
      {
        /*!
         * Porter-Duff clear mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where
         * F.rgba = (0, 0, 0, 0).
         */
        composite_porter_duff_clear,

        /*!
         * Porter-Duff src mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F = S.
         */
        composite_porter_duff_src,

        /*!
         * Porter-Duff dst mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F = D.
         */
        composite_porter_duff_dst,

        /*!
         * Porter-Duff src-over mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = S.rgb + D.rgb * (1 - S.a)
         * \endcode
         */
        composite_porter_duff_src_over,

        /*!
         * Porter-Duff dst-over mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = D.a + S.a * (1 - D.a)
         * F.rgb = D.rgb + S.rgb * (1 - D.a)
         * \endcode
         */
        composite_porter_duff_dst_over,

        /*!
         * Porter-Duff src-in mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a * D.a
         * F.rgb = S.rgb * D.a
         * \endcode
         */
        composite_porter_duff_src_in,

        /*!
         * Porter-Duff dst-in mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is
         * \code
         * F.a = S.a * D.a
         * F.rgb = D.rgb * S.a
         * \endcode
         */
        composite_porter_duff_dst_in,

        /*!
         * Porter-Duff  mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a * (1 - D.a)
         * F.rgb =  S.rgb * (1 - D.a)
         * \endcode
         */
        composite_porter_duff_src_out,

        /*!
         * Porter-Duff src-out mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = D.a * (1.0 - S.a)
         * F.rgb = D.rgb * (1.0 - S.a)
         * \endcode
         */
        composite_porter_duff_dst_out,

        /*!
         * Porter-Duff src-atop mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = D.a
         * F.rgb = S.rgb * D.a + D.rgb * (1.0 - S.a)
         * \endcode
         */
        composite_porter_duff_src_atop,

        /*!
         * Porter-Duff dst-atop mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a
         * F.rgb = D.rgb * S.a + S.rgb * (1 - D.a)
         * \endcode
         */
        composite_porter_duff_dst_atop,

        /*!
         * Porter-Duff xor mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a * (1 - D.a) + D.a * (1 - S.a)
         * F.rgb = S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         */
        composite_porter_duff_xor,

        /*!
         * Plus blend mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a
         * F.rgb = S.rgb + D.rgb
         * \endcode
         */
        composite_porter_duff_plus,

        /*!
         * Modulate blend mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a * D.a
         * F.rgb = S.rgb * D.rgb
         * \endcode
         */
        composite_porter_duff_modulate,

        /*!
         * Screen mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c = S.c + D.c - S.c * D.c
         * \endcode
         */
        blend_w3c_screen,

        /*!
         * Overlay mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c =
         *           2 * S * D, if D <= 0.5
         *           1 - 2 * (1 - S) * (1 - D), otherwise
         * \endcode
         */
        blend_w3c_overlay,

        /*!
         * Darken mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c = min(S, D)
         * \endcode
         */
        blend_w3c_darken,

        /*!
         * Lighten mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c = max(S.c, D.c)
         * \endcode
         */
        blend_w3c_lighten,

        /*!
         * Color dodge mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c =
         *           0, if D.c <= 0
         *           min(1, D.c / (1 - S.c)), if D.c > 0 and S.c < 1
         *           1, if D.c > 0 and S.c >= 1
         * \endcode
         */
        blend_w3c_color_dodge,

        /*!
         * Color burn mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c =
         *           1, if D.c >= 1
         *           1 - min(1, (1 - D.c) / S.c), if D.c < 1 and S.c > 0
         *           0, if D.c < 1 and S.c <= 0
         * \endcode
         */
        blend_w3c_color_burn,

        /*!
         * Harlight mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c = 2 * S.c * D.c, if S.c <= 0.5
         *           1 - 2 * (1 - S.c) * (1 - D.c), otherwise
         * \endcode
         */
        blend_w3c_hardlight,

        /*!
         * Softlight mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c =
         *          D.c - (1 - 2 * S.c) * D.c * (1 - D.c), if S.c <= 0.5
         *          D.c + (2 * S.c - 1) * D.c * ((16 * D.c - 12) * D.c + 3), if S.c > 0.5 and D.c <= 0.25
         *          D.c + (2 * S.c - 1) * (sqrt(D.c) - D.c), if S.c > 0.5 and D.c > 0.25
         * \endcode
         */
        blend_w3c_softlight,

        /*!
         * Difference mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a  +  D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c = abs(S.c - D.c)
         * \endcode
         */
        blend_w3c_difference,

        /*!
         * Exclusion mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c = S.c + D.c - 2 * S.c * D.c
         * \endcode
         */
        blend_w3c_exclusion,

        /*!
         * Multiply mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where for each channel c,
         * \code
         * f(S, D).c = S.c * D.c
         * \endcode
         */
        blend_w3c_multiply,

        /*!
         * Hue mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where
         * \code
         * f(S.rgb, D.rgb).rgb = OverrideLuminosityAndSaturation(S.rgb, D.rgb, D.rgb)
         * \endcode
         */
        blend_w3c_hue,

        /*!
         * Saturation mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where
         * \code
         * f(S.rgb, D.rgb).rgb = OverrideLuminosityAndSaturation(D.rgb, S.rgb, D.rgb)
         * \endcode
         */
        blend_w3c_saturation,

        /*!
         * Color mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where
         * \code
         * f(S.rgb, D.rgb).rgb = OverrideLuminosity(S.rgb, D.rgb)
         * \endcode
         */
        blend_w3c_color,

        /*!
         * Luminosity mode. Letting S be the value from the
         * fragment shader and D be the current value in the framebuffer,
         * replaces the value in the framebuffer with F where F is:
         * \code
         * F.a = S.a + D.a * (1 - S.a)
         * F.rgb = f(UndoAlpha(S), UndoAlpha(D)) * S.a * D.a + S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
         * \endcode
         * where
         * \code
         * f(S.rgb, D.rgb).rgb = OverrideLuminosity(D.rgb, S.rgb)
         * \endcode
         */
        blend_w3c_luminosity,

        number_composite_mode,
      };

    /*!
     * \brief
     * Enumeration to query the statistics of how
     * much data has been packed
     */
    enum query_stats_t
      {
        /*!
         * Offset to how many attributes processed
         */
        num_attributes,

        /*!
         * Offset to how many indices processed
         */
        num_indices,

        /*!
         * Offset to how many generic_data values placed
         * onto store buffer(s).
         */
        num_generic_datas,

        /*!
         * Offset to how many PainterDraw objects sent
         */
        num_draws,

        /*!
         * Offset to how many painter headers packed.
         */
        num_headers,

        /*!
         * Number of distinct render targets needed.
         */
        num_render_targets,

        /*!
         * Number of times PainterBackend::end() was called
         */
        num_ends,

        /*!
         * Number of begin_layer()/end_layer() pairs called
         */
        num_layers,

        /*!
         * Number of begin_coverage_buffer()/end_coverage_buffer() pairs called
         */
        num_deferred_coverages,
      };

    /*!
     * Given a fill rule, return the fill rule for the complement.
     */
    static
    enum fill_rule_t
    complement_fill_rule(enum fill_rule_t f);

    /*!
     * Returns true if a \ref join_style is a miter-type join, i.e.
     * one of \ref miter_clip_joins, \ref miter_bevel_joins or
     * \ref miter_joins.
     * \param js join style to query
     */
    static
    inline
    bool
    is_miter_join(enum join_style js)
    {
      return js == miter_clip_joins
        || js == miter_bevel_joins
        || js == miter_joins;
    }

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum screen_orientation v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum rotation_orientation_t v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum glyph_layout_type v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum cap_style v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum join_style v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum fill_rule_t v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum immediate_coverage_support_t v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum shader_anti_alias_t v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum stroking_method_t v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum composite_mode_t v);

    /*!
     * Returns a \ref c_string for an enumerated value.
     * \param v value to get the label-string of.
     */
    static
    c_string
    label(enum query_stats_t v);
  };
/*! @} */
}
