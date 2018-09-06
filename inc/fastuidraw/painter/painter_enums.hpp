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

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief Namespace to encapsulate enumerations used in Painter
   *        interface, part of the main library libFastUIDraw.
   */
  namespace PainterEnums
  {
    /*!
     * \brief
     * Enumeration to indicate in what direction the
     * y-coordinate increases
     */
    enum screen_orientation
      {
        y_increases_downwards, /*!< y-coordinate increases downwards */
        y_increases_upwards /*!< y-coordinate increases upwards */
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
         * \ref GlyphLayoutData::horizontal_layout_offset()
         * to offset the glyphs.
         */
        glyph_layout_horizontal,

        /*!
         * Glyphs are layed out vertically, thus will use
         * \ref GlyphLayoutData::vertical_layout_offset()
         * to offset the glyphs.
         */
        glyph_layout_vertical
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

        fill_rule_data_count /*!< count of enums */
      };

    /*!
     * \brief
     * Enumeration specifying composite modes
     */
    enum composite_mode_t
      {
        composite_porter_duff_clear, /*!< Clear mode of Porter-Duff */
        composite_porter_duff_src, /*!< Source mode of Porter-Duff */
        composite_porter_duff_dst, /*!< Destination mode of Porter-Duff */
        composite_porter_duff_src_over, /*!< Source over mode of Porter-Duff */
        composite_porter_duff_dst_over, /*!< Destination over mode of Porter-Duff */
        composite_porter_duff_src_in, /*!< Source In mode of Porter-Duff */
        composite_porter_duff_dst_in, /*!< Destination In mode of Porter-Duff */
        composite_porter_duff_src_out, /*!< Source Out mode of Porter-Duff */
        composite_porter_duff_dst_out, /*!< Destination Out mode of Porter-Duff */
        composite_porter_duff_src_atop, /*!< Source Atop mode of Porter-Duff */
        composite_porter_duff_dst_atop, /*!< Destination Atop mode of Porter-Duff */
        composite_porter_duff_xor, /*!< Xor mode of Porter-Duff */
      };

    /*!
     * \brief
     * Enumeration specifying W3C blending modes
     */
    enum blend_w3c_mode_t
      {
        blend_w3c_normal, /*!< W3C multiply mode: Src, i.e. no blending operation */
        blend_w3c_multiply, /*!< W3C multiply mode: Dest * Src */
        blend_w3c_screen, /*!< W3C screen mode: 1 - (1 - Dest) * (1 - Src) */

        /*!
         * W3C overlay mode: for each channel,
         * (Dst <= 0.5) ?
         *    2.0 * Dest * Src :
         *    1 - 2 * (1 - Dst) * (1 - Src)
         */
        blend_w3c_overlay,
        blend_w3c_darken, /*!< W3C darken mode: min(Src, Dest) */
        blend_w3c_lighten, /*!< W3C lighten mode: max(Src, Dest) */

        /*!
         * W3C color-dodge mode: for each channel
         * (Dest == 0) ? 0 : (Src == 1) ? 1 : min(1, Dst / (1 - Src) )
         * i.e. if Dest is 0, write 0. If Src is 1, write 1. Otherwise
         * write Dst / (1 - Src)
         */
        blend_w3c_color_dodge,

        /*!
         * W3C color-burn mode: for each channel
         * (Dest == 1) ? 1 : (Src == 0) ? 0 : 1 - min(1, (1 - Dst) / Src)
         * i.e. if Dest is 1, write 1. If Src is 0, write 0. Otherwise
         * write (1 - Dst ) / Src
         */
        blend_w3c_color_burn,

        /*!
         * W3C hardlight mode: for each channel,
         * (Src <= 0.5) ?
         *    2.0 * Dest * Src :
         *    1 - 2 * (1 - Dst) * (1 - Src)
         */
        blend_w3c_hardlight,

        /*!
         * W3C soft light mode: for each channel:
         * (Src <= 0.5) ?
         *   Dst - (1 - 2 * Src) * Dst * (1 - Dst) :
         *   Dst + (2 * Src - 1) * (Z - Dst)
         * where
         *   Z = (Dst <= 0.25) ?
         *     ((16 * Dst - 12) * Dst + 4) * Dst :
         *     sqrt(Dst)
         */
        blend_w3c_softlight,

        blend_w3c_difference, /*!< W3C difference mode: for each channel, abs(Dest - Src) */
        blend_w3c_exclusion, /*!< W3C exclusion mode: for each channel, Dest + Src - 2 * Dest * Src */

        /*!
         * w3c hue mode, see w3c for formula
         */
        blend_w3c_hue,

        /*!
         * w3c saturation mode, see w3c for formula
         */
        blend_w3c_saturation,

        /*!
         * w3c color mode, see w3c for formula
         */
        blend_w3c_color,

        /*!
         * w3c luminosity mode, see w3c for formula
         */
        blend_w3c_luminosity,
      };

    /*!
     * Given a fill rule, return the fill rule for the complement.
     */
    enum fill_rule_t
    complement_fill_rule(enum fill_rule_t f);
  };
/*! @} */
}
