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

template<typename T>
class BoundingBox
{
public:
  typedef fastuidraw::vecN<T, 2> pt_type;

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
  inflated_polygon(fastuidraw::vecN<pt_type, 4> &out_data, T rad) const
  {
    FASTUIDRAWassert(!m_empty);
    out_data[0] = pt_type(m_min.x() - rad, m_min.y() - rad);
    out_data[1] = pt_type(m_max.x() + rad, m_min.y() - rad);
    out_data[2] = pt_type(m_max.x() + rad, m_max.y() + rad);
    out_data[3] = pt_type(m_min.x() - rad, m_max.y() + rad);
  }

  BoundingBox&
  union_point(const pt_type &pt)
  {
    using namespace fastuidraw;
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
    return *this;
  }

  template<typename iterator>
  BoundingBox&
  union_points(iterator begin, iterator end)
  {
    for(; begin != end; ++begin)
      {
        union_point(*begin);
      }
    return *this;
  }

  BoundingBox&
  union_box(const BoundingBox &b)
  {
    if (!b.m_empty)
      {
        union_point(b.m_min);
        union_point(b.m_max);
      }
    return *this;
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

  bool
  intersects(const BoundingBox &v) const
  {
    using namespace fastuidraw;

    return !m_empty && !v.m_empty
      && t_max(m_min.x(), v.m_min.x()) <= t_min(m_max.x(), v.m_max.x())
      && t_max(m_min.y(), v.m_min.y()) <= t_min(m_max.y(), v.m_max.y());
  }

  bool
  intersects(const pt_type &v) const
  {
    using namespace fastuidraw;

    return !m_empty
      && v.x() >= m_min.x()
      && v.x() <= m_max.x()
      && v.y() >= m_min.y()
      && v.y() <= m_max.y();
  }

  fastuidraw::vecN<BoundingBox, 2>
  split(unsigned int coordinate) const
  {
    fastuidraw::vecN<BoundingBox, 2> R;
    if (m_empty)
      {
        return R;
      }

    pt_type splitMin, splitMax;
    T v;

    v = (m_min[coordinate] + m_max[coordinate]) / T(2);
    splitMin[coordinate] = splitMax[coordinate] = v;
    splitMin[1 - coordinate] = m_min[1 - coordinate];
    splitMax[1 - coordinate] = m_max[1 - coordinate];

    R[0].union_point(m_min);
    R[0].union_point(splitMax);

    R[1].union_point(m_max);
    R[1].union_point(splitMin);

    return R;
  }

private:
  pt_type m_min, m_max;
  bool m_empty;
};
