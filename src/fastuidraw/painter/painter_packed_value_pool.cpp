/*!
 * \file painter_packed_value_pool.cpp
 * \brief file painter_packed_value_pool.cpp
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

#include "packing/private/painter_packer.hpp"

/////////////////////////////////////////////
// fastuidraw::PainterPackedValueBase methods
fastuidraw::PainterPackedValueBase::
PainterPackedValueBase(void)
{
  m_d = nullptr;
}

fastuidraw::PainterPackedValueBase::
PainterPackedValueBase(void *p):
  m_d(p)
{
  if (m_d)
    {
      PainterPacker::acquire_packed_value(m_d);
    }
}

fastuidraw::PainterPackedValueBase::
PainterPackedValueBase(const PainterPackedValueBase &obj):
  m_d(obj.m_d)
{
  if (m_d)
    {
      PainterPacker::acquire_packed_value(m_d);
    }
}

fastuidraw::PainterPackedValueBase::
~PainterPackedValueBase()
{
  if (m_d)
    {
      PainterPacker::release_packed_value(m_d);
      m_d = nullptr;
    }
}

void
fastuidraw::PainterPackedValueBase::
swap(PainterPackedValueBase &obj)
{
  std::swap(m_d, obj.m_d);
}

const fastuidraw::PainterPackedValueBase&
fastuidraw::PainterPackedValueBase::
operator=(const PainterPackedValueBase &obj)
{
  if (m_d != obj.m_d)
    {
      PainterPackedValueBase v(obj);
      swap(v);
    }
  return *this;
}

const void*
fastuidraw::PainterPackedValueBase::
raw_value(void) const
{
  return PainterPacker::raw_data_of_packed_value(m_d);
}

/////////////////////////////////////////////////////
// PainterPackedValuePool methods
fastuidraw::PainterPackedValuePool::
PainterPackedValuePool(void)
{
  m_d = PainterPacker::create_painter_packed_value_pool_d();
}

fastuidraw::PainterPackedValuePool::
~PainterPackedValuePool()
{
  PainterPacker::delete_painter_packed_value_pool_d(m_d);
  m_d = nullptr;
}

fastuidraw::PainterPackedValue<fastuidraw::PainterBrush>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterBrush &value)
{
  return fastuidraw::PainterPackedValue<PainterBrush>(PainterPacker::create_packed_value(m_d, value));
}

fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterClipEquations &value)
{
  return fastuidraw::PainterPackedValue<PainterClipEquations>(PainterPacker::create_packed_value(m_d, value));
}

fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterItemMatrix &value)
{
  return fastuidraw::PainterPackedValue<PainterItemMatrix>(PainterPacker::create_packed_value(m_d, value));
}

fastuidraw::PainterPackedValue<fastuidraw::PainterItemShaderData>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterItemShaderData &value)
{
  return fastuidraw::PainterPackedValue<PainterItemShaderData>(PainterPacker::create_packed_value(m_d, value));
}

fastuidraw::PainterPackedValue<fastuidraw::PainterCompositeShaderData>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterCompositeShaderData &value)
{
  return fastuidraw::PainterPackedValue<PainterCompositeShaderData>(PainterPacker::create_packed_value(m_d, value));
}

fastuidraw::PainterPackedValue<fastuidraw::PainterBlendShaderData>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterBlendShaderData &value)
{
  return fastuidraw::PainterPackedValue<PainterBlendShaderData>(PainterPacker::create_packed_value(m_d, value));
}
