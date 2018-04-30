#include <fastuidraw/painter/stroked_point.hpp>

//////////////////////////////////////
// fastuidraw::StrokedPoint methods
void
fastuidraw::StrokedPoint::
pack_point(PainterAttribute *dst) const
{
  dst->m_attrib0 = pack_vec4(m_position.x(),
                             m_position.y(),
                             m_pre_offset.x(),
                             m_pre_offset.y());

  dst->m_attrib1 = pack_vec4(m_distance_from_edge_start,
                             m_distance_from_contour_start,
                             m_auxiliary_offset.x(),
                             m_auxiliary_offset.y());

  dst->m_attrib2 = uvec4(m_packed_data,
                         pack_float(m_edge_length),
                         pack_float(m_open_contour_length),
                         pack_float(m_closed_contour_length));
}

void
fastuidraw::StrokedPoint::
unpack_point(StrokedPoint *dst, const PainterAttribute &a)
{
  dst->m_position.x() = unpack_float(a.m_attrib0.x());
  dst->m_position.y() = unpack_float(a.m_attrib0.y());

  dst->m_pre_offset.x() = unpack_float(a.m_attrib0.z());
  dst->m_pre_offset.y() = unpack_float(a.m_attrib0.w());

  dst->m_distance_from_edge_start = unpack_float(a.m_attrib1.x());
  dst->m_distance_from_contour_start = unpack_float(a.m_attrib1.y());
  dst->m_auxiliary_offset.x() = unpack_float(a.m_attrib1.z());
  dst->m_auxiliary_offset.y() = unpack_float(a.m_attrib1.w());

  dst->m_packed_data = a.m_attrib2.x();
  dst->m_edge_length = unpack_float(a.m_attrib2.y());
  dst->m_open_contour_length = unpack_float(a.m_attrib2.z());
  dst->m_closed_contour_length = unpack_float(a.m_attrib2.w());
}

float
fastuidraw::StrokedPoint::
miter_distance(void) const
{
  enum offset_type_t tp;
  tp = offset_type();

  switch(tp)
    {
    case offset_miter_clip_join:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxiliary_offset), Jn1(n1.y(), -n1.x());
        float r, det;

        det = dot(Jn1, n0);
        r = (det != 0.0f) ? (dot(n0, n1) - 1.0f) / det : 0.0f;
        return t_sqrt(1.0f + r * r);
      }

    case offset_miter_bevel_join:
    case offset_miter_join:
      {
        vec2 n0(m_pre_offset), n1(m_auxiliary_offset);
        vec2 n0_plus_n1(n0 + n1);
        float den;

        den = 1.0f + dot(n0, n1);
        return den != 0.0f ?
          n0_plus_n1.magnitude() / den :
          0.0f;
      }

    default:
      return 0.0f;
    }
}
