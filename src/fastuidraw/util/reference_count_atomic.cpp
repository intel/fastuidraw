/*!
 * \file reference_count_atomic.cpp
 * \brief file reference_count_atomic.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <atomic>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/reference_count_atomic.hpp>

namespace
{
  class ReferenceCountAtomicPrivate
  {
  public:
    ReferenceCountAtomicPrivate(void):
      m_reference_count(0)
    {}

    std::atomic<int> m_reference_count;
  };
}

//////////////////////////////////
// fastuidraw::reference_count_atomic methods
fastuidraw::reference_count_atomic::
reference_count_atomic(void)
{
  m_d = FASTUIDRAWnew ReferenceCountAtomicPrivate();
}

fastuidraw::reference_count_atomic::
~reference_count_atomic()
{
  ReferenceCountAtomicPrivate *d;

  d = static_cast<ReferenceCountAtomicPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::reference_count_atomic::
add_reference(void)
{
  ReferenceCountAtomicPrivate *d;

  d = static_cast<ReferenceCountAtomicPrivate*>(m_d);
  d->m_reference_count.fetch_add(1, std::memory_order_relaxed);
}


bool
fastuidraw::reference_count_atomic::
remove_reference(void)
{
  ReferenceCountAtomicPrivate *d;
  bool return_value;

  d = static_cast<ReferenceCountAtomicPrivate*>(m_d);
  if (d->m_reference_count.fetch_sub(1, std::memory_order_release) == 1)
    {
      std::atomic_thread_fence(std::memory_order_acquire);
      return_value = true;
    }
  else
    {
      return_value = false;
    }

  return return_value;
}
