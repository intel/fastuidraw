/*!
 * \file reference_count_mutex.cpp
 * \brief file reference_count_mutex.cpp
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/reference_count_mutex.hpp>

#include "../private/util_private.hpp"

namespace
{
  class RefernceCounterPrivate:fastuidraw::noncopyable
  {
  public:
    RefernceCounterPrivate(void):
      m_reference_count(0)
    {}

    std::mutex m_mutex;
    int m_reference_count;
  };
}


/////////////////////////////////////////
// fastuidraw::reference_counter_mutex methods
fastuidraw::reference_count_mutex::
reference_count_mutex(void)
{
  m_d = FASTUIDRAWnew RefernceCounterPrivate();
}

fastuidraw::reference_count_mutex::
~reference_count_mutex()
{
  RefernceCounterPrivate *d;

  d = static_cast<RefernceCounterPrivate*>(m_d);
  FASTUIDRAWassert(d->m_reference_count == 0);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::reference_count_mutex::
add_reference(void)
{
  RefernceCounterPrivate *d;

  d = static_cast<RefernceCounterPrivate*>(m_d);
  d->m_mutex.lock();
  FASTUIDRAWassert(d->m_reference_count >= 0);
  ++d->m_reference_count;
  d->m_mutex.unlock();
}

bool
fastuidraw::reference_count_mutex::
remove_reference(void)
{
  bool return_value;
  RefernceCounterPrivate *d;

  d = static_cast<RefernceCounterPrivate*>(m_d);
  d->m_mutex.lock();
  --d->m_reference_count;
  FASTUIDRAWassert(d->m_reference_count >= 0);
  if (d->m_reference_count == 0)
    {
      return_value = true;
    }
  else
    {
      return_value = false;
    }
  d->m_mutex.unlock();
  return return_value;
}
