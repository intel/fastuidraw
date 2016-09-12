/*!
 * \file painter_stroke_params.cpp
 * \brief file painter_stroke_params.cpp
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

#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
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
