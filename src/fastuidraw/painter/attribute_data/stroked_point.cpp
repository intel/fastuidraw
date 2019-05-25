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
#include <algorithm>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <private/path_util_private.hpp>

namespace
{
  inline
  uint32_t
  stroked_point_pack_bits(int on_boundary,
                          enum fastuidraw::StrokedPoint::offset_type_t pt,
                          uint32_t depth)
  {
    FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);
    const uint32_t max_depth(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(fastuidraw::StrokedPoint::depth_num_bits));

    /* clamp depth to maximum allowed value */
    depth = fastuidraw::t_min(depth, max_depth);
    return pack_bits(fastuidraw::StrokedPoint::offset_type_bit0,
                     fastuidraw::StrokedPoint::offset_type_num_bits, pp)
      | pack_bits(fastuidraw::StrokedPoint::boundary_bit, 1u, bb)
      | pack_bits(fastuidraw::StrokedPoint::depth_bit0,
                  fastuidraw::StrokedPoint::depth_num_bits, depth);
  }

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

  void
  set_distance_values(const fastuidraw::TessellatedPath::cap &C,
                      fastuidraw::StrokedPoint *pt)
  {
    pt->m_distance_from_edge_start = (C.m_is_starting_cap) ? 0.0f : C.m_edge_length;
    pt->m_edge_length = C.m_edge_length;
    pt->m_contour_length = C.m_contour_length;
    pt->m_distance_from_contour_start = (C.m_is_starting_cap) ? 0.0f : C.m_contour_length;
  }

  inline
  uint32_t
  pack_data(int on_boundary,
            enum fastuidraw::StrokedPoint::offset_type_t pt,
            uint32_t depth)
  {
    return stroked_point_pack_bits(on_boundary, pt, depth);
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

  void
  pack_square_cap(const fastuidraw::TessellatedPath::cap &C,
                  unsigned int depth,
                  fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                  fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                  unsigned int index_adjust)
  {
    fastuidraw::vec2 n, v;
    fastuidraw::StrokedPoint pt;
    unsigned int current_vertex(0);

    v = C.m_unit_vector;
    n = fastuidraw::vec2(-v.y(), v.x());
    set_distance_values(C, &pt);

    pt.m_position = C.m_position;
    pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = n;
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = n;
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_square_cap, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = -n;
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_square_cap, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = -n;
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    fastuidraw::detail::add_triangle_fan(index_adjust, index_adjust + current_vertex, dst_indices);
  }

  void
  pack_adjustable_or_flat_cap(const enum fastuidraw::StrokedPoint::offset_type_t type,
                              const fastuidraw::TessellatedPath::cap &C,
                              unsigned int depth,
                              fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                              fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                              unsigned int index_adjust)
  {
    using namespace fastuidraw;

    vec2 n, v;
    StrokedPoint pt;
    uint32_t mask, end_contour_mask, end_cap_mask;
    unsigned int current_vertex(0);

    FASTUIDRAWassert(type == StrokedPoint::offset_adjustable_cap
                     || type == StrokedPoint::offset_flat_cap);

    end_contour_mask = (type == StrokedPoint::offset_adjustable_cap) ?
      StrokedPoint::adjustable_cap_is_end_contour_mask :
      StrokedPoint::flat_cap_is_end_contour_mask;

    end_cap_mask = (type == StrokedPoint::offset_adjustable_cap) ?
      StrokedPoint::adjustable_cap_ending_mask :
      StrokedPoint::flat_cap_ending_mask;

    mask = (C.m_is_starting_cap) ? 0u : end_contour_mask;

    v = C.m_unit_vector;
    n = vec2(-v.y(), v.x());
    set_distance_values(C, &pt);

    pt.m_position = C.m_position;
    pt.m_pre_offset = vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(0, type, depth) | mask;
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = n;
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(1, type, depth) | mask;
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = n;
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(1, type, depth) | end_cap_mask | mask;
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(0, type, depth) | end_cap_mask | mask;
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = -n;
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(1, type, depth) | end_cap_mask | mask;
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = -n;
    pt.m_auxiliary_offset = v;
    pt.m_packed_data = pack_data(1, type, depth) | mask;
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    detail::add_triangle_fan(index_adjust, index_adjust + current_vertex, dst_indices);
  }

  void
  pack_rounded_cap(const fastuidraw::TessellatedPath::cap &C,
                   unsigned int depth,
                   fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                   fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                   unsigned int index_adjust)
  {
    unsigned int i, num_arc_points_per_cap, current_vertex(0);
    float theta, delta_theta;
    fastuidraw::vec2 n, v;
    fastuidraw::StrokedPoint pt;

    v = C.m_unit_vector;
    n = fastuidraw::vec2(-v.y(), v.x());
    set_distance_values(C, &pt);

    num_arc_points_per_cap = dst_attribs.size() - 1;
    delta_theta = FASTUIDRAW_PI / static_cast<float>(num_arc_points_per_cap - 1);

    pt.m_position = C.m_position;
    pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    pt.m_position = C.m_position;
    pt.m_pre_offset = n;
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    for(i = 1, theta = delta_theta; i < num_arc_points_per_cap - 1; ++i, theta += delta_theta, ++current_vertex)
      {
        float s, c;

        s = fastuidraw::t_sin(theta);
        c = fastuidraw::t_cos(theta);
        pt.m_position = C.m_position;
        pt.m_pre_offset = n;
        pt.m_auxiliary_offset = fastuidraw::vec2(s, c);
        pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_rounded_cap, depth);
        pt.pack_point(&dst_attribs[current_vertex]);
      }

    pt.m_position = C.m_position;
    pt.m_pre_offset = -n;
    pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
    pt.pack_point(&dst_attribs[current_vertex]);
    ++current_vertex;

    fastuidraw::detail::add_triangle_fan(index_adjust, index_adjust + current_vertex, dst_indices);
  }

  bool
  segment_has_bevel(const fastuidraw::TessellatedPath::segment *prev,
                    const fastuidraw::TessellatedPath::segment &S)
  {
    /* Bevel requires that the previous segment is not
     * a continuation of the previous, that there is previous.
     * Lastly, if the segment changes direction (indicated
     * by the dot() of the vectors begin negative), then
     * it is likely at a cusp of the curve and cusps do not
     * get bevel eithers.
     */
    return !S.m_continuation_with_predecessor
      && prev
      && fastuidraw::dot(prev->m_leaving_segment_unit_vector,
                         S.m_enter_segment_unit_vector) >= 0.0f;
  }

  void
  pack_segment_bevel(bool is_inner_bevel,
                     const fastuidraw::TessellatedPath::segment *prev,
                     const fastuidraw::TessellatedPath::segment &S,
                     unsigned int &current_depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                     unsigned int &vertex_offset,
                     fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                     unsigned int &index_offset,
                     unsigned int index_adjust)
  {
    using namespace fastuidraw;
    using namespace detail;

    StrokedPoint pt;
    float lambda;
    vec2 start_bevel, end_bevel;

    for (unsigned int k = 0; k < 3; ++k, ++index_offset)
      {
        dst_indices[index_offset] = k + index_adjust + vertex_offset;
      }

    lambda = compute_bevel_lambda(is_inner_bevel, prev, S,
                                  start_bevel, end_bevel);

    pt.m_position = S.m_start_pt;
    pt.m_distance_from_edge_start = S.m_distance_from_edge_start;
    pt.m_distance_from_contour_start = S.m_distance_from_contour_start;
    pt.m_edge_length = S.m_edge_length;
    pt.m_contour_length = S.m_contour_length;
    pt.m_auxiliary_offset = vec2(0.0f, 0.0f);

    pt.m_pre_offset = vec2(0.0f, 0.0f);
    pt.m_packed_data = pack_data(0, StrokedPoint::offset_sub_edge, current_depth)
      | StrokedPoint::bevel_edge_mask;
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_pre_offset = lambda * start_bevel;
    pt.m_packed_data = pack_data(1, StrokedPoint::offset_sub_edge, current_depth)
      | StrokedPoint::bevel_edge_mask;
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_pre_offset = lambda * end_bevel;
    pt.m_packed_data = pack_data(1, StrokedPoint::offset_sub_edge, current_depth)
      | StrokedPoint::bevel_edge_mask;
    pt.pack_point(&dst_attribs[vertex_offset++]);

    if (is_inner_bevel)
      {
        ++current_depth;
      }
  }

  void
  pack_segment(const fastuidraw::TessellatedPath::segment &S,
               unsigned int &current_depth,
               fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
               unsigned int &current_vertex,
               fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
               unsigned int &index_offset,
               unsigned int index_adjust)
  {
    using namespace fastuidraw;

    const int boundary_values[3] = { 1, 1, 0 };
    const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
    vecN<StrokedPoint, 6> pts;
    vec2 delta(S.m_end_pt - S.m_start_pt);
    vec2 begin_normal(-S.m_enter_segment_unit_vector.y(), S.m_enter_segment_unit_vector.x());
    vec2 end_normal(-S.m_leaving_segment_unit_vector.y(), S.m_leaving_segment_unit_vector.x());

    /* The quad is:
     * (p, n, delta,  1),
     * (p,-n, delta,  1),
     * (p, 0,     0,  0),
     * (p_next,  n, -delta, 1),
     * (p_next, -n, -delta, 1),
     * (p_next,  0, 0)
     */
    for(unsigned int k = 0; k < 3; ++k)
      {
        pts[k].m_position = S.m_start_pt;
        pts[k].m_distance_from_edge_start = S.m_distance_from_edge_start;
        pts[k].m_distance_from_contour_start = S.m_distance_from_contour_start;
        pts[k].m_edge_length = S.m_edge_length;
        pts[k].m_contour_length = S.m_contour_length;
        pts[k].m_pre_offset = normal_sign[k] * begin_normal;
        pts[k].m_auxiliary_offset = delta;
        pts[k].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, current_depth);

        pts[k + 3].m_position = S.m_end_pt;
        pts[k + 3].m_distance_from_edge_start = S.m_distance_from_edge_start + S.m_length;
        pts[k + 3].m_distance_from_contour_start = S.m_distance_from_contour_start + S.m_length;
        pts[k + 3].m_edge_length = S.m_edge_length;
        pts[k + 3].m_contour_length = S.m_contour_length;
        pts[k + 3].m_pre_offset = normal_sign[k] * end_normal;
        pts[k + 3].m_auxiliary_offset = -delta;
        pts[k + 3].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, current_depth)
          | StrokedPoint::end_sub_edge_mask;
      }

    const unsigned int tris[12] =
      {
        0, 2, 5,
        0, 5, 3,
        2, 1, 4,
        2, 4, 5
      };

    for(unsigned int i = 0; i < 12; ++i, ++index_offset)
      {
        dst_indices[index_offset] = current_vertex + index_adjust + tris[i];
      }

    for (unsigned int k = 0; k < 6; ++k, ++current_vertex)
      {
        pts[k].pack_point(&dst_attribs[current_vertex]);
      }

    ++current_depth;
  }

  void
  pack_single_segment_chain(const fastuidraw::TessellatedPath::segment_chain &chain,
                            unsigned int &current_depth,
                            fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                            unsigned int &current_vertex,
                            fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                            unsigned int &current_index,
                            unsigned int index_adjust)
  {
    const fastuidraw::TessellatedPath::segment *prev(chain.m_prev_to_start);
    for (const auto &S : chain.m_segments)
      {
        bool has_bevel;

        has_bevel = segment_has_bevel(prev, S);
        if (has_bevel)
          {
            pack_segment_bevel(false, prev, S, current_depth,
                               dst_attribs, current_vertex,
                               dst_indices, current_index,
                               index_adjust);
          }

        pack_segment(S, current_depth,
                     dst_attribs, current_vertex,
                     dst_indices, current_index,
                     index_adjust);

        if (has_bevel)
          {
            pack_segment_bevel(true, prev, S, current_depth,
                               dst_attribs, current_vertex,
                               dst_indices, current_index,
                               index_adjust);
          }

        prev = &S;
      }
  }
}

//////////////////////////////////////
// fastuidraw::StrokedPointPacking methods
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

//////////////////////////////////////
// fastuidraw::StrokedPointPacking methods
void
fastuidraw::StrokedPointPacking::
pack_rounded_join_size(const TessellatedPath::join &join,
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
fastuidraw::StrokedPointPacking::
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
      pack_miter_join<StrokedPoint::offset_miter_bevel_join>(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case PainterEnums::miter_joins:
      pack_miter_join<StrokedPoint::offset_miter_join>(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case PainterEnums::rounded_joins:
      pack_rounded_join(join, depth, dst_attribs, dst_indices, index_adjust);
      break;

    default:
      FASTUIDRAWmessaged_assert(false, "Invalid join style passed to pack_join()");
      return;
    }
}

void
fastuidraw::StrokedPointPacking::
pack_rounded_cap_size(float thresh,
                      unsigned int *num_attributes,
                      unsigned int *num_indices)
{
  unsigned int num_arc_points;

  num_arc_points = detail::number_segments_for_tessellation(FASTUIDRAW_PI, thresh);
  *num_attributes = 1 + num_arc_points;
  *num_indices = 3 * (num_arc_points - 1);
}

void
fastuidraw::StrokedPointPacking::
pack_cap(enum cap_type_t cp,
         const TessellatedPath::cap &cap,
         unsigned int depth,
         c_array<PainterAttribute> dst_attribs,
         c_array<PainterIndex> dst_indices,
         unsigned int index_adjust)
{
  typedef packing_size<enum cap_type_t, square_cap> square_cap_sizes;
  typedef packing_size<enum cap_type_t, flat_cap> flat_cap_sizes;
  typedef packing_size<enum cap_type_t, adjustable_cap> adjustable_cap_sizes;

  switch (cp)
    {
    case square_cap:
      FASTUIDRAWassert(dst_attribs.size() == square_cap_sizes::number_attributes);
      FASTUIDRAWassert(dst_indices.size() == square_cap_sizes::number_indices);
      pack_square_cap(cap, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case flat_cap:
      FASTUIDRAWassert(dst_attribs.size() == flat_cap_sizes::number_attributes);
      FASTUIDRAWassert(dst_indices.size() == flat_cap_sizes::number_indices);
      pack_adjustable_or_flat_cap(StrokedPoint::offset_flat_cap, cap, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case adjustable_cap:
      FASTUIDRAWassert(dst_attribs.size() == adjustable_cap_sizes::number_attributes);
      FASTUIDRAWassert(dst_indices.size() == adjustable_cap_sizes::number_indices);
      pack_adjustable_or_flat_cap(StrokedPoint::offset_adjustable_cap, cap, depth, dst_attribs, dst_indices, index_adjust);
      break;

    case rounded_cap:
      pack_rounded_cap(cap, depth, dst_attribs, dst_indices, index_adjust);
      break;

    default:
      FASTUIDRAWmessaged_assert(false, "Invalid cap style passed to pack_cap()");
      return;
    }
}

void
fastuidraw::StrokedPointPacking::
pack_segment_chain_size(c_array<const TessellatedPath::segment_chain> chains,
                        unsigned int *pdepth_range_size,
                        unsigned int *pnum_attributes,
                        unsigned int *pnum_indices)
{
  unsigned int &depth_range_size(*pdepth_range_size);
  unsigned int &num_attributes(*pnum_attributes);
  unsigned int &num_indices(*pnum_indices);

  depth_range_size = 0;
  num_attributes = 0;
  num_indices = 0;

  for (const TessellatedPath::segment_chain &chain : chains)
    {
      unsigned int d, a, i;

      pack_segment_chain_size(chain, &d, &a, &i);
      depth_range_size += d;
      num_attributes += a;
      num_indices += i;
    }
}

void
fastuidraw::StrokedPointPacking::
pack_segment_chain_size(const TessellatedPath::segment_chain &chain,
                        unsigned int *pdepth_range_size,
                        unsigned int *pnum_attributes,
                        unsigned int *pnum_indices)
{
  unsigned int &depth_range_size(*pdepth_range_size);
  unsigned int &num_attributes(*pnum_attributes);
  unsigned int &num_indices(*pnum_indices);
  const fastuidraw::TessellatedPath::segment *prev(chain.m_prev_to_start);

  depth_range_size = 0;
  num_attributes = 0;
  num_indices = 0;

  for (const auto &S : chain.m_segments)
    {
      if (segment_has_bevel(prev, S))
        {
          /* there is an inner and outer-bevel, each
           * needs a single triangle; but only the
           * inner bevel needs a depth value
           */
          num_attributes += 6;
          num_indices += 6;
          ++depth_range_size;
        }

      /* each segment is two quads that share an edge;
       * thus 6 attributes and 12 indices all sharing
       * a single depth value.
       */
      num_attributes += 6;
      num_indices += 12;
      ++depth_range_size;

      prev = &S;
    }
}

void
fastuidraw::StrokedPointPacking::
pack_segment_chain(c_array<const TessellatedPath::segment_chain> chains,
                   unsigned int depth_start,
                   c_array<PainterAttribute> dst_attribs,
                   c_array<PainterIndex> dst_indices,
                   unsigned int index_adjust)
{
  /* pack the segments in the order we get them, incrementing the
   * depth value as we go. After we are done, then we will reverse
   * the indices to make it do in depth decreasing order.
   */
  unsigned int current_vertex(0), current_index(0), current_depth(depth_start);
  for (const TessellatedPath::segment_chain &chain : chains)
    {
      pack_single_segment_chain(chain, current_depth,
                                dst_attribs, current_vertex,
                                dst_indices, current_index,
                                index_adjust);
    }
  FASTUIDRAWassert(current_vertex == dst_attribs.size());
  FASTUIDRAWassert(current_index == dst_indices.size());

  std::reverse(dst_indices.begin(), dst_indices.end());
}

void
fastuidraw::StrokedPointPacking::
pack_segment_chain(const TessellatedPath::segment_chain &chain,
                   unsigned int depth_start,
                   c_array<PainterAttribute> dst_attribs,
                   c_array<PainterIndex> dst_indices,
                   unsigned int index_adjust)
{
  /* pack the segments in the order we get them, incrementing the
   * depth value as we go. After we are done, then we will reverse
   * the indices to make it do in depth decreasing order.
   */
  unsigned int current_vertex(0), current_index(0), current_depth(depth_start);
  pack_single_segment_chain(chain, current_depth,
                            dst_attribs, current_vertex,
                            dst_indices, current_index,
                            index_adjust);
  FASTUIDRAWassert(current_vertex == dst_attribs.size());
  FASTUIDRAWassert(current_index == dst_indices.size());

  std::reverse(dst_indices.begin(), dst_indices.end());
}

enum fastuidraw::StrokedPointPacking::cap_type_t
fastuidraw::StrokedPointPacking::
cap_type(enum PainterEnums::cap_style cp, bool for_dashed_stroking)
{
  if (for_dashed_stroking)
    {
      return StrokedPointPacking::adjustable_cap;
    }

  switch (cp)
    {
    case PainterEnums::rounded_caps:
      return StrokedPointPacking::rounded_cap;
      break;

    case PainterEnums::square_caps:
      return StrokedPointPacking::square_cap;
      break;

    default:
    case PainterEnums::flat_caps:
      return StrokedPointPacking::flat_cap;
      break;
    }
}
