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
  class BoundingBox
  {
  public:
    BoundingBox(void):
      m_empty(true)
    {}

    BoundingBox(vec2 pmin, vec2 pmax):
      m_min(pmin),
      m_max(pmax),
      m_empty(false)
    {
      assert(pmin.x() <= pmax.x());
      assert(pmin.y() <= pmax.y());
    }

    void
    inflated_polygon(vecN<vec2, 4> &out_data, float rad) const
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

    fastuidraw::vec2
    size(void) const
    {
      return m_empty ?
        fastuidraw::vec2(0.0f, 0.0f) :
        m_max - m_min;
    }

    bool
    empty(void) const
    {
      return m_empty;
    }

    const vec2&
    min_point(void) const
    {
      return m_min;
    }

    const vec2&
    max_point(void) const
    {
      return m_max;
    }
  private:
    vec2 m_min, m_max;
    bool m_empty;
  };
}
