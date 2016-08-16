/*!
 * \file painter_stroke_value.hpp
 * \brief file painter_stroke_value.hpp
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

#include <fastuidraw/painter/painter_value.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    Class to specify stroking parameters, data is packed
    as according to PainterStrokeParams::stroke_data_offset_t.
   */
  class PainterStrokeParams:public PainterItemShaderData
  {
  public:
    /*!
      Enumeration that provides offsets for the stroking
      parameters.
     */
    enum stroke_data_offset_t
      {
        stroke_width_offset, /*!< offset to stroke width (packed as float) */
        stroke_miter_limit_offset, /*!< offset to stroke miter limit (packed as float) */

        stroke_data_size /*!< size of data for stroking*/
      };

    /*!
      Ctor.
     */
    PainterStrokeParams(void);

    /*!
      The miter limit for miter joins
     */
    float
    miter_limit(void) const;

    /*!
      Set the value of miter_limit(void) const
     */
    PainterStrokeParams&
    miter_limit(float f);

    /*!
      The stroking width
     */
    float
    width(void) const;

    /*!
      Set the value of width(void) const
     */
    PainterStrokeParams&
    width(float f);
  };

  /*!
    Class to specify dashed stroking parameters, data is packed
    as according to PainterDashedStrokeParams::stroke_data_offset_t.
    Data for dashing is packed [TODO describe].
   */
  class PainterDashedStrokeParams:public PainterItemShaderData
  {
  public:
    /*!
      Enumeration that provides offsets for the stroking
      parameters. The dashed pattern is packed in the next
      block of the data store.
     */
    enum stroke_data_offset_t
      {
        stroke_width_offset, /*!< offset to stroke width (packed as float) */
        stroke_miter_limit_offset, /*!< offset to stroke miter limit (packed as float) */
        stroke_dash_pattern_dash_offset, /*!< offset to dash offset value for dashed stroking (packed as float) */
        stroke_dash_pattern_total_length, /*!< offset to total legnth of dash pattern (packed as float) */

        stroke_static_data_size /*!< size of static data for stroking */
      };

    /*!
      Ctor.
     */
    PainterDashedStrokeParams(void);

    /*!
      The miter limit for miter joins
     */
    float
    miter_limit(void) const;

    /*!
      Set the value of miter_limit(void) const
     */
    PainterDashedStrokeParams&
    miter_limit(float f);

    /*!
      The stroking width
     */
    float
    width(void) const;

    /*!
      Set the value of width(void) const
     */
    PainterDashedStrokeParams&
    width(float f);

    /*!
      The dashed offset, i.e. the starting point of the
      dash pattern to start dashed stroking.
     */
    float
    dash_offset(void) const;

    /*!
      Set the value of dash_offset(void) const
     */
    PainterDashedStrokeParams&
    dash_offset(float f);

    /*!
      Returns the dash pattern for stroking. The dash
      pattern is an even number of entries giving the
      dash pattern, where:
       - for all integers i, [2 * i] is how long the i'th dash is
       - for all integers i, [2 * i + 1] is how long the space is between the i'th and (i+1)'th dash
     */
    const_c_array<float>
    dash_pattern(void) const;

    /*!
      Set the value return by dash_pattern(void) const.
      \param v dash pattern; the values are -copied-.
     */
    PainterDashedStrokeParams&
    dash_pattern(const_c_array<float> v);
  };
/*! @} */

} //namespace fastuidraw
