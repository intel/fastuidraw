/*!
 * \file simple_pool.hpp
 * \brief file simple_pool.hpp
 *
 * Copyright 2019 by Intel.
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

#ifndef FASTUIDRAW_SIMPLE_POOL_HPP
#define FASTUIDRAW_SIMPLE_POOL_HPP

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <vector>

#include <private/util_private_ostream.hpp>
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
template<typename T, size_t PoolSize>
class SimplePool:fastuidraw::noncopyable
{
public:
  ~SimplePool()
  {
    for (SinglePool *p : m_all)
      {
        FASTUIDRAWdelete(p);
      }
  }

  template<typename ...Args>
  T*
  create(Args&&... args)
  {
    void *data;
    T *p;

    data = allocate();
    p = new(data) T(std::forward<Args>(args)...);
    FASTUIDRAWassert(p == data);

    return p;
  }

  void
  clear(void)
  {
    m_usable.clear();
    for (SinglePool *pool : m_all)
      {
        pool->clear();
        m_usable.push_back(pool);
      }
  }

private:
  typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type element;

  class SinglePool
  {
  public:
    SinglePool(void):
      m_allocated(0)
    {}

    void*
    allocate(void)
    {
      unsigned int ct(m_allocated);

      FASTUIDRAWassert(m_allocated < m_pool.size());
      ++m_allocated;

      return &m_pool[ct];
    }

    bool
    full(void) const
    {
      return m_allocated == m_pool.size();
    }

    void
    clear(void)
    {
      m_allocated = 0;
    }

  private:
    vecN<element, PoolSize> m_pool;
    unsigned int m_allocated;
  };

  void*
  allocate(void)
  {
    void *return_value;

    if (m_usable.empty())
      {
        m_usable.push_back(FASTUIDRAWnew SinglePool());
        m_all.push_back(m_usable.back());
      }

    return_value = m_usable.back()->allocate();
    if (m_usable.back()->full())
      {
        m_usable.pop_back();
      }

    return return_value;
  }

  std::vector<SinglePool*> m_usable, m_all;
};

}}

#endif
