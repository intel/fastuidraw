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
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/util/pixel_distance_math.hpp>
#include "../private/util_private.hpp"


namespace
{

  class PainterDashedStrokeParamsData:public fastuidraw::PainterShaderData::DataBase
  {
  public:
    PainterDashedStrokeParamsData(void);

    virtual
    fastuidraw::PainterShaderData::DataBase*
    copy(void) const;

    virtual
    unsigned int
    data_size(unsigned int alignment) const;

    virtual
    void
    pack_data(unsigned int alignment, fastuidraw::c_array<fastuidraw::generic_data> dst) const;

    float m_miter_limit;
    float m_radius;
    enum fastuidraw::PainterStrokeParams::stroking_units_t m_stroking_units;
    float m_dash_offset;
    float m_total_length;
    float m_first_interval_start;
    std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> m_dash_pattern;
    std::vector<fastuidraw::generic_data> m_dash_pattern_packed;
  };
}

//////////////////////////////////////
// PainterDashedStrokeParamsData methods
PainterDashedStrokeParamsData::
PainterDashedStrokeParamsData(void):
  m_miter_limit(15.0f),
  m_radius(1.0f),
  m_stroking_units(fastuidraw::PainterStrokeParams::path_stroking_units),
  m_dash_offset(0.0f),
  m_total_length(0.0f),
  m_first_interval_start(0.0f)
{}

fastuidraw::PainterShaderData::DataBase*
PainterDashedStrokeParamsData::
copy(void) const
{
  return FASTUIDRAWnew PainterDashedStrokeParamsData(*this);
}

unsigned int
PainterDashedStrokeParamsData::
data_size(unsigned int alignment) const
{
  using namespace fastuidraw;
  return round_up_to_multiple(PainterDashedStrokeParams::stroke_static_data_size, alignment)
    + round_up_to_multiple(2 * m_dash_pattern.size(), alignment);
}

void
PainterDashedStrokeParamsData::
pack_data(unsigned int alignment, fastuidraw::c_array<fastuidraw::generic_data> dst) const
{
  using namespace fastuidraw;

  dst[PainterDashedStrokeParams::stroke_miter_limit_offset].f = m_miter_limit;
  if (m_stroking_units == PainterStrokeParams::pixel_stroking_units)
    {
      dst[PainterDashedStrokeParams::stroke_radius_offset].f = -m_radius;
    }
  else
    {
      dst[PainterDashedStrokeParams::stroke_radius_offset].f = m_radius;
    }
  dst[PainterDashedStrokeParams::stroke_dash_offset_offset].f = m_dash_offset;
  dst[PainterDashedStrokeParams::stroke_total_length_offset].f = m_total_length;
  dst[PainterDashedStrokeParams::stroke_first_interval_start_offset].f = m_first_interval_start;
  dst[PainterDashedStrokeParams::stroke_number_intervals_offset].u = m_dash_pattern_packed.size();

  if (!m_dash_pattern_packed.empty())
    {
      c_array<generic_data> dst_pattern;
      dst_pattern = dst.sub_array(round_up_to_multiple(PainterDashedStrokeParams::stroke_static_data_size, alignment));
      std::copy(m_dash_pattern_packed.begin(), m_dash_pattern_packed.end(), dst_pattern.begin());
      for(unsigned int i = m_dash_pattern_packed.size(), endi = dst_pattern.size(); i < endi; ++i)
        {
          //make the last entry larger than the total length so a
          //shader can use that to know when it has reached the end.
          dst_pattern[i].f = m_total_length * 2.0f + 1.0f;
        }
    }
}

///////////////////////////////////
// fastuidraw::PainterDashedStrokeParams methods
fastuidraw::PainterDashedStrokeParams::
PainterDashedStrokeParams(void)
{
  m_data = FASTUIDRAWnew PainterDashedStrokeParamsData();
}

float
fastuidraw::PainterDashedStrokeParams::
miter_limit(void) const
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_miter_limit;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
miter_limit(float f)
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_miter_limit = f;
  return *this;
}

float
fastuidraw::PainterDashedStrokeParams::
width(void) const
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_radius * 2.0f;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
width(float f)
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_radius = (f > 0.0f) ? 0.5f * f : 0.0f;
  return *this;
}

float
fastuidraw::PainterDashedStrokeParams::
radius(void) const
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_radius;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
radius(float f)
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_radius = (f > 0.0f) ? f : 0.0f;
  return *this;
}

float
fastuidraw::PainterDashedStrokeParams::
dash_offset(void) const
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_dash_offset;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
dash_offset(float f)
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_dash_offset = f;
  return *this;
}

fastuidraw::c_array<const fastuidraw::PainterDashedStrokeParams::DashPatternElement>
fastuidraw::PainterDashedStrokeParams::
dash_pattern(void) const
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return make_c_array(d->m_dash_pattern);
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
dash_pattern(c_array<const DashPatternElement> f)
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);

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
   *    - starting draw length can be 0
   *    - any other 0 lengths are to be joined
   */
  unsigned int current_write(0);

  d->m_dash_pattern[current_write].m_draw_length = std::max(0.0f, f[0].m_draw_length);
  d->m_dash_pattern[current_write].m_space_length = std::max(0.0f, f[0].m_space_length);
  for(unsigned int i = 1, endi = f.size(); i < endi; ++i)
    {
      /* things to do:
       *    - if d->m_dash_pattern[current_write].m_space_length is 0,
       *      then we join it with the element we are on by adding the
       *      draw lengths.
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
      d->m_first_interval_start = -d->m_dash_pattern.back().m_draw_length;
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

enum fastuidraw::PainterStrokeParams::stroking_units_t
fastuidraw::PainterDashedStrokeParams::
stroking_units(void) const
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_stroking_units;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
stroking_units(enum PainterStrokeParams::stroking_units_t v)
{
  PainterDashedStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_stroking_units = v;
  return *this;
}

fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>
fastuidraw::PainterDashedStrokeParams::
stroking_data_selector(void)
{
  return PainterStrokeParams::stroking_data_selector();
}
