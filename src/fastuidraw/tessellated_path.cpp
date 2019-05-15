/*!
 * \file tessellated_path.cpp
 * \brief file tessellated_path.cpp
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


#include <list>
#include <vector>
#include <algorithm>
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/partitioned_tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/attribute_data/stroked_path.hpp>
#include <fastuidraw/painter/attribute_data/filled_path.hpp>
#include <private/util_private.hpp>
#include <private/bounding_box.hpp>
#include <private/path_util_private.hpp>

namespace
{
  class RefinerEdge
  {
  public:
    typedef fastuidraw::PathContour PathContour;
    typedef PathContour::tessellation_state tessellation_state;
    typedef fastuidraw::reference_counted_ptr<tessellation_state> tessellation_state_ref;
    typedef PathContour::interpolator_base interpolator_base;
    typedef fastuidraw::reference_counted_ptr<const interpolator_base> interpolator_base_ref;

    tessellation_state_ref m_tess_state;
    interpolator_base_ref m_interpolator;
  };

  class RefinerContour
  {
  public:
    std::vector<RefinerEdge> m_edges;
    bool m_is_closed;
    fastuidraw::vec2 m_start_pt;
  };

  class RefinerPrivate
  {
  public:
    fastuidraw::reference_counted_ptr<fastuidraw::TessellatedPath> m_path;
    std::vector<RefinerContour> m_contours;
  };

  class TessellatedPathBuildingState
  {
  public:
    TessellatedPathBuildingState(void):
      m_loc(0)
    {}

    unsigned int m_loc, m_ende;
    std::list<std::vector<fastuidraw::TessellatedPath::segment> > m_temp;
    float m_contour_length;
    std::list<std::vector<fastuidraw::TessellatedPath::segment> >::iterator m_start_contour;
  };

  class Edge
  {
  public:
    fastuidraw::range_type<unsigned int> m_edge_range;
    enum fastuidraw::PathEnums::edge_type_t m_edge_type;
  };

  class TessellatedContour
  {
  public:
    std::vector<Edge> m_edges;
    bool m_is_closed;
  };

  class TessellatedPathPrivate
  {
  public:
    TessellatedPathPrivate(unsigned int number_contours,
                           fastuidraw::TessellatedPath::TessellationParams TP);

    explicit
    TessellatedPathPrivate(const fastuidraw::TessellatedPath &with_arcs,
                           float thresh);

    void
    start_contour(TessellatedPathBuildingState &b,
                  unsigned int contour,
                  const fastuidraw::vec2 contour_start,
                  unsigned int num_edges_in_contour);

    void
    add_edge(TessellatedPathBuildingState &b,
             unsigned int contour, unsigned int edge,
             std::vector<fastuidraw::TessellatedPath::segment> &segments,
             float edge_max_distance);

    void
    end_contour(TessellatedPathBuildingState &b);

    void
    finalize(TessellatedPathBuildingState &b);

    std::vector<TessellatedContour> m_contours;
    std::vector<fastuidraw::TessellatedPath::segment> m_segment_data;
    std::vector<fastuidraw::TessellatedPath::join> m_join_data;
    std::vector<fastuidraw::TessellatedPath::cap> m_cap_data;
    fastuidraw::BoundingBox<float> m_bounding_box;
    fastuidraw::TessellatedPath::TessellationParams m_params;
    float m_max_distance;
    bool m_has_arcs;
    unsigned int m_max_recursion;
    fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> m_stroked;
    fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath> m_filled;
    fastuidraw::reference_counted_ptr<const fastuidraw::PartitionedTessellatedPath> m_partitioned;
    std::vector<fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath> > m_linearization;
  };

  float
  one_minus_cos(float theta)
  {
    /* naively doing 1.0 - cos(theta) will rise to
     * catostrophic cancellation when theta is small.
     * Instead, use the trig identity:
     *  cos(t) = 1 - 2sin^2(t / 2)
     * which gives
     *  1 - cos(t) = 2sin^2(t / 2)
     */
    float sin_half_theta;

    sin_half_theta = fastuidraw::t_sin(theta * 0.5f);
    return 2.0f * sin_half_theta * sin_half_theta;
  }

  void
  union_segment(const fastuidraw::TessellatedPath::segment &S,
                fastuidraw::BoundingBox<float> &BB)
  {
    using namespace fastuidraw;

    BB.union_point(S.m_start_pt);
    BB.union_point(S.m_end_pt);
    if (S.m_type == TessellatedPath::arc_segment)
      {
        float s, c, l, half_angle;
        float2x2 mat;
        vec2 tau;

        half_angle = 0.5f * (S.m_arc_angle.m_end - S.m_arc_angle.m_begin);
        c = t_cos(S.m_arc_angle.m_begin + half_angle);
        s = t_sin(S.m_arc_angle.m_begin + half_angle);
        l = one_minus_cos(half_angle);
        tau = l * vec2(c, s);

        BB.union_point(S.m_start_pt + tau);
        BB.union_point(S.m_end_pt + tau);
      }
  }

  void
  compute_local_segment_values(fastuidraw::TessellatedPath::segment &S)
  {
    using namespace fastuidraw;
    if (S.m_type == TessellatedPath::line_segment)
      {
        vec2 delta(S.m_end_pt - S.m_start_pt);

        S.m_length = delta.magnitude();
        if (S.m_length > 0.0f)
          {
            delta /= S.m_length;
          }
        else
          {
            delta = vec2(1.0f, 0.0f);
          }

        S.m_enter_segment_unit_vector = delta;
        S.m_leaving_segment_unit_vector = delta;
      }
    else
      {
        float sgn;

        sgn = (S.m_arc_angle.m_begin < S.m_arc_angle.m_end) ? 1.0f : -1.0f;
        S.m_length = t_abs(S.m_arc_angle.m_end - S.m_arc_angle.m_begin) * S.m_radius;
        S.m_enter_segment_unit_vector = sgn * vec2(-t_sin(S.m_arc_angle.m_begin), t_cos(S.m_arc_angle.m_begin));
        S.m_leaving_segment_unit_vector = sgn * vec2(-t_sin(S.m_arc_angle.m_end), t_cos(S.m_arc_angle.m_end));
      }
  }

  void
  add_tessellated_arc_segment(fastuidraw::vec2 start, fastuidraw::vec2 end,
                              fastuidraw::vec2 center, float radius,
                              fastuidraw::range_type<float> arc_angle,
                              bool continuation_with_predecessor,
                              std::vector<fastuidraw::TessellatedPath::segment> *d)
  {
    using namespace fastuidraw;

    float a, da, theta;
    unsigned int i, cnt;
    float max_arc(FASTUIDRAW_PI / 4.0f);

    a = t_abs(arc_angle.m_end - arc_angle.m_begin);
    cnt = 1u + static_cast<unsigned int>(a / max_arc);
    da = (arc_angle.m_end - arc_angle.m_begin) / static_cast<float>(cnt);

    for (i = 0, theta = arc_angle.m_begin; i < cnt; ++i, theta += da)
      {
        TessellatedPath::segment S;

        if (i == 0)
          {
            S.m_start_pt = start;
            S.m_continuation_with_predecessor = continuation_with_predecessor;
          }
        else
          {
            S.m_start_pt = d->back().m_end_pt;
            S.m_continuation_with_predecessor = true;
          }

        if (i + 1 == cnt)
          {
            S.m_end_pt = end;
          }
        else
          {
            S.m_end_pt = center + radius * vec2(t_cos(theta + da), t_sin(theta + da));
          }

        S.m_center = center;
        S.m_arc_angle.m_begin = theta;
        S.m_arc_angle.m_end = theta + da;
        S.m_radius = radius;
        S.m_type = TessellatedPath::arc_segment;

        d->push_back(S);
      }
  }

  unsigned int
  recursion_allowed_size(float angle)
  {
    using namespace fastuidraw;
    using namespace detail;

    const unsigned int max_d(MAX_REFINE_RECURSION_LIMIT);
    const float pi(FASTUIDRAW_PI), two_pi(2.0f * pi);
    float max_size_f;
    unsigned int max_size;

    max_size_f = static_cast<float>(1u << max_d) / two_pi;
    max_size_f *= t_abs(angle);

    max_size = static_cast<unsigned int>(max_size_f);
    return t_max(max_size, 3u);
  }

  float
  add_linearization_of_segment(float thresh,
                               const fastuidraw::TessellatedPath::segment &S,
                               std::vector<fastuidraw::TessellatedPath::segment> *dst)
  {
    using namespace fastuidraw;
    if (S.m_type == TessellatedPath::line_segment)
      {
        dst->push_back(S);
        return 0.0f;
      }

    float a, da, delta_angle;
    vec2 prev_pt;
    unsigned int needed_size;
    unsigned int max_size_allowed;

    delta_angle = S.m_arc_angle.m_end - S.m_arc_angle.m_begin;
    max_size_allowed = recursion_allowed_size(delta_angle);

    if (thresh > 0.0f)
      {
        needed_size = detail::number_segments_for_tessellation(t_abs(delta_angle), thresh / S.m_radius);
      }
    else
      {
        needed_size = 3u;
      }

    if (needed_size > max_size_allowed)
      {
        needed_size = max_size_allowed;
      }

    delta_angle /= static_cast<float>(needed_size);
    a = S.m_arc_angle.m_begin + delta_angle;
    da = delta_angle;
    prev_pt = S.m_start_pt;

    for(unsigned int i = 1; i <= needed_size; ++i, a += delta_angle, da += delta_angle)
      {
        TessellatedPath::segment newS;
        vec2 p;

        if (i == needed_size)
          {
            p = S.m_end_pt;
          }
        else
          {
            float c, s;

            c = S.m_radius * t_cos(a);
            s = S.m_radius * t_sin(a);
            p = S.m_center + vec2(c, s);
          }

        newS.m_type = TessellatedPath::line_segment;
        newS.m_start_pt = prev_pt;
        newS.m_end_pt = p;
        newS.m_center = vec2(0.0f, 0.0f);
        newS.m_arc_angle = range_type<float>(0.0f, 0.0f);
        newS.m_radius = 0;
        newS.m_continuation_with_predecessor = false;

        dst->push_back(newS);
        prev_pt = p;
      }

    const float pi(FASTUIDRAW_PI);
    float error, eff(t_min(pi, t_abs(0.5f * delta_angle)));

    error = S.m_radius * one_minus_cos(eff);
    return error;
  }

  bool
  intersection_point_in_arc(const fastuidraw::TessellatedPath::segment &src,
                            fastuidraw::vec2 pt, float *out_arc_angle)
  {
    using namespace fastuidraw;
    const vec2 &pt0(src.m_start_pt);
    const vec2 &pt1(src.m_end_pt);

    vec2 v0(pt0 - src.m_center), v1(pt1 - src.m_center);
    vec2 n0(-v0.y(), v0.x()), n1(-v1.y(), v1.x());

    /* The arc's radial edges coincide with the edges of the
     * triangle [m_center, m_pt0, m_pt1], as such we can use the
     * edges that use m_center define the clipping planes to
     * check if an intersection point is inside the arc.
     */
    pt -= src.m_center;
    if ((dot(n0, pt) > 0.0) == (dot(n0, pt1 - src.m_center) > 0.0)
        && (dot(n1, pt) > 0.0) == (dot(n1, pt0 - src.m_center) > 0.0))
      {
        /* pt is within the arc; compute the angle */
        float theta;

        theta = t_atan2(pt.y(), pt.x());
        if (theta < t_min(src.m_arc_angle.m_begin, src.m_arc_angle.m_end))
          {
            theta += 2.0f * static_cast<float>(FASTUIDRAW_PI);
          }
        else if (theta > t_max(src.m_arc_angle.m_begin, src.m_arc_angle.m_end))
          {
            theta -= 2.0f * static_cast<float>(FASTUIDRAW_PI);
          }

        FASTUIDRAWassert(theta >= t_min(src.m_arc_angle.m_begin, src.m_arc_angle.m_end));
        FASTUIDRAWassert(theta <= t_max(src.m_arc_angle.m_begin, src.m_arc_angle.m_end));
        *out_arc_angle = theta;

        return true;
      }
    return false;
  }

  enum fastuidraw::TessellatedPath::split_t
  compute_segment_split(int splitting_coordinate,
                        const fastuidraw::TessellatedPath::segment &src,
                        float split_value,
                        fastuidraw::TessellatedPath::segment *dst_before,
                        fastuidraw::TessellatedPath::segment *dst_after)
  {
    using namespace fastuidraw;
    float s, t, mid_angle;
    vec2 p, mid_unit_vector;
    vecN<TessellatedPath::segment*, 2> dst;
    enum TessellatedPath::split_t return_value;

    FASTUIDRAWassert(dst_before);
    FASTUIDRAWassert(dst_after);
    FASTUIDRAWassert(splitting_coordinate == 0 || splitting_coordinate == 1);

    if (src.m_start_pt[splitting_coordinate] <= split_value
        && src.m_end_pt[splitting_coordinate] <= split_value)
      {
        return TessellatedPath::segment_completey_before_split;
      }

    if (src.m_start_pt[splitting_coordinate] >= split_value
        && src.m_end_pt[splitting_coordinate] >= split_value)
      {
        return TessellatedPath::segment_completey_after_split;
      }

    if (src.m_type == TessellatedPath::line_segment)
      {
        float v0, v1;

        v0 = src.m_start_pt[splitting_coordinate];
        v1 = src.m_end_pt[splitting_coordinate];
        t = (split_value - v0) / (v1 - v0);
        s = 1.0f - t;

        p[splitting_coordinate] = split_value;
        p[1 - splitting_coordinate] = s * src.m_start_pt[1 - splitting_coordinate]
          + t * src.m_end_pt[1 - splitting_coordinate];

        mid_unit_vector = src.m_enter_segment_unit_vector;

        /* strictly speaking this is non-sense, but lets silence
         * any writes with uninitialized data here.
         */
        mid_angle = 0.5f * (src.m_arc_angle.m_begin + src.m_arc_angle.m_end);
      }
    else
      {
        FASTUIDRAWassert(src.m_type == TessellatedPath::arc_segment);

        /* compute t, s, p, mid_normal; We are gauranteed
         * that the arc is monotonic, thus the splitting will
         * split it into at most two pieces. In addition, because
         * it is monotonic, we know it will split it in exactly
         * two pieces.
         */
        float radical, d;
        fastuidraw::vec2 test_pt;

        d = split_value - src.m_center[splitting_coordinate];
        radical = src.m_radius * src.m_radius - d * d;

        FASTUIDRAWassert(radical > 0.0f);
        radical = t_sqrt(radical);

        test_pt[splitting_coordinate] = split_value;
        test_pt[1 - splitting_coordinate] = src.m_center[1 - splitting_coordinate] - radical;

        if (!intersection_point_in_arc(src, test_pt, &mid_angle))
          {
            bool result;

            test_pt[1 - splitting_coordinate] = src.m_center[1 - splitting_coordinate] + radical;
            result = intersection_point_in_arc(src, test_pt, &mid_angle);

            FASTUIDRAWassert(result);
            FASTUIDRAWunused(result);
          }

        p = test_pt;
        t = (mid_angle - src.m_arc_angle.m_begin) / (src.m_arc_angle.m_end - src.m_arc_angle.m_begin);
        s = 1.0f - t;

        mid_unit_vector.x() = -src.m_radius * fastuidraw::t_sin(mid_angle);
        mid_unit_vector.y() = +src.m_radius * fastuidraw::t_cos(mid_angle);
        if (src.m_arc_angle.m_begin > src.m_arc_angle.m_end)
          {
            mid_unit_vector *= -1.0f;
          }
      }

    /* we are going to write the sub-segment corrensponding to [0, t]
     * to dst[0] and the sub-segment corrensponding to [t, 1] to
     * dst[1], so we set the pointer values dependening on which
     * side of the split the starting point is on.
     */
    if (src.m_start_pt[splitting_coordinate] < split_value)
      {
        return_value = TessellatedPath::segment_split_start_before;
        dst[0] = dst_before;
        dst[1] = dst_after;
      }
    else
      {
        return_value = TessellatedPath::segment_split_start_after;
        dst[0] = dst_after;
        dst[1] = dst_before;
      }

    dst[0]->m_type = src.m_type;
    dst[0]->m_start_pt = src.m_start_pt;
    dst[0]->m_end_pt = p;
    dst[0]->m_center = src.m_center;
    dst[0]->m_arc_angle.m_begin = src.m_arc_angle.m_begin;
    dst[0]->m_arc_angle.m_end = mid_angle;
    dst[0]->m_radius = src.m_radius;
    dst[0]->m_length = t * src.m_length;
    dst[0]->m_distance_from_edge_start = src.m_distance_from_edge_start;
    dst[0]->m_distance_from_contour_start = src.m_distance_from_contour_start;
    dst[0]->m_edge_length = src.m_edge_length;
    dst[0]->m_contour_length = src.m_contour_length;
    dst[0]->m_enter_segment_unit_vector = src.m_enter_segment_unit_vector;
    dst[0]->m_leaving_segment_unit_vector = mid_unit_vector;
    dst[0]->m_continuation_with_predecessor = src.m_continuation_with_predecessor;
    dst[0]->m_contour_id = src.m_contour_id;
    dst[0]->m_edge_id = src.m_edge_id;
    dst[0]->m_first_segment_of_edge = src.m_first_segment_of_edge;
    dst[0]->m_last_segment_of_edge = false;

    dst[1]->m_type = src.m_type;
    dst[1]->m_start_pt = p;
    dst[1]->m_end_pt = src.m_end_pt;
    dst[1]->m_center = src.m_center;
    dst[1]->m_arc_angle.m_begin = mid_angle;
    dst[1]->m_arc_angle.m_end = src.m_arc_angle.m_end;
    dst[1]->m_radius = src.m_radius;
    dst[1]->m_length = s * src.m_length;
    dst[1]->m_distance_from_edge_start = dst[0]->m_distance_from_edge_start + dst[0]->m_length;
    dst[1]->m_distance_from_contour_start = dst[0]->m_distance_from_contour_start + dst[0]->m_length;
    dst[1]->m_edge_length = src.m_edge_length;
    dst[1]->m_contour_length = src.m_contour_length;
    dst[1]->m_enter_segment_unit_vector = mid_unit_vector;
    dst[1]->m_leaving_segment_unit_vector = src.m_leaving_segment_unit_vector;
    dst[1]->m_continuation_with_predecessor = true;
    dst[1]->m_contour_id = src.m_contour_id;
    dst[1]->m_edge_id = src.m_edge_id;
    dst[1]->m_first_segment_of_edge = false;
    dst[1]->m_last_segment_of_edge = src.m_last_segment_of_edge;

    return return_value;
  }

}

//////////////////////////////////////////////
// TessellatedPathPrivate methods
TessellatedPathPrivate::
TessellatedPathPrivate(unsigned int num_contours,
                       fastuidraw::TessellatedPath::TessellationParams TP):
  m_contours(num_contours),
  m_params(TP),
  m_max_distance(0.0f),
  m_has_arcs(false),
  m_max_recursion(0u)
{
}

TessellatedPathPrivate::
TessellatedPathPrivate(const fastuidraw::TessellatedPath &with_arcs,
                       float thresh):
  m_contours(with_arcs.number_contours()),
  m_params(with_arcs.tessellation_parameters()),
  m_max_distance(with_arcs.max_distance()),
  m_has_arcs(false),
  m_max_recursion(with_arcs.max_recursion())
{
  using namespace fastuidraw;

  FASTUIDRAWassert(with_arcs.has_arcs());

  TessellatedPathBuildingState builder;

  for (unsigned int C = 0, endC = with_arcs.number_contours(); C < endC; ++C)
    {
      start_contour(builder, C,
                    with_arcs.contour_segment_data(C).front().m_start_pt,
                    with_arcs.number_edges(C));
      for (unsigned int E = 0, endE = with_arcs.number_edges(C); E < endE; ++E)
        {
          float d(m_max_distance);
          std::vector<TessellatedPath::segment> tmp;
          c_array<const TessellatedPath::segment> segs;

          segs = with_arcs.edge_segment_data(C, E);
          for (const TessellatedPath::segment &S : segs)
            {
              float f;
              f = add_linearization_of_segment(thresh, S, &tmp);
              d = t_max(f, d);
            }
          add_edge(builder, C, E, tmp, d);
        }

      end_contour(builder);
      m_contours[C].m_is_closed = with_arcs.contour_closed(C);
    }
  finalize(builder);
}

void
TessellatedPathPrivate::
start_contour(TessellatedPathBuildingState &b,
              unsigned int contour,
              const fastuidraw::vec2 contour_start,
              unsigned int num_edges)
{
  FASTUIDRAWassert(contour < m_contours.size());

  b.m_contour_length = 0.0f;
  b.m_ende = num_edges;
  m_contours[contour].m_edges.resize(num_edges);

  if (num_edges == 0)
    {
      /* If a contour has no edges, that indicates it is just
       * a dot; that dot still needs to be realized and given
       * room.
       */
      std::vector<fastuidraw::TessellatedPath::segment> tmp(1);

      m_contours[contour].m_edges.resize(1);
      tmp[0].m_type = fastuidraw::TessellatedPath::line_segment;
      tmp[0].m_start_pt = tmp[0].m_end_pt = contour_start;
      tmp[0].m_length = 0.0f;
      add_edge(b, contour, 0, tmp, 0.0f);
    }
}

void
TessellatedPathPrivate::
add_edge(TessellatedPathBuildingState &builder,
         unsigned int o, unsigned int e,
         std::vector<fastuidraw::TessellatedPath::segment> &work_room,
         float edge_max_distance)
{
  using namespace fastuidraw;

  unsigned int needed;

  needed = work_room.size();
  m_contours[o].m_edges[e].m_edge_range = range_type<unsigned int>(builder.m_loc, builder.m_loc + needed);
  builder.m_loc += needed;

  FASTUIDRAWassert(needed > 0u);
  m_max_distance = t_max(m_max_distance, edge_max_distance);

  for(unsigned int n = 0; n < work_room.size(); ++n)
    {
      m_has_arcs = m_has_arcs || (work_room[n].m_type == TessellatedPath::arc_segment);
      union_segment(work_room[n], m_bounding_box);
      compute_local_segment_values(work_room[n]);

      if (n != 0)
        {
          work_room[n].m_distance_from_edge_start =
            work_room[n - 1].m_distance_from_edge_start + work_room[n - 1].m_length;
        }
      else
        {
          work_room[n].m_distance_from_edge_start = 0.0f;
        }

      work_room[n].m_distance_from_contour_start = builder.m_contour_length;
      builder.m_contour_length += work_room[n].m_length;
    }

  for(unsigned int n = 0; n < needed; ++n)
    {
      work_room[n].m_edge_length = work_room[needed - 1].m_distance_from_edge_start
        + work_room[needed - 1].m_length;
    }

  /* append the data to temp and clear work_room for the next edge */
  std::list<std::vector<TessellatedPath::segment> >::iterator t;
  t = builder.m_temp.insert(builder.m_temp.end(), std::vector<TessellatedPath::segment>());
  t->swap(work_room);

  if (e == 0)
    {
      builder.m_start_contour = t;
    }
}

void
TessellatedPathPrivate::
end_contour(TessellatedPathBuildingState &builder)
{
  using namespace fastuidraw;

  if (builder.m_start_contour == builder.m_temp.end())
    {
      return;
    }

  for(std::list<std::vector<TessellatedPath::segment> >::iterator
        t = builder.m_start_contour, endt = builder.m_temp.end(); t != endt; ++t)
    {
      for(unsigned int e = 0, ende = t->size(); e < ende; ++e)
        {
          TessellatedPath::segment &S(t->operator[](e));
          S.m_contour_length = builder.m_contour_length;
        }
    }
}

void
TessellatedPathPrivate::
finalize(TessellatedPathBuildingState &b)
{
  using namespace fastuidraw;

  unsigned int total_needed;

  total_needed = m_contours.back().m_edges.back().m_edge_range.m_end;
  m_segment_data.reserve(total_needed);
  for(auto iter = b.m_temp.begin(), end_iter = b.m_temp.end(); iter != end_iter; ++iter)
    {
      std::copy(iter->begin(), iter->end(), std::back_inserter(m_segment_data));
    }
  FASTUIDRAWassert(total_needed == m_segment_data.size());

  /* set the edge/contour properties of each segment */
  for (unsigned int C = 0, endC = m_contours.size(); C < endC; ++C)
    {
      const TessellatedContour &contour(m_contours[C]);
      const TessellatedPath::segment *prev_segment(nullptr);

      if (contour.m_edges.empty())
        {
          continue;
        }

      if (contour.m_is_closed)
        {
          prev_segment = &m_segment_data[contour.m_edges.back().m_edge_range.m_end - 1];
        }
      else
        {
          TessellatedPath::cap cap;
          const TessellatedPath::segment *start, *end;

          start = &m_segment_data[contour.m_edges.front().m_edge_range.m_begin];
          end = &m_segment_data[contour.m_edges.back().m_edge_range.m_end - 1];

          cap.m_contour_id = C;
          cap.m_contour_length = start->m_contour_length;

          cap.m_position = start->m_start_pt;
          cap.m_unit_vector = -start->m_enter_segment_unit_vector;
          cap.m_edge_length = start->m_edge_length;
          cap.m_is_starting_cap = true;
          m_cap_data.push_back(cap);

          cap.m_position = end->m_end_pt;
          cap.m_unit_vector = end->m_leaving_segment_unit_vector;
          cap.m_edge_length = end->m_edge_length;
          cap.m_is_starting_cap = false;
          m_cap_data.push_back(cap);
        }

      for (unsigned int E = 0, endE = contour.m_edges.size(); E < endE; ++E)
        {
          const Edge &edge(contour.m_edges[E]);
          c_array<TessellatedPath::segment> edge_segs;

          edge_segs = make_c_array(m_segment_data).sub_array(edge.m_edge_range);
          FASTUIDRAWassert(!edge_segs.empty());

          if (prev_segment && edge.m_edge_type == fastuidraw::PathEnums::starts_new_edge)
            {
              TessellatedPath::join J;

              J.m_position = prev_segment->m_end_pt;
              J.m_enter_join_unit_vector = prev_segment->m_leaving_segment_unit_vector;
              J.m_leaving_join_unit_vector = edge_segs.front().m_enter_segment_unit_vector;
              J.m_contour_id = C;
              J.m_edge_into_join_id = (E == 0) ? (endE - 1) : (E + 1);
              J.m_edge_leaving_join_id = E;
              J.m_distance_from_previous_join = prev_segment->m_distance_from_edge_start + prev_segment->m_length;
              J.m_distance_from_contour_start = prev_segment->m_distance_from_contour_start + prev_segment->m_length;
              J.m_contour_length = edge_segs.front().m_contour_length;
              m_join_data.push_back(J);
            }

          for (TessellatedPath::segment &S : edge_segs)
            {
              S.m_contour_id = C;
              S.m_edge_id = E;
              S.m_first_segment_of_edge = false;
              S.m_last_segment_of_edge = false;
              prev_segment = &S;
            }
          edge_segs.front().m_first_segment_of_edge = true;
          edge_segs.back().m_last_segment_of_edge = true;
        }
    }
}

//////////////////////////////////////////
// fastuidraw::TessellatedPath::segment methods
enum fastuidraw::TessellatedPath::split_t
fastuidraw::TessellatedPath::segment::
compute_split_x(float x_split,
                segment *dst_before_split,
                segment *dst_after_split) const
{
  return compute_segment_split(0, *this, x_split, dst_before_split, dst_after_split);
}

enum fastuidraw::TessellatedPath::split_t
fastuidraw::TessellatedPath::segment::
compute_split_y(float y_split,
                segment *dst_before_split,
                segment *dst_after_split) const
{
  return compute_segment_split(1, *this, y_split, dst_before_split, dst_after_split);
}

enum fastuidraw::TessellatedPath::split_t
fastuidraw::TessellatedPath::segment::
compute_split(float split,
              segment *dst_before_split,
              segment *dst_after_split,
              int splitting_coordinate) const
{
  return compute_segment_split(splitting_coordinate, *this, split, dst_before_split, dst_after_split);
}

//////////////////////////////////////////////////////////
// fastuidraw::TessellatedPath::SegmentStorage methods
void
fastuidraw::TessellatedPath::SegmentStorage::
add_line_segment(vec2 start, vec2 end)
{
  segment S;
  std::vector<segment> *d;

  d = static_cast<std::vector<segment>*>(m_d);
  S.m_start_pt = start;
  S.m_end_pt = end;
  S.m_radius = 0;
  S.m_center = vec2(0.0f, 0.0f);
  S.m_arc_angle = range_type<float>(0.0f, 0.0f);
  S.m_type = line_segment;
  S.m_continuation_with_predecessor = false;

  d->push_back(S);
}

void
fastuidraw::TessellatedPath::SegmentStorage::
add_arc_segment(vec2 start, vec2 end,
                vec2 center, float radius,
                range_type<float> arc_angle)
{
  std::vector<segment> *d;
  d = static_cast<std::vector<segment>*>(m_d);

  /* tessellate the segment so that each sub-segment is monotonic in both
   * x and y coordinates. We are relying very closely on the implementation
   * of Tessellator::arc_tessellation_worker() in Path.cpp where m_begin
   * is ALWAYS computed by atan2 and that it is possible that one of
   * m_begin or m_end has 2 * PI added to it.
   */
  const float crit_angles[] =
    {
      -0.5f * FASTUIDRAW_PI,
      0.0f,
      0.5f * FASTUIDRAW_PI,
      FASTUIDRAW_PI,
      1.5f * FASTUIDRAW_PI,
      2.0f * FASTUIDRAW_PI,
      2.5f * FASTUIDRAW_PI,
    };

  const vec2 crit_pts[] =
    {
      vec2(+0.0f, -1.0f), // -PI/2
      vec2(+1.0f, +0.0f), // 0
      vec2(+0.0f, +1.0f), // +PI/2
      vec2(-1.0f, +0.0f), // +PI
      vec2(+0.0f, -1.0f), // 3PI/2
      vec2(+1.0f, +0.0f), // 2PI
      vec2(+0.0f, +1.0f), // 5PI/2
    };

  float prev_angle(arc_angle.m_begin);
  vec2 prev_pt(start);
  bool continuation_with_predessor(false);

  for (unsigned int i = 0, reverse_i = 6; i < 7; ++i, --reverse_i)
    {
      unsigned int K;
      bool should_split;

      if (arc_angle.m_begin < arc_angle.m_end)
        {
          K = i;
          should_split = (arc_angle.m_begin < crit_angles[K]
                          && crit_angles[K] < arc_angle.m_end);
        }
      else
        {
          K = reverse_i;
          should_split = (arc_angle.m_end < crit_angles[K]
                          && crit_angles[K] < arc_angle.m_begin);
        }

      if (should_split)
        {
          vec2 end_pt;

          end_pt = center + radius * crit_pts[K];
          add_tessellated_arc_segment(prev_pt, end_pt, center, radius,
                                      range_type<float>(prev_angle, crit_angles[K]),
                                      continuation_with_predessor, d);
          prev_pt = end_pt;
          prev_angle = crit_angles[K];
          continuation_with_predessor = true;
        }
    }

  add_tessellated_arc_segment(prev_pt, end, center, radius,
                              range_type<float>(prev_angle, arc_angle.m_end),
                              continuation_with_predessor, d);
}

///////////////////////////////////////////////
// fastuidraw::TessellatedPath::Refiner methods
fastuidraw::TessellatedPath::Refiner::
Refiner(fastuidraw::TessellatedPath *p, const Path &input)
{
  RefinerPrivate *d;
  m_d = d = FASTUIDRAWnew RefinerPrivate();
  d->m_path = p;
  d->m_contours.resize(input.number_contours());
  for (unsigned int i = 0, endi = input.number_contours(); i < endi; ++i)
    {
      d->m_contours[i].m_is_closed = input.contour(i)->closed();
    }
}

fastuidraw::TessellatedPath::Refiner::
~Refiner()
{
  RefinerPrivate *d;
  d = static_cast<RefinerPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::reference_counted_ptr<fastuidraw::TessellatedPath>
fastuidraw::TessellatedPath::Refiner::
tessellated_path(void) const
{
  RefinerPrivate *d;
  d = static_cast<RefinerPrivate*>(m_d);
  return d->m_path;
}

void
fastuidraw::TessellatedPath::Refiner::
refine_tessellation(float max_distance,
                    unsigned int additional_recursion_count)
{
  RefinerPrivate *d;
  d = static_cast<RefinerPrivate*>(m_d);

  if (d->m_path->max_distance() > max_distance)
    {
      d->m_path = FASTUIDRAWnew TessellatedPath(this, max_distance, additional_recursion_count);
    }
}

//////////////////////////////////////
// fastuidraw::TessellatedPath methods
fastuidraw::TessellatedPath::
TessellatedPath(Refiner *p, float max_distance,
                unsigned int additional_recursion_count)
{
  RefinerPrivate *ref_d;
  TessellatedPathPrivate *d;

  ref_d = static_cast<RefinerPrivate*>(p->m_d);

  TessellationParams params;
  params.m_max_distance = max_distance;
  params.m_max_recursion = ref_d->m_path->max_recursion() + additional_recursion_count;

  m_d = d = FASTUIDRAWnew TessellatedPathPrivate(ref_d->m_contours.size(), params);
  if (ref_d->m_contours.empty())
    {
      return;
    }

  std::vector<segment> work_room;
  TessellatedPathBuildingState builder;
  for(unsigned int o = 0, endo = ref_d->m_contours.size(); o < endo; ++o)
    {
      const RefinerContour &contour(ref_d->m_contours[o]);
      d->start_contour(builder, o, contour.m_start_pt, contour.m_edges.size());

      for(unsigned int e = 0, ende = contour.m_edges.size(); e < ende; ++e)
        {
          const RefinerEdge edge(contour.m_edges[e]);
          SegmentStorage segment_storage;
          float tmp;

          FASTUIDRAWassert(work_room.empty());
          segment_storage.m_d = &work_room;

          if (edge.m_tess_state)
            {
              edge.m_tess_state->resume_tessellation(d->m_params, &segment_storage, &tmp);
              d->m_max_recursion = t_max(d->m_max_recursion, edge.m_tess_state->recursion_depth());
            }
          else
            {
              edge.m_interpolator->produce_tessellation(d->m_params, &segment_storage, &tmp);
            }

          d->add_edge(builder, o, e, work_room, tmp);
          d->m_contours[o].m_edges[e].m_edge_type = edge.m_interpolator->edge_type();
        }

      d->end_contour(builder);
      d->m_contours[o].m_is_closed = contour.m_is_closed;
    }
  d->finalize(builder);
}

fastuidraw::TessellatedPath::
TessellatedPath(const Path &input,
                fastuidraw::TessellatedPath::TessellationParams TP,
                reference_counted_ptr<Refiner> *ref)
{
  TessellatedPathPrivate *d;
  m_d = d = FASTUIDRAWnew TessellatedPathPrivate(input.number_contours(), TP);

  if (input.number_contours() == 0)
    {
      return;
    }

  std::vector<segment> work_room;
  RefinerPrivate *refiner_d(nullptr);

  if (ref)
    {
      Refiner *r;
      r = FASTUIDRAWnew Refiner(this, input);
      *ref = r;
      refiner_d = static_cast<RefinerPrivate*>(r->m_d);
    }

  TessellatedPathBuildingState builder;
  for(unsigned int o = 0, endo = input.number_contours(); o < endo; ++o)
    {
      const reference_counted_ptr<const PathContour> &contour(input.contour(o));

      if (refiner_d)
        {
          refiner_d->m_contours[o].m_edges.resize(contour->number_interpolators());
          refiner_d->m_contours[o].m_start_pt = contour->point(0);
        }

      d->start_contour(builder, o, contour->point(0), contour->number_interpolators());
      for(unsigned int e = 0, ende = contour->number_interpolators(); e < ende; ++e)
        {
          SegmentStorage segment_storage;
          const reference_counted_ptr<const PathContour::interpolator_base> &interpolator(contour->interpolator(e));
          reference_counted_ptr<PathContour::tessellation_state> tess_state;
          float tmp;

          FASTUIDRAWassert(interpolator);
          FASTUIDRAWassert(work_room.empty());
          segment_storage.m_d = &work_room;

          tess_state = interpolator->produce_tessellation(d->m_params, &segment_storage, &tmp);
          if (tess_state)
            {
              d->m_max_recursion = t_max(d->m_max_recursion, tess_state->recursion_depth());
            }

          if (refiner_d)
            {
              refiner_d->m_contours[o].m_edges[e].m_tess_state = tess_state;
              refiner_d->m_contours[o].m_edges[e].m_interpolator = interpolator;
            }

          d->add_edge(builder, o, e, work_room, tmp);
          d->m_contours[o].m_edges[e].m_edge_type = interpolator->edge_type();
        }

      d->end_contour(builder);
      d->m_contours[o].m_is_closed = contour->closed();
    }
  d->finalize(builder);
}

fastuidraw::TessellatedPath::
TessellatedPath(const TessellatedPath &with_arcs, float thresh)
{
  m_d = FASTUIDRAWnew TessellatedPathPrivate(with_arcs, thresh);
}

fastuidraw::TessellatedPath::
~TessellatedPath()
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

const fastuidraw::TessellatedPath::TessellationParams&
fastuidraw::TessellatedPath::
tessellation_parameters(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_params;
}

float
fastuidraw::TessellatedPath::
max_distance(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  return d->m_max_distance;
}

unsigned int
fastuidraw::TessellatedPath::
max_recursion(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  return d->m_max_recursion;
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::segment>
fastuidraw::TessellatedPath::
segment_data(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data);
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::join>
fastuidraw::TessellatedPath::
join_data(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_join_data);
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::cap>
fastuidraw::TessellatedPath::
cap_data(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_cap_data);
}

unsigned int
fastuidraw::TessellatedPath::
number_contours(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_contours.size();
}

bool
fastuidraw::TessellatedPath::
contour_closed(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_contours[contour].m_is_closed;
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
contour_range(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return range_type<unsigned int>(d->m_contours[contour].m_edges.front().m_edge_range.m_begin,
                                  d->m_contours[contour].m_edges.back().m_edge_range.m_end);
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::segment>
fastuidraw::TessellatedPath::
contour_segment_data(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(contour_range(contour));
}

unsigned int
fastuidraw::TessellatedPath::
number_edges(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_contours[contour].m_edges.size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
edge_range(unsigned int contour, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_contours[contour].m_edges[edge].m_edge_range;
}

enum fastuidraw::PathEnums::edge_type_t
fastuidraw::TessellatedPath::
edge_type(unsigned int contour, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_contours[contour].m_edges[edge].m_edge_type;
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::segment>
fastuidraw::TessellatedPath::
edge_segment_data(unsigned int contour, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(edge_range(contour, edge));
}

const fastuidraw::Rect&
fastuidraw::TessellatedPath::
bounding_box(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  return d->m_bounding_box.as_rect();
}

bool
fastuidraw::TessellatedPath::
has_arcs(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_has_arcs;
}

const fastuidraw::StrokedPath&
fastuidraw::TessellatedPath::
stroked(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  if (!d->m_stroked)
    {
      d->m_stroked = FASTUIDRAWnew StrokedPath(*this);
    }
  return *(d->m_stroked);
}

const fastuidraw::TessellatedPath&
fastuidraw::TessellatedPath::
linearization(float thresh) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  if (!d->m_has_arcs)
    {
      return *this;
    }

  if (d->m_linearization.empty())
    {
      /* default tessellation where arcs are barely tessellated */
      d->m_linearization.push_back(FASTUIDRAWnew TessellatedPath(*this, -1.0f));
    }

  if (thresh < 0.0f)
    {
      return *(d->m_linearization.front());
    }

  /* asking for a finer tessellation than the
   * max-distance of this TessellatedPath is
   * pointless.
   */
  thresh = t_max(thresh, d->m_max_distance);
  if (d->m_linearization.back()->max_distance() <= thresh)
    {
      typename std::vector<reference_counted_ptr<const TessellatedPath> >::const_iterator iter;
      iter = std::lower_bound(d->m_linearization.begin(),
                              d->m_linearization.end(),
                              thresh, detail::reverse_compare_max_distance);
      FASTUIDRAWassert(iter != d->m_linearization.end());
      FASTUIDRAWassert(*iter);
      FASTUIDRAWassert((*iter)->max_distance() <= thresh);
      return **iter;
    }

  float current(d->m_linearization.back()->max_distance());
  while (current > thresh)
    {
      current *= 0.5f;
      d->m_linearization.push_back(FASTUIDRAWnew TessellatedPath(*this, current));
    }
  return *(d->m_linearization.back());
}

const fastuidraw::TessellatedPath&
fastuidraw::TessellatedPath::
linearization(void) const
{
  return linearization(-1.0f);
}

const fastuidraw::FilledPath&
fastuidraw::TessellatedPath::
filled(float thresh) const
{
  const TessellatedPath *tess;
  TessellatedPathPrivate *tess_d;

  tess = &linearization(thresh);
  tess_d = static_cast<TessellatedPathPrivate*>(tess->m_d);
  if (!tess_d->m_filled)
    {
      tess_d->m_filled = FASTUIDRAWnew FilledPath(*tess);
    }
  return *(tess_d->m_filled);
}

const fastuidraw::FilledPath&
fastuidraw::TessellatedPath::
filled(void) const
{
  return filled(-1.0f);
}

const fastuidraw::PartitionedTessellatedPath&
fastuidraw::TessellatedPath::
partitioned(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  if (!d->m_partitioned)
    {
      d->m_partitioned = FASTUIDRAWnew PartitionedTessellatedPath(*this);
    }
  return *(d->m_partitioned);
}
