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

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * Class to specify the geometry of a rounded rectangle.
   */
  class RoundedRect
  {
  public:
    /*!
     * Specifies the min-corner of the bounding box of
     * the rounded rectangle
     */
    vec2 m_min_point;

    /*!
     * Specifies the max-corner of the bounding box of
     * the rounded rectangle.
     */
    vec2 m_max_point;

    /*!
     * Specifies the radii at the min corner of the
     * rounded rectangle.
     */
    vec2 m_min_corner_radii;

    /*!
     * Specifies the radii at the max corner of the
     * rounded rectangle.
     */
    vec2 m_max_corner_radii;
  };
/*! @} */
}
