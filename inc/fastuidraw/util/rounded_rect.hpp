/*!
 * \file rounded_rect.hpp
 * \brief file rounded_rect.hpp
 *
 * Copyright 2018 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#pragma once

#include <fastuidraw/util/rect.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * Class to specify the geometry of a rounded rectangle;
   * the geometry of the base class Rect defines the -bounding-
   * rectangle of the \ref RoundedRect
   */
  class RoundedRect:public Rect
  {
  public:
    /*!
     * Defautl ctor, intializing all fields as zero,
     */
    RoundedRect(void):
      m_corner_radii(vec2(0.0f))
    {}

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * m_corner_radii[c] = v;
     * \endcode
     * \param c which radii corner to set
     * \param v value to which to set the corner radii
     */
    RoundedRect&
    corner_radius(enum corner_t c, vec2 v)
    {
      m_corner_radii[c] = v;
      return *this;
    }

    /*!
     * Provided as a conveniance, sets each element of
     * \ref m_corner_radii to the named value.
     * \param v value to which to set the corner radii
     */
    RoundedRect&
    corner_radii(vec2 v)
    {
      m_corner_radii[0] = m_corner_radii[1] = v;
      m_corner_radii[2] = m_corner_radii[3] = v;
      return *this;
    }

    /*!
     * Set all corner radii values to 0.
     */
    RoundedRect&
    make_flat(void)
    {
      return corner_radii(vec2(0.0f));
    }

    /*!
     * Returns true if each corner radii is 0.
     */
    bool
    is_flat(void) const
    {
      return m_corner_radii[0] == vec2(0.0f)
        && m_corner_radii[1] == vec2(0.0f)
        && m_corner_radii[2] == vec2(0.0f)
        && m_corner_radii[3] == vec2(0.0f);
    }

    /*!
     * Sanitize the RoundedRect as follows:
     *  - each of the corner radii is non-negative
     *  - each of the corner radii is no more than half the dimension
     *  - both the width and height are non-negative
     */
    RoundedRect&
    sanitize(void)
    {
      float fw, fh;

      sanitize_size();
      fw = 0.5f * width();
      fh = 0.5f * height();

      m_corner_radii[0].x() = t_min(fw, t_max(m_corner_radii[0].x(), 0.0f));
      m_corner_radii[1].x() = t_min(fw, t_max(m_corner_radii[1].x(), 0.0f));
      m_corner_radii[2].x() = t_min(fw, t_max(m_corner_radii[2].x(), 0.0f));
      m_corner_radii[3].x() = t_min(fw, t_max(m_corner_radii[3].x(), 0.0f));

      m_corner_radii[0].y() = t_min(fh, t_max(m_corner_radii[0].y(), 0.0f));
      m_corner_radii[1].y() = t_min(fh, t_max(m_corner_radii[1].y(), 0.0f));
      m_corner_radii[2].y() = t_min(fh, t_max(m_corner_radii[2].y(), 0.0f));
      m_corner_radii[3].y() = t_min(fh, t_max(m_corner_radii[3].y(), 0.0f));

      return *this;
    }

    /*!
     * Specifies the radii at each of the corners,
     * enumerated by \ref corner_t
     */
    vecN<vec2, 4> m_corner_radii;
  };
/*! @} */
}
