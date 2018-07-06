/*!
 * \file clip.cpp
 * \brief file clip.cpp
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

#include "clip.hpp"
#include "util_private.hpp"

namespace
{
  inline
  float
  compute_clip_dist(const fastuidraw::vec3 &clip_eq,
                    const fastuidraw::vec2 &pt)
  {
    return clip_eq.x() * pt.x() + clip_eq.y() * pt.y() + clip_eq.z();
  }

  inline
  fastuidraw::vec2
  compute_intersection(const fastuidraw::vec2 &p0, float d0,
                       const fastuidraw::vec2 &p1, float d1)
  {
    float t;

    t = d0 / (d0 - d1);
    return (1.0 - t) * p0 + t * p1;
  }
}

bool
fastuidraw::detail::
clip_against_plane(const vec3 &clip_eq, c_array<const vec2> pts,
                   std::vector<vec2> &out_pts)
{
  if (pts.empty())
    {
      return false;
    }

  vec2 prev(pts.back());
  float prev_d(compute_clip_dist(clip_eq, prev));
  bool prev_in(prev_d >= 0.0f), unclipped(true);

  out_pts.clear();
  for (unsigned int i = 0; i < pts.size(); ++i)
    {
      const vec2 &current(pts[i]);
      float current_d(compute_clip_dist(clip_eq, pts[i]));
      bool current_in(current_d >= 0.0f);

      unclipped = unclipped && current_in;
      if (current_in != prev_in)
        {
          vec2 q;
          q = compute_intersection(prev, prev_d,
                                   current, current_d);
          out_pts.push_back(q);
        }

      if (current_in)
        {
          out_pts.push_back(current);
        }
      prev = current;
      prev_d = current_d;
      prev_in = current_in;
    }

  return unclipped;
}

bool
fastuidraw::detail::
clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec2> in_pts,
                    std::vector<vec2> &out_pts,
                    vecN<std::vector<vec2>, 2> &scratch_space_vec2s)
{
  unsigned int src(0), dst(1), i;
  bool unclipped(true);

  scratch_space_vec2s[src].resize(in_pts.size());
  std::copy(in_pts.begin(), in_pts.end(), scratch_space_vec2s[src].begin());

  for(i = 0; i < clip_eq.size() && !scratch_space_vec2s[src].empty(); ++i, std::swap(src, dst))
    {
      bool r;
      r = clip_against_plane(clip_eq[i],
                             make_c_array(scratch_space_vec2s[src]),
                             scratch_space_vec2s[dst]);
      unclipped = unclipped && r;
    }
  std::swap(out_pts, scratch_space_vec2s[src]);
  return unclipped;
}
