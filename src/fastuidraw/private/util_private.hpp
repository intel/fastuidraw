/*!
 * \file util_private.hpp
 * \brief file util_private.hpp
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


#pragma once

#include <sys/time.h>
#include <boost/thread.hpp>

#include <vector>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
  /*!
    Wrapper over mutex type so that we can replace
    mutex implementation easily.
   */
  class mutex:fastuidraw::noncopyable
  {
  public:
    void
    lock(void)
    {
      m_mutex.lock();
    }

    void
    unlock(void)
    {
      m_mutex.unlock();
    }

  private:
    boost::mutex m_mutex;
  };

  /*!
    Locks mutex on ctor and unlocks un dtor.
   */
  class autolock_mutex:fastuidraw::noncopyable
  {
  public:
    explicit
    autolock_mutex(mutex &m):
      m_mutex(m)
    {
      m_mutex.lock();
    }

    ~autolock_mutex()
    {
      m_mutex.unlock();
    }
  private:
    mutex &m_mutex;
  };

  inline
  int32_t
  time_difference_ms(const struct timeval &end, const struct timeval &begin)
  {
    return (end.tv_sec - begin.tv_sec) * 1000 +
      (end.tv_usec - begin.tv_usec) / 1000;
  }

  inline
  int64_t
  time_difference_us(const struct timeval &end, const struct timeval &begin)
  {
    int64_t delta_usec, delta_sec;

    delta_usec = int64_t(end.tv_usec) - int64_t(begin.tv_usec);
    delta_sec = int64_t(end.tv_sec) - int64_t(begin.tv_sec);
    return delta_sec * int64_t(1000) * int64_t(1000) + delta_usec;
  }

  template<typename T>
  c_array<T>
  make_c_array(std::vector<T> &p)
  {
    if(p.empty())
      {
        return c_array<T>();
      }
    else
      {
        return c_array<T>(&p[0], p.size());
      }
  }

  template<typename T>
  const_c_array<T>
  make_c_array(const std::vector<T> &p)
  {
    if(p.empty())
      {
        return const_c_array<T>();
      }
    else
      {
        return const_c_array<T>(&p[0], p.size());
      }
  }

  template<typename T>
  c_array<T>
  const_cast_c_array(const_c_array<T> p)
  {
    T *q;
    q = const_cast<T*>(p.c_ptr());
    return c_array<T>(q, p.size());
  }
}
