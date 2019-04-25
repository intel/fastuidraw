/*!
 * \file painter_dashed_stroke_params.cpp
 * \brief file painter_dashed_stroke_params.cpp
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

#include <algorithm>
#include <cmath>
#include <fastuidraw/util/pixel_distance_math.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/attribute_data/stroked_path.hpp>
#include <private/util_private.hpp>
#include <private/painter_stroking_data_selector_common.hpp>

namespace
{
  class PainterDashedStrokedParamsPrivate
  {
  public:
    PainterDashedStrokedParamsPrivate(void):
      m_miter_limit(15.0f),
      m_radius(1.0f),
      m_stroking_units(fastuidraw::PainterStrokeParams::path_stroking_units),
      m_dash_offset(0.0f),
      m_total_length(0.0f),
      m_first_interval_start(0.0f),
      m_first_interval_start_on_looping(0.0f)
    {}

    float m_miter_limit;
    float m_radius;
    enum fastuidraw::PainterStrokeParams::stroking_units_t m_stroking_units;
    float m_dash_offset;
    float m_total_length;
    float m_first_interval_start;
    float m_first_interval_start_on_looping;
    std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> m_dash_pattern;
    std::vector<fastuidraw::generic_data> m_dash_pattern_packed;
  };
}

///////////////////////////////////
// fastuidraw::PainterDashedStrokeParams methods
fastuidraw::PainterDashedStrokeParams::
PainterDashedStrokeParams(void)
{
  m_d = FASTUIDRAWnew PainterDashedStrokedParamsPrivate();
}

fastuidraw::PainterDashedStrokeParams::
~PainterDashedStrokeParams(void)
{
  PainterDashedStrokedParamsPrivate *d;
  d = static_cast<PainterDashedStrokedParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

copy_ctor(fastuidraw::PainterDashedStrokeParams,
          PainterDashedStrokeParams,
          PainterDashedStrokedParamsPrivate)

assign_swap_implement(fastuidraw::PainterDashedStrokeParams)

setget_implement(fastuidraw::PainterDashedStrokeParams,
                 PainterDashedStrokedParamsPrivate,
                 float, miter_limit)

setget_implement(fastuidraw::PainterDashedStrokeParams,
                 PainterDashedStrokedParamsPrivate,
                 float, radius)

setget_implement(fastuidraw::PainterDashedStrokeParams,
                 PainterDashedStrokedParamsPrivate,
                 enum fastuidraw::PainterStrokeParams::stroking_units_t,
                 stroking_units)

float
fastuidraw::PainterDashedStrokeParams::
width(void) const
{
  return radius() * 2.0f;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
width(float f)
{
  return radius(0.5f * f);
}

float
fastuidraw::PainterDashedStrokeParams::
dash_offset(void) const
{
  PainterDashedStrokedParamsPrivate *d;
  d = static_cast<PainterDashedStrokedParamsPrivate*>(m_d);
  return d->m_dash_offset;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
dash_offset(float f)
{
  PainterDashedStrokedParamsPrivate *d;
  d = static_cast<PainterDashedStrokedParamsPrivate*>(m_d);
  d->m_dash_offset = f;
  return *this;
}

fastuidraw::c_array<const fastuidraw::PainterDashedStrokeParams::DashPatternElement>
fastuidraw::PainterDashedStrokeParams::
dash_pattern(void) const
{
  PainterDashedStrokedParamsPrivate *d;
  d = static_cast<PainterDashedStrokedParamsPrivate*>(m_d);
  return make_c_array(d->m_dash_pattern);
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
dash_pattern(c_array<const DashPatternElement> f)
{
  PainterDashedStrokedParamsPrivate *d;
  d = static_cast<PainterDashedStrokedParamsPrivate*>(m_d);

  /* skip to first element on f[] that is non-zero.   */
  while(f.front().m_draw_length <= 0.0f && f.front().m_space_length <= 0.0f)
    {
      f = f.sub_array(1);
    }

  d->m_dash_pattern.resize(f.size());
  if (d->m_dash_pattern.empty())
    {
      return *this;
    }

  /* santize the dash pattern:
   *   - starting draw length can be 0
   *   - any other 0 lengths are to be joined
   */
  unsigned int current_write(0);

  d->m_dash_pattern[current_write].m_draw_length = std::max(0.0f, f[0].m_draw_length);
  d->m_dash_pattern[current_write].m_space_length = std::max(0.0f, f[0].m_space_length);
  for(unsigned int i = 1, endi = f.size(); i < endi; ++i)
    {
      /* things to do:
       *   - if d->m_dash_pattern[current_write].m_space_length is 0,
       *     then we join it with the element we are on by adding the
       *     draw lengths.
       */
      if (d->m_dash_pattern[current_write].m_space_length <= 0.0f)
        {
          d->m_dash_pattern[current_write].m_draw_length += std::max(0.0f, f[i].m_draw_length);
        }
      else
        {
          ++current_write;
          d->m_dash_pattern[current_write].m_draw_length = std::max(0.0f, f[i].m_draw_length);
          d->m_dash_pattern[current_write].m_space_length = std::max(0.0f, f[i].m_space_length);
        }
    }

  d->m_dash_pattern.resize(current_write + 1);

  d->m_total_length = 0.0f;
  for(unsigned int i = 0, endi = current_write + 1; i < endi; ++i)
    {
      d->m_total_length += d->m_dash_pattern[i].m_draw_length;
      d->m_total_length += d->m_dash_pattern[i].m_space_length;
    }

  if (d->m_dash_pattern.back().m_space_length <= 0.0f && d->m_dash_pattern.front().m_draw_length > 0.0f)
    {
      d->m_first_interval_start_on_looping = -d->m_dash_pattern.back().m_draw_length;
    }
  else
    {
      d->m_first_interval_start_on_looping = 0.0;
    }

  if (d->m_dash_pattern.front().m_draw_length > 0.0f)
    {
      /* make sure that any sample point in rasterizing that
       * it near zero, gets a positive distance from the
       * dash boundary when the dash pattern starts with
       * a draw length.
       */
      d->m_first_interval_start = -d->m_dash_pattern.front().m_draw_length;
    }
  else
    {
      d->m_first_interval_start = 0.0f;
    }

  d->m_dash_pattern_packed.resize(d->m_dash_pattern.size() * 2);
  if (!d->m_dash_pattern_packed.empty())
    {
      float total_length = 0.0f;
      for(unsigned int i = 0, j = 0, endi = d->m_dash_pattern.size(); i < endi; ++i, j += 2)
        {
          total_length += d->m_dash_pattern[i].m_draw_length;
          d->m_dash_pattern_packed[j].f = total_length;

          total_length += d->m_dash_pattern[i].m_space_length;
          d->m_dash_pattern_packed[j + 1].f = total_length;
        }
    }

  return *this;
}

unsigned int
fastuidraw::PainterDashedStrokeParams::
data_size(void) const
{
  PainterDashedStrokedParamsPrivate *d;
  d = static_cast<PainterDashedStrokedParamsPrivate*>(m_d);

  return FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(stroke_static_data_size)
    + FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(2 * d->m_dash_pattern.size());
}

void
fastuidraw::PainterDashedStrokeParams::
pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  PainterDashedStrokedParamsPrivate *d;
  d = static_cast<PainterDashedStrokedParamsPrivate*>(m_d);

  dst[stroke_miter_limit_offset].f = t_max(0.0f, d->m_miter_limit);
  dst[stroke_radius_offset].f = t_max(0.0f, d->m_radius);
  dst[stroking_units_offset].u = d->m_stroking_units;
  dst[stroke_dash_offset_offset].f = d->m_dash_offset;
  dst[stroke_total_length_offset].f = d->m_total_length;
  dst[stroke_first_interval_start_offset].f = d->m_first_interval_start;
  dst[stroke_first_interval_start_on_looping_offset].f = d->m_first_interval_start_on_looping;
  dst[stroke_number_intervals_offset].u = d->m_dash_pattern_packed.size();

  if (!d->m_dash_pattern_packed.empty())
    {
      c_array<generic_data> dst_pattern;
      dst_pattern = dst.sub_array(FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(stroke_static_data_size));
      std::copy(d->m_dash_pattern_packed.begin(), d->m_dash_pattern_packed.end(), dst_pattern.begin());
      for(unsigned int i = d->m_dash_pattern_packed.size(), endi = dst_pattern.size(); i < endi; ++i)
        {
          //make the last entry larger than the total length so a
          //shader can use that to know when it has reached the end.
          dst_pattern[i].f = d->m_total_length * 2.0f + 1.0f;
        }
    }
}

fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>
fastuidraw::PainterDashedStrokeParams::
stroking_data_selector(bool pixel_arc_stroking_possible)
{
  return FASTUIDRAWnew detail::StrokingDataSelector<stroke_miter_limit_offset, stroke_radius_offset, stroking_units_offset>(pixel_arc_stroking_possible);
}
