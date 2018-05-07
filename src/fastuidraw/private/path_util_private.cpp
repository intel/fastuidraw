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
  return number_segments_for_tessellation(arc_angle, P.m_threshhold / radius);
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
   *  the tessellation requirement.
   */
  return 1 + fastuidraw::t_max(3u, static_cast<unsigned int>(needed_sizef));
}

float
fastuidraw::detail::
distance_to_line(const vec2 &q, const vec2 &p1, const vec2 &p2)
{
  vec2 delta(p2 - p1);
  float num, den;

  num = delta.y() * q.x() - delta.x() * q.y() - p2.x() * p1.y() - p1.x() * p2.y();
  den = delta.magnitudeSq();

  return t_sqrt(num / den);
}

void
fastuidraw::detail::
bouding_box_union_arc(const vec2 &center, float radius,
                      float start_angle, float end_angle,
                      BoundingBox<float> *dst)
{
  float delta_angle(start_angle - end_angle);
  float half_angle(0.5f * delta_angle), d;
  vec2 p0, p1, z, z0, z1;

  p0 = vec2(t_cos(start_angle), t_sin(start_angle));
  p1 = vec2(t_cos(end_angle), t_sin(end_angle));

  d = 1.0f - t_cos(delta_angle * 0.5f);
  z = vec2(t_cos(start_angle + half_angle), t_sin(start_angle + half_angle));
  z0 = p0 + d * z;
  z1 = p1 + d * z;

  dst->union_point(center + radius * p0);
  dst->union_point(center + radius * p1);
  dst->union_point(center + radius * z0);
  dst->union_point(center + radius * z1);
}
