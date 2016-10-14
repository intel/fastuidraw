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
#include <fastuidraw/painter/stroked_path.hpp>
#include "../private/util_private.hpp"

namespace
{
  class PainterStrokeParamsData:public fastuidraw::PainterShaderData::DataBase
  {
  public:
    PainterStrokeParamsData(void):
      m_miter_limit(15.0f),
      m_radius(1.0f)
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
      dst[fastuidraw::PainterStrokeParams::stroke_radius_offset].f = m_radius;
    }

    float m_miter_limit;
    float m_radius;
  };

  class StrokingDataSelector:public fastuidraw::StrokingDataSelectorBase
  {
  public:
    explicit
    StrokingDataSelector(bool pixel_width);

    virtual
    float
    compute_rounded_thresh(const fastuidraw::PainterShaderData::DataBase *data,
                           float thresh) const;
    void
    stroking_distances(const fastuidraw::PainterShaderData::DataBase *data,
                       float *out_pixel_distance,
                       float *out_item_space_distance) const;

  private:
    bool m_pixel_width;
  };

}

////////////////////////////////
// StrokingDataSelector methods
StrokingDataSelector::
StrokingDataSelector(bool pixel_width):
  m_pixel_width(pixel_width)
{}

float
StrokingDataSelector::
compute_rounded_thresh(const fastuidraw::PainterShaderData::DataBase *data,
                       float thresh) const
{
  const PainterStrokeParamsData *d;
  d = static_cast<const PainterStrokeParamsData*>(data);

  if(d->m_radius <= 0.0f)
    {
      /* Not really stroking, just select a LARGE value
         to get a very low level of detail.
       */
      return 10000.0f;
    }
  else
    {
      float return_value;

      return_value = 1.0f / d->m_radius;
      if(!m_pixel_width)
        {
          return_value *= thresh;
        }
      return return_value;
    }
}

void
StrokingDataSelector::
stroking_distances(const fastuidraw::PainterShaderData::DataBase *data,
                   float *out_pixel_distance,
                   float *out_item_space_distance) const
{
  const PainterStrokeParamsData *d;
  d = static_cast<const PainterStrokeParamsData*>(data);

  if(m_pixel_width)
    {
      *out_pixel_distance = d->m_radius;
      *out_item_space_distance = 0.0f;
    }
  else
    {
      *out_pixel_distance = 0.0f;
      *out_item_space_distance = d->m_radius;
    }
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
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_miter_limit = f;
  return *this;
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
  return d->m_radius * 2.0f;
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
width(float f)
{
  PainterStrokeParamsData *d;
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_radius = 0.5f * f;
  return *this;
}

float
fastuidraw::PainterStrokeParams::
radius(void) const
{
  PainterStrokeParamsData *d;
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  return d->m_radius;
}

fastuidraw::PainterStrokeParams&
fastuidraw::PainterStrokeParams::
radius(float f)
{
  PainterStrokeParamsData *d;
  assert(dynamic_cast<PainterStrokeParamsData*>(m_data) != NULL);
  d = static_cast<PainterStrokeParamsData*>(m_data);
  d->m_radius = f;
  return *this;
}

fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>
fastuidraw::PainterStrokeParams::
stroking_data_selector(bool pixel_width_stroking)
{
  return FASTUIDRAWnew StrokingDataSelector(pixel_width_stroking);
}
