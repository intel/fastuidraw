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

#include <fastuidraw/painter/rect.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * Class to specify the geometry of a rounded rectangle.
   */
  class RoundedRect:public Rect
  {
  public:
    /*!
     * Specifies the radii at each of the corners,
     * enumerated by \ref corner_t
     */
    vecN<vec2, 4> m_corner_radii;
  };
/*! @} */
}
