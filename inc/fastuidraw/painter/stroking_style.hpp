/*!
 * \file stroking_style.hpp
 * \brief file stroking_style.hpp
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

#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * Simple class to wrap how to stroke: closed or open,
   * join style and cap style. Does NOT include stroking
   * parameters (i.e. \ref PainterStrokeParams).
   */
  class StrokingStyle
  {
  public:
    StrokingStyle(void):
      m_draw_closing_edges_of_contours(true),
      m_cap_style(PainterEnums::square_caps),
      m_join_style(PainterEnums::miter_clip_joins),
      m_stroke_with_shader_aa(PainterEnums::shader_anti_alias_auto)
    {}

    /*!
     * Set \ref m_cap_style to the specified value.
     * Default value is \ref PainterEnums::square_caps.
     */
    StrokingStyle&
    cap_style(enum PainterEnums::cap_style c)
    {
      m_cap_style = c;
      return *this;
    }

    /*!
     * Sets \ref m_draw_closing_edges_of_contours to
     * the specified value. Default value is true.
     */
    StrokingStyle&
    stroke_closing_edges_of_contours(bool v)
    {
      m_draw_closing_edges_of_contours = v;
      return *this;
    }

    /*!
     * Set \ref m_join_style to the specified value.
     */
    StrokingStyle&
    join_style(enum PainterEnums::join_style j)
    {
      m_join_style = j;
      return *this;
    }

    /*!
     * Set the value of \ref m_stroke_with_shader_aa
     * to the specified value.
     */
    StrokingStyle&
    stroke_with_shader_aa(enum PainterEnums::shader_anti_alias_t v)
    {
      m_stroke_with_shader_aa = v;
      return *this;
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * enum PainterEnums::shader_anti_alias_t value;
     * value = v ?
     *    PainterEnums::shader_anti_alias_auto :
     *    PainterEnums::shader_anti_alias_none;
     * return stroke_with_shader_aa(value);
     * \endcode
     */
    StrokingStyle&
    stroke_with_shader_aa(bool v)
    {
      enum PainterEnums::shader_anti_alias_t value;
      value = v ?
         PainterEnums::shader_anti_alias_auto :
         PainterEnums::shader_anti_alias_none;
      return stroke_with_shader_aa(value);
    }

    /*!
     * If true, stroke the closing edge of each of the
     * contours of the \ref Path or \ref StrokedPath.
     * Default value is true.
     */
    bool m_draw_closing_edges_of_contours;

    /*!
     * If \ref m_draw_closing_edges_of_contours is false,
     * specifies the what cap-style to use when stroking
     * the path. Default value is PainterEnums::square_caps.
     */
    enum PainterEnums::cap_style m_cap_style;

    /*!
     * Specifies what join style to use when stroking the
     * path. Default value is PainterEnums::miter_clip_joins
     */
    enum PainterEnums::join_style m_join_style;

    /*!
     * Specifies if and how to do shader anti-aliasing
     * when stroking the path. Default value is \ref
     * PainterEnums::shader_anti_alias_auto.
     */
    enum PainterEnums::shader_anti_alias_t m_stroke_with_shader_aa;
  };
/*! @} */
}
