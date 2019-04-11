/*!
 * \file painter_custom_brush_shader_data.cpp
 * \brief file painter_custom_brush_shader_data.cpp
 *
 * Copyright 2019 by Intel.
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


#include <fastuidraw/painter/painter_custom_brush_shader_data.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>

//////////////////////////////////////////////
// fastuidraw::PainterCustomBrushShaderData methods
fastuidraw::PainterCustomBrushShaderData::
PainterCustomBrushShaderData(void)
{
  m_data = nullptr;
}

fastuidraw::PainterCustomBrushShaderData::
PainterCustomBrushShaderData(const PainterCustomBrushShaderData &obj)
{
  if (obj.m_data)
    {
      m_data = obj.m_data->copy();
    }
  else
    {
      m_data = nullptr;
    }
}

fastuidraw::PainterCustomBrushShaderData::
~PainterCustomBrushShaderData()
{
  if (m_data)
    {
      FASTUIDRAWdelete(m_data);
    }
  m_data = nullptr;
}

void
fastuidraw::PainterCustomBrushShaderData::
swap(PainterCustomBrushShaderData &obj)
{
  std::swap(m_data, obj.m_data);
}

fastuidraw::PainterCustomBrushShaderData&
fastuidraw::PainterCustomBrushShaderData::
operator=(const PainterCustomBrushShaderData &rhs)
{
  if (this != &rhs)
    {
      PainterCustomBrushShaderData v(rhs);
      swap(v);
    }
  return *this;
}

void
fastuidraw::PainterCustomBrushShaderData::
pack_data(c_array<generic_data> dst) const
{
  if (m_data)
    {
      m_data->pack_data(dst);
    }
}

unsigned int
fastuidraw::PainterCustomBrushShaderData::
data_size(void) const
{
  return m_data ?
    FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(m_data->data_size()) :
    0;
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::Image> >
fastuidraw::PainterCustomBrushShaderData::
bind_images(void) const
{
  c_array<const reference_counted_ptr<const Image> > return_value;
  if (m_data)
    {
      return_value = m_data->bind_images();
    }
  return return_value;
}
