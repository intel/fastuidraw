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

fastuidraw::vec2
fastuidraw::StrokedPoint::
offset_vector(void)
{
  enum offset_type_t tp;

  tp = offset_type();

  switch(tp)
    {
    case offset_start_sub_edge:
    case offset_end_sub_edge:
    case offset_shared_with_edge:
      return m_pre_offset;

    case offset_square_cap:
      return m_pre_offset + m_auxiliary_offset;

    case offset_rounded_cap:
      {
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        return m_auxiliary_offset.x() * v + m_auxiliary_offset.y() * n;
      }

    case offset_miter_clip_join:
    case offset_miter_clip_join_lambda_negated:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxiliary_offset), Jn1(n1.y(), -n1.x());
        float r, det, lambda;

        det = dot(Jn1, n0);
        lambda = -t_sign(det);
        r = (det != 0.0f) ? (dot(n0, n1) - 1.0f) / det : 0.0f;

        if (tp == offset_miter_clip_join_lambda_negated)
          {
            lambda = -lambda;
          }

        return lambda * (n0 + r * Jn0);
      }

    case offset_miter_bevel_join:
    case offset_miter_join:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxiliary_offset), Jn1(n1.y(), -n1.x());
        float r, lambda, den;

        lambda = t_sign(dot(Jn0, n1));
        den = 1.0f + dot(n0, n1);
        r = (den != 0.0f) ? lambda / den : 0.0f;
        return r * (n0 + n1);
      }

    case offset_rounded_join:
      {
        vec2 cs;

        cs.x() = m_auxiliary_offset.y();
        cs.y() = sqrt(1.0 - cs.x() * cs.x());
        if (m_packed_data & sin_sign_mask)
          {
            cs.y() = -cs.y();
          }
        return cs;
      }

    default:
      return vec2(0.0f, 0.0f);
    }
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
    case offset_miter_clip_join_lambda_negated:
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
