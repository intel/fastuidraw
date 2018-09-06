/*!
 * \file glyph_generate_params.hpp
 * \brief file glyph_generate_params.hpp
 *
 * Copyright 2018 by Intel.
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
/*!\addtogroup Text
 * @{
 */
  /*!
   * !\brief
   * GlyphGenerateParams encapsulates values to determine how glyphs
   * are to be produced. These values cannot be changed if
   * there are any \ref FontBase derived objects alive.
   */
  namespace GlyphGenerateParams
  {
    /*!
     * Pixel size at which to generate distance field scalable glyphs.
     */
    unsigned int
    distance_field_pixel_size(void);

    /*!
     * Set the value returned by distance_field_pixel_size(void) const,
     * initial value is 48. Return \ref routine_success if value is
     * successfully changed.
     * \param v value
     */
    enum return_code
    distance_field_pixel_size(unsigned int v);

    /*!
     * When creating distance field data, the distances are normalized
     * and clamped to [0, 1]. This value provides the normalization
     * which effectivly gives the maximum distance recorded in the
     * distance field texture. Recall that the values stored in texels
     * are uint8_t's so larger values will have lower accuracy. The
     * units are in pixels. Default value is 1.5.
     */
    float
    distance_field_max_distance(void);

    /*!
     * Set the value returned by distance_field_max_distance(void) const,
     * initial value is 96.0, i.e. 1.5 pixels. Return \ref routine_success
     * if value is successfully changed.
     * \param v value
     */
    enum return_code
    distance_field_max_distance(float v);

    /*!
     * Pixel size at which to generate curve pair scalable glyphs.
     */
    unsigned int
    curve_pair_pixel_size(void);

    /*!
     * Set the value returned by curve_pair_pixel_size(void) const,
     * initial value is 32. Return \ref routine_success if value is
     * successfully changed.
     * \param v value
     */
    enum return_code
    curve_pair_pixel_size(unsigned int v);
  }
}
