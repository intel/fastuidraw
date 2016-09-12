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

#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
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
    float m_width;
    float m_dash_offset;
    float m_total_length;
    float m_first_interval_start;
    std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> m_dash_pattern;
  };

  class DashEvaluator:public fastuidraw::DashEvaluatorBase
  {
  public:
    virtual
    bool
    compute_dash_interval(const fastuidraw::PainterShaderData::DataBase *data,
                          float distance,
                          fastuidraw::range_type<float> &out_interval) const;
  };
}
//////////////////////////////////////
// PainterDashedStrokeParamsData methods
PainterDashedStrokeParamsData::
PainterDashedStrokeParamsData(void):
  m_miter_limit(15.0f),
  m_width(2.0f),
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
  dst[PainterDashedStrokeParams::stroke_width_offset].f = m_width;
  dst[PainterDashedStrokeParams::stroke_dash_offset_offset].f = m_dash_offset;
  dst[PainterDashedStrokeParams::stroke_total_length_offset].f = m_total_length;
  dst[PainterDashedStrokeParams::stroke_first_interval_start].f = m_first_interval_start;

  if(!m_dash_pattern.empty())
    {
      float total_length = 0.0f;
      unsigned int i, endi, j;

      c_array<generic_data> dst_pattern;
      dst_pattern = dst.sub_array(round_up_to_multiple(PainterDashedStrokeParams::stroke_static_data_size, alignment));
      for(i = 0, j = 0, endi = m_dash_pattern.size(); i < endi; ++i, j += 2)
        {
          total_length += m_dash_pattern[i].m_draw_length;
          dst_pattern[j].f = total_length;

          total_length += m_dash_pattern[i].m_space_length;
          dst_pattern[j + 1].f = total_length;
        }
      for(i = j, endi = dst_pattern.size(); i < endi; ++i)
        {
          //make the last entry larger than the total length so a
          //shader can use that to know when it has reached the end.
          dst_pattern[i].f = (total_length + 1.0f) * 2.0f;
        }
    }
}

///////////////////////////////
// DashEvaluator methods
bool
DashEvaluator::
compute_dash_interval(const fastuidraw::PainterShaderData::DataBase *data,
                      float distance,
                      fastuidraw::range_type<float> &out_interval) const
{
  const PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<const PainterDashedStrokeParamsData*>(data) != NULL);
  d = static_cast<const PainterDashedStrokeParamsData*>(data);

  if(d->m_total_length <= 0.0f)
    {
      out_interval.m_begin = out_interval.m_end = 0.0f;
      return false;
    }

  float q, r, iq;
  q = distance / d->m_total_length;
  r = std::modf(q, &iq);

  distance = r * d->m_total_length;
  iq *= d->m_total_length;

  out_interval.m_begin = 0.0f;
  for(unsigned int i = 0, endi = d->m_dash_pattern.size(); i < endi; ++i)
    {
      float f, draw, skip;

      f = distance - out_interval.m_begin;
      draw = d->m_dash_pattern[i].m_draw_length;
      skip = d->m_dash_pattern[i].m_space_length;
      if(f <= draw)
        {
          out_interval.m_begin += iq;
          out_interval.m_end = out_interval.m_begin + draw;
          return true;
        }

      out_interval.m_begin += draw;
      f -= draw;
      if(f <= skip)
        {
          out_interval.m_begin += iq;
          out_interval.m_end = out_interval.m_begin + skip;
          return false;
        }
    }

  out_interval.m_end = out_interval.m_begin;
  return false;
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
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_miter_limit;
}

float
fastuidraw::PainterDashedStrokeParams::
width(void) const
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_miter_limit;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
miter_limit(float f)
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_miter_limit = f;
  return *this;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
width(float f)
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_width = f;
  return *this;
}

float
fastuidraw::PainterDashedStrokeParams::
dash_offset(void) const
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return d->m_dash_offset;
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
dash_offset(float f)
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_dash_offset = f;
  return *this;
}

fastuidraw::const_c_array<fastuidraw::PainterDashedStrokeParams::DashPatternElement>
fastuidraw::PainterDashedStrokeParams::
dash_pattern(void) const
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  return make_c_array(d->m_dash_pattern);
}

fastuidraw::PainterDashedStrokeParams&
fastuidraw::PainterDashedStrokeParams::
dash_pattern(const_c_array<DashPatternElement> f)
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);

  /* skip to first element on f[] that is non-zero.
   */
  while(f.front().m_draw_length <= 0.0f && f.front().m_space_length <= 0.0f)
    {
      f = f.sub_array(1);
    }

  d->m_dash_pattern.resize(f.size());
  if(d->m_dash_pattern.empty())
    {
      return *this;
    }

  /* santize the dash pattern:
       - starting draw length can be 0
       - any other 0 lengths are to be joined
   */
  unsigned int current_write(0);

  d->m_dash_pattern[current_write].m_draw_length = std::max(0.0f, f[0].m_draw_length);
  d->m_dash_pattern[current_write].m_space_length = std::max(0.0f, f[0].m_space_length);
  for(unsigned int i = 1, endi = f.size(); i < endi; ++i)
    {
      /* things to do:
           - if d->m_dash_pattern[current_write].m_space_length is 0,
             then we join it with the element we are on by adding the
             draw lengths.
           - if f[i] has draw length 0 we join it's skip with
             d->m_dash_pattern[current_write]
       */
      if(d->m_dash_pattern[current_write].m_space_length <= 0.0f)
        {
          d->m_dash_pattern[current_write].m_draw_length += std::max(0.0f, f[i].m_draw_length);
          d->m_dash_pattern[current_write].m_space_length = std::max(0.0f, f[i].m_space_length);
        }
      else if(f[i].m_draw_length <= 0.0f)
        {
          d->m_dash_pattern[current_write].m_space_length += std::max(0.0f, f[i].m_space_length);
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

  if(d->m_dash_pattern.back().m_space_length <= 0.0f && d->m_dash_pattern.front().m_draw_length > 0.0f)
    {
      d->m_first_interval_start = -d->m_dash_pattern.back().m_draw_length;
    }
  else if(d->m_dash_pattern.back().m_space_length > 0.0f && d->m_dash_pattern.front().m_draw_length <= 0.0f)
    {
      d->m_first_interval_start = -d->m_dash_pattern.back().m_space_length;
    }
  else
    {
      d->m_first_interval_start = 0.0f;
    }

  return *this;
}

fastuidraw::reference_counted_ptr<const fastuidraw::DashEvaluatorBase>
fastuidraw::PainterDashedStrokeParams::
dash_evaluator(void)
{
  return FASTUIDRAWnew DashEvaluator();
}
