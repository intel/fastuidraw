/*!
 * \file arc_stroked_point.cpp
 * \brief file arc_stroked_point.cpp
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

#include <complex>
#include <algorithm>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>
#include <private/path_util_private.hpp>

namespace
{
  void
  set_distance_values(const fastuidraw::TessellatedPath::join &J,
                      fastuidraw::ArcStrokedPoint *pt)
  {
    pt->m_distance_from_edge_start = J.m_distance_from_previous_join;
    pt->m_edge_length = J.m_distance_from_previous_join;
    pt->m_contour_length = J.m_contour_length;
    pt->m_distance_from_contour_start = J.m_distance_from_contour_start;
  }

  void
  set_distance_values(const fastuidraw::TessellatedPath::cap &C,
                      fastuidraw::ArcStrokedPoint *pt)
  {
    pt->m_distance_from_edge_start = (C.m_is_starting_cap) ? 0.0f : C.m_edge_length;
    pt->m_edge_length = C.m_edge_length;
    pt->m_contour_length = C.m_contour_length;
    pt->m_distance_from_contour_start = (C.m_is_starting_cap) ? 0.0f : C.m_contour_length;
  }
}

//////////////////////////////////////
// fastuidraw::ArcStrokedPoint methods
void
fastuidraw::ArcStrokedPoint::
pack_point(PainterAttribute *dst) const
{
  dst->m_attrib0 = pack_vec4(m_position.x(),
                             m_position.y(),
                             m_offset_direction.x(),
                             m_offset_direction.y());

  dst->m_attrib1 = pack_vec4(m_distance_from_edge_start,
                             m_distance_from_contour_start,
                             m_data[0],
                             m_data[1]);

  dst->m_attrib2 = uvec4(m_packed_data,
                         pack_float(m_edge_length),
                         pack_float(m_contour_length),
                         0u);
}

void
fastuidraw::ArcStrokedPoint::
unpack_point(ArcStrokedPoint *dst, const PainterAttribute &a)
{
  dst->m_position.x() = unpack_float(a.m_attrib0.x());
  dst->m_position.y() = unpack_float(a.m_attrib0.y());

  dst->m_offset_direction.x() = unpack_float(a.m_attrib0.z());
  dst->m_offset_direction.y() = unpack_float(a.m_attrib0.w());

  dst->m_distance_from_edge_start = unpack_float(a.m_attrib1.x());
  dst->m_distance_from_contour_start = unpack_float(a.m_attrib1.y());
  dst->m_data[0] = unpack_float(a.m_attrib1.z());
  dst->m_data[1] = unpack_float(a.m_attrib1.w());

  dst->m_packed_data = a.m_attrib2.x();
  dst->m_edge_length = unpack_float(a.m_attrib2.y());
  dst->m_contour_length = unpack_float(a.m_attrib2.z());
}

void
fastuidraw::ArcStrokedPointPacking::
pack_join_size(const TessellatedPath::join &join,
               unsigned int *num_attributes,
               unsigned int *num_indices)
{
  const float per_arc_angle_max(FASTUIDRAW_PI / arcs_per_cap);
  float delta_angle_mag;
  unsigned int count;

  delta_angle_mag = fastuidraw::t_abs(join.m_join_angle);
  count = 1u + static_cast<unsigned int>(delta_angle_mag / per_arc_angle_max);

  *num_attributes = 3u * count + 2u;
  *num_indices = 9u * count;
}

void
fastuidraw::ArcStrokedPointPacking::
pack_join(const TessellatedPath::join &join,
          unsigned int depth,
          c_array<PainterAttribute> dst_attribs,
          c_array<PainterIndex> dst_indices,
          unsigned int index_adjust)
{
  unsigned int count, num_verts_used(0), num_indices_used(0);

  count = dst_indices.size() / 9u;
  FASTUIDRAWassert(9u * count == dst_indices.size());
  FASTUIDRAWassert(3u * count + 2u == dst_attribs.size());

  ArcStrokedPoint pt;

  set_distance_values(join, &pt);
  pt.m_position = join.m_position;
  detail::pack_arc_join(pt, count, join.m_lambda * join.enter_join_normal(),
                        join.m_join_angle, join.m_lambda * join.leaving_join_normal(),
                        depth, dst_attribs, num_verts_used,
                        dst_indices, num_indices_used, true);
  FASTUIDRAWassert(num_verts_used == dst_attribs.size());
  FASTUIDRAWassert(num_indices_used == dst_indices.size());
  for (PainterIndex &idx : dst_indices)
    {
      idx += index_adjust;
    }
}

void
fastuidraw::ArcStrokedPointPacking::
pack_cap(const TessellatedPath::cap &C,
         unsigned int depth,
         c_array<PainterAttribute> dst_attribs,
         c_array<PainterIndex> dst_indices,
         unsigned int index_adjust)
{
  FASTUIDRAWassert(dst_attribs.size() == num_attributes_per_cap);
  FASTUIDRAWassert(dst_indices.size() == num_indices_per_cap);

  ArcStrokedPoint pt;
  unsigned num_verts_used(0), num_indices_used(0);
  vec2 v(C.m_unit_vector), n(v.y(), -v.x());

  set_distance_values(C, &pt);
  pt.m_position = C.m_position;
  detail::pack_arc_join(pt, arcs_per_cap,
                        n, FASTUIDRAW_PI, -n,
                        depth, dst_attribs, num_verts_used,
                        dst_indices, num_indices_used, true);
  FASTUIDRAWassert(num_verts_used == dst_attribs.size());
  FASTUIDRAWassert(num_indices_used == dst_indices.size());
  for (PainterIndex &idx : dst_indices)
    {
      idx += index_adjust;
    }
}
