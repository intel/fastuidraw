/*!
 * \file reference_count_atomic.cpp
 * \brief file reference_count_atomic.cpp
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

#include <boost/version.hpp>

#if (BOOST_VERSION < 105300)
  #define FASTUIDRAW_USE_DETAIL_ATOMIC
#endif

#ifndef FASTUIDRAW_USE_DETAIL_ATOMIC
  #include <boost/atomic.hpp>
  typedef boost::atomic<int> atomic_int;
#else
  #include <boost/detail/atomic_count.hpp>
  typedef boost::detail::atomic_count atomic_int;
#endif

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

    atomic_int m_reference_count;
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

  d = reinterpret_cast<ReferenceCountAtomicPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::reference_count_atomic::
add_reference(void)
{
  ReferenceCountAtomicPrivate *d;

  d = reinterpret_cast<ReferenceCountAtomicPrivate*>(m_d);
  #ifndef FASTUIDRAW_USE_DETAIL_ATOMIC
    {
      d->m_reference_count.fetch_add(1, boost::memory_order_relaxed);
    }
  #else
    {
      ++(d->m_reference_count);
    }
  #endif
}


bool
fastuidraw::reference_count_atomic::
remove_reference(void)
{
  ReferenceCountAtomicPrivate *d;
  bool return_value;

  d = reinterpret_cast<ReferenceCountAtomicPrivate*>(m_d);
  #ifndef FASTUIDRAW_USE_DETAIL_ATOMIC
    {
      if (d->m_reference_count.fetch_sub(1, boost::memory_order_release) == 1)
        {
          boost::atomic_thread_fence(boost::memory_order_acquire);
          return_value = true;
        }
      else
        {
          return_value = false;
        }
    }
  #else
    {
      long v;
      v = --(d->m_reference_count);
      return_value = (v == 0);
    }
  #endif

  return return_value;
}
