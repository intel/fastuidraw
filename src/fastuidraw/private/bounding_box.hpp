/*!
 * \file bounding_box.hpp
 * \brief file bounding_box.hpp
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

#include <fastuidraw/util/vecN.hpp>

namespace fastuidraw
{
  /*!
    Simple bounding box class
   */
  template<typename T>
  class BoundingBox
  {
  public:
    typedef vecN<T, 2> pt_type;

    BoundingBox(void):
      m_empty(true)
    {}

    BoundingBox(pt_type pmin, pt_type pmax):
      m_min(pmin),
      m_max(pmax),
      m_empty(false)
    {
      assert(pmin.x() <= pmax.x());
      assert(pmin.y() <= pmax.y());
    }

    void
    inflated_polygon(vecN<pt_type, 4> &out_data, T rad) const
    {
      assert(!m_empty);
      out_data[0] = pt_type(m_min.x() - rad, m_min.y() - rad);
      out_data[1] = pt_type(m_max.x() + rad, m_min.y() - rad);
      out_data[2] = pt_type(m_max.x() + rad, m_max.y() + rad);
      out_data[3] = pt_type(m_min.x() - rad, m_max.y() + rad);
    }

    void
    union_point(const pt_type &pt)
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

    pt_type
    size(void) const
    {
      return m_empty ?
        pt_type(T(0), T(0)) :
        m_max - m_min;
    }

    bool
    empty(void) const
    {
      return m_empty;
    }

    const pt_type&
    min_point(void) const
    {
      return m_min;
    }

    const pt_type&
    max_point(void) const
    {
      return m_max;
    }
  private:
    pt_type m_min, m_max;
    bool m_empty;
  };
}
