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

bool
fastuidraw::detail::
clip_against_plane(const vec3 &clip_eq, const_c_array<vec2> pts,
                   std::vector<vec2> &out_pts, std::vector<float> &work_room)
{
  /* clip the convex polygon of pts, placing the results
     into out_pts.
   */
  bool all_clipped, all_unclipped;
  unsigned int first_unclipped;

  if(pts.empty())
    {
      out_pts.resize(0);
      return false;
    }

  work_room.resize(pts.size());
  all_clipped = true;
  all_unclipped = true;
  first_unclipped = pts.size();
  for(unsigned int i = 0; i < pts.size(); ++i)
    {
      work_room[i] = clip_eq.x() * pts[i].x() + clip_eq.y() * pts[i].y() + clip_eq.z();
      all_clipped = all_clipped && work_room[i] < 0.0f;
      all_unclipped = all_unclipped && work_room[i] >= 0.0f;
      if(first_unclipped == pts.size() && work_room[i] >= 0.0f)
        {
          first_unclipped = i;
        }
    }

  if(all_clipped)
    {
      /* all clipped, nothing to do!
       */
      out_pts.resize(0);
      return false;
    }

  if(all_unclipped)
    {
      out_pts.resize(pts.size());
      std::copy(pts.begin(), pts.end(), out_pts.begin());
      return true;
    }

  /* the polygon is convex, and atleast one point is clipped, thus
     the clip line goes through 2 edges.
   */
  fastuidraw::vecN<std::pair<unsigned int, unsigned int>, 2> edges;
  unsigned int num_edges(0);

  for(unsigned int i = 0, k = first_unclipped; i < pts.size() && num_edges < 2; ++i, ++k)
    {
      bool b0, b1;

      if(k == pts.size())
        {
          k = 0;
        }
      assert(k < pts.size());

      unsigned int next_k(k+1);
      if(next_k == pts.size())
        {
          next_k = 0;
        }
      assert(next_k < pts.size());

      b0 = work_room[k] >= 0.0f;
      b1 = work_room[next_k] >= 0.0f;
      if(b0 != b1)
        {
          edges[num_edges] = std::pair<unsigned int, unsigned int>(k, next_k);
          ++num_edges;
        }
    }

  assert(num_edges == 2);

  out_pts.reserve(pts.size() + 1);
  out_pts.resize(0);

  /* now add the points that are unclipped (in order)
     and the 2 new points representing the new points
     added by the clipping plane.
   */

  for(unsigned int i = first_unclipped; i <= edges[0].first; ++i)
    {
      out_pts.push_back(pts[i]);
    }

  /* now add the implicitely made vertex of the clip equation
     intersecting against the edge between pts[edges[0].first] and
     pts[edges[0].second]
   */
  {
    fastuidraw::vec2 pp;
    float t;

    t = -work_room[edges[0].first] / (work_room[edges[0].second] - work_room[edges[0].first]);
    pp = pts[edges[0].first] + t * (pts[edges[0].second] - pts[edges[0].first]);
    out_pts.push_back(pp);
  }

  /* the vertices from pts[edges[0].second] to pts[edges[1].first]
     are all on the clipped size of the plane, so they are skipped.
   */

  /* now add the implicitely made vertex of the clip equation
     intersecting against the edge between pts[edges[1].first] and
     pts[edges[1].second]
   */
  {
    fastuidraw::vec2 pp;
    float t;

    t = -work_room[edges[1].first] / (work_room[edges[1].second] - work_room[edges[1].first]);
    pp = pts[edges[1].first] + t * (pts[edges[1].second] - pts[edges[1].first]);
    out_pts.push_back(pp);
  }

  /* add all vertices starting from pts[edges[1].second] wrapping
     around until the points are clipped again.
   */
  for(unsigned int i = edges[1].second; i != first_unclipped && work_room[i] >= 0.0f;)
    {
      out_pts.push_back(pts[i]);
      ++i;
      if(i == pts.size())
        {
          i = 0;
        }
    }

  return false;
}

bool
fastuidraw::detail::
clip_against_planes(const_c_array<vec3> clip_eq, const_c_array<vec2> in_pts,
                    std::vector<vec2> &out_pts,
                    std::vector<float> &scratch_space_floats,
                    vecN<std::vector<vec2>, 2> &scratch_space_vec2s)
{
  unsigned int src(0), dst(1), i;
  bool return_value(true);

  scratch_space_vec2s[src].resize(in_pts.size());
  std::copy(in_pts.begin(), in_pts.end(), scratch_space_vec2s[src].begin());

  for(i = 0; i < clip_eq.size(); ++i, std::swap(src, dst))
    {
      bool r;
      r = clip_against_plane(clip_eq[i], make_c_array(scratch_space_vec2s[src]),
                             scratch_space_vec2s[dst],
                             scratch_space_floats);
      return_value = return_value && r;
    }
  std::swap(out_pts, scratch_space_vec2s[src]);
  return return_value;
}
