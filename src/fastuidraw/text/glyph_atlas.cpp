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
  class GlyphAtlasBackingStoreBasePrivate
  {
  public:
    GlyphAtlasBackingStoreBasePrivate(unsigned int psize, bool presizable):
      m_resizeable(presizable),
      m_size(psize)
    {
    }

    bool m_resizeable;
    unsigned int m_size;
  };

  class DelayedDeallocate
  {
  public:
    int m_location;
    int m_count;
  };

  class GlyphAtlasPrivate
  {
  public:
    explicit
    GlyphAtlasPrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasBackingStoreBase> pstore):
      m_store(pstore),
      m_store_constant(m_store),
      m_data_allocator(pstore->size()),
      m_data_allocated(0),
      m_number_times_cleared(0),
      m_lock_resource_counter(0),
      m_clear_issued(false)
    {
      FASTUIDRAWassert(m_store);
    };

    void
    clear_implement(void)
    {
      m_data_allocator.reset(m_data_allocator.size());
      ++m_number_times_cleared;
      m_clear_issued = false;
      m_delayed_deallocates.clear();
    }

    void
    deallocate_implement(int location, int count)
    {
      m_data_allocated -= count;
      m_data_allocator.free_interval(location, count);
    }

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasBackingStoreBase> m_store;
    fastuidraw::reference_counted_ptr<const fastuidraw::GlyphAtlasBackingStoreBase> m_store_constant;
    fastuidraw::interval_allocator m_data_allocator;
    unsigned int m_data_allocated;
    unsigned int m_number_times_cleared;
    int m_lock_resource_counter;
    bool m_clear_issued;
    std::vector<DelayedDeallocate> m_delayed_deallocates;
  };
}

///////////////////////////////////////////
// fastuidraw::GlyphAtlasBackingStoreBase methods
fastuidraw::GlyphAtlasBackingStoreBase::
GlyphAtlasBackingStoreBase(unsigned int psize, bool presizable)
{
  m_d = FASTUIDRAWnew GlyphAtlasBackingStoreBasePrivate(psize, presizable);
}

fastuidraw::GlyphAtlasBackingStoreBase::
~GlyphAtlasBackingStoreBase()
{
  GlyphAtlasBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasBackingStoreBasePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

unsigned int
fastuidraw::GlyphAtlasBackingStoreBase::
size(void)
{
  GlyphAtlasBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasBackingStoreBasePrivate*>(m_d);
  return d->m_size;
}

bool
fastuidraw::GlyphAtlasBackingStoreBase::
resizeable(void) const
{
  GlyphAtlasBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasBackingStoreBasePrivate*>(m_d);
  return d->m_resizeable;
}

void
fastuidraw::GlyphAtlasBackingStoreBase::
resize(unsigned int new_size)
{
  GlyphAtlasBackingStoreBasePrivate *d;
  d = static_cast<GlyphAtlasBackingStoreBasePrivate*>(m_d);
  FASTUIDRAWassert(d->m_resizeable);
  FASTUIDRAWassert(new_size > d->m_size);
  resize_implement(new_size);
  d->m_size = new_size;
}

///////////////////////////////////////////////
// fastuidraw::GlyphAtlas methods
fastuidraw::GlyphAtlas::
GlyphAtlas(reference_counted_ptr<GlyphAtlasBackingStoreBase> pstore)
{
  m_d = FASTUIDRAWnew GlyphAtlasPrivate(pstore);
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
allocate_data(c_array<const generic_data> pdata)
{
  if (pdata.empty())
    {
      return 0;
    }

  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  int return_value;
  return_value = d->m_data_allocator.allocate_interval(pdata.size());
  if (return_value == -1)
    {
      if (d->m_store->resizeable())
        {
          d->m_store->resize(pdata.size() + 2 * d->m_store->size());
          d->m_data_allocator.resize(d->m_store->size());
          return_value = d->m_data_allocator.allocate_interval(pdata.size());
          FASTUIDRAWassert(return_value != -1);
        }
      else
        {
          return return_value;
        }
    }

  d->m_data_allocated += pdata.size();
  d->m_store->set_values(return_value, pdata);
  return return_value;
}

void
fastuidraw::GlyphAtlas::
deallocate_data(int location, int count)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  if (location < 0 || count == 0)
    {
      FASTUIDRAWassert(count == 0);
      return;
    }

  FASTUIDRAWassert(count > 0);
  if (d->m_lock_resource_counter == 0)
    {
      d->deallocate_implement(location, count);
    }
  else
    {
      DelayedDeallocate m;
      m.m_location = location;
      m.m_count = count;
      d->m_delayed_deallocates.push_back(m);
    }
}

unsigned int
fastuidraw::GlyphAtlas::
data_allocated(void)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  return d->m_data_allocated;
}

void
fastuidraw::GlyphAtlas::
clear(void)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  if (d->m_lock_resource_counter == 0)
    {
      d->clear_implement();
    }
  else
    {
      d->m_clear_issued = true;
    }
}

unsigned int
fastuidraw::GlyphAtlas::
number_times_cleared(void) const
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  return d->m_number_times_cleared;
}

void
fastuidraw::GlyphAtlas::
flush(void) const
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  d->m_store->flush();
}

const fastuidraw::reference_counted_ptr<const fastuidraw::GlyphAtlasBackingStoreBase>&
fastuidraw::GlyphAtlas::
store(void) const
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  return d->m_store_constant;
}

void
fastuidraw::GlyphAtlas::
lock_resources(void)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);
  ++d->m_lock_resource_counter;
}

void
fastuidraw::GlyphAtlas::
unlock_resources(void)
{
  GlyphAtlasPrivate *d;
  d = static_cast<GlyphAtlasPrivate*>(m_d);

  FASTUIDRAWassert(d->m_lock_resource_counter > 0);
  --d->m_lock_resource_counter;
  if (d->m_lock_resource_counter == 0)
    {
      if (d->m_clear_issued)
        {
          d->clear_implement();
        }
      else
        {
          for (const auto &m : d->m_delayed_deallocates)
            {
              d->deallocate_implement(m.m_location, m.m_count);
            }
          d->m_delayed_deallocates.clear();
        }
      FASTUIDRAWassert(d->m_delayed_deallocates.empty());
    }
}
