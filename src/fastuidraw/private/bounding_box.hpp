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
   * Simple bounding box class
   */
  template<typename T>
  class BoundingBox
  {
  public:
    typedef vecN<T, 2> pt_type;

    BoundingBox(void):
      m_min(T(0), T(0)),
      m_max(T(0), T(0)),
      m_empty(true)
    {}

    BoundingBox(pt_type pmin, pt_type pmax):
      m_min(pmin),
      m_max(pmax),
      m_empty(false)
    {
      FASTUIDRAWassert(pmin.x() <= pmax.x());
      FASTUIDRAWassert(pmin.y() <= pmax.y());
    }

    void
    inflated_polygon(vecN<pt_type, 4> &out_data, T rad) const
    {
      FASTUIDRAWassert(!m_empty);
      out_data[0] = pt_type(m_min.x() - rad, m_min.y() - rad);
      out_data[1] = pt_type(m_max.x() + rad, m_min.y() - rad);
      out_data[2] = pt_type(m_max.x() + rad, m_max.y() + rad);
      out_data[3] = pt_type(m_min.x() - rad, m_max.y() + rad);
    }

    void
    translate(const pt_type &tr)
    {
      if (!m_empty)
        {
          m_min += tr;
          m_max += tr;
        }
    }

    void
    scale_down(const pt_type &tr)
    {
      if (!m_empty)
        {
          m_min /= tr;
          m_max /= tr;
        }
    }

    void
    scale_up(const pt_type &tr)
    {
      if (!m_empty)
        {
          m_min *= tr;
          m_max *= tr;
        }
    }

    bool //returns true if box became larger
    union_point(const pt_type &pt)
    {
      bool R;

      R = !contains(pt);
      if (m_empty)
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
      return R;
    }

    template<typename iterator>
    bool
    union_points(iterator begin, iterator end)
    {
      bool R(false);
      for(; begin != end; ++begin)
        {
          R = union_point(*begin) || R;
        }
      return R;
    }

    bool
    union_box(const BoundingBox &b)
    {
      if (!b.m_empty)
        {
	  bool r0, r1;
          r0 = union_point(b.m_min);
          r1 = union_point(b.m_max);
	  return r0 || r1;
        }
      return false;
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

    pt_type
    corner_point(bool max_x, bool max_y) const
    {
      pt_type R;

      R.x() = (max_x) ? m_max.x() : m_min.x();
      R.y() = (max_y) ? m_max.y() : m_min.y();
      return R;
    }

    vecN<BoundingBox<T>, 2>
    split_x(void) const
    {
      vecN<BoundingBox<T>, 2> R;

      if (empty())
        {
          return R;
        }

      pt_type center;

      center = (m_min + m_max) / T(2);
      R[0] = BoundingBox(m_min, pt_type(center.x(), m_max.y()));
      R[1] = BoundingBox(pt_type(center.x(), m_min.y()), m_max);
      return R;
    }

    vecN<BoundingBox<T>, 2>
    split_y(void) const
    {
      vecN<BoundingBox<T>, 2> R;

      if (empty())
        {
          return R;
        }

      pt_type center;

      center = (m_min + m_max) / T(2);
      R[0] = BoundingBox(m_min, pt_type(m_max.x(), center.y()));
      R[1] = BoundingBox(pt_type(m_min.x(), center.y()), m_max);
      return R;
    }

    bool
    intersects(const BoundingBox &obj) const
    {
      return !m_empty &&
        !(obj.m_min.x() > m_max.x()
          || m_min.x() > obj.m_max.x()
          || obj.m_min.y() > m_max.y()
          || m_min.y() > obj.m_max.y());
    }

    bool
    contains(const pt_type &p) const
    {
      return !m_empty
        && p.x() >= m_min.x()
        && p.x() <= m_max.x()
        && p.y() >= m_min.y()
        && p.y() <= m_max.y();
    }

  private:
    pt_type m_min, m_max;
    bool m_empty;
  };
}
