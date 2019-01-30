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
   * Class to specify enumerations used by \ref RectT.
   */
  class RectEnums
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
  };

  /*!
   * Class to specify the geometry of a rectangle.
   */
  template<typename T>
  class RectT:public RectEnums
  {
  public:
    /*!
     * Empty ctor; intializes both \ref m_min_point and
     * \ref m_max_point to (0, 0);
     */
    RectT(void):
      m_min_point(T(0), T(0)),
      m_max_point(T(0), T(0))
    {}

    /*!
     * Copy ctor from different rect type
     */
    template<typename S>
    explicit
    RectT(const RectT<S> &rect):
      m_min_point(rect.m_min_point),
      m_max_point(rect.m_max_point)
    {}

    /*!
     * Set \ref m_min_point.
     */
    RectT&
    min_point(const vecN<T, 2> &p)
    {
      m_min_point = p;
      return *this;
    }

    /*!
     * Set \ref m_min_point.
     */
    RectT&
    min_point(T x, T y)
    {
      m_min_point.x() = x;
      m_min_point.y() = y;
      return *this;
    }

    /*!
     * Set \ref m_max_point.
     */
    RectT&
    max_point(const vecN<T, 2> &p)
    {
      m_max_point = p;
      return *this;
    }

    /*!
     * Set \ref m_max_point.
     */
    RectT&
    max_point(T x, T y)
    {
      m_max_point.x() = x;
      m_max_point.y() = y;
      return *this;
    }

    /*!
     * Equivalent to \code m_min_point.x() \endcode
     */
    T&
    min_x(void) { return m_min_point.x(); }

    /*!
     * Equivalent to \code m_min_point.x() \endcode
     */
    T
    min_x(void) const { return m_min_point.x(); }

    /*!
     * Equivalent to \code m_min_point.y() \endcode
     */
    T&
    min_y(void) { return m_min_point.y(); }

    /*!
     * Equivalent to \code m_min_point.y() \endcode
     */
    T
    min_y(void) const { return m_min_point.y(); }

    /*!
     * Equivalent to \code m_max_point.x() \endcode
     */
    T&
    max_x(void) { return m_max_point.x(); }

    /*!
     * Equivalent to \code m_max_point.x() \endcode
     */
    T
    max_x(void) const { return m_max_point.x(); }

    /*!
     * Equivalent to \code m_max_point.y() \endcode
     */
    T&
    max_y(void) { return m_max_point.y(); }

    /*!
     * Equivalent to \code m_max_point.y() \endcode
     */
    T
    max_y(void) const { return m_max_point.y(); }

    /*!
     * Return the named point of the Rect.
     * \param c which corner of the rect.
     */
    vecN<T, 2>
    point(enum corner_t c) const
    {
      vecN<T, 2> return_value;

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
    RectT&
    translate(const vecN<T, 2> &tr)
    {
      m_min_point += tr;
      m_max_point += tr;
      return *this;
    }

    /*!
     * Translate the Rect, equivalent to
     * \code
     * translate(vecN<T, 2>(x, y))
     * \endcode
     * \param x amount by which to translate in x-coordinate
     * \param y amount by which to translate in y-coordinate
     */
    RectT&
    translate(T x, T y)
    {
      return translate(vecN<T, 2>(x,y));
    }

    /*!
     * Set \ref m_max_point from \ref m_min_point
     * and a size. Equivalent to
     * \code
     * max_point(min_point() + sz)
     * \endcode
     * \param sz size to which to set the Rect
     */
    RectT&
    size(const vecN<T, 2> &sz)
    {
      m_max_point = m_min_point + sz;
      return *this;
    }

    /*!
     * Set \ref m_max_point from \ref m_min_point
     * and a size. Equivalent to
     * \code
     * max_point(min_point() + vecN<T, 2>(width, height))
     * \endcode
     * \param width width to which to set the rect
     * \param height height to which to set the rect
     */
    RectT&
    size(T width, T height)
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
    vecN<T, 2>
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
    RectT&
    width(T w)
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
    RectT&
    height(T h)
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
    T
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
    T
    height(void) const
    {
      return m_max_point.y() - m_min_point.y();
    }

    /*!
     * Sanitizes the Rect so that both width() and
     * height() are non-negative.
     */
    RectT&
    sanitize_size(void)
    {
      width(t_max(T(0), width()));
      height(t_max(T(0), height()));
      return *this;
    }

    /*!
     * Specifies the min-corner of the rectangle
     */
    vecN<T, 2> m_min_point;

    /*!
     * Specifies the max-corner of the rectangle.
     */
    vecN<T, 2> m_max_point;
  };

  /*!
   * Conveniance typedef for RectT<float>
   */
  typedef RectT<float> Rect;

/*! @} */
}
