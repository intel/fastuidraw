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

namespace
{
  class TessellatedPathPrivate
  {
  public:
    TessellatedPathPrivate(const fastuidraw::Path &input,
                              fastuidraw::TessellatedPath::TessellationParams TP);

    std::vector<std::vector<fastuidraw::range_type<unsigned int> > > m_edge_ranges;
    std::vector<fastuidraw::TessellatedPath::segment> m_segment_data;
    fastuidraw::BoundingBox<float> m_bounding_box;
    fastuidraw::TessellatedPath::TessellationParams m_params;
    float m_effective_threshhold;
    bool m_has_arcs;
    unsigned int m_max_segments, m_max_recursion;
    fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> m_stroked;
    fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath> m_filled;
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
}

//////////////////////////////////////////////
// TessellatedPathPrivate methods
TessellatedPathPrivate::
TessellatedPathPrivate(const fastuidraw::Path &input,
                       fastuidraw::TessellatedPath::TessellationParams TP):
  m_edge_ranges(input.number_contours()),
  m_params(TP),
  m_effective_threshhold(0.0f),
  m_has_arcs(false),
  m_max_segments(0u),
  m_max_recursion(0u)
{
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
   * is ALWAYS computed by atan2 and m_end is from atan2 with optionally
   * having 2PI added. Thus we know that -PI <= m_begin <= PI and
   * -PI <= end <= 3 * PI
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

//////////////////////////////////////
// fastuidraw::TessellatedPath methods
fastuidraw::TessellatedPath::
TessellatedPath(const Path &input,
                fastuidraw::TessellatedPath::TessellationParams TP)
{
  TessellatedPathPrivate *d;
  m_d = d = FASTUIDRAWnew TessellatedPathPrivate(input, TP);

  if (input.number_contours() > 0)
    {
      std::list<std::vector<segment> > temp;
      std::vector<segment> work_room;
      std::list<std::vector<segment> >::const_iterator iter, end_iter;

      for(unsigned int loc = 0, o = 0, endo = input.number_contours(); o < endo; ++o)
        {
          reference_counted_ptr<const PathContour> contour(input.contour(o));
          float contour_length(0.0f), open_contour_length(0.0f), closed_contour_length(0.0f);
          std::list<std::vector<segment> >::iterator start_contour;

          d->m_edge_ranges[o].resize(contour->number_points());
          for(unsigned int e = 0, ende = contour->number_points(); e < ende; ++e)
            {
              unsigned int needed, recurse_depth;
              float tmp;
              SegmentStorage segment_storage;

              FASTUIDRAWassert(work_room.empty());
              segment_storage.m_d = &work_room;

              recurse_depth = contour->interpolator(e)->produce_tessellation(d->m_params, &segment_storage, &tmp);
              d->m_max_recursion = t_max(d->m_max_recursion, recurse_depth);

              needed = work_room.size();
              d->m_edge_ranges[o][e] = range_type<unsigned int>(loc, loc + needed);
              loc += needed;

              FASTUIDRAWassert(needed > 0u);
              d->m_max_segments = t_max(d->m_max_segments, needed);
              d->m_effective_threshhold = t_max(d->m_effective_threshhold, tmp);

              work_room.resize(needed);
              for(unsigned int n = 0; n < work_room.size(); ++n)
                {
                  d->m_has_arcs = d->m_has_arcs || (work_room[n].m_type == arc_segment);
                  union_segment(work_room[n], d->m_bounding_box);
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

                  work_room[n].m_distance_from_contour_start = contour_length;
                  contour_length += work_room[n].m_length;
                }

              for(unsigned int n = 0; n < needed; ++n)
                {
                  work_room[n].m_edge_length = work_room[needed - 1].m_distance_from_edge_start
                    + work_room[needed - 1].m_length;
                }

              if (e + 2 == ende)
                {
                  open_contour_length = contour_length;
                }
              else if (e + 1 == ende)
                {
                  closed_contour_length = contour_length;
                }

              /* append the data to temp and clear work_room for the next edge */
              std::list<std::vector<segment> >::iterator t;
              t = temp.insert(temp.end(), std::vector<segment>());
              t->swap(work_room);

              if (e == 0)
                {
                  start_contour = t;
                }
            }

          for(std::list<std::vector<segment> >::iterator
                t = start_contour, endt = temp.end(); t != endt; ++t)
            {
              for(unsigned int e = 0, ende = t->size(); e < ende; ++e)
                {
                  (*t)[e].m_open_contour_length = open_contour_length;
                  (*t)[e].m_closed_contour_length = closed_contour_length;
                }
            }
        }

      unsigned int total_needed;

      total_needed = d->m_edge_ranges.back().back().m_end;
      d->m_segment_data.reserve(total_needed);
      for(iter = temp.begin(), end_iter = temp.end(); iter != end_iter; ++iter)
        {
          std::copy(iter->begin(), iter->end(), std::back_inserter(d->m_segment_data));
        }
      FASTUIDRAWassert(total_needed == d->m_segment_data.size());
    }
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
effective_threshhold(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  return d->m_effective_threshhold;
}

unsigned int
fastuidraw::TessellatedPath::
max_segments(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  return d->m_max_segments;
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

  return d->m_edge_ranges.size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
contour_range(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return range_type<unsigned int>(d->m_edge_ranges[contour].front().m_begin,
                                  d->m_edge_ranges[contour].back().m_end);
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
unclosed_contour_range(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  range_type<unsigned int> return_value;
  unsigned int num_edges(number_edges(contour));

  return_value.m_begin = d->m_edge_ranges[contour].front().m_begin;
  return_value.m_end = (num_edges > 1) ?
    d->m_edge_ranges[contour][num_edges - 2].m_end:
    d->m_edge_ranges[contour][num_edges - 1].m_end;

  return return_value;
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::segment>
fastuidraw::TessellatedPath::
contour_segment_data(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(contour_range(contour));
}

fastuidraw::c_array<const fastuidraw::TessellatedPath::segment>
fastuidraw::TessellatedPath::
unclosed_contour_segment_data(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(unclosed_contour_range(contour));
}

unsigned int
fastuidraw::TessellatedPath::
number_edges(unsigned int contour) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges[contour].size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
edge_range(unsigned int contour, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges[contour][edge];
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

const fastuidraw::reference_counted_ptr<const fastuidraw::FilledPath>&
fastuidraw::TessellatedPath::
filled(void) const
{
  TessellatedPathPrivate *d;
  d = static_cast<TessellatedPathPrivate*>(m_d);
  if (!d->m_filled && !d->m_has_arcs)
    {
      d->m_filled = FASTUIDRAWnew FilledPath(*this);
    }
  return d->m_filled;
}
