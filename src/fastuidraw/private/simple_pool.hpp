/*!
 * \file simple_pool.hpp
 * \brief file simple_pool.hpp
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

#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <list>

#include "util_private_ostream.hpp"
#include <iostream>
#include <typeinfo>

namespace fastuidraw { namespace detail {

/* We would like to do the simple idea that we can
 * allocate once and rather than freeing the memory
 * we just issue "nuke" which returns the memory to
 * a freestore. The issue is that we cannot resize
 * our memory pools, so we end up having a list
 * of pools. This class does NOT call the dtors
 * of objects created by create() when clear() is
 * called. The purpose of this object is to allow
 * for a simple "nuke" (embodied by clear()).
 */
template<size_t PoolSize>
class SimplePool:fastuidraw::noncopyable
{
public:
  template<typename T, typename ...Args>
  T*
  create(Args&&... args)
  {
    void *data;
    T *p;

    data = allocate(sizeof(T));
    p = new(data) T(std::forward<Args>(args)...);
    FASTUIDRAWassert(p == data);

    return p;
  }

  void
  clear(void);

  void*
  allocate(uint64_t num_bytes);

private:
  class SinglePool
  {
  public:
    void*
    allocate(uint64_t num_chunks)
    {
      uint64_t return_value(m_chunks_allocated);

      FASTUIDRAWassert(m_chunks_allocated + num_chunks <= m_pool.size());
      m_chunks_allocated += num_chunks;
      return &m_pool[return_value];
    }

    void
    clear(void)
    {
      m_chunks_allocated = 0;
    }

    uint64_t
    num_free_chunks(void) const
    {
      return m_pool.size() - m_chunks_allocated;
    }

  private:
    vecN<uint64_t, PoolSize> m_pool;
    unsigned int m_chunks_allocated;
  };

  std::list<SinglePool> m_usable, m_full;
};

/////////////////////////////////
// fastuidraw::detail::SimplePool methods
template<size_t PoolSize>
void
fastuidraw::detail::SimplePool<PoolSize>::
clear(void)
{
  m_usable.splice(m_usable.begin(), m_full);
  for (auto &pool : m_usable)
    {
      pool.clear();
    }
}

template<size_t PoolSize>
void*
fastuidraw::detail::SimplePool<PoolSize>::
allocate(uint64_t num_bytes)
{
  void *return_value(nullptr);
  unsigned int num_chunks;

  /* make sure num_bytes is a multiple of 8 */
  num_bytes = (num_bytes + 7u) & ~7u;
  num_chunks = (num_bytes >> 3u);

  FASTUIDRAWassert(num_chunks * sizeof(uint64_t) >= num_bytes);
  for (auto iter = m_usable.begin(), end = m_usable.end(); iter != end; ++iter)
    {
      if (iter->num_free_chunks() >= num_chunks)
        {
          return_value = iter->allocate(num_chunks);
          if (iter->num_free_chunks() == 0)
            {
              m_full.splice(m_full.end(), m_usable, iter);
            }
          return return_value;
        }
    }

  m_usable.push_back(SinglePool());
  return m_usable.back().allocate(num_chunks);
}

}}
