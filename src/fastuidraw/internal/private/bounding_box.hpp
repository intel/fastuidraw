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
#include <fastuidraw/util/rect.hpp>

namespace fastuidraw
{
  /*!
   * Simple bounding box class
   */
  template<typename T>
  class BoundingBox:private RectT<T>
  {
  public:
    typedef vecN<T, 2> pt_type;

    BoundingBox(void):
      m_empty(true)
    {
      this->m_min_point = this->m_max_point = pt_type(T(0), T(0));
    }

    BoundingBox(pt_type pmin, pt_type pmax):
      m_empty(false)
    {
      FASTUIDRAWassert(pmin.x() <= pmax.x());
      FASTUIDRAWassert(pmin.y() <= pmax.y());
      this->m_min_point = pmin;
      this->m_max_point = pmax;
    }

    template<typename S>
    explicit
    BoundingBox(const RectT<S> &rect):
      RectT<T>(rect),
      m_empty(false)
    {
      FASTUIDRAWassert(this->m_min_point.x() <= this->m_max_point.x());
      FASTUIDRAWassert(this->m_min_point.y() <= this->m_max_point.y());
    }

    void
    inflated_polygon(vecN<pt_type, 4> &out_data, T rad) const
    {
      FASTUIDRAWassert(!m_empty);
      out_data[0] = pt_type(this->m_min_point.x() - rad, this->m_min_point.y() - rad);
      out_data[1] = pt_type(this->m_max_point.x() + rad, this->m_min_point.y() - rad);
      out_data[2] = pt_type(this->m_max_point.x() + rad, this->m_max_point.y() + rad);
      out_data[3] = pt_type(this->m_min_point.x() - rad, this->m_max_point.y() + rad);
    }

    void
    clear(void)
    {
      m_empty = true;
    }

    void
    enlarge(pt_type delta)
    {
      if (!m_empty)
        {
          delta.x() = t_max(T(0), delta.x());
          delta.y() = t_max(T(0), delta.y());
          this->m_min_point -= delta;
          this->m_max_point += delta;
        }
    }

    void
    enlarge(pt_type delta, BoundingBox *dst) const
    {
      dst->m_min_point = this->m_min_point;
      dst->m_max_point = this->m_max_point;
      dst->m_empty = m_empty;
      dst->enlarge(delta);
    }

    void
    translate(const pt_type &tr)
    {
      if (!m_empty)
        {
          this->m_min_point += tr;
          this->m_max_point += tr;
        }
    }

    void
    scale_down(const pt_type &tr)
    {
      if (!m_empty)
        {
          this->m_min_point /= tr;
          this->m_max_point /= tr;
        }
    }

    void
    scale_up(const pt_type &tr)
    {
      if (!m_empty)
        {
          this->m_min_point *= tr;
          this->m_max_point *= tr;
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
          this->m_min_point = this->m_max_point = pt;
        }
      else
        {
          this->m_min_point.x() = t_min(this->m_min_point.x(), pt.x());
          this->m_min_point.y() = t_min(this->m_min_point.y(), pt.y());

          this->m_max_point.x() = t_max(this->m_max_point.x(), pt.x());
          this->m_max_point.y() = t_max(this->m_max_point.y(), pt.y());
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
          r0 = union_point(b.m_min_point);
          r1 = union_point(b.m_max_point);
          return r0 || r1;
        }
      return false;
    }

    bool
    union_box(const RectT<T> &b)
    {
      bool r0, r1;
      r0 = union_point(b.m_min_point);
      r1 = union_point(b.m_max_point);
      return r0 || r1;
    }

    pt_type
    size(void) const
    {
      return m_empty ?
        pt_type(T(0), T(0)) :
        this->m_max_point - this->m_min_point;
    }

    bool
    empty(void) const
    {
      return m_empty;
    }

    const pt_type&
    min_point(void) const
    {
      return this->m_min_point;
    }

    const pt_type&
    max_point(void) const
    {
      return this->m_max_point;
    }

    pt_type
    center_point(void) const
    {
      return (this->m_min_point + this->m_max_point) / T(2);
    }

    pt_type
    corner_point(bool max_x, bool max_y) const
    {
      pt_type R;

      R.x() = (max_x) ? this->m_max_point.x() : this->m_min_point.x();
      R.y() = (max_y) ? this->m_max_point.y() : this->m_min_point.y();
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

      center = center_point();
      R[0] = BoundingBox(this->m_min_point, pt_type(center.x(), this->m_max_point.y()));
      R[1] = BoundingBox(pt_type(center.x(), this->m_min_point.y()), this->m_max_point);
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

      center = center_point();
      R[0] = BoundingBox(this->m_min_point, pt_type(this->m_max_point.x(), center.y()));
      R[1] = BoundingBox(pt_type(this->m_min_point.x(), center.y()), this->m_max_point);
      return R;
    }

    bool
    intersects(const BoundingBox &obj) const
    {
      return !m_empty &&
        !(obj.m_min_point.x() > this->m_max_point.x()
          || this->m_min_point.x() > obj.m_max_point.x()
          || obj.m_min_point.y() > this->m_max_point.y()
          || this->m_min_point.y() > obj.m_max_point.y());
    }

    void
    intersect_against(const BoundingBox &obj)
    {
      m_empty = !intersects(obj);
      if (!m_empty)
        {
          this->m_min_point.x() = t_max(obj.m_min_point.x(), this->m_min_point.x());
          this->m_min_point.y() = t_max(obj.m_min_point.y(), this->m_min_point.y());
          this->m_max_point.x() = t_min(obj.m_max_point.x(), this->m_max_point.x());
          this->m_max_point.y() = t_min(obj.m_max_point.y(), this->m_max_point.y());
        }
    }

    bool
    contains(const pt_type &p) const
    {
      return !m_empty
        && p.x() >= this->m_min_point.x()
        && p.x() <= this->m_max_point.x()
        && p.y() >= this->m_min_point.y()
        && p.y() <= this->m_max_point.y();
    }

    const RectT<T>&
    as_rect(void) const
    {
      return *this;
    }

  private:
    bool m_empty;
  };
}
