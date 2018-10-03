/*!
 * \file glyph_atlas.cpp
 * \brief file glyph_atlas.cpp
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


#include <mutex>
#include <fastuidraw/text/glyph_atlas.hpp>

#include "../private/interval_allocator.hpp"
#include "../private/util_private.hpp"

namespace
{
  class GlyphAtlasGeometryBackingStoreBasePrivate
  {
  public:
    GlyphAtlasGeometryBackingStoreBasePrivate(unsigned int psize, bool presizable):
      m_resizeable(presizable),
      m_size(psize)
    {
    }

    bool m_resizeable;
    unsigned int m_size;
  };

  class GlyphAtlasPrivate
  {
  public:
    explicit
    GlyphAtlasPrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasGeometryBackingStoreBase> pgeometry_store):
      m_geometry_store(pgeometry_store),
      m_geometry_data_allocator(pgeometry_store->size()),
      m_geometry_data_allocated(0)
    {
      FASTUIDRAWassert(m_geometry_store);
    };

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasGeometryBackingStoreBase> m_geometry_store;
    fastuidraw::interval_allocator m_geometry_data_allocator;
    unsigned int m_geometry_data_allocated;
  };
}

///////////////////////////////////////////
// fastuidraw::GlyphAtlasGeometryBackingStoreBase methods
fastuidraw::GlyphAtlasGeometryBackingStoreBase::
GlyphAtlasGeometryBackingStoreBase(unsigned int psize, bool presizable)
{
  m_d = FASTUIDRAWnew GlyphAtlasGeometryBackingStoreBasePrivate(psize, presizable);
}

fastuidraw::GlyphAtlasGeometryBackingStoreBase::
~GlyphAtlasGeometryBackingStoreBase()
{
  GlyphAtlasGeometryBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasGeometryBackingStoreBasePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

unsigned int
fastuidraw::GlyphAtlasGeometryBackingStoreBase::
size(void)
{
  GlyphAtlasGeometryBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasGeometryBackingStoreBasePrivate*>(m_d);
  return d->m_size;
}

bool
fastuidraw::GlyphAtlasGeometryBackingStoreBase::
resizeable(void) const
{
  GlyphAtlasGeometryBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasGeometryBackingStoreBasePrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::GlyphAtlasGeometryBackingStoreBase::
resize(unsigned int new_size)
{
  GlyphAtlasGeometryBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasGeometryBackingStoreBasePrivate*>(m_d);
  FASTUIDRAWassert(d->m_resizeable);
  FASTUIDRAWassert(new_size > d->m_size);
  resize_implement(new_size);
  d->m_size = new_size;
}

///////////////////////////////////////////////
// fastuidraw::GlyphAtlas methods
fastuidraw::GlyphAtlas::
GlyphAtlas(reference_counted_ptr<GlyphAtlasGeometryBackingStoreBase> pgeometry_store)
{
  m_d = FASTUIDRAWnew GlyphAtlasPrivate(pgeometry_store);
};

fastuidraw::GlyphAtlas::
~GlyphAtlas()
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

int
fastuidraw::GlyphAtlas::
allocate_geometry_data(c_array<const generic_data> pdata)
{
  if (pdata.empty())
    {
      return 0;
    }

  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  int return_value;
  return_value = d->m_geometry_data_allocator.allocate_interval(pdata.size());
  if (return_value == -1)
    {
      if (d->m_geometry_store->resizeable())
        {
          d->m_geometry_store->resize(pdata.size() + 2 * d->m_geometry_store->size());
          d->m_geometry_data_allocator.resize(d->m_geometry_store->size());
          return_value = d->m_geometry_data_allocator.allocate_interval(pdata.size());
          FASTUIDRAWassert(return_value != -1);
        }
      else
        {
          return return_value;
        }
    }

  d->m_geometry_data_allocated += pdata.size();
  d->m_geometry_store->set_values(return_value, pdata);
  return return_value;
}

void
fastuidraw::GlyphAtlas::
deallocate_geometry_data(int location, int count)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  if (location < 0 || count == 0)
    {
      FASTUIDRAWassert(count == 0);
      return;
    }

  FASTUIDRAWassert(count > 0);
  d->m_geometry_data_allocated -= count;
  d->m_geometry_data_allocator.free_interval(location, count);
}

unsigned int
fastuidraw::GlyphAtlas::
geometry_data_allocated(void)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  return d->m_geometry_data_allocated;
}

void
fastuidraw::GlyphAtlas::
clear(void)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  d->m_geometry_data_allocator.reset(d->m_geometry_data_allocator.size());
}

void
fastuidraw::GlyphAtlas::
flush(void) const
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  d->m_geometry_store->flush();
}

fastuidraw::reference_counted_ptr<const fastuidraw::GlyphAtlasGeometryBackingStoreBase>
fastuidraw::GlyphAtlas::
geometry_store(void) const
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  return d->m_geometry_store;
}
