/*!
 * \file int_path.hpp
 * \brief file int_path.hpp
 *
 * Copyright 2017 by Intel.
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
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/path.hpp>

#include "array2d.hpp"
#include "bounding_box.hpp"
#include "util_private.hpp"

namespace fastuidraw
{
  namespace detail
  {
    class IntBezierCurve:public reference_counted<IntBezierCurve>::non_concurrent
    {
    public:
      enum coordinate_type
        {
          x_fixed = 0,
          y_fixed = 1,
          x_varying = y_fixed,
          y_varying = x_fixed
        };

      enum solution_type
        {
          within_0_1    = 1,
          on_0_boundary = 2,
          on_1_boundary = 4,
          outside_0_1   = 8,
        };

      template<typename T>
      class transformation
      {
      public:
        transformation(T sc = T(1), vecN<T, 2> tr = vecN<T, 2>(T(0))):
          m_scale(sc),
          m_translate(tr)
        {}

        T
        scale(void) const
        {
          return m_scale;
        }

        vecN<T, 2>
        translate(void) const
        {
          return m_translate;
        }

        template<typename U>
        transformation<U>
        cast(void) const
        {
          return transformation<U>(U(m_scale), vecN<U, 2>(m_translate));
        }

        vecN<T, 2>
        operator()(vecN<T, 2> p) const
        {
          return m_translate + m_scale * p;
        }

      private:
        T m_scale;
        vecN<T, 2> m_translate;
      };

      class evaluated_pt
      {
      public:
        float m_t;
        vec2 m_p;
        vec2 m_p_t;
      };

      class solution_pt:public evaluated_pt
      {
      public:
        int m_multiplicity;
        enum solution_type m_type;
        reference_counted_ptr<const IntBezierCurve> m_src;
      };

      /* returns what coordinate is fixed for a
         given coordinate_type
       */
      static
      int
      fixed_coordinate(enum coordinate_type tp)
      {
        return tp;
      }

      /* returns what coordinate is varying for a
         given coordinate_type
       */
      static
      int
      varying_coordinate(enum coordinate_type tp)
      {
        return 1 - fixed_coordinate(tp);
      }

      IntBezierCurve(const ivec2 &pt0, const ivec2 &pt1)
      {
        m_control_pts.push_back(pt0);
        m_control_pts.push_back(pt1);
        process_control_pts();
      }

      IntBezierCurve(const ivec2 &pt0, const ivec2 &ct, const ivec2 &pt1)
      {
        m_control_pts.push_back(pt0);
        m_control_pts.push_back(ct);
        m_control_pts.push_back(pt1);
        process_control_pts();
      }

      IntBezierCurve(const ivec2 &pt0, const ivec2 &ct0, const ivec2 &ct1, const ivec2 &pt1)
      {
        m_control_pts.push_back(pt0);
        m_control_pts.push_back(ct0);
        m_control_pts.push_back(ct1);
        m_control_pts.push_back(pt1);
        process_control_pts();
      }

      ~IntBezierCurve()
      {}

      enum return_code
      approximate_cubic_with_quadratics(vecN<reference_counted_ptr<IntBezierCurve>, 4> &out_curves) const;

      enum return_code
      approximate_cubic_with_quadratics(vecN<reference_counted_ptr<IntBezierCurve>, 2> &out_curves) const;

      enum return_code
      approximate_cubic_with_quadratics(vecN<reference_counted_ptr<IntBezierCurve>, 1> &out_curves) const;

      const_c_array<ivec2>
      control_pts(void) const
      {
        return make_c_array(m_control_pts);
      }

      const BoundingBox<int>&
      bounding_box(void) const
      {
        return m_bb;
      }

      BoundingBox<int>
      bounding_box(const transformation<int> &tr) const
      {
        BoundingBox<int> R;
        R.union_point(tr(m_bb.min_point()));
        R.union_point(tr(m_bb.max_point()));
        return R;
      }

      static
      bool
      are_ordered_neighbors(const IntBezierCurve &curve0,
                            const IntBezierCurve &curve1)
      {
        return curve0.m_control_pts.back() == curve1.m_control_pts.front();
      }

      int
      degree(void) const
      {
        FASTUIDRAWassert(!m_control_pts.empty());
        return m_control_pts.size() - 1;
      }

      const std::vector<solution_pt>&
      derivatives_cancel(void) const
      {
        return m_derivatives_cancel;
      }

      /*
        \param pt fixed value of line
        \param line_type if line if vertical (x-fixed) or horizontal (y-fixed)
        \param out_value location to which to put values
        \param solution_types_accepted bit mask using enums of solution_type
                                       of what solutions are accepted
        \param tr transformation to apply to the IntPath
      */
      void
      compute_line_intersection(int pt, enum coordinate_type line_type,
                                std::vector<solution_pt> *out_value,
                                uint32_t solution_types_accepted,
                                const transformation<int> &tr) const;

    private:
      void
      process_control_pts(void);

      void
      compute_derivatives_cancel_pts(void);

      std::vector<ivec2> m_control_pts;
      vecN<std::vector<int>, 2> m_as_polynomial;

      // where dx/dt +- dy/dt = 0
      std::vector<solution_pt> m_derivatives_cancel;

      BoundingBox<int> m_bb;
    };

    class IntContour
    {
    public:
      IntContour(void)
      {}

      void
      add_curve(const reference_counted_ptr<const IntBezierCurve> &curve)
      {
        FASTUIDRAWassert(curve);
        FASTUIDRAWassert(m_curves.empty() || IntBezierCurve::are_ordered_neighbors(*m_curves.back(), *curve));
        m_curves.push_back(curve);
      }

      bool
      closed(void)
      {
        return !m_curves.empty()
          && IntBezierCurve::are_ordered_neighbors(*m_curves.back(), *m_curves.front());
      }

      const std::vector<reference_counted_ptr<const IntBezierCurve> >&
      curves(void) const
      {
        return m_curves;
      }

    private:
      std::vector<reference_counted_ptr<const IntBezierCurve> > m_curves;
    };

    class IntPath
    {
    public:

      class distance_value
      {
      public:
        enum winding_ray_t
          {
            from_pt_to_x_negative_infinity,
            from_pt_to_x_positive_infinity,
            from_pt_to_y_negative_infinity,
            from_pt_to_y_positive_infinity,
          };

        distance_value(void):
          m_distance(-1.0f),
          m_ray_intersection_counts(0, 0, 0, 0),
          m_winding_numbers(0, 0)
        {}

        void
        record_distance_value(float v)
        {
          FASTUIDRAWassert(v >= 0.0f);
          m_distance = (m_distance >= 0.0f) ?
            t_min(v, m_distance) :
            v;
        }

        void
        increment_ray_intersection_count(enum winding_ray_t tp, int mult)
        {
          FASTUIDRAWassert(mult >= 0);
          m_ray_intersection_counts[tp] += mult;
        }

        void
        set_winding_number(enum IntBezierCurve::coordinate_type tp, int w)
        {
          m_winding_numbers[tp] = w;
        }

        float
        distance(float max_distance) const
        {
          return (m_distance < 0.0f) ?
            max_distance :
            t_min(max_distance, m_distance);
        }

        float
        raw_distance(void) const
        {
          return m_distance;
        }

        int
        ray_intersection_count(enum winding_ray_t tp) const
        {
          return m_ray_intersection_counts[tp];
        }

        int
        winding_number(enum IntBezierCurve::coordinate_type tp = IntBezierCurve::x_fixed) const
        {
          return m_winding_numbers[tp];
        }

      private:
        /* unsigned distance in IntPath coordinates,
           a negative value indicates value is not
           assigned
        */
        float m_distance;

        /* number of intersection (counted with multiplicity)
           of a ray agains the path.
         */
        vecN<int, 4> m_ray_intersection_counts;

        /* winding number computed from horizontal or vertical lines
         */
        vecN<int, 2> m_winding_numbers;
      };

      IntPath(void)
      {}

      void
      move_to(const ivec2 &pt);

      void
      line_to(const ivec2 &pt);

      void
      conic_to(const ivec2 &control_pt, const ivec2 &pt);

      void
      cubic_to(const ivec2 &control_pt0, const ivec2 &control_pt1, const ivec2 &pt);

      const BoundingBox<int>&
      bounding_box(void) const
      {
        return m_bb;
      }

      bool
      empty(void) const
      {
        return m_contours.empty();
      }

      const std::vector<IntContour>&
      contours(void) const
      {
        return m_contours;
      }

      void
      add_to_path(const vec2 &offset, const vec2 &scale, Path *dst) const;

      /*
        Compute distance_value for the domain
             D = { (x(i), y(j)) : 0 <= i < count.x(), 0 <= j < count.y() }
        where
             x(i) = start.x() + step.x() * i
             y(j) = start.y() + step.y() * j
      */
      void
      compute_distance_values(const ivec2 &start, const ivec2 &step, const ivec2 &count,
                              const IntBezierCurve::transformation<int> &tr,
                              int radius, array2d<distance_value> &out_values) const;

    private:
      void
      compute_outline_point_values(const ivec2 &start, const ivec2 &step, const ivec2 &count,
                                   const IntBezierCurve::transformation<int> &tr,
                                   int radius, array2d<distance_value> &dst) const;
      void
      compute_derivative_cancel_values(const ivec2 &start, const ivec2 &step, const ivec2 &count,
                                       const IntBezierCurve::transformation<int> &tr,
                                       int radius, array2d<distance_value> &dst) const;
      void
      compute_fixed_line_values(const ivec2 &start, const ivec2 &step, const ivec2 &count,
                                const IntBezierCurve::transformation<int> &tr,
                                array2d<distance_value> &dst) const;
      void
      compute_fixed_line_values(enum IntBezierCurve::coordinate_type tp,
                                std::vector<std::vector<IntBezierCurve::solution_pt> > &work_room,
                                const ivec2 &start, const ivec2 &step, const ivec2 &count,
                                const IntBezierCurve::transformation<int> &tr,
                                array2d<distance_value> &dst) const;
      void
      compute_winding_values(enum IntBezierCurve::coordinate_type tp, int c,
                             const std::vector<IntBezierCurve::solution_pt> &L,
                             int start, int step, int count,
                             array2d<distance_value> &dst) const;

      fastuidraw::ivec2 m_last_pt;
      std::vector<IntContour> m_contours;
      BoundingBox<int> m_bb;
    };

  } //namespace fastuidraw
} //namespace detail
