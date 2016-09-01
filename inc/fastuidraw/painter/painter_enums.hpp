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
  @{
 */

  /*!
    \brief Namespace to encapsulate enumerations used in Painter
           interface, part of the main library libFastUIDraw.
   */
  namespace PainterEnums
  {
    /*!
      Enumeration for text drawing to indicating in what
      direction the y-coordinate increases
     */
    enum glyph_orientation
      {
        y_increases_downwards, /*!< y-coordinate increases downwards */
        y_increases_upwards /*!< y-coordinate increases upwards */
      };

    /*!
      Enumeration specifying if and how to draw caps when stroking.
     */
    enum cap_style
      {
        no_caps,        /*!< indicates to stroke with contours of path are not closed and caps are not added     */
        rounded_caps,   /*!< indicates to stroke with contours of path are not closed and rounded caps are added */
        square_caps,    /*!< indicates to stroke with contours of path are not closed and square caps are added  */
        close_contours, /*!< indicates to stroke with contours of path closed */

        number_cap_styles /*!< number of cap styles */
      };

    /*!
      Enumeration specifying if and how to draw joins when stroking
     */
    enum join_style
      {
        no_joins, /*!< indicates to stroke without joins */
        rounded_joins, /*!< indicates to stroke with rounded joins */
        bevel_joins, /*!< indicates to stroke with bevel joins */
        miter_joins, /*!< indicates to stroke with miter joins */

        number_join_styles, /*!< number of join styles */
      };

    /*!
      Enumerations specifying common fill rules.
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
      Enumeration specifying blend modes
     */
    enum blend_mode_t
      {
        blend_porter_duff_clear, /*!< Clear mode of Porter-Duff */
        blend_porter_duff_src, /*!< Source mode of Porter-Duff */
        blend_porter_duff_dst, /*!< Destination mode of Porter-Duff */
        blend_porter_duff_src_over, /*!< Source over mode of Porter-Duff */
        blend_porter_duff_dst_over, /*!< Destination over mode of Porter-Duff */
        blend_porter_duff_src_in, /*!< Source In mode of Porter-Duff */
        blend_porter_duff_dst_in, /*!< Destination In mode of Porter-Duff */
        blend_porter_duff_src_out, /*!< Source Out mode of Porter-Duff */
        blend_porter_duff_dst_out, /*!< Destination Out mode of Porter-Duff */
        blend_porter_duff_src_atop, /*!< Source Atop mode of Porter-Duff */
        blend_porter_duff_dst_atop, /*!< Destination Atop mode of Porter-Duff */
        blend_porter_duff_xor, /*!< Xor mode of Porter-Duff */

        blend_w3c_mulitply, /*!< W3C multiply mode: Dest * Src */
        blend_w3c_screen, /*!< W3C screen mode: 1 - (1 - Dest) * (1 - Src) */

        /*!
          W3C overlay mode: for each channel,
          (Dst <= 0.5) ?
             2.0 * Dest * Src :
             1 - 2 * (1 - Dst) * (1 - Src)
         */
        blend_w3c_overlay,
        blend_w3c_darken, /*!< W3C darken mode: min(Src, Dest) */
        blend_w3c_lighten, /*!< W3C lighten mode: max(Src, Dest) */

        /*!
          W3C color-dodge mode: for each channel
          (Dest == 0) ? 0 : (Src == 1) ? 1 : min(1, Dst / (1 - Src) )
          i.e. if Dest is 0, write 0. If Src is 1, write 1. Otherwise
          write Dst / (1 - Src)
        */
        blend_w3c_color_dodge,

        /*!
          W3C color-burn mode: for each channel
          (Dest == 1) ? 1 : (Src == 0) ? 0 : 1 - min(1, (1 - Dst) / Src)
          i.e. if Dest is 1, write 1. If Src is 0, write 0. Otherwise
          write (1 - Dst ) / Src
        */
        blend_w3c_color_burn,

        /*!
          W3C hardlight mode: for each channel,
          (Src <= 0.5) ?
             2.0 * Dest * Src :
             1 - 2 * (1 - Dst) * (1 - Src)
         */
        blend_w3c_hardlight,

        /*!
          W3C soft light mode: for each channel:
          (Src <= 0.5) ?
            Dst - (1 - 2 * Src) * Dst * (1 - Dst) :
            Dst + (2 * Src - 1) * (Z - Dst)
          where
            Z = (Dst <= 0.25) ?
              ((16 * Dst - 12) * Dst + 4) * Dst :
              sqrt(Dst)
        */
        blend_w3c_soft_light,

        blend_w3c_difference, /*!< W3C difference mode: for each channel, abs(Dest - Src) */
        blend_w3c_exclusion, /*!< W3C exclusion mode: for each channel, Dest + Src - 2 * Dest * Src */

        /*!
          w3c hue mode, see w3c for formula
         */
        blend_w3c_hue,

        /*!
          w3c saturation mode, see w3c for formula
         */
        blend_w3c_saturation,

        /*!
          w3c color mode, see w3c for formula
         */
        blend_w3c_color,

        /*!
          w3c luminosity mode, see w3c for formula
         */
        blend_w3c_luminosity,

      };

    /*!
      Given a fill rule, return the fill rule for the complement.
     */
    enum fill_rule_t
    complement_fill_rule(enum fill_rule_t f);
  };
/*! @} */
}
