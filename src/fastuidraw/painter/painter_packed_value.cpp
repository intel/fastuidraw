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
#include <fastuidraw/painter/painter_packed_value_pool.hpp>
#include <private/painter_backend/painter_packed_value_pool_private.hpp>
#include <private/util_private.hpp>

namespace
{
  class PainterPackedValuePoolPrivate
  {
  public:
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterItemShaderData>::Holder m_item_shader_data_pool;
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterBlendShaderData>::Holder m_blend_shader_data_pool;
    fastuidraw::detail::PackedValuePool<fastuidraw::PainterBrushShaderData>::Holder m_custom_brush_shader_data_pool;
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

fastuidraw::c_array<fastuidraw::generic_data>
fastuidraw::PainterPackedValueBase::
packed_data(void) const
{
  detail::PackedValuePoolBase::ElementBase *d;
  d = static_cast<detail::PackedValuePoolBase::ElementBase*>(m_d);
  return make_c_array(d->m_data);
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::Image> >
fastuidraw::PainterPackedValueBase::
bind_images(void) const
{
  detail::PackedValuePoolBase::ElementBase *d;
  d = static_cast<detail::PackedValuePoolBase::ElementBase*>(m_d);
  return make_c_array(d->m_bind_images);
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

fastuidraw::PainterPackedValue<fastuidraw::PainterBrushShaderData>
fastuidraw::PainterPackedValuePool::
create_packed_value(const PainterBrushShaderData &value)
{
  PainterPackedValuePoolPrivate *d;
  detail::PackedValuePool<PainterBrushShaderData>::Element *e;

  d = static_cast<PainterPackedValuePoolPrivate*>(m_d);
  e = d->m_custom_brush_shader_data_pool.allocate(value);
  return PainterPackedValue<PainterBrushShaderData>(e);
}
