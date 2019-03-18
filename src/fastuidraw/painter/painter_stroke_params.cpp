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

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/painter/attribute_data/stroked_path.hpp>
#include <private/util_private.hpp>
#include <private/painter_stroking_data_selector_common.hpp>

namespace
{
  class PainterStrokeParamsData:public fastuidraw::PainterShaderData::DataBase
  {
  public:
    explicit
    PainterStrokeParamsData(void):
      m_miter_limit(15.0f),
      m_radius(1.0f),
      m_stroking_units(fastuidraw::PainterStrokeParams::path_stroking_units)
    {}

    virtual
    fastuidraw::PainterShaderData::DataBase*
    copy(void) const
    {
      return FASTUIDRAWnew PainterStrokeParamsData(*this);
    }

    virtual
    unsigned int
    data_size(void) const
    {
      using namespace fastuidraw;
      return FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(PainterStrokeParams::stroke_data_size);
    }

    virtual
    void
    pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const
    {
      using namespace fastuidraw;
      dst[PainterStrokeParams::stroke_miter_limit_offset].f = m_miter_limit;
      if (m_stroking_units == PainterStrokeParams::pixel_stroking_units)
        {
          dst[PainterStrokeParams::stroke_radius_offset].f = -m_radius;
        }
      else
        {
          dst[PainterStrokeParams::stroke_radius_offset].f = m_radius;
        }
    }

    float m_miter_limit;
    float m_radius;
    enum fastuidraw::PainterStrokeParams::stroking_units_t m_stroking_units;
  };
}

///////////////////////////////////
// fastuidraw::PainterStrokeParams methods
fastuidraw::PainterStrokeParams::
PainterStrokeParams(void)
{
  m_data = FASTUIDRAWnew PainterStrokeParamsData();
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
miter_limit(float f)
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_miter_limit = t_max(0.0f, f);
  return *this;
}

float
fastuidraw::PainterStrokeParams::
miter_limit(void) const
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  return d->m_miter_limit;
}

float
fastuidraw::PainterStrokeParams::
width(void) const
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  return d->m_radius * 2.0f;
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
width(float f)
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_radius = (f > 0.0f ) ? 0.5f * f : 0.0f;
  return *this;
}

float
fastuidraw::PainterStrokeParams::
radius(void) const
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  return d->m_radius;
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
radius(float f)
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_radius = (f > 0.0f ) ? f : 0.0f;
  return *this;
}

enum fastuidraw::PainterStrokeParams::stroking_units_t
fastuidraw::PainterStrokeParams::
stroking_units(void) const
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  return d->m_stroking_units;
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
stroking_units(enum stroking_units_t v)
{
  PainterStrokeParamsData *d;
  FASTUIDRAWassert(dynamic_cast<PainterStrokeParamsData*>(m_data) != nullptr);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_stroking_units = v;
  return *this;
}

fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>
fastuidraw::PainterStrokeParams::
stroking_data_selector(bool pixel_arc_stroking_possible)
{
  return FASTUIDRAWnew detail::StrokingDataSelectorT<PainterStrokeParamsData>(pixel_arc_stroking_possible);
}
