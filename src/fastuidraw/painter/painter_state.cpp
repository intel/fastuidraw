/*!
 * \file painter_state.cpp
 * \brief file painter_state.cpp
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
#include <fastuidraw/painter/painter_state.hpp>
#include "../private/util_private.hpp"

namespace
{
  class ShaderDataPrivate
  {
  public:
    ShaderDataPrivate(void)
    {}

    ShaderDataPrivate(fastuidraw::const_c_array<fastuidraw::generic_data> pdata):
      m_data(pdata.begin(), pdata.end())
    {}

    std::vector<fastuidraw::generic_data> m_data;
  };
}


//////////////////////////////////////////////
// fastuidraw::PainterState::ClipEquations methods
void
fastuidraw::PainterState::ClipEquations::
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
// fastuidraw::PainterState::ItemMatrix methods
void
fastuidraw::PainterState::ItemMatrix::
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
// fastuidraw::PainterState::ShaderData methods
fastuidraw::PainterState::ShaderData::
ShaderData(const_c_array<generic_data> pdata)
{
  m_d = FASTUIDRAWnew ShaderDataPrivate(pdata);
}

fastuidraw::PainterState::ShaderData::
ShaderData(void)
{
  m_d = FASTUIDRAWnew ShaderDataPrivate();
}

fastuidraw::PainterState::ShaderData::
ShaderData(const ShaderData &obj)
{
  ShaderDataPrivate *d;
  d = reinterpret_cast<ShaderDataPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ShaderDataPrivate(*d);
}

fastuidraw::PainterState::ShaderData::
~ShaderData()
{
  ShaderDataPrivate *d;
  d = reinterpret_cast<ShaderDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterState::ShaderData&
fastuidraw::PainterState::ShaderData::
operator=(const ShaderData &rhs)
{
  ShaderDataPrivate *d, *rhs_d;
  d = reinterpret_cast<ShaderDataPrivate*>(m_d);
  rhs_d = reinterpret_cast<ShaderDataPrivate*>(rhs.m_d);
  *d = *rhs_d;
  return *this;
}

fastuidraw::c_array<fastuidraw::generic_data>
fastuidraw::PainterState::ShaderData::
data(void)
{
  ShaderDataPrivate *d;
  d = reinterpret_cast<ShaderDataPrivate*>(m_d);
  return make_c_array(d->m_data);
}

fastuidraw::const_c_array<fastuidraw::generic_data>
fastuidraw::PainterState::ShaderData::
data(void) const
{
  ShaderDataPrivate *d;
  d = reinterpret_cast<ShaderDataPrivate*>(m_d);
  return make_c_array(d->m_data);
}

void
fastuidraw::PainterState::ShaderData::
resize_data(unsigned int sz)
{
  ShaderDataPrivate *d;
  d = reinterpret_cast<ShaderDataPrivate*>(m_d);
  d->m_data.resize(sz);
}

unsigned int
fastuidraw::PainterState::ShaderData::
data_size(unsigned int alignment) const
{
  return round_up_to_multiple(data().size(), alignment);
}

void
fastuidraw::PainterState::ShaderData::
pack_data(unsigned int, c_array<generic_data> dst) const
{
  const_c_array<generic_data> p(data());
  if(!p.empty())
    {
      assert(dst.size() >= p.size());
      std::copy(p.begin(), p.end(), dst.begin());
    }
}

///////////////////////////////////
// fastuidraw::PainterState::StrokeParams methods
fastuidraw::PainterState::StrokeParams::
StrokeParams(void)
{
  resize_data(2);
}

float
fastuidraw::PainterState::StrokeParams::
miter_limit(void) const
{
  return data()[PainterPacking::stroke_miter_limit_offset].f;
}

float
fastuidraw::PainterState::StrokeParams::
width(void) const
{
  return data()[PainterPacking::stroke_width_offset].f;
}

fastuidraw::PainterState::StrokeParams&
fastuidraw::PainterState::StrokeParams::
miter_limit(float f)
{
  data()[PainterPacking::stroke_miter_limit_offset].f = f;
  return *this;
}

fastuidraw::PainterState::StrokeParams&
fastuidraw::PainterState::StrokeParams::
width(float f)
{
  data()[PainterPacking::stroke_width_offset].f = f;
  return *this;
}
