/*!
 * \file stroked_point.cpp
 * \brief file stroked_point.cpp
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
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <private/path_util_private.hpp>

namespace
{
  inline
  void
  set_distance_values(const fastuidraw::TessellatedPath::join &J,
                      fastuidraw::StrokedPoint *pt)
  {
    pt->m_distance_from_edge_start = J.m_distance_from_previous_join;
    pt->m_edge_length = J.m_distance_from_previous_join;
    pt->m_contour_length = J.m_contour_length;
    pt->m_distance_from_contour_start = J.m_distance_from_contour_start;
  }

  inline
  uint32_t
  pack_data(int on_boundary,
            enum fastuidraw::StrokedPoint::offset_type_t pt,
            uint32_t depth)
  {
    return fastuidraw::detail::stroked_point_pack_bits(on_boundary, pt, depth);
  }

  inline
  uint32_t
  pack_data_join(int on_boundary,
                 enum fastuidraw::StrokedPoint::offset_type_t pt,
                 uint32_t depth)
  {
    return pack_data(on_boundary, pt, depth) | fastuidraw::StrokedPoint::join_mask;
  }

  void
  pack_bevel_join(const fastuidraw::TessellatedPath::join &J,
                  unsigned int depth,
                  fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                  fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                  unsigned int index_adjust)
  {
    fastuidraw::StrokedPoint pt;
    set_distance_values(J, &pt);

    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.enter_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[0]);

    pt.m_position = J.m_position;
    pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[1]);

    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.leaving_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[2]);

    fastuidraw::detail::add_triangle_fan(index_adjust, index_adjust + 3, dst_indices);
  }

  void
  pack_miter_clip_join(const fastuidraw::TessellatedPath::join &J,
                       unsigned int depth,
                       fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                       fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                       unsigned int index_adjust)
  {
    fastuidraw::StrokedPoint pt;
    set_distance_values(J, &pt);

    // join center point.
    pt.m_position = J.m_position;
    pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[0]);

    // join point from curve into join
    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.enter_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[1]);

    // miter point A
    pt.m_position = J.m_position;
    pt.m_pre_offset = J.enter_join_normal();
    pt.m_auxiliary_offset = J.leaving_join_normal();
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_miter_clip_join, depth);
    pt.pack_point(&dst_attribs[2]);

    // miter point B
    pt.m_position = J.m_position;
    pt.m_pre_offset = J.leaving_join_normal();
    pt.m_auxiliary_offset = J.enter_join_normal();
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_miter_clip_join, depth);
    pt.m_packed_data |= fastuidraw::StrokedPoint::lambda_negated_mask;
    pt.pack_point(&dst_attribs[3]);

    // join point from curve out from join
    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.leaving_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[4]);

    fastuidraw::detail::add_triangle_fan(index_adjust, index_adjust + 5, dst_indices);
  }

  template<enum fastuidraw::StrokedPoint::offset_type_t tp>
  void
  pack_miter_join(const fastuidraw::TessellatedPath::join &J,
                  unsigned int depth,
                  fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                  fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                  unsigned int index_adjust)
  {
    fastuidraw::StrokedPoint pt;
    set_distance_values(J, &pt);

    // join center point.
    pt.m_position = J.m_position;
    pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[0]);

    // join point from curve into join
    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.enter_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[1]);

    // miter point
    pt.m_position = J.m_position;
    pt.m_pre_offset = J.enter_join_normal();
    pt.m_auxiliary_offset = J.leaving_join_normal();
    pt.m_packed_data = pack_data_join(1, tp, depth);
    pt.pack_point(&dst_attribs[2]);

    // join point from curve out from join
    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.leaving_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[3]);

    fastuidraw::detail::add_triangle_fan(index_adjust, index_adjust + 4, dst_indices);
  }

  void
  pack_rounded_join(const fastuidraw::TessellatedPath::join &J,
                    unsigned int depth,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                    fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                    unsigned int index_adjust)
  {
    /* the number of attributes will determine how many arc-points we have */
    unsigned int i, num_arc_points, current_vertex(0);
    fastuidraw::StrokedPoint pt;
    float theta, delta_theta;
    fastuidraw::vec2 n0(J.m_lambda * J.enter_join_normal());
    fastuidraw::vec2 n1(J.m_lambda * J.leaving_join_normal());
    fastuidraw::vec2 pre_offset(n0.x(), n1.x());
    std::complex<float> start(n0.x(), n0.y());

    num_arc_points = dst_attribs.size() - 1;
    delta_theta = J.m_join_angle / static_cast<float>(num_arc_points - 1);
    set_distance_values(J, &pt);

    pt.m_position = J.m_position;
    pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.enter_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    for(i = 1, theta = delta_theta; i < num_arc_points - 1; ++i, theta += delta_theta, ++current_vertex)
      {
        float t, c, s;
        std::complex<float> cs_as_complex;

        t = static_cast<float>(i) / static_cast<float>(num_arc_points - 1);
        c = fastuidraw::t_cos(theta);
        s = fastuidraw::t_sin(theta);
        cs_as_complex = std::complex<float>(c, s) * start;

        pt.m_position = J.m_position;
        pt.m_pre_offset = pre_offset;
        pt.m_auxiliary_offset = fastuidraw::vec2(t, cs_as_complex.real());
        pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_rounded_join, depth);

        if (n0.y() < 0.0f)
          {
            pt.m_packed_data |= fastuidraw::StrokedPoint::normal0_y_sign_mask;
          }

        if (n1.y() < 0.0f)
          {
            pt.m_packed_data |= fastuidraw::StrokedPoint::normal1_y_sign_mask;
          }

        if (cs_as_complex.imag() < 0.0f)
          {
            pt.m_packed_data |= fastuidraw::StrokedPoint::sin_sign_mask;
          }
        pt.pack_point(&dst_attribs[current_vertex]);
      }

    pt.m_position = J.m_position;
    pt.m_pre_offset = J.m_lambda * J.leaving_join_normal();
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    fastuidraw::detail::add_triangle_fan(index_adjust, index_adjust + current_vertex, dst_indices);
  }
}

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
                         pack_float(m_contour_length),
                         0u);
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
  dst->m_contour_length = unpack_float(a.m_attrib2.z());
}

void
fastuidraw::StrokedPoint::
pack_size(enum PainterEnums::join_style js,
          unsigned int *num_attributes,
          unsigned int *num_indices)
{
  switch (js)
    {
    case PainterEnums::no_joins:
      *num_attributes = 0;
      *num_indices = 0;
      break;

    case PainterEnums::bevel_joins:
      *num_attributes = 3;
      *num_indices = 3;
      break;

    case PainterEnums::miter_clip_joins:
      *num_attributes = 5;
      *num_indices = 9;
      break;

    case PainterEnums::miter_bevel_joins:
      *num_attributes = 4;
      *num_indices = 6;
      break;

    case PainterEnums::miter_joins:
      *num_attributes = 4;
      *num_indices = 6;
      break;

    default:
      FASTUIDRAWmessaged_assert(false, "Invalid join style passed to pack_size()");
      *num_attributes = 0;
      *num_indices = 0;
    }
}

void
fastuidraw::StrokedPoint::
pack_rounded_size(const TessellatedPath::join &join,
                  float thresh,
                  unsigned int *num_attributes,
                  unsigned int *num_indices)
{
  unsigned int num_arc_points;

  num_arc_points = detail::number_segments_for_tessellation(join.m_join_angle, thresh);
  *num_attributes = 1 + num_arc_points;
  *num_indices = 3 * (num_arc_points - 1);
}

void
fastuidraw::StrokedPoint::
pack_join(enum PainterEnums::join_style js,
          const TessellatedPath::join &join,
          unsigned int depth,
          c_array<PainterAttribute> dst_attribs,
          c_array<PainterIndex> dst_indices,
          unsigned int index_adjust)
{
  switch (js)
    {
    case PainterEnums::no_joins:
      break;

    case PainterEnums::bevel_joins:
      pack_bevel_join(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case PainterEnums::miter_clip_joins:
      pack_miter_clip_join(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case PainterEnums::miter_bevel_joins:
      pack_miter_join<offset_miter_bevel_join>(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case PainterEnums::miter_joins:
      pack_miter_join<offset_miter_join>(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case PainterEnums::rounded_joins:
      pack_rounded_join(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    default:
      FASTUIDRAWmessaged_assert(false, "Invalid join style passed to pack_size()");
      return;
    }
}
