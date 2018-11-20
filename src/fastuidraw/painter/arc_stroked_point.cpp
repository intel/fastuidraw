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

#include <fastuidraw/painter/arc_stroked_point.hpp>

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
