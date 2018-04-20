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
  unsigned int needed_size;

  if (P.m_curvature_tessellation)
    {
      theta = P.m_threshhold;
    }
  else
    {
      float d;
      d = t_max(1.0f - P.m_threshhold / radius, 0.5f);
      theta = t_max(0.00001f, 0.5f * std::acos(d));
    }
  needed_sizef = t_abs(arc_angle) / theta;

  /* we ask for one more than necessary, to ensure that we BEAT
     the tessellation requirement.
   */
  needed_size = 1 + fastuidraw::t_max(3u, static_cast<unsigned int>(needed_sizef));
  return fastuidraw::t_min(needed_size, P.m_max_segments);
}

unsigned int
fastuidraw::detail::
number_segments_for_tessellation(float arc_angle, float distance_thresh)
{
  float needed_sizef, d, theta;

  d = t_max(1.0f - distance_thresh, 0.5f);
  theta = t_max(0.00001f, 0.5f * std::acos(d));
  needed_sizef = t_abs(arc_angle) / theta;

  /* we ask for one more than necessary, to ensure that we BEAT
     the tessellation requirement.
   */
  return 1 + fastuidraw::t_max(3u, static_cast<unsigned int>(needed_sizef));
}
