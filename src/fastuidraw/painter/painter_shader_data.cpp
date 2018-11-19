/*!
 * \file painter_shader_data.cpp
 * \brief file painter_shader_data.cpp
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


#include <fastuidraw/painter/painter_shader_data.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>

//////////////////////////////////////////////
// fastuidraw::PainterShaderData methods
fastuidraw::PainterShaderData::
PainterShaderData(void)
{
  m_data = nullptr;
}

fastuidraw::PainterShaderData::
PainterShaderData(const PainterShaderData &obj)
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

fastuidraw::PainterShaderData::
~PainterShaderData()
{
  if (m_data)
    {
      FASTUIDRAWdelete(m_data);
    }
  m_data = nullptr;
}

void
fastuidraw::PainterShaderData::
swap(PainterShaderData &obj)
{
  std::swap(m_data, obj.m_data);
}

fastuidraw::PainterShaderData&
fastuidraw::PainterShaderData::
operator=(const PainterShaderData &rhs)
{
  if (this != &rhs)
    {
      PainterShaderData v(rhs);
      swap(v);
    }
  return *this;
}

void
fastuidraw::PainterShaderData::
pack_data(c_array<generic_data> dst) const
{
  if (m_data)
    {
      m_data->pack_data(dst);
    }
}

unsigned int
fastuidraw::PainterShaderData::
data_size(void) const
{
  return m_data ?
    FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(m_data->data_size()) :
    0;
}
