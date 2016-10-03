/*!
 * \file path_util_private.cpp
 * \brief file path_util_private.cpp
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

#include <cmath>
#include <fastuidraw/util/math.hpp>
#include "path_util_private.hpp"

unsigned int
fastuidraw::detail::
number_segments_for_tessellation(float radius, float arc_angle,
                                 const TessellatedPath::TessellationParams &P)
{
  float needed_sizef, theta;
  if(P.m_curvature_tessellation)
    {
      theta = P.m_threshhold;
    }
  else
    {
      float d;
      d = t_max(1.0f - P.m_threshhold / radius, 0.5f);
      theta = std::acos(d);
    }
  needed_sizef = std::max(3.0f, arc_angle / theta);
  return fastuidraw::t_min(static_cast<unsigned int>(needed_sizef), P.m_max_segments);
}
