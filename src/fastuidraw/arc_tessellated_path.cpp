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
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
#include <fastuidraw/painter/filled_path.hpp>
#include "private/util_private.hpp"
#include "private/bounding_box.hpp"

namespace
{
  class ArcTessellatedPathPrivate
  {
  public:
    ArcTessellatedPathPrivate(const fastuidraw::Path &input,
                              fastuidraw::ArcTessellatedPath::TessellationParams TP);

    std::vector<std::vector<fastuidraw::range_type<unsigned int> > > m_edge_ranges;
    std::vector<fastuidraw::ArcTessellatedPath::segment> m_segment_data;
    fastuidraw::BoundingBox<float> m_bounding_box;
    fastuidraw::ArcTessellatedPath::TessellationParams m_params;
    float m_effective_threshhold;
    unsigned int m_max_segments;
  };

  void
  union_segment(const fastuidraw::ArcTessellatedPath::segment &S,
                fastuidraw::BoundingBox<float> &BB)
  {
    using namespace fastuidraw;
    if (S.m_type == ArcTessellatedPath::line_segment)
      {
        BB.union_point(S.m_p);
        BB.union_point(S.m_data);
      }
    else
      {
        vec2 s, e;
        for (int i = 0; i < 2; ++i)
          {
            vec2 q;
            q = S.m_p + S.m_radius * vec2(t_cos(S.m_data[i]), t_sin(S.m_data[i]));
            BB.union_point(q);
          }
      }
  }

  float
  segment_length(const fastuidraw::ArcTessellatedPath::segment &S)
  {
    using namespace fastuidraw;
    if (S.m_type == ArcTessellatedPath::line_segment)
      {
        return (S.m_p - S.m_data).magnitude();
      }
    else
      {
        return t_abs(S.m_data[0] - S.m_data[1]) * S.m_radius;
      }
  }
}

//////////////////////////////////////////////
// TessellatedPathPrivate methods
ArcTessellatedPathPrivate::
ArcTessellatedPathPrivate(const fastuidraw::Path &input,
                          fastuidraw::ArcTessellatedPath::TessellationParams TP):
  m_edge_ranges(input.number_contours()),
  m_params(TP),
  m_effective_threshhold(0.0f),
  m_max_segments(0u)
{
}

//////////////////////////////////////////////////////////
// fastuidraw::ArcTessellatedPath::SegmentStorage methods
void
fastuidraw::ArcTessellatedPath::SegmentStorage::
add_segment(const segment &S)
{
  std::vector<segment> *d;
  d = static_cast<std::vector<segment>*>(m_d);
  d->push_back(S);
}

//////////////////////////////////////
// fastuidraw::ArcTessellatedPath methods
fastuidraw::ArcTessellatedPath::
ArcTessellatedPath(const Path &input,
                   fastuidraw::ArcTessellatedPath::TessellationParams TP)
{
  ArcTessellatedPathPrivate *d;
  m_d = d = FASTUIDRAWnew ArcTessellatedPathPrivate(input, TP);

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
              unsigned int needed;
              float tmp;
              SegmentStorage segment_storage;

              FASTUIDRAWassert(work_room.empty());
              segment_storage.m_d = &work_room;

              contour->interpolator(e)->produce_tessellation(d->m_params, &segment_storage, &tmp);

              needed = work_room.size();
              d->m_edge_ranges[o][e] = range_type<unsigned int>(loc, loc + needed);
              loc += needed;

              FASTUIDRAWassert(needed > 0u);
              d->m_max_segments = t_max(d->m_max_segments, needed);
              d->m_effective_threshhold = t_max(d->m_effective_threshhold, tmp);

              work_room.resize(needed);
              for(unsigned int n = 0; n < work_room.size(); ++n)
                {
                  union_segment(work_room[n], d->m_bounding_box);
                  work_room[n].m_length = segment_length(work_room[n]);

                  if (n != 0)
                    {
                      work_room[n].m_distance_from_edge_start =
                        work_room[n - 1].m_distance_from_edge_start
                        + work_room[n - 1].m_length;
                    }
                  else
                    {
                      work_room[n].m_distance_from_edge_start = 0.0f;
                    }

                  work_room[n].m_distance_from_contour_start = contour_length;
                  contour_length += work_room[n].m_distance_from_edge_start;
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

fastuidraw::ArcTessellatedPath::
~ArcTessellatedPath()
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

const fastuidraw::ArcTessellatedPath::TessellationParams&
fastuidraw::ArcTessellatedPath::
tessellation_parameters(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return d->m_params;
}

float
fastuidraw::ArcTessellatedPath::
effective_threshhold(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);
  return d->m_effective_threshhold;
}

unsigned int
fastuidraw::ArcTessellatedPath::
max_segments(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);
  return d->m_max_segments;
}

fastuidraw::c_array<const fastuidraw::ArcTessellatedPath::segment>
fastuidraw::ArcTessellatedPath::
segment_data(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data);
}

unsigned int
fastuidraw::ArcTessellatedPath::
number_contours(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges.size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::ArcTessellatedPath::
contour_range(unsigned int contour) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return range_type<unsigned int>(d->m_edge_ranges[contour].front().m_begin,
                                  d->m_edge_ranges[contour].back().m_end);
}

fastuidraw::range_type<unsigned int>
fastuidraw::ArcTessellatedPath::
unclosed_contour_range(unsigned int contour) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  range_type<unsigned int> return_value;
  unsigned int num_edges(number_edges(contour));

  return_value.m_begin = d->m_edge_ranges[contour].front().m_begin;
  return_value.m_end = (num_edges > 1) ?
    d->m_edge_ranges[contour][num_edges - 2].m_end:
    d->m_edge_ranges[contour][num_edges - 1].m_end;

  return return_value;
}

fastuidraw::c_array<const fastuidraw::ArcTessellatedPath::segment>
fastuidraw::ArcTessellatedPath::
contour_segment_data(unsigned int contour) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(contour_range(contour));
}

fastuidraw::c_array<const fastuidraw::ArcTessellatedPath::segment>
fastuidraw::ArcTessellatedPath::
unclosed_contour_segment_data(unsigned int contour) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(unclosed_contour_range(contour));
}

unsigned int
fastuidraw::ArcTessellatedPath::
number_edges(unsigned int contour) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges[contour].size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::ArcTessellatedPath::
edge_range(unsigned int contour, unsigned int edge) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges[contour][edge];
}

fastuidraw::c_array<const fastuidraw::ArcTessellatedPath::segment>
fastuidraw::ArcTessellatedPath::
edge_segment_data(unsigned int contour, unsigned int edge) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_segment_data).sub_array(edge_range(contour, edge));
}

fastuidraw::vec2
fastuidraw::ArcTessellatedPath::
bounding_box_min(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return d->m_bounding_box.min_point();
}

fastuidraw::vec2
fastuidraw::ArcTessellatedPath::
bounding_box_max(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return d->m_bounding_box.max_point();;
}

fastuidraw::vec2
fastuidraw::ArcTessellatedPath::
bounding_box_size(void) const
{
  ArcTessellatedPathPrivate *d;
  d = static_cast<ArcTessellatedPathPrivate*>(m_d);

  return d->m_bounding_box.size();
}
