/*!
 * \file arc_tessellated_path.cpp
 * \brief file arc_tessellated_path.cpp
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
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include "private/util_private.hpp"
#include "private/bounding_box.hpp"
#include "private/path_util_private.hpp"

namespace
{
  class PerEdge
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

  class PerContour
  {
  public:
    std::vector<PerEdge> m_edges;
  };

  class RefinerPrivate
  {
  public:
    fastuidraw::reference_counted_ptr<fastuidraw::TessellatedPath> m_path;
    std::vector<PerContour> m_contours;
  };

  class TessellatedPathBuildingState
  {
  public:
    TessellatedPathBuildingState(void):
      m_loc(0)
    {}

    unsigned int m_loc, m_ende;
    std::list<std::vector<fastuidraw::TessellatedPath::segment> > m_temp;
    float m_contour_length, m_open_contour_length, m_closed_contour_length;
    std::list<std::vector<fastuidraw::TessellatedPath::segment> >::iterator m_start_contour;
  };

  class Edge
  {
  public:
    fastuidraw::range_type<unsigned int> m_edge_range;
    enum fastuidraw::PathEnums::edge_type_t m_edge_type;
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
    start_contour(TessellatedPathBuildingState &b, unsigned int contour,
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

    std::vector<std::vector<Edge> > m_edges;
    std::vector<fastuidraw::TessellatedPath::segment> m_segment_data;
    fastuidraw::BoundingBox<float> m_bounding_box;
    fastuidraw::TessellatedPath::TessellationParams m_params;
    float m_max_distance;
    bool m_has_arcs;
    unsigned int m_max_recursion;
    fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> m_stroked;
    fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath> m_filled;
    std::vector<fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath> > m_linearization;
  };

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
        l = 1.0f - t_cos(half_angle);
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
                              bool tangent_with_predecessor,
                              std::vector<fastuidraw::TessellatedPath::segment> *d)
  {
    using namespace fastuidraw;

    float a, da, theta;
    unsigned int i, cnt;
    float max_arc(M_PI / 4.0);

    a = t_abs(arc_angle.m_end - arc_angle.m_begin);
    cnt = 1u + static_cast<unsigned int>(a / max_arc);
    da = (arc_angle.m_end - arc_angle.m_begin) / static_cast<float>(cnt);

    for (i = 0, theta = arc_angle.m_begin; i < cnt; ++i, theta += da)
      {
        TessellatedPath::segment S;

        if (i == 0)
          {
            S.m_start_pt = start;
            S.m_tangent_with_predecessor = tangent_with_predecessor;
          }
        else
          {
            S.m_start_pt = d->back().m_end_pt;
            S.m_tangent_with_predecessor = true;
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
    const float pi(M_PI), two_pi(2.0f * pi);
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
        newS.m_tangent_with_predecessor = false;

        dst->push_back(newS);
        prev_pt = p;
      }

    const float pi(M_PI);
    float error, eff(t_min(pi, t_abs(0.5f * delta_angle)));

    error = S.m_radius * (1.0f - t_cos(eff));
    return error;
  }
}

//////////////////////////////////////////////
// TessellatedPathPrivate methods
TessellatedPathPrivate::
TessellatedPathPrivate(unsigned int num_contours,
                       fastuidraw::TessellatedPath::TessellationParams TP):
  m_edges(num_contours),
  m_params(TP),
  m_max_distance(0.0f),
  m_has_arcs(false),
  m_max_recursion(0u)
{
}

TessellatedPathPrivate::
TessellatedPathPrivate(const fastuidraw::TessellatedPath &with_arcs,
                       float thresh):
  m_edges(with_arcs.number_contours()),
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
      start_contour(builder, C, with_arcs.number_edges(C));
      for (unsigned int E = 0, endE = with_arcs.number_edges(C); E < endE; ++E)
        {
          float d;
          std::vector<TessellatedPath::segment> tmp;
          c_array<const TessellatedPath::segment> segs;

          segs = with_arcs.edge_segment_data(C, E);
          for (const TessellatedPath::segment &S : segs)
            {
              d = add_linearization_of_segment(thresh, S, &tmp);
            }
          add_edge(builder, C, E, tmp, d);
        }
      end_contour(builder);
    }
  finalize(builder);
}

void
TessellatedPathPrivate::
start_contour(TessellatedPathBuildingState &b, unsigned int o, unsigned int num_edges)
{
  FASTUIDRAWassert(o < m_edges.size());

  b.m_contour_length = 0.0f;
  b.m_open_contour_length = 0.0f;
  b.m_closed_contour_length = 0.0f;
  b.m_ende = num_edges;
  m_edges[o].resize(num_edges);
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
  m_edges[o][e].m_edge_range = range_type<unsigned int>(builder.m_loc, builder.m_loc + needed);
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

  if (e + 2 == builder.m_ende)
    {
      builder.m_open_contour_length = builder.m_contour_length;
    }
  else if (e + 1 == builder.m_ende)
    {
      builder.m_closed_contour_length = builder.m_contour_length;
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
          S.m_open_contour_length = builder.m_open_contour_length;
          S.m_closed_contour_length = builder.m_closed_contour_length;
        }
    }
}

void
TessellatedPathPrivate::
finalize(TessellatedPathBuildingState &b)
{
  unsigned int total_needed;

  total_needed = m_edges.back().back().m_edge_range.m_end;
  m_segment_data.reserve(total_needed);
  for(auto iter = b.m_temp.begin(), end_iter = b.m_temp.end(); iter != end_iter; ++iter)
    {
      std::copy(iter->begin(), iter->end(), std::back_inserter(m_segment_data));
    }
  FASTUIDRAWassert(total_needed == m_segment_data.size());
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
  S.m_tangent_with_predecessor = false;

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
      -0.5 * M_PI,
      0.0,
      0.5 * M_PI,
      M_PI,
      1.5 * M_PI,
      2.0 * M_PI,
      2.5 * M_PI,
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
  bool tangent_with_predessor(false);

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
                                      tangent_with_predessor, d);
          prev_pt = end_pt;
          prev_angle = crit_angles[K];
          tangent_with_predessor = false;
        }
    }

  add_tessellated_arc_segment(prev_pt, end, center, radius,
                              range_type<float>(prev_angle, arc_angle.m_end),
                              tangent_with_predessor, d);
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
      const PerContour &contour(ref_d->m_contours[o]);
      d->start_contour(builder, o, contour.m_edges.size());

      for(unsigned int e = 0, ende = contour.m_edges.size(); e < ende; ++e)
        {
          const PerEdge edge(contour.m_edges[e]);
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
          d->m_edges[o][e].m_edge_type = edge.m_interpolator->edge_type();
        }
      d->end_contour(builder);
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
          refiner_d->m_contours[o].m_edges.resize(contour->number_points());
        }

      d->start_contour(builder, o, contour->number_points());
      for(unsigned int e = 0, ende = contour->number_points(); e < ende; ++e)
        {
          SegmentStorage segment_storage;
          const reference_counted_ptr<const PathContour::interpolator_base> &interpolator(contour->interpolator(e));
          reference_counted_ptr<PathContour::tessellation_state> tess_state;
          float tmp;

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
          d->m_edges[o][e].m_edge_type = interpolator->edge_type();
        }

      d->end_contour(builder);
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

unsigned int
fastuidraw::TessellatedPath::
number_contours(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edges.size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
contour_range(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return range_type<unsigned int>(d->m_edges[contour].front().m_edge_range.m_begin,
                                  d->m_edges[contour].back().m_edge_range.m_end);
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

  return d->m_edges[contour].size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
edge_range(unsigned int contour, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edges[contour][edge].m_edge_range;
}

enum fastuidraw::PathEnums::edge_type_t
fastuidraw::TessellatedPath::
edge_type(unsigned int contour, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edges[contour][edge].m_edge_type;
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::segment>
fastuidraw::TessellatedPath::
edge_segment_data(unsigned int contour, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(edge_range(contour, edge));
}

fastuidraw::vec2
fastuidraw::TessellatedPath::
bounding_box_min(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_bounding_box.min_point();
}

fastuidraw::vec2
fastuidraw::TessellatedPath::
bounding_box_max(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_bounding_box.max_point();;
}

fastuidraw::vec2
fastuidraw::TessellatedPath::
bounding_box_size(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_bounding_box.size();
}

bool
fastuidraw::TessellatedPath::
has_arcs(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_has_arcs;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath>&
fastuidraw::TessellatedPath::
stroked(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  if (!d->m_stroked)
    {
      d->m_stroked = FASTUIDRAWnew StrokedPath(*this);
    }
  return d->m_stroked;
}

const fastuidraw::TessellatedPath*
fastuidraw::TessellatedPath::
linearization(float thresh) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  if (!d->m_has_arcs)
    {
      return this;
    }

  if (d->m_linearization.empty())
    {
      /* default tessellation where arcs are barely tessellated */
      d->m_linearization.push_back(FASTUIDRAWnew TessellatedPath(*this, -1.0f));
    }

  if (thresh < 0.0f)
    {
      return d->m_linearization.front().get();
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
      return iter->get();
    }

  float current(d->m_linearization.back()->max_distance());
  while (current > thresh)
    {
      current *= 0.5f;
      d->m_linearization.push_back(FASTUIDRAWnew TessellatedPath(*this, current));
    }
  return d->m_linearization.back().get();
}

const fastuidraw::TessellatedPath*
fastuidraw::TessellatedPath::
linearization(void) const
{
  return linearization(-1.0f);
}

const fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath>&
fastuidraw::TessellatedPath::
filled(float thresh) const
{
  const TessellatedPath *tess;
  TessellatedPathPrivate *tess_d;

  tess = linearization(thresh);
  tess_d = static_cast<TessellatedPathPrivate*>(tess->m_d);
  if (!tess_d->m_filled)
    {
      tess_d->m_filled = FASTUIDRAWnew FilledPath(*tess);
    }
  return tess_d->m_filled;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath>&
fastuidraw::TessellatedPath::
filled(void) const
{
  return filled(-1.0f);
}
