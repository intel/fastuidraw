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
#include <vector>
#include "private/util_private.hpp"

namespace
{
  class PainterShaderDataPrivate
  {
  public:
    PainterShaderDataPrivate(void):
      m_dirty(true)
    {}

    void
    update(const fastuidraw::PainterShaderData *p);

    bool m_dirty;
    std::vector<fastuidraw::generic_data> m_packed_data;
    std::vector<fastuidraw::reference_counted_ptr<const fastuidraw::resource_base> > m_resources;
  };
}

//////////////////////////////////////
// PainterShaderDataPrivate methods
void
PainterShaderDataPrivate::
update(const fastuidraw::PainterShaderData *p)
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

      m_dirty = false;
    }
}

//////////////////////////////////////////////
// fastuidraw::PainterShaderData methods
fastuidraw::PainterShaderData::
PainterShaderData(void)
{
  m_d = FASTUIDRAWnew PainterShaderDataPrivate();
}

fastuidraw::PainterShaderData::
PainterShaderData(const PainterShaderData &obj)
{
  PainterShaderDataPrivate *d;
  d = static_cast<PainterShaderDataPrivate*>(obj.m_d);
  d->update(&obj);
  m_d = FASTUIDRAWnew PainterShaderDataPrivate(*d);
}

fastuidraw::PainterShaderData::
~PainterShaderData()
{
  PainterShaderDataPrivate *d;
  d = static_cast<PainterShaderDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::PainterShaderData::
mark_dirty(void)
{
  PainterShaderDataPrivate *d;
  d = static_cast<PainterShaderDataPrivate*>(m_d);
  d->m_dirty = true;
}

fastuidraw::c_array<const fastuidraw::generic_data>
fastuidraw::PainterShaderData::
packed_data(void) const
{
  PainterShaderDataPrivate *d;
  d = static_cast<PainterShaderDataPrivate*>(m_d);
  d->update(this);
  return make_c_array(d->m_packed_data);
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::resource_base> >
fastuidraw::PainterShaderData::
resources(void) const
{
  PainterShaderDataPrivate *d;
  d = static_cast<PainterShaderDataPrivate*>(m_d);
  d->update(this);
  return make_c_array(d->m_resources);
}

assign_swap_implement(fastuidraw::PainterShaderData)
