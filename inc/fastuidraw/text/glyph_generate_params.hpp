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
/*!\addtogroup Glyph
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
     * When generating restricted rays glyph data see (\ref
     * GlyphRenderDataRestrictedRays), specifies the expected
     * smallest size on the scrren at which to render glyphs
     * via a \ref GlyphRenderDataRestrictedRays. The value is
     * used to include curves near the boundary of bounding box
     * so that anti-aliasing works correctly when the glyph
     * is renderer very small. A negative value indicates that
     * no slack is taken/used.
     */
    float
    restricted_rays_minimum_render_size(void);

    /*!
     * Set the value returned by
     * restricted_rays_minimum_render_size(void) const,
     * initial value is 32.0. Returns \ref routine_success
     * if value is successfully changed.
     * \param v value
     */
    enum return_code
    restricted_rays_minimum_render_size(float v);

    /*!
     * When generating restricted rays glyph data see (\ref
     * GlyphRenderDataRestrictedRays), specifies the
     * threshhold value for number of curves allowed in
     * a single box before a box.
     */
    int
    restricted_rays_split_thresh(void);

    /*!
     * Set the value returned by
     * restricted_rays_split_thresh(void) const,
     * initial value is 4. Returns \ref routine_success
     * if value is successfully changed.
     * \param v value
     */
    enum return_code
    restricted_rays_split_thresh(int v);

    /*!
     * When generating restricted rays glyph data see (\ref
     * GlyphRenderDataRestrictedRays), specifies the
     * maximum level of recursion that will be used to
     * generate the hierarchy of boxes.
     */
    int
    restricted_rays_max_recursion(void);

    /*!
     * Set the value returned by
     * restricted_rays_max_recursion(void) const,
     * initial value is 12. Returns \ref routine_success
     * if value is successfully changed.
     * \param v value
     */
    enum return_code
    restricted_rays_max_recursion(int v);

    /*!
     * When generating banded rays glyphs see (\ref
     * GlyphRenderDataBandedRays), specifies the
     * maximum number of times to recurse when
     * generating sub-bands. The number of bands
     * that are generated in a dimension is 2^N
     * where N is the number of levels of recurion
     * used to generate bands.
     */
    unsigned int
    banded_rays_max_recursion(void);

    /*!
     * Set the value returned by
     * banded_rays_max_recursion(void) const,
     * initial value is 11. Returns \ref routine_success
     * if value is successfully changed.
     * \param v value
     */
    enum return_code
    banded_rays_max_recursion(unsigned int v);

    /*!
     * When generating banded rays glyphs see (\ref
     * GlyphRenderDataBandedRays), specifies the
     * threshhold for the average number of curves
     * across bands to stop recursing to finer bands.
     */
    float
    banded_rays_average_number_curves_thresh(void);

    /*!
     * Set the value returned by
     * banded_rays_average_number_curves_thresh(void) const,
     * initial value is 2.5. Returns \ref routine_success
     * if value is successfully changed.
     * \param v value
     */
    enum return_code
    banded_rays_average_number_curves_thresh(float v);
  }
}
