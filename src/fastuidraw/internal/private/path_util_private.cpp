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
#include <complex>
#include <fastuidraw/util/math.hpp>
#include <private/path_util_private.hpp>

unsigned int
fastuidraw::detail::
number_segments_for_tessellation(float arc_angle, float distance_thresh)
{
  if (distance_thresh <= 0.0f)
    {
      /* a zero or negative value represents
       *  non-sense so just say one segment.
       */
      return 1;
    }
  else
    {
      /* The distance between an arc of angle theta and
       * a line connecting the end points of the segment
       * is given by 1 - cos(theta / 2). This is numerically
       * unstable, but algebraically is the same as
       * sin^2(theta / 4). So if the angle is smaller than
       * PI, the distance between the line and the arc is
       * given by:
       *
       *   distance = 2 sin^2 (theta / 4)
       *
       * and we want distance < distance_thresh, so:
       *
       *   sin^2 (theta / 4) < distance_thresh
       *   sin (theta / 4) < sqrt(distance_thresh / 2)
       *   theta < 4 * arcsin(sqrt(distance_thresh / 2))
       */
      float d, needed_sizef, theta, num_half_circles;
      const float pi(FASTUIDRAW_PI);

      /* compute the number of half circles arc_angle has.
       */
      num_half_circles = std::floor(t_abs(arc_angle / pi));

      /* remove the half circles from arc_angle */
      arc_angle = t_max(-pi, t_min(arc_angle, pi));

      /* follow formula outline above with a bounding of
       * the angle away from zero; although analytically
       * we should multiply by 4, this still lets us
       * see bumps, so we ask for twice the number of points.
       */
      d = t_sqrt(0.5f * distance_thresh);
      theta = t_max(0.00001f, 2.0f * std::asin(d));
      needed_sizef = (pi * num_half_circles + t_abs(arc_angle)) / theta;

      /* we ask for one more than necessary, to ensure that we BEAT
       * the tessellation requirement.
       */
      return 1 + fastuidraw::t_max(3u, static_cast<unsigned int>(needed_sizef));
    }
}

void
fastuidraw::detail::
add_triangle(PainterIndex v0, PainterIndex v1, PainterIndex v2,
             c_array<PainterIndex> dst_indices, unsigned int &index_offset)
{
  dst_indices[index_offset++] = v0;
  dst_indices[index_offset++] = v1;
  dst_indices[index_offset++] = v2;
}

void
fastuidraw::detail::
add_triangle_fan(PainterIndex begin, PainterIndex end,
                 c_array<PainterIndex> indices,
                 unsigned int &index_offset)
{
  for(unsigned int i = begin + 1; i < end - 1; ++i, index_offset += 3)
    {
      indices[index_offset + 0] = begin;
      indices[index_offset + 1] = i;
      indices[index_offset + 2] = i + 1;
    }
}
