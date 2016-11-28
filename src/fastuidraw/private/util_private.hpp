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

#include <boost/thread.hpp>

#include <vector>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>

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

  /*!
    Simple bounding box class
   */
  class BoundingBox
  {
  public:
    BoundingBox(void):
      m_empty(true)
    {}

    void
    inflated_polygon(vecN<vec2, 4> &out_data, float rad)
    {
      assert(!m_empty);
      out_data[0] = vec2(m_min.x() - rad, m_min.y() - rad);
      out_data[1] = vec2(m_max.x() + rad, m_min.y() - rad);
      out_data[2] = vec2(m_max.x() + rad, m_max.y() + rad);
      out_data[3] = vec2(m_min.x() - rad, m_max.y() + rad);
    }

    void
    union_point(const vec2 &pt)
    {
      if(m_empty)
        {
          m_empty = false;
          m_min = m_max = pt;
        }
      else
        {
          m_min.x() = t_min(m_min.x(), pt.x());
          m_min.y() = t_min(m_min.y(), pt.y());

          m_max.x() = t_max(m_max.x(), pt.x());
          m_max.y() = t_max(m_max.y(), pt.y());
        }
    }

    void
    union_box(const BoundingBox &b)
    {
      if(!b.m_empty)
        {
          union_point(b.m_min);
          union_point(b.m_max);
        }
    }

    vec2 m_min, m_max;
    bool m_empty;
  };

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
