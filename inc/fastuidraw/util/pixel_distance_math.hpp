/*!
 * \file pixel_distance_math.hpp
 * \brief file pixel_distance_math.hpp
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

#include <fastuidraw/util/matrix.hpp>

namespace fastuidraw
{
/*!\addtogroup Utility
 * @{
 */
  /*!
   * Given a direction and location in LOCAL coordinates and a
   * distance value in pixel coordinates, return that distance
   * value in local coordinates.
   * \param distance distance value in pixel coordinates
   * \param resolution resolution of viewport
   * \param transformation_matrix transformation matrix from local
   *                              coordinates to clip-coordinates
   * \param point location in local coordinates
   * \param direction direction in local coordinates
   */
  float
  local_distance_from_pixel_distance(float distance,
                                     const vec2 &resolution,
                                     const float3x3 &transformation_matrix,
                                     const vec2 &point,
                                     const vec2 &direction);

/*! @} */
}
