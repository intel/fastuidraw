/*!
 * \file rounded_rect.hpp
 * \brief file rounded_rect.hpp
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/math.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * Class to specify the geometry of a rounded rectangle.
   */
  class Rect
  {
  public:
    enum
      {
        maxx_mask = 1, /*<! bitmask on \ref corner_t to test if on max-x side */
        maxy_mask = 2  /*<! bitmask on \ref corner_t to test if on max-y side */
      };

    /*!
     * Conveniance enumeration to name the rounded corner
     * radii of a RoundedRect.
     */
    enum corner_t
      {
        minx_miny_corner = 0,
        minx_maxy_corner = maxy_mask,
        maxx_miny_corner = maxx_mask,
        maxx_maxy_corner = maxx_mask | maxy_mask,
      };

    /*!
     * Empty ctor; intializes both \ref m_min_point and
     * \ref m_max_point to (0, 0);
     */
    Rect(void):
      m_min_point(0.0f, 0.0f),
      m_max_point(0.0f, 0.0f)
    {}

    /*!
     * Set \ref m_min_point.
     */
    Rect&
    min_point(const vec2 &p)
    {
      m_min_point = p;
      return *this;
    }

    /*!
     * Set \ref m_min_point.
     */
    Rect&
    min_point(float x, float y)
    {
      m_min_point.x() = x;
      m_min_point.y() = y;
      return *this;
    }

    /*!
     * Set \ref m_max_point.
     */
    Rect&
    max_point(const vec2 &p)
    {
      m_max_point = p;
      return *this;
    }

    /*!
     * Set \ref m_max_point.
     */
    Rect&
    max_point(float x, float y)
    {
      m_max_point.x() = x;
      m_max_point.y() = y;
      return *this;
    }

    /*!
     * Equivalent to \code m_min_point.x() \endcode
     */
    float&
    min_x(void) { return m_min_point.x(); }

    /*!
     * Equivalent to \code m_min_point.x() \endcode
     */
    float
    min_x(void) const { return m_min_point.x(); }

    /*!
     * Equivalent to \code m_min_point.y() \endcode
     */
    float&
    min_y(void) { return m_min_point.y(); }

    /*!
     * Equivalent to \code m_min_point.y() \endcode
     */
    float
    min_y(void) const { return m_min_point.y(); }

    /*!
     * Equivalent to \code m_max_point.x() \endcode
     */
    float&
    max_x(void) { return m_max_point.x(); }

    /*!
     * Equivalent to \code m_max_point.x() \endcode
     */
    float
    max_x(void) const { return m_max_point.x(); }

    /*!
     * Equivalent to \code m_max_point.y() \endcode
     */
    float&
    max_y(void) { return m_max_point.y(); }

    /*!
     * Equivalent to \code m_max_point.y() \endcode
     */
    float
    max_y(void) const { return m_max_point.y(); }

    /*!
     * Return the named point of the Rect.
     * \param c which corner of the rect.
     */
    vec2
    point(enum corner_t c) const
    {
      vec2 return_value;

      return_value.x() = (c & maxx_mask) ? max_x() : min_x();
      return_value.y() = (c & maxy_mask) ? max_y() : min_y();
      return return_value;
    }

    /*!
     * Translate the Rect, equivalent to
     * \code
     * m_min_point += tr;
     * m_max_point += tr;
     * \endcode
     * \param tr amount by which to translate
     */
    Rect&
    translate(const vec2 &tr)
    {
      m_min_point += tr;
      m_max_point += tr;
      return *this;
    }

    /*!
     * Translate the Rect, equivalent to
     * \code
     * translate(vec2(x, y))
     * \endcode
     * \param x amount by which to translate in x-coordinate
     * \param y amount by which to translate in y-coordinate
     */
    Rect&
    translate(float x, float y)
    {
      return translate(vec2(x,y));
    }

    /*!
     * Set \ref m_max_point from \ref m_min_point
     * and a size. Equivalent to
     * \code
     * max_point(min_point() + sz)
     * \endcode
     * \param sz size to which to set the Rect
     */
    Rect&
    size(const vec2 &sz)
    {
      m_max_point = m_min_point + sz;
      return *this;
    }

    /*!
     * Set \ref m_max_point from \ref m_min_point
     * and a size. Equivalent to
     * \code
     * max_point(min_point() + vec2(width, height))
     * \endcode
     * \param width width to which to set the rect
     * \param height height to which to set the rect
     */
    Rect&
    size(float width, float height)
    {
      m_max_point.x() = m_min_point.x() + width;
      m_max_point.y() = m_min_point.y() + height;
      return *this;
    }

    /*!
     * Returns the size of the Rect; provided as
     * a conveniance, equivalent to
     * \code
     * m_max_point - m_min_point
     * \endcode
     */
    vec2
    size(void) const
    {
      return m_max_point - m_min_point;
    }

    /*!
     * Set the width of the Rect, equivalent to
     * \code
     * m_max_point.x() = w + m_min_point.x();
     * \endcode
     * \param w value to make the width of the rect
     */
    Rect&
    width(float w)
    {
      m_max_point.x() = w + m_min_point.x();
      return *this;
    }

    /*!
     * Set the height of the Rect, equivalent to
     * \code
     * m_max_point.y() = h + m_min_point.y();
     * \endcode
     * \param h value to make the height of the rect
     */
    Rect&
    height(float h)
    {
      m_max_point.y() = h + m_min_point.y();
      return *this;
    }

    /*!
     * Returns the width of the Rect, equivalent to
     * \code
     * m_max_point.x() - m_min_point.x();
     * \endcode
     */
    float
    width(void) const
    {
      return m_max_point.x() - m_min_point.x();
    }

    /*!
     * Returns the width of the Rect, equivalent to
     * \code
     * m_max_point.y() - m_min_point.y();
     * \endcode
     */
    float
    height(void) const
    {
      return m_max_point.y() - m_min_point.y();
    }

    /*!
     * Sanitizes the Rect so that both width() and
     * height() are non-negative.
     */
    Rect&
    sanitize_size(void)
    {
      width(t_max(0.0f, width()));
      height(t_max(0.0f, height()));
      return *this;
    }

    /*!
     * Specifies the min-corner of the rectangle
     */
    vec2 m_min_point;

    /*!
     * Specifies the max-corner of the rectangle.
     */
    vec2 m_max_point;
  };
/*! @} */
}
