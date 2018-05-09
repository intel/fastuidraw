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
#include <fastuidraw/painter/arc_stroked_point.hpp>
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

void
fastuidraw::detail::
compute_arc_join_size(unsigned int cnt,
                      unsigned int *out_vertex_cnt,
                      unsigned int *out_index_cnt)
{
  *out_vertex_cnt = 3 * (cnt - 1) + 5;
  *out_index_cnt = 9 * cnt;
}

void
fastuidraw::detail::
pack_arc_join(ArcStrokedPoint pt, unsigned int count,
              vec2 n_start, float delta_angle, vec2 n_end,
              unsigned int depth,
              c_array<PainterAttribute> dst_pts,
              unsigned int &vertex_offset,
              c_array<PainterIndex> dst_indices,
              unsigned int &index_offset)
{
  float per_element(delta_angle / static_cast<float>(count));
  std::complex<float> arc_start(n_start.x(), n_start.y());
  std::complex<float> da(t_cos(per_element), t_sin(per_element));
  std::complex<float> theta;

  unsigned int i, center;
  uint32_t arc_value, beyond_arc_value;

  center = vertex_offset;
  arc_value = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point_outer_stroking_boundary, depth);
  beyond_arc_value = arc_value | ArcStrokedPoint::beyond_boundary_mask;

  pt.radius() = 0.0f;
  pt.arc_angle() = per_element;
  pt.m_offset_direction = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point_on_path, depth);
  pt.pack_point(&dst_pts[vertex_offset++]);

  for (theta = arc_start, i = 0; i <= count; ++i, theta *= da)
    {
      vec2 n;
      unsigned int start;

      if (i == 0)
        {
          n = n_start;
        }
      else if (i == count)
        {
          n = n_end;
        }
      else
        {
          n = vec2(theta.real(), theta.imag());
        }

      pt.m_offset_direction = n;

      if (i != 0)
        {
          pt.m_packed_data = beyond_arc_value | ArcStrokedPoint::end_segment_mask;
          pt.pack_point(&dst_pts[vertex_offset++]);
        }

      start = vertex_offset;
      pt.m_packed_data = arc_value;
      pt.pack_point(&dst_pts[vertex_offset++]);

      if (i != count)
        {
          pt.m_packed_data = beyond_arc_value;
          pt.pack_point(&dst_pts[vertex_offset++]);

          add_triangle(center, start, vertex_offset + 1, dst_indices, index_offset);
          add_triangle(start, start + 1, vertex_offset, dst_indices, index_offset);
          add_triangle(start, vertex_offset, vertex_offset + 1, dst_indices, index_offset);
        }
    }
}

void
fastuidraw::detail::
pack_arc_join(ArcStrokedPoint pt, unsigned int count,
              vec2 n0, vec2 n1, unsigned int depth,
              c_array<PainterAttribute> dst_pts,
              unsigned int &vertex_offset,
              c_array<PainterIndex> dst_indices,
              unsigned int &index_offset)
{
  std::complex<float> n0z(n0.x(), n0.y());
  std::complex<float> n1z(n1.x(), n1.y());
  std::complex<float> n1z_times_conj_n0z(n1z * std::conj(n0z));
  float angle(std::atan2(n1z_times_conj_n0z.imag(), n1z_times_conj_n0z.real()));

  pack_arc_join(pt, count, n0, angle, n1, depth,
                dst_pts, vertex_offset,
                dst_indices, index_offset);
}
