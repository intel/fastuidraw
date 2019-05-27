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
  inline
  uint32_t
  arc_stroked_point_pack_bits(int on_boundary,
                              enum fastuidraw::ArcStrokedPoint::offset_type_t pt,
                              uint32_t depth)
  {
    FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);
    return pack_bits(fastuidraw::ArcStrokedPoint::offset_type_bit0,
                     fastuidraw::ArcStrokedPoint::offset_type_num_bits, pp)
      | pack_bits(fastuidraw::ArcStrokedPoint::boundary_bit, 1u, bb)
      | pack_bits(fastuidraw::ArcStrokedPoint::depth_bit0,
                  fastuidraw::ArcStrokedPoint::depth_num_bits, depth);
  }

  inline
  void
  init(const fastuidraw::TessellatedPath::join &J,
       fastuidraw::ArcStrokedPoint *pt)
  {
    pt->m_position = J.m_position;
    pt->m_distance_from_edge_start = J.m_distance_from_previous_join;
    pt->m_edge_length = J.m_distance_from_previous_join;
    pt->m_contour_length = J.m_contour_length;
    pt->m_distance_from_contour_start = J.m_distance_from_contour_start;
  }

  inline
  void
  init(const fastuidraw::TessellatedPath::cap &C,
       fastuidraw::ArcStrokedPoint *pt)
  {
    pt->m_position = C.m_position;
    pt->m_distance_from_edge_start = C.m_distance_from_edge_start;
    pt->m_edge_length = C.m_edge_length;
    pt->m_contour_length = C.m_contour_length;
    pt->m_distance_from_contour_start = C.m_distance_from_contour_start;
  }

  inline
  void
  init_start_pt(const fastuidraw::TessellatedPath::segment &S,
                fastuidraw::ArcStrokedPoint *pt)
  {
    pt->m_position = S.m_start_pt;
    pt->m_distance_from_edge_start = S.m_distance_from_edge_start;
    pt->m_distance_from_contour_start = S.m_distance_from_contour_start;
    pt->m_edge_length = S.m_edge_length;
    pt->m_contour_length = S.m_contour_length;
    pt->m_data = fastuidraw::vec2(0.0f, 0.0f);
  }

  inline
  void
  init_end_pt(const fastuidraw::TessellatedPath::segment &S,
                fastuidraw::ArcStrokedPoint *pt)
  {
    pt->m_position = S.m_end_pt;
    pt->m_distance_from_edge_start = S.m_distance_from_edge_start + S.m_length;
    pt->m_distance_from_contour_start = S.m_distance_from_contour_start + S.m_length;
    pt->m_edge_length = S.m_edge_length;
    pt->m_contour_length = S.m_contour_length;
    pt->m_data = fastuidraw::vec2(0.0f, 0.0f);
  }

  /* \param pt gives position of join location and all distance values
   * \param count how many arc-joins to make
   * \param n_start normal vector at join start
   * \param n_end normal vector at join end
   * \param delta_angle angle difference between n_start and n_end
   * \param depth depth value to use for all arc-join points
   * \param is_join points are for a join
   */
  void
  pack_arc_join(unsigned int index_adjust,
                fastuidraw::ArcStrokedPoint pt, unsigned int count,
                fastuidraw::vec2 n_start, float delta_angle, fastuidraw::vec2 n_end,
                unsigned int depth,
                fastuidraw::c_array<fastuidraw::PainterAttribute> dst_pts,
                unsigned int &vertex_offset,
                fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                unsigned int &index_offset,
                bool is_join)
  {
    using namespace fastuidraw;
    using namespace detail;

    float per_element(delta_angle / static_cast<float>(count));
    std::complex<float> arc_start(n_start.x(), n_start.y());
    std::complex<float> da(t_cos(per_element), t_sin(per_element));
    std::complex<float> theta;

    unsigned int i, center;
    uint32_t arc_value, beyond_arc_value, join_mask;

    center = vertex_offset;
    join_mask = is_join ? uint32_t(ArcStrokedPoint::join_mask) : 0u;

    arc_value = join_mask
      | ArcStrokedPoint::distance_constant_on_primitive_mask
      | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
    beyond_arc_value = arc_value | ArcStrokedPoint::beyond_boundary_mask;

    pt.radius() = 0.0f;
    pt.arc_angle() = per_element;
    pt.m_offset_direction = fastuidraw::vec2(0.0f, 0.0f);
    pt.m_packed_data = join_mask
    | ArcStrokedPoint::distance_constant_on_primitive_mask
      | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
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

            add_triangle(index_adjust + center,
                         index_adjust + start,
                         index_adjust + vertex_offset + 1,
                         dst_indices, index_offset);
            add_triangle(index_adjust + start,
                         index_adjust + start + 1,
                         index_adjust + vertex_offset,
                         dst_indices, index_offset);
            add_triangle(index_adjust + start,
                         index_adjust + vertex_offset,
                         index_adjust + vertex_offset + 1,
                         dst_indices, index_offset);
          }
      }
  }

  void
  pack_arc_join(unsigned int index_adjust,
                fastuidraw::ArcStrokedPoint pt, unsigned int count,
                fastuidraw::vec2 n0, fastuidraw::vec2 n1, unsigned int depth,
                fastuidraw::c_array<fastuidraw::PainterAttribute> dst_pts,
                unsigned int &vertex_offset,
                fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                unsigned int &index_offset,
                bool is_join)
  {
    std::complex<float> n0z(n0.x(), n0.y());
    std::complex<float> n1z(n1.x(), n1.y());
    std::complex<float> n1z_times_conj_n0z(n1z * std::conj(n0z));
    float angle(fastuidraw::t_atan2(n1z_times_conj_n0z.imag(),
                                    n1z_times_conj_n0z.real()));

    pack_arc_join(index_adjust, pt, count, n0, angle, n1, depth,
                  dst_pts, vertex_offset,
                  dst_indices, index_offset,
                  is_join);
  }

  inline
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

  inline
  bool
  use_arc_bevel(const fastuidraw::TessellatedPath::segment *prev,
                const fastuidraw::TessellatedPath::segment &S)
  {
    /* Use an arc-bevel whenever one of the segments it connects between
     * is an arc-segment.
     */
    FASTUIDRAWassert(prev);
    return prev->m_type == fastuidraw::TessellatedPath::arc_segment
      || S.m_type == fastuidraw::TessellatedPath::arc_segment;
  }

  inline
  bool
  has_start_dashed_capper(const fastuidraw::TessellatedPath::segment &S)
  {
    return S.m_first_segment_of_edge && S.m_contour_length > 0.0f;
  }

  inline
  bool
  has_end_dashed_capper(const fastuidraw::TessellatedPath::segment &S)
  {
    return S.m_last_segment_of_edge && S.m_contour_length > 0.0f;
  }

  inline
  void
  pack_arc_bevel(bool is_inner_bevel,
                 const fastuidraw::TessellatedPath::segment *prev,
                 const fastuidraw::TessellatedPath::segment &S,
                 unsigned int &current_depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                 unsigned int &current_vertex,
                 fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                 unsigned int &current_index,
                 unsigned int index_adjust)
  {
    using namespace fastuidraw;

    ArcStrokedPoint pt;
    float lambda;
    fastuidraw::vec2 start_bevel, end_bevel;

    FASTUIDRAWassert(use_arc_bevel(prev, S));

    init_start_pt(S, &pt);
    lambda = detail::compute_bevel_lambda(is_inner_bevel, prev, S,
                                          start_bevel, end_bevel);
    pack_arc_join(index_adjust, pt, 1,
                  lambda * start_bevel, lambda * end_bevel,
                  current_depth,
                  dst_attribs, current_vertex,
                  dst_indices, current_index, false);

    ++current_depth;
  }

  inline
  void
  pack_line_bevel(bool is_inner_bevel,
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

    ArcStrokedPoint pt;
    float lambda;
    vec2 start_bevel, end_bevel;

    FASTUIDRAWassert(segment_has_bevel(prev, S));
    FASTUIDRAWassert(!use_arc_bevel(prev, S));

    init_start_pt(S, &pt);
    lambda = detail::compute_bevel_lambda(is_inner_bevel, prev, S,
                                          start_bevel, end_bevel);

    for (unsigned int k = 0; k < 3; ++k, ++index_offset)
      {
        dst_indices[index_offset] = k + index_adjust + vertex_offset;
      }

    pt.m_offset_direction = vec2(0.0f, 0.0f);
    pt.m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
      | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_line_segment, current_depth);
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_offset_direction = lambda * start_bevel;
    pt.m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
      | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_line_segment, current_depth);
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_offset_direction = lambda * end_bevel;
    pt.m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
      | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_line_segment, current_depth);
    pt.pack_point(&dst_attribs[vertex_offset++]);

    ++current_depth;
  }

  inline
  void
  pack_bevel(bool is_inner_bevel,
             const fastuidraw::TessellatedPath::segment *prev,
             const fastuidraw::TessellatedPath::segment &S,
             unsigned int &current_depth,
             fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
             unsigned int &current_vertex,
             fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
             unsigned int &current_index,
             unsigned int index_adjust)
  {
    if (use_arc_bevel(prev, S))
      {
        pack_arc_bevel(is_inner_bevel, prev, S, current_depth,
                       dst_attribs, current_vertex,
                       dst_indices, current_index,
                       index_adjust);
      }
    else
      {
        pack_line_bevel(is_inner_bevel, prev, S, current_depth,
                        dst_attribs, current_vertex,
                        dst_indices, current_index,
                        index_adjust);
      }
  }

  inline
  void
  pack_dashed_capper(bool for_start_dashed_capper,
                     const fastuidraw::TessellatedPath::segment &S,
                     unsigned int &current_depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                     unsigned int &vertex_offset,
                     fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                     unsigned int &index_offset,
                     unsigned int index_adjust)
  {
    using namespace fastuidraw;

    const PainterIndex tris[12] =
      {
        0, 3, 4,
        0, 4, 1,
        1, 4, 5,
        1, 5, 2,
      };

    for (unsigned int i = 0; i < 12; ++i, ++index_offset)
      {
        dst_indices[index_offset] = tris[i] + index_adjust + vertex_offset;
      }

    ArcStrokedPoint pt;
    vec2 normal;
    uint32_t packed_data, packed_data_mid;

    packed_data = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point_dashed_capper, current_depth);
    packed_data_mid = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point_dashed_capper, current_depth);
    if (for_start_dashed_capper)
      {
        init_start_pt(S, &pt);
        normal = S.enter_segment_normal();
        pt.m_data = -S.m_enter_segment_unit_vector;
      }
    else
      {
        init_end_pt(S, &pt);
        normal = S.leaving_segment_normal();
        pt.m_data = S.m_leaving_segment_unit_vector;
        packed_data |= ArcStrokedPoint::end_segment_mask;
        packed_data_mid |= ArcStrokedPoint::end_segment_mask;
      }

    pt.m_packed_data = packed_data;
    pt.m_offset_direction = normal;
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_packed_data = packed_data_mid;
    pt.m_offset_direction = vec2(0.0f, 0.0f);
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_packed_data = packed_data;
    pt.m_offset_direction = -normal;
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
    pt.m_offset_direction = normal;
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_packed_data = packed_data_mid | ArcStrokedPoint::extend_mask;
    pt.m_offset_direction = vec2(0.0f, 0.0f);
    pt.pack_point(&dst_attribs[vertex_offset++]);

    pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
    pt.m_offset_direction = -normal;
    pt.pack_point(&dst_attribs[vertex_offset++]);

    ++current_depth;
  }

  inline
  void
  pack_line_segment(const fastuidraw::TessellatedPath::segment &S,
                    unsigned int &current_depth,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                    unsigned int &current_vertex,
                    fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                    unsigned int &current_index,
                    unsigned int index_adjust)
  {
    using namespace fastuidraw;
    using namespace detail;

    const unsigned int tris[12] =
      {
        0, 2, 5,
        0, 5, 3,
        2, 1, 4,
        2, 4, 5
      };

    for (unsigned int i = 0; i < 12; ++i, ++current_index)
      {
        dst_indices[current_index] = current_vertex + index_adjust + tris[i];
      }

    const int boundary_values[3] = { 1, 1, 0 };
    const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
    vec2 delta(S.m_end_pt - S.m_start_pt);
    vecN<ArcStrokedPoint, 6> pts;

    for(unsigned int k = 0; k < 3; ++k)
      {
        pts[k].m_position = S.m_start_pt;
        pts[k].m_distance_from_edge_start = S.m_distance_from_edge_start;
        pts[k].m_distance_from_contour_start = S.m_distance_from_contour_start;
        pts[k].m_edge_length = S.m_edge_length;
        pts[k].m_contour_length = S.m_contour_length;
        pts[k].m_offset_direction = normal_sign[k] * S.enter_segment_normal();
        pts[k].m_data = delta;
        pts[k].m_packed_data = arc_stroked_point_pack_bits(boundary_values[k],
                                                           ArcStrokedPoint::offset_line_segment,
                                                           current_depth);

        pts[k + 3].m_position = S.m_end_pt;
        pts[k + 3].m_distance_from_edge_start = S.m_distance_from_edge_start + S.m_length;
        pts[k + 3].m_distance_from_contour_start = S.m_distance_from_contour_start + S.m_length;
        pts[k + 3].m_edge_length = S.m_edge_length;
        pts[k + 3].m_contour_length = S.m_contour_length;
        pts[k + 3].m_offset_direction = normal_sign[k] * S.leaving_segment_normal();
        pts[k + 3].m_data = -delta;
        pts[k + 3].m_packed_data = ArcStrokedPoint::end_segment_mask
          | arc_stroked_point_pack_bits(boundary_values[k],
                                        ArcStrokedPoint::offset_line_segment,
                                        current_depth);
    }

    for (unsigned int i = 0; i < 6; ++i)
      {
        pts[i].pack_point(&dst_attribs[current_vertex++]);
      }

    ++current_depth;
  }

  inline
  void
  pack_arc_segment(const fastuidraw::TessellatedPath::segment &S,
                   unsigned int &depth,
                   fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
                   unsigned int &current_vertex,
                   fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
                   unsigned int &current_index,
                   unsigned int index_adjust)
  {
    using namespace fastuidraw;
    using namespace detail;

    ArcStrokedPoint begin_pt, end_pt;
    vec2 begin_radial, end_radial;

    const unsigned int tris[24] =
      {
        4, 6, 7,
        4, 7, 5,
        2, 4, 5,
        2, 5, 3,
        8, 0, 1,
        9, 2, 3,

        0,  1, 10,
        1, 10, 11
      };

    for (unsigned int i = 0; i < 24; ++i, ++current_index)
      {
        dst_indices[current_index] = current_vertex + index_adjust + tris[i];
      }

    init_start_pt(S, &begin_pt);
    begin_pt.radius() = S.m_radius;
    begin_pt.arc_angle() = S.m_arc_angle.m_end - S.m_arc_angle.m_begin;

    init_end_pt(S, &end_pt);
    end_pt.radius() = begin_pt.radius();
    end_pt.arc_angle() = begin_pt.arc_angle();

    begin_radial = vec2(t_cos(S.m_arc_angle.m_begin), t_sin(S.m_arc_angle.m_begin));
    end_radial = vec2(t_cos(S.m_arc_angle.m_end), t_sin(S.m_arc_angle.m_end));

    /* inner stroking boundary points (points 0, 1) */
    begin_pt.m_packed_data = ArcStrokedPoint::inner_stroking_mask
      | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
    begin_pt.m_position = S.m_start_pt;
    begin_pt.m_offset_direction = begin_radial;
    begin_pt.pack_point(&dst_attribs[current_vertex++]);

    end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
    end_pt.m_position = S.m_end_pt;
    end_pt.m_offset_direction = end_radial;
    end_pt.pack_point(&dst_attribs[current_vertex++]);

    /* the points that are on the arc (points 2, 3) */
    begin_pt.m_packed_data = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
    begin_pt.m_position = S.m_start_pt;
    begin_pt.m_offset_direction = begin_radial;
    begin_pt.pack_point(&dst_attribs[current_vertex++]);

    end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
    end_pt.m_position = S.m_end_pt;
    end_pt.m_offset_direction = end_radial;
    end_pt.pack_point(&dst_attribs[current_vertex++]);

    /* outer stroking boundary points (points 4, 5) */
    begin_pt.m_packed_data = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
    begin_pt.m_position = S.m_start_pt;
    begin_pt.m_offset_direction = begin_radial;
    begin_pt.pack_point(&dst_attribs[current_vertex++]);

    end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
    end_pt.m_position = S.m_end_pt;
    end_pt.m_offset_direction = end_radial;
    end_pt.pack_point(&dst_attribs[current_vertex++]);

    /* beyond outer stroking boundary points (points 6, 7) */
    begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
      | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
    begin_pt.m_position = S.m_start_pt;
    begin_pt.m_offset_direction = begin_radial;
    begin_pt.pack_point(&dst_attribs[current_vertex++]);

    end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
    end_pt.m_position = S.m_end_pt;
    end_pt.m_offset_direction = end_radial;
    end_pt.pack_point(&dst_attribs[current_vertex++]);

    /* points that move to origin when stroking radius is larger than arc radius (points 8, 9) */
    begin_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
      | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
    begin_pt.m_position = S.m_start_pt;
    begin_pt.m_offset_direction = begin_radial;
    begin_pt.pack_point(&dst_attribs[current_vertex++]);

    end_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
      | ArcStrokedPoint::end_segment_mask
      | ArcStrokedPoint::inner_stroking_mask
      | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
    end_pt.m_position = S.m_end_pt;
    end_pt.m_offset_direction = end_radial;
    end_pt.pack_point(&dst_attribs[current_vertex++]);

    /* beyond inner stroking boundary (points 10, 11) */
    begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
      | ArcStrokedPoint::inner_stroking_mask
      | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
    begin_pt.m_position = S.m_start_pt;
    begin_pt.m_offset_direction = begin_radial;
    begin_pt.pack_point(&dst_attribs[current_vertex++]);

    end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
    end_pt.m_position = S.m_end_pt;
    end_pt.m_offset_direction = end_radial;
    end_pt.pack_point(&dst_attribs[current_vertex++]);

    ++depth;
  }

  inline
  void
  pack_segment(const fastuidraw::TessellatedPath::segment &S,
               unsigned int &current_depth,
               fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
               unsigned int &current_vertex,
               fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
               unsigned int &current_index,
               unsigned int index_adjust)
  {
    if (S.m_type == fastuidraw::TessellatedPath::arc_segment)
      {
        pack_arc_segment(S, current_depth,
                         dst_attribs, current_vertex,
                         dst_indices, current_index,
                         index_adjust);
      }
    else
      {
        pack_line_segment(S, current_depth,
                          dst_attribs, current_vertex,
                          dst_indices, current_index,
                          index_adjust);
      }
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
            pack_bevel(false, prev, S, current_depth,
                       dst_attribs, current_vertex,
                       dst_indices, current_index,
                       index_adjust);
          }

        if (has_start_dashed_capper(S))
          {
            pack_dashed_capper(true, S, current_depth,
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
            pack_bevel(true, prev, S, current_depth,
                       dst_attribs, current_vertex,
                       dst_indices, current_index,
                       index_adjust);
          }

        if (has_end_dashed_capper(S))
          {
            pack_dashed_capper(false, S, current_depth,
                               dst_attribs, current_vertex,
                               dst_indices, current_index,
                               index_adjust);
          }

        prev = &S;
      }
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

  init(join, &pt);
  pack_arc_join(index_adjust, pt, count,
                join.m_lambda * join.enter_join_normal(),
                join.m_join_angle,
                join.m_lambda * join.leaving_join_normal(),
                depth, dst_attribs, num_verts_used,
                dst_indices, num_indices_used, true);
  FASTUIDRAWassert(num_verts_used == dst_attribs.size());
  FASTUIDRAWassert(num_indices_used == dst_indices.size());
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

  init(C, &pt);
  pack_arc_join(index_adjust, pt, arcs_per_cap,
                n, FASTUIDRAW_PI, -n,
                depth, dst_attribs, num_verts_used,
                dst_indices, num_indices_used, true);
  FASTUIDRAWassert(num_verts_used == dst_attribs.size());
  FASTUIDRAWassert(num_indices_used == dst_indices.size());
}

void
fastuidraw::ArcStrokedPointPacking::
pack_segment_chain_size(const TessellatedPath::segment_chain& chain,
                        unsigned int *pdepth_range_size,
                        unsigned int *pnum_attributes,
                        unsigned int *pnum_indices)
{
  unsigned int &depth_range_size(*pdepth_range_size);
  unsigned int &num_attributes(*pnum_attributes);
  unsigned int &num_indices(*pnum_indices);
  const TessellatedPath::segment *prev(chain.m_prev_to_start);

  depth_range_size = 0;
  num_attributes = 0;
  num_indices = 0;

  for (const auto &S : chain.m_segments)
    {
      if (segment_has_bevel(prev, S))
        {
          depth_range_size += 2;
          if (use_arc_bevel(prev, S))
            {
              /* each arc-bevel uses 5 attributes and 9 indices
               * (because each is realized in one arc). Since
               * there is an inner and outer bevel, then double
               * it.
               */
              num_attributes += 10;
              num_indices += 18;
            }
          else
            {
              num_attributes += 6;
              num_indices += 6;
            }
        }

      if (has_start_dashed_capper(S))
        {
          num_attributes += 6;
          num_indices += 12;
          ++depth_range_size;
        }

      if (has_end_dashed_capper(S))
        {
          num_attributes += 6;
          num_indices += 12;
          ++depth_range_size;
        }

      ++depth_range_size;
      if (S.m_type == TessellatedPath::arc_segment)
        {
          num_attributes += 12;
          num_indices += 24;
        }
      else
        {
          /* each segment is two quads that share an edge;
           * thus 6 attributes and 12 indices all sharing
           * a single depth value.
           */
          num_attributes += 6;
          num_indices += 12;
        }

      prev = &S;
    }
}

void
fastuidraw::ArcStrokedPointPacking::
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
fastuidraw::ArcStrokedPointPacking::
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
fastuidraw::ArcStrokedPointPacking::
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
