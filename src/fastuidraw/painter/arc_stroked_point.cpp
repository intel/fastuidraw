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
                             m_radius,
                             m_arc_angle);

  dst->m_attrib2 = uvec4(m_packed_data,
                         pack_float(m_edge_length),
                         pack_float(m_open_contour_length),
                         pack_float(m_closed_contour_length));
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
  dst->m_radius = unpack_float(a.m_attrib1.z());
  dst->m_arc_angle = unpack_float(a.m_attrib1.w());

  dst->m_packed_data = a.m_attrib2.x();
  dst->m_edge_length = unpack_float(a.m_attrib2.y());
  dst->m_open_contour_length = unpack_float(a.m_attrib2.z());
  dst->m_closed_contour_length = unpack_float(a.m_attrib2.w());
}
