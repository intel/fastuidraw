/*!
 * \file tessellated_path.cpp
 * \brief file tessellated_path.cpp
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


#include <list>
#include <vector>
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include "private/util_private.hpp"

namespace
{
  class TessellatedPathPrivate
  {
  public:
    TessellatedPathPrivate(const fastuidraw::Path &input,
                           fastuidraw::TessellatedPath::TessellationParams TP);

    std::vector<std::vector<fastuidraw::range_type<unsigned int> > > m_edge_ranges;
    std::vector<fastuidraw::TessellatedPath::point> m_point_data;
    fastuidraw::vec2 m_box_min, m_box_max;
    fastuidraw::TessellatedPath::TessellationParams m_params;
    fastuidraw::StrokedPath::const_handle m_stroked;
    fastuidraw::FilledPath::const_handle m_filled;
  };
}

//////////////////////////////////////////////
// TessellatedPathPrivate methods
TessellatedPathPrivate::
TessellatedPathPrivate(const fastuidraw::Path &input,
                       fastuidraw::TessellatedPath::TessellationParams TP):
  m_edge_ranges(input.number_outlines()),
  m_box_min(0.0f, 0.0f),
  m_box_max(0.0f, 0.0f),
  m_params(TP)
{
  if(input.number_outlines() > 0)
    {

      std::list<std::vector<fastuidraw::TessellatedPath::point> > temp;
      std::vector<fastuidraw::TessellatedPath::point> work_room(m_params.m_max_segments + 1);
      std::list<std::vector<fastuidraw::TessellatedPath::point> >::const_iterator iter, end_iter;

      for(unsigned int loc = 0, o = 0, endo = input.number_outlines(); o < endo; ++o)
        {
          fastuidraw::PathContour::const_handle outline(input.outline(o));
          float outline_length(0.0f);

          m_edge_ranges[o].resize(outline->number_points());
          for(unsigned int e = 0, ende = outline->number_points(); e < ende; ++e)
            {
              temp.push_back(std::vector<fastuidraw::TessellatedPath::point>());

              unsigned int needed;

              needed = outline->interpolator(e)->produce_tessellation(m_params,
                                                                      fastuidraw::make_c_array(work_room));
              m_edge_ranges[o][e] = fastuidraw::range_type<unsigned int>(loc, loc + needed);
              loc += needed;

              for(unsigned int n = 0; n < needed; ++n)
                {
                  const fastuidraw::vec2 &pt(work_room[n].m_p);

                  work_room[n].m_distance_from_outline_start = outline_length
                    + work_room[n].m_distance_from_edge_start;

                  if(o == 0 and e == 0 and n == 0)
                    {
                      m_box_min = pt;
                      m_box_max = pt;
                    }
                  else
                    {
                      m_box_min.x() = std::min(m_box_min.x(), pt.x());
                      m_box_min.y() = std::min(m_box_min.y(), pt.y());
                      m_box_max.x() = std::max(m_box_max.x(), pt.x());
                      m_box_max.y() = std::max(m_box_max.y(), pt.y());
                    }
                }
              temp.push_back(std::vector<fastuidraw::TessellatedPath::point>(work_room.begin(), work_room.begin() + needed));
              outline_length = temp.back().back().m_distance_from_outline_start;
            }
        }

      unsigned int total_needed;

      total_needed = m_edge_ranges.back().back().m_end;
      m_point_data.reserve(total_needed);
      for(iter = temp.begin(), end_iter = temp.end(); iter != end_iter; ++iter)
        {
          std::copy(iter->begin(), iter->end(),
                    std::back_insert_iterator<std::vector<fastuidraw::TessellatedPath::point> >(m_point_data));
        }
      assert(total_needed == m_point_data.size());
    }
  else
    {
      m_box_min = m_box_max = fastuidraw::vec2(0.0f, 0.0f);
    }
}

//////////////////////////////////////
// fastuidraw::TessellatedPath methods
fastuidraw::TessellatedPath::
TessellatedPath(const Path &input,
                fastuidraw::TessellatedPath::TessellationParams TP)
{
  m_d = FASTUIDRAWnew TessellatedPathPrivate(input, TP);
}

fastuidraw::TessellatedPath::
~TessellatedPath()
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::StrokedPath::const_handle&
fastuidraw::TessellatedPath::
stroked(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);
  if(!d->m_stroked)
    {
      d->m_stroked = FASTUIDRAWnew StrokedPath(*this);
    }
  return d->m_stroked;
}

const fastuidraw::FilledPath::const_handle&
fastuidraw::TessellatedPath::
filled(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);
  if(!d->m_filled)
    {
      d->m_filled = FASTUIDRAWnew FilledPath(*this);
    }
  return d->m_filled;
}

const fastuidraw::TessellatedPath::TessellationParams&
fastuidraw::TessellatedPath::
tessellation_parameters(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return d->m_params;
}

fastuidraw::const_c_array<fastuidraw::TessellatedPath::point>
fastuidraw::TessellatedPath::
point_data(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_point_data);
}

unsigned int
fastuidraw::TessellatedPath::
number_outlines(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges.size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
outline_range(unsigned int outline) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return range_type<unsigned int>(d->m_edge_ranges[outline].front().m_begin,
                                  d->m_edge_ranges[outline].back().m_end);
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
unclosed_outline_range(unsigned int outline) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  range_type<unsigned int> return_value;
  unsigned int num_edges(number_edges(outline));

  return_value.m_begin = d->m_edge_ranges[outline].front().m_begin;
  return_value.m_end = (num_edges > 1) ?
    d->m_edge_ranges[outline][num_edges - 2].m_end:
    d->m_edge_ranges[outline][num_edges - 1].m_end;

  return return_value;
}

fastuidraw::const_c_array<fastuidraw::TessellatedPath::point>
fastuidraw::TessellatedPath::
outline_point_data(unsigned int outline) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_point_data).sub_array(outline_range(outline));
}

fastuidraw::const_c_array<fastuidraw::TessellatedPath::point>
fastuidraw::TessellatedPath::
unclosed_outline_point_data(unsigned int outline) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_point_data).sub_array(unclosed_outline_range(outline));
}

unsigned int
fastuidraw::TessellatedPath::
number_edges(unsigned int outline) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges[outline].size();
}

fastuidraw::range_type<unsigned int>
fastuidraw::TessellatedPath::
edge_range(unsigned int outline, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return d->m_edge_ranges[outline][edge];
}

fastuidraw::const_c_array<fastuidraw::TessellatedPath::point>
fastuidraw::TessellatedPath::
edge_point_data(unsigned int outline, unsigned int edge) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return make_c_array(d->m_point_data).sub_array(edge_range(outline, edge));
}

fastuidraw::vec2
fastuidraw::TessellatedPath::
bounding_box_min(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return d->m_box_min;
}

fastuidraw::vec2
fastuidraw::TessellatedPath::
bounding_box_max(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return d->m_box_max;
}

fastuidraw::vec2
fastuidraw::TessellatedPath::
bounding_box_size(void) const
{
  TessellatedPathPrivate *d;
  d = reinterpret_cast<TessellatedPathPrivate*>(m_d);

  return d->m_box_max - d->m_box_min;
}
