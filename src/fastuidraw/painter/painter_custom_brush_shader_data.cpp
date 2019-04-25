/*!
 * \file painter_custom_brush_shader_data.cpp
 * \brief file painter_custom_brush_shader_data.cpp
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

#include <fastuidraw/painter/painter_custom_brush_shader_data.hpp>
#include <vector>
#include "private/util_private.hpp"

namespace
{
  class PainterCustomBrushShaderDataPrivate
  {
  public:
    PainterCustomBrushShaderDataPrivate(void):
      m_dirty(true)
    {}

    void
    update(const fastuidraw::PainterCustomBrushShaderData *p);

    bool m_dirty;
    std::vector<fastuidraw::generic_data> m_packed_data;
    std::vector<fastuidraw::reference_counted_ptr<const fastuidraw::resource_base> > m_resources;
    std::vector<fastuidraw::reference_counted_ptr<const fastuidraw::Image> > m_bind_images;
  };
}

//////////////////////////////////////
// PainterCustomBrushShaderDataPrivate methods
void
PainterCustomBrushShaderDataPrivate::
update(const fastuidraw::PainterCustomBrushShaderData *p)
{
  if (m_dirty)
    {
      unsigned int padded_size, unpadded_size;

      unpadded_size = p->data_size();
      padded_size = FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(unpadded_size);
      m_packed_data.resize(padded_size);
      p->pack_data(make_c_array(m_packed_data));

      m_resources.clear();
      m_resources.resize(p->number_resources());
      p->save_resources(make_c_array(m_resources));

      m_bind_images.clear();
      m_bind_images.resize(p->number_bind_images());
      p->save_bind_images(make_c_array(m_bind_images));

      m_dirty = false;
    }
}

//////////////////////////////////////////////
// fastuidraw::PainterCustomBrushShaderData methods
fastuidraw::PainterCustomBrushShaderData::
PainterCustomBrushShaderData(void)
{
  m_d = FASTUIDRAWnew PainterCustomBrushShaderDataPrivate();
}

fastuidraw::PainterCustomBrushShaderData::
PainterCustomBrushShaderData(const PainterCustomBrushShaderData &obj)
{
  PainterCustomBrushShaderDataPrivate *d;
  d = static_cast<PainterCustomBrushShaderDataPrivate*>(obj.m_d);
  d->update(&obj);
  m_d = FASTUIDRAWnew PainterCustomBrushShaderDataPrivate(*d);
}

fastuidraw::PainterCustomBrushShaderData::
~PainterCustomBrushShaderData()
{
  PainterCustomBrushShaderDataPrivate *d;
  d = static_cast<PainterCustomBrushShaderDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::PainterCustomBrushShaderData::
mark_dirty(void)
{
  PainterCustomBrushShaderDataPrivate *d;
  d = static_cast<PainterCustomBrushShaderDataPrivate*>(m_d);
  d->m_dirty = true;
}

fastuidraw::c_array<const fastuidraw::generic_data>
fastuidraw::PainterCustomBrushShaderData::
packed_data(void) const
{
  PainterCustomBrushShaderDataPrivate *d;
  d = static_cast<PainterCustomBrushShaderDataPrivate*>(m_d);
  d->update(this);
  return make_c_array(d->m_packed_data);
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::resource_base> >
fastuidraw::PainterCustomBrushShaderData::
resources(void) const
{
  PainterCustomBrushShaderDataPrivate *d;
  d = static_cast<PainterCustomBrushShaderDataPrivate*>(m_d);
  d->update(this);
  return make_c_array(d->m_resources);
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::Image> >
fastuidraw::PainterCustomBrushShaderData::
bind_images(void) const
{
  PainterCustomBrushShaderDataPrivate *d;
  d = static_cast<PainterCustomBrushShaderDataPrivate*>(m_d);
  d->update(this);
  return make_c_array(d->m_bind_images);
}

assign_swap_implement(fastuidraw::PainterCustomBrushShaderData)
