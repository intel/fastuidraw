/*!
 * \file path_util_private.hpp
 * \brief file path_util_private.hpp
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


#pragma once

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/painter/painter_attribute.hpp>
#include <fastuidraw/painter/stroked_point.hpp>
#include <fastuidraw/painter/arc_stroked_point.hpp>
#include "bounding_box.hpp"
#include "util_private.hpp"

namespace fastuidraw
{
  namespace detail
  {
    enum
      {
        /* The starting data is floating point which has
         * a 23-bit significand; arc-tessellation needs more
         * accuracy to not produce garbage.
         */
        MAX_REFINE_RECURSION_LIMIT = 16
      };

    unsigned int
    number_segments_for_tessellation(float arc_angle, float distance_thresh);

    float
    distance_to_line(const vec2 &q, const vec2 &p0, const vec2 &p1);

    void
    bouding_box_union_arc(const vec2 &center, float radius,
                          float start_angle, float end_angle,
                          BoundingBox<float> *dst);

    inline
    uint32_t
    stroked_point_pack_bits(int on_boundary,
                            enum StrokedPoint::offset_type_t pt,
                            uint32_t depth)
    {
      FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

      uint32_t bb(on_boundary), pp(pt);
      return pack_bits(StrokedPoint::offset_type_bit0,
                       StrokedPoint::offset_type_num_bits, pp)
        | pack_bits(StrokedPoint::boundary_bit, 1u, bb)
        | pack_bits(StrokedPoint::depth_bit0,
                    StrokedPoint::depth_num_bits, depth);
    }

    inline
    uint32_t
    arc_stroked_point_pack_bits(int on_boundary,
                                enum ArcStrokedPoint::offset_type_t pt,
                                uint32_t depth)
    {
      FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

      uint32_t bb(on_boundary), pp(pt);
      return pack_bits(ArcStrokedPoint::offset_type_bit0,
                       ArcStrokedPoint::offset_type_num_bits, pp)
        | pack_bits(ArcStrokedPoint::boundary_bit, 1u, bb)
        | pack_bits(ArcStrokedPoint::depth_bit0,
                    ArcStrokedPoint::depth_num_bits, depth);
    }

    void
    compute_arc_join_size(unsigned int cnt,
                          unsigned int *out_vertex_cnt,
                          unsigned int *out_index_cnt);

    /* \param pt gives position of join location and all distance values
     * \param count how many arc-joins to make
     * \param n_start normal vector at join start
     * \param n_end normal vector at join end
     * \param delta_angle angle difference between n_start and n_end
     * \param depth depth value to use for all arc-join points
     * \param is_join points are for a join
     */
    void
    pack_arc_join(ArcStrokedPoint pt, unsigned int count,
                  vec2 n_start, float delta_angle, vec2 n_end,
                  unsigned int depth,
                  c_array<PainterAttribute> dst_pts,
                  unsigned int &vertex_offset,
                  c_array<PainterIndex> dst_indices,
                  unsigned int &index_offset,
                  bool is_join);

    /* Same as above, except that delta-angle is computed
     * from n_start and n_end
     */
    void
    pack_arc_join(ArcStrokedPoint pt, unsigned int count,
                  vec2 n_start, vec2 n_end,
                  unsigned int depth,
                  c_array<PainterAttribute> dst_pts,
                  unsigned int &vertex_offset,
                  c_array<PainterIndex> dst_indices,
                  unsigned int &index_offset,
                  bool is_join);

    inline
    void
    pack_arc_join(ArcStrokedPoint pt, unsigned int count,
                  vec2 n_start, vec2 n_end,
                  unsigned int depth,
                  std::vector<PainterAttribute> &dst_pts,
                  std::vector<PainterIndex> &dst_indices,
                  bool is_join)
    {
      unsigned int num_verts, vertex_offset(dst_pts.size());
      unsigned int num_indices, index_offset(dst_indices.size());

      compute_arc_join_size(count, &num_verts, &num_indices);
      dst_pts.resize(num_verts + vertex_offset);
      dst_indices.resize(num_indices + index_offset);
      pack_arc_join(pt, count, n_start, n_end, depth,
                    make_c_array(dst_pts), vertex_offset,
                    make_c_array(dst_indices), index_offset,
                    is_join);
    }

    inline
    void
    pack_arc_join(ArcStrokedPoint pt, unsigned int count,
                  vec2 n_start, float delta_angle, vec2 n_end,
                  unsigned int depth,
                  std::vector<PainterAttribute> &dst_pts,
                  std::vector<PainterIndex> &dst_indices,
                  bool is_join)
    {
      unsigned int num_verts, vertex_offset(dst_pts.size());
      unsigned int num_indices, index_offset(dst_indices.size());

      compute_arc_join_size(count, &num_verts, &num_indices);
      dst_pts.resize(num_verts + vertex_offset);
      dst_indices.resize(num_indices + index_offset);
      pack_arc_join(pt, count, n_start, delta_angle, n_end, depth,
                    make_c_array(dst_pts), vertex_offset,
                    make_c_array(dst_indices), index_offset,
                    is_join);
    }

    void
    add_triangle(PainterIndex v0, PainterIndex v1, PainterIndex v2,
                 c_array<PainterIndex> dst_indices, unsigned int &index_offset);

    void
    add_triangle_fan(PainterIndex begin, PainterIndex end,
                     c_array<PainterIndex> indices,
                     unsigned int &index_offset);
  }
}
