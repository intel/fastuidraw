/*!
 * \file painter_value.cpp
 * \brief file painter_value.cpp
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


#include <vector>
#include <fastuidraw/painter/painter_value.hpp>
#include "../private/util_private.hpp"

//////////////////////////////////////////////
// fastuidraw::PainterClipEquations methods
void
fastuidraw::PainterClipEquations::
pack_data(unsigned int, c_array<generic_data> dst) const
{
  using namespace PainterPacking;
  dst[clip0_coeff_x].f = m_clip_equations[0].x();
  dst[clip0_coeff_y].f = m_clip_equations[0].y();
  dst[clip0_coeff_w].f = m_clip_equations[0].z();

  dst[clip1_coeff_x].f = m_clip_equations[1].x();
  dst[clip1_coeff_y].f = m_clip_equations[1].y();
  dst[clip1_coeff_w].f = m_clip_equations[1].z();

  dst[clip2_coeff_x].f = m_clip_equations[2].x();
  dst[clip2_coeff_y].f = m_clip_equations[2].y();
  dst[clip2_coeff_w].f = m_clip_equations[2].z();

  dst[clip3_coeff_x].f = m_clip_equations[3].x();
  dst[clip3_coeff_y].f = m_clip_equations[3].y();
  dst[clip3_coeff_w].f = m_clip_equations[3].z();
}

//////////////////////////////////////////////
// fastuidraw::PainterItemMatrix methods
void
fastuidraw::PainterItemMatrix::
pack_data(unsigned int, c_array<generic_data> dst) const
{
  using namespace PainterPacking;
  dst[item_matrix_m00_offset].f = m_item_matrix(0, 0);
  dst[item_matrix_m01_offset].f = m_item_matrix(0, 1);
  dst[item_matrix_m02_offset].f = m_item_matrix(0, 2);

  dst[item_matrix_m10_offset].f = m_item_matrix(1, 0);
  dst[item_matrix_m11_offset].f = m_item_matrix(1, 1);
  dst[item_matrix_m12_offset].f = m_item_matrix(1, 2);

  dst[item_matrix_m20_offset].f = m_item_matrix(2, 0);
  dst[item_matrix_m21_offset].f = m_item_matrix(2, 1);
  dst[item_matrix_m22_offset].f = m_item_matrix(2, 2);
}

//////////////////////////////////////////////
// fastuidraw::PainterShaderData methods
fastuidraw::PainterShaderData::
PainterShaderData(void)
{
  m_data = NULL;
}

fastuidraw::PainterShaderData::
PainterShaderData(const PainterShaderData &obj)
{
  if(obj.m_data)
    {
      m_data = obj.m_data->copy();
    }
  else
    {
      m_data = NULL;
    }
}

fastuidraw::PainterShaderData::
~PainterShaderData()
{
  if(m_data)
    {
      FASTUIDRAWdelete(m_data);
    }
  m_data = NULL;
}

fastuidraw::PainterShaderData&
fastuidraw::PainterShaderData::
operator=(const PainterShaderData &rhs)
{
  if(this != &rhs)
    {
      if(m_data)
        {
          FASTUIDRAWdelete(m_data);
        }

      if(rhs.m_data)
        {
          m_data = rhs.m_data->copy();
        }
    }
  return *this;
}

void
fastuidraw::PainterShaderData::
pack_data(unsigned int alignment, c_array<generic_data> dst) const
{
  if(m_data)
    {
      m_data->pack_data(alignment, dst);
    }
}

unsigned int
fastuidraw::PainterShaderData::
data_size(unsigned int alignment) const
{
  return m_data ?
    m_data->data_size(alignment) :
    0;
}
