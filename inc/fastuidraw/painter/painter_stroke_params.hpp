/*!
 * \file painter_stroke_params.hpp
 * \brief file painter_stroke_params.hpp
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

#include <fastuidraw/painter/painter_shader_data.hpp>
#include <fastuidraw/painter/painter_stroke_shader.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * Class to specify stroking parameters, data is packed
   * as according to PainterStrokeParams::stroke_data_offset_t.
   */
  class PainterStrokeParams:public PainterItemShaderData
  {
  public:
    /*!
     * \brief
     * Enumeration to specify the units of the stroking radius
     */
    enum stroking_units_t
      {
        /*!
         * Indicates that the stroking units is in local
         * coordinates of the Path being stroking
         */
        path_stroking_units,

        /*!
         * Indicates that the stroking units are in pixels
         */
        pixel_stroking_units,
      };

    /*!
     * \brief
     * Enumeration that provides offsets for the stroking
     * parameters.
     */
    enum stroke_data_offset_t
      {
        /*!
         * Offset to stroke radius (packed as float).
         * The absolute value gives the stroking radius,
         * if teh value is negative then the stroking radius
         * is in units of pixels, if positive it is in local
         * path coordinates.
         */
        stroke_radius_offset,
        stroke_miter_limit_offset, /*!< offset to stroke miter limit (packed as float) */

        stroke_data_size /*!< size of data for stroking*/
      };

    /*!
     * Ctor.
     */
    PainterStrokeParams(void);

    /*!
     * The miter limit for miter joins
     */
    float
    miter_limit(void) const;

    /*!
     * Set the value of miter_limit(void) const
     */
    PainterStrokeParams&
    miter_limit(float f);

    /*!
     * The stroking width, always non-negative
     */
    float
    width(void) const;

    /*!
     * Set the value of width(void) const,
     * values are clamped to be non-negative.
     */
    PainterStrokeParams&
    width(float f);

    /*!
     * The stroking radius, equivalent to
     * \code
     * width() * 0.5
     * \endcode
     */
    float
    radius(void) const;

    /*!
     * Set the value of radius(void) const,
     * equivalent to
     * \code
     * width(2.0 * f)
     * \endcode
     */
    PainterStrokeParams&
    radius(float f);

    /*!
     * Returns the units of the stroking, default value is
     * \ref path_stroking_units
     */
    enum stroking_units_t
    stroking_units(void) const;

    /*!
     * Set the value of stroking_units(void) const,
     * values are clamped to be non-negative.
     */
    PainterStrokeParams&
    stroking_units(enum stroking_units_t);

    /*!
     * Returns a StrokingDataSelectorBase suitable for PainterStrokeParams.
     * \param pixel_arc_stroking_possible if true, will inform that arc-stroking
     *                                    width in \ref pixel_stroking_units is
     *                                    possible.
     */
    static
    reference_counted_ptr<const StrokingDataSelectorBase>
    stroking_data_selector(bool pixel_arc_stroking_possible);
  };

/*! @} */

} //namespace fastuidraw
