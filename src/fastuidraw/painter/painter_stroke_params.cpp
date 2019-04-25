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
  class PainterStrokeParamsPrivate
  {
  public:
    explicit
    PainterStrokeParamsPrivate(void):
      m_miter_limit(15.0f),
      m_radius(1.0f),
      m_stroking_units(fastuidraw::PainterStrokeParams::path_stroking_units)
    {}

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
  m_d = FASTUIDRAWnew PainterStrokeParamsPrivate();
}

fastuidraw::PainterStrokeParams::
~PainterStrokeParams()
{
  PainterStrokeParamsPrivate *d;
  d = static_cast<PainterStrokeParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

copy_ctor(fastuidraw::PainterStrokeParams,
          PainterStrokeParams,
          PainterStrokeParamsPrivate)

assign_swap_implement(fastuidraw::PainterStrokeParams)

setget_implement(fastuidraw::PainterStrokeParams,
                 PainterStrokeParamsPrivate,
                 float, radius)

setget_implement(fastuidraw::PainterStrokeParams,
                 PainterStrokeParamsPrivate,
                 float, miter_limit)

setget_implement(fastuidraw::PainterStrokeParams,
                 PainterStrokeParamsPrivate,
                 enum fastuidraw::PainterStrokeParams::stroking_units_t,
                 stroking_units)

float
fastuidraw::PainterStrokeParams::
width(void) const
{
  return 2.0f * radius();
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
width(float f)
{
  return radius(0.5f * f);
}

unsigned int
fastuidraw::PainterStrokeParams::
data_size(void) const
{
  return FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(PainterStrokeParams::stroke_data_size);
}

void
fastuidraw::PainterStrokeParams::
pack_data(c_array<generic_data> dst) const
{
  PainterStrokeParamsPrivate *d;
  d = static_cast<PainterStrokeParamsPrivate*>(m_d);

  dst[stroke_miter_limit_offset].f = t_max(0.0f, d->m_miter_limit);
  dst[stroke_radius_offset].f = t_max(0.0f, d->m_radius);
  dst[stroking_units_offset].u = d->m_stroking_units;
}

fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>
fastuidraw::PainterStrokeParams::
stroking_data_selector(bool pixel_arc_stroking_possible)
{
  return FASTUIDRAWnew detail::StrokingDataSelector<stroke_miter_limit_offset, stroke_radius_offset, stroking_units_offset>(pixel_arc_stroking_possible);
}
