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

#include <fastuidraw/util/math.hpp>
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/partitioned_tessellated_path.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>
#include <private/bounding_box.hpp>
#include <private/util_private.hpp>

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

    inline
    bool
    reverse_compare_max_distance(const reference_counted_ptr<const TessellatedPath> &lhs,
                                 float rhs)
    {
      return lhs->max_distance() > rhs;
    }

    unsigned int
    number_segments_for_tessellation(float arc_angle, float distance_thresh);

    inline
    uint32_t
    stroked_point_pack_bits(int on_boundary,
                            enum StrokedPoint::offset_type_t pt,
                            uint32_t depth)
    {
      FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

      uint32_t bb(on_boundary), pp(pt);

      /* clamp depth to maximum allowed value */
      depth = t_min(depth,
                    FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(StrokedPoint::depth_num_bits));
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

    inline
    float
    compute_bevel_lambda(bool is_inner_bevel,
                         const TessellatedPath::segment *prev,
                         const TessellatedPath::segment &S,
                         vec2 &start_bevel, vec2 &end_bevel)
    {
      float lambda;

      FASTUIDRAWassert(prev);
      end_bevel.x() = -S.m_enter_segment_unit_vector.y();
      end_bevel.y() = +S.m_enter_segment_unit_vector.x();
      start_bevel.x() = -prev->m_leaving_segment_unit_vector.y();
      start_bevel.y() = +prev->m_leaving_segment_unit_vector.x();
      lambda = t_sign(dot(end_bevel, prev->m_leaving_segment_unit_vector));
      if (is_inner_bevel)
        {
          lambda *= -1.0f;
        }
      return lambda;
    }

    void
    add_triangle(PainterIndex v0, PainterIndex v1, PainterIndex v2,
                 c_array<PainterIndex> dst_indices, unsigned int &index_offset);

    void
    add_triangle_fan(PainterIndex begin, PainterIndex end,
                     c_array<PainterIndex> indices,
                     unsigned int &index_offset);
    inline
    unsigned int
    add_triangle_fan(PainterIndex begin, PainterIndex end,
                     c_array<PainterIndex> indices)
    {
      unsigned int index_offset(0);
      add_triangle_fan(begin, end, indices, index_offset);
      return index_offset;
    }
  }
}
