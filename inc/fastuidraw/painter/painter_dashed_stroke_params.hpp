/*!
 * \file painter_dashed_stroke_params.hpp
 * \brief file painter_dashed_stroke_params.hpp
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

#include <fastuidraw/painter/painter_dashed_stroke_shader_set.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

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
        stroke_radius_offset, /*!< offset to dashed stroke radius (packed as float) */
        stroke_miter_limit_offset, /*!< offset to dashed stroke miter limit (packed as float) */
        stroke_dash_offset_offset, /*!< offset to dash offset value for dashed stroking (packed as float) */
        stroke_total_length_offset, /*!< offset to total legnth of dash pattern (packed as float) */
        stroke_first_interval_start_offset, /*!< offset to value recording the start of the first interval (packed as float) */
        stroke_number_intervals_offset, /*!< offset to value giving the number of intervals (packed as uint) */

        stroke_static_data_size /*!< size of static data for dashed stroking */
      };

    /*!
      A DashPatternElement is an element of a dash pattern.
      It specifies how long to draw then how much space to
      emit before the next DashPatternElement.
     */
    class DashPatternElement
    {
    public:
      /*!
        Ctor, intializes both \ref m_draw_length and
        \ref m_space_length as 0.
       */
      DashPatternElement(void):
        m_draw_length(0.0f),
        m_space_length(0.0f)
      {}

      /*!
        Ctor.
        \param d value with which to initialize \ref m_draw_length
        \param s value with which to initialize \ref m_space_length
       */
      DashPatternElement(float d, float s):
        m_draw_length(d),
        m_space_length(s)
      {}

      /*!
        How long to draw
       */
      float m_draw_length;

      /*!
        How much space to next DashPatternElement
       */
      float m_space_length;
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
      The stroking radius, equivalent to
      \code
      width() * 0.5
      \endcode
     */
    float
    radius(void) const;

    /*!
      Set the value of radius(void) const,
      equivalent to
      \code
      width(2.0 * f)
      \endcode
     */
    PainterDashedStrokeParams&
    radius(float f);

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
      Returns the dash pattern for stroking.
     */
    const_c_array<DashPatternElement>
    dash_pattern(void) const;

    /*!
      Set the value return by dash_pattern(void) const.
      \param v dash pattern; the values are -copied-.
     */
    PainterDashedStrokeParams&
    dash_pattern(const_c_array<DashPatternElement> v);

    /*!
      Constructs and returns a DashEvaluator compatible
      with the data of PainterDashedStrokeParams.
      \param pixel_width_stroking if true return an object to
                                  be used when stroking width
                                  is in pixels; if false return
                                  an object to be used when
                                  stroking width is in coordinates
                                  of the path.
     */
    static
    reference_counted_ptr<const DashEvaluatorBase>
    dash_evaluator(bool pixel_width_stroking);

    /*!
      Returns a StrokingDataSelectorBase suitable for
      PainterDashedStrokeParams.
     */
    static
    reference_counted_ptr<const StrokingDataSelectorBase>
    stroking_data_selector(bool pixel_width_stroking);
  };
/*! @} */

} //namespace fastuidraw
