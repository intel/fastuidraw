/*!
 * \file painter_packed_value.cpp
 * \brief file painter_packed_value.cpp
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

#include <fastuidraw/painter/painter_packed_value.hpp>
#include "backend/private/painter_packed_value_pool_private.hpp"

namespace
{
  class PainterPackedValuePoolPrivate
  {
  public:
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterBrush>::Holder m_brush_pool;
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterClipEquations>::Holder m_clip_equations_pool;
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterItemMatrix>::Holder m_item_matrix_pool;
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterItemShaderData>::Holder m_item_shader_data_pool;
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterCompositeShaderData>::Holder m_composite_shader_data_pool;
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterBlendShaderData>::Holder m_blend_shader_data_pool;
  };
}

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
  detail::PackedValuePoolBase::ElementBase *d;
  d = static_cast<detail::PackedValuePoolBase::ElementBase*>(m_d);
  if (d)
    {
      d->acquire();
    }
}

fastuidraw::PainterPackedValueBase::
PainterPackedValueBase(const PainterPackedValueBase &obj):
  m_d(obj.m_d)
{
  detail::PackedValuePoolBase::ElementBase *d;
  d = static_cast<detail::PackedValuePoolBase::ElementBase*>(m_d);
  if (d)
    {
      d->acquire();
    }
}

fastuidraw::PainterPackedValueBase::
~PainterPackedValueBase()
{
  detail::PackedValuePoolBase::ElementBase *d;
  d = static_cast<detail::PackedValuePoolBase::ElementBase*>(m_d);
  if (d)
    {
      detail::PackedValuePoolBase::ElementBase::release(d);
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
  detail::PackedValuePoolBase::ElementBase *d;
  d = static_cast<detail::PackedValuePoolBase::ElementBase*>(m_d);
  return d->m_raw_data;
}

/////////////////////////////////////////////////////
// PainterPackedValuePool methods
fastuidraw::PainterPackedValuePool::
PainterPackedValuePool(void)
{
  m_d = FASTUIDRAWnew PainterPackedValuePoolPrivate();
}

fastuidraw::PainterPackedValuePool::
~PainterPackedValuePool()
{
  PainterPackedValuePoolPrivate *d;
  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::PainterPackedValue<fastuidraw::PainterBrush>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterBrush &value)
{
  PainterPackedValuePoolPrivate *d;
  detail::PackedValuePool<PainterBrush>::Element *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  e = d->m_brush_pool.allocate(value);
  return PainterPackedValue<PainterBrush>(e);
}

fastuidraw::PainterPackedValue<fastuidraw::PainterClipEquations>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterClipEquations &value)
{
  PainterPackedValuePoolPrivate *d;
  detail::PackedValuePool<PainterClipEquations>::Element *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  e = d->m_clip_equations_pool.allocate(value);
  return PainterPackedValue<PainterClipEquations>(e);
}

fastuidraw::PainterPackedValue<fastuidraw::PainterItemMatrix>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterItemMatrix &value)
{
  PainterPackedValuePoolPrivate *d;
  detail::PackedValuePool<PainterItemMatrix>::Element *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  e = d->m_item_matrix_pool.allocate(value);
  return PainterPackedValue<PainterItemMatrix>(e);
}

fastuidraw::PainterPackedValue<fastuidraw::PainterItemShaderData>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterItemShaderData &value)
{
  PainterPackedValuePoolPrivate *d;
  detail::PackedValuePool<PainterItemShaderData>::Element *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  e = d->m_item_shader_data_pool.allocate(value);
  return PainterPackedValue<PainterItemShaderData>(e);
}

fastuidraw::PainterPackedValue<fastuidraw::PainterCompositeShaderData>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterCompositeShaderData &value)
{
  PainterPackedValuePoolPrivate *d;
  detail::PackedValuePool<PainterCompositeShaderData>::Element *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  e = d->m_composite_shader_data_pool.allocate(value);
  return PainterPackedValue<PainterCompositeShaderData>(e);
}

fastuidraw::PainterPackedValue<fastuidraw::PainterBlendShaderData>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterBlendShaderData &value)
{
  PainterPackedValuePoolPrivate *d;
  detail::PackedValuePool<PainterBlendShaderData>::Element *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  e = d->m_blend_shader_data_pool.allocate(value);
  return PainterPackedValue<PainterBlendShaderData>(e);
}
