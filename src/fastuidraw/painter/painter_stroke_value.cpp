/*!
 * \file painter_stroke_value.cpp
 * \brief file painter_stroke_vvalue.cpp
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

#include <fastuidraw/painter/painter_stroke_value.hpp>
#include "../private/util_private.hpp"

namespace
{
  class PainterStrokeParamsData:public fastuidraw::PainterShaderData::DataBase
  {
  public:
    PainterStrokeParamsData(void):
      m_miter_limit(15.0f),
      m_width(2.0f)
    {}

    virtual
    fastuidraw::PainterShaderData::DataBase*
    copy(void) const
    {
      return FASTUIDRAWnew PainterStrokeParamsData(*this);
    }

    virtual
    unsigned int
    data_size(unsigned int alignment) const
    {
      return fastuidraw::round_up_to_multiple(fastuidraw::PainterStrokeParams::stroke_data_size, alignment);
    }

    virtual
    void
    pack_data(unsigned int alignment, fastuidraw::c_array<fastuidraw::generic_data> dst) const
    {
      FASTUIDRAWunused(alignment);
      dst[fastuidraw::PainterStrokeParams::stroke_miter_limit_offset].f = m_miter_limit;
      dst[fastuidraw::PainterStrokeParams::stroke_width_offset].f = m_width;
    }

    float m_miter_limit;
    float m_width;
  };

  class PainterDashedStrokeParamsData:public fastuidraw::PainterShaderData::DataBase
  {
  public:
    PainterDashedStrokeParamsData(void):
      m_miter_limit(15.0f),
      m_width(2.0f),
      m_dash_offset(0.0f)
    {}

    virtual
    fastuidraw::PainterShaderData::DataBase*
    copy(void) const
    {
      return FASTUIDRAWnew PainterDashedStrokeParamsData(*this);
    }

    virtual
    unsigned int
    data_size(unsigned int alignment) const
    {
      return fastuidraw::round_up_to_multiple(fastuidraw::PainterDashedStrokeParams::stroke_static_data_size, alignment)
        + fastuidraw::round_up_to_multiple(m_dash_pattern.size(), alignment);
    }

    virtual
    void
    pack_data(unsigned int alignment, fastuidraw::c_array<fastuidraw::generic_data> dst) const
    {
      dst[fastuidraw::PainterDashedStrokeParams::stroke_miter_limit_offset].f = m_miter_limit;
      dst[fastuidraw::PainterDashedStrokeParams::stroke_width_offset].f = m_width;
      dst[fastuidraw::PainterDashedStrokeParams::stroke_dash_pattern_dash_offset].f = m_dash_offset;

      if(!m_dash_pattern.empty())
        {
          float total_length = 0.0f;
          unsigned int i, endi;

          fastuidraw::c_array<fastuidraw::generic_data> dst_pattern;
          dst_pattern = dst.sub_array(fastuidraw::round_up_to_multiple(fastuidraw::PainterDashedStrokeParams::stroke_static_data_size, alignment));
          for(i = 0, endi = m_dash_pattern.size(); i < endi; ++i)
            {
              total_length += m_dash_pattern[i];
              dst_pattern[i].f = total_length;
            }
          for(i = i - 1, endi = dst_pattern.size(); i < endi; ++i)
            {
              //make the last entry larger than the total length so a
              //shader can use that to know when it has reached the end.
              dst_pattern[i].f = (total_length + 1.0f) * 2.0f;
            }
          dst[fastuidraw::PainterDashedStrokeParams::stroke_dash_pattern_total_length_offset].f = total_length;
        }
      else
        {
          dst[fastuidraw::PainterDashedStrokeParams::stroke_dash_pattern_total_length_offset].f = -1.0f;
        }
    }

    float m_miter_limit;
    float m_width;
    float m_dash_offset;
    std::vector<float> m_dash_pattern;
  };
}

///////////////////////////////////
// fastuidraw::PainterStrokeParams methods
fastuidraw::PainterStrokeParams::
PainterStrokeParams(void)
{
  m_data = FASTUIDRAWnew PainterStrokeParamsData();
}

float
fastuidraw::PainterStrokeParams::
miter_limit(void) const
{
  PainterStrokeParamsData *d;
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  return d->m_miter_limit;
}

float
fastuidraw::PainterStrokeParams::
width(void) const
{
  PainterStrokeParamsData *d;
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  return d->m_miter_limit;
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
miter_limit(float f)
{
  PainterStrokeParamsData *d;
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_miter_limit = f;
  return *this;
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
width(float f)
{
  PainterStrokeParamsData *d;
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_width = f;
  return *this;
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

fastuidraw::const_c_array<float>
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
dash_pattern(const_c_array<float> f)
{
  PainterDashedStrokeParamsData *d;
  assert(dynamic_cast<PainterDashedStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterDashedStrokeParamsData*>(m_data);
  d->m_dash_pattern.resize(f.size());
  std::copy(f.begin(), f.end(), d->m_dash_pattern.begin());
  return *this;
}
