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

      explicit
      IntBezierCurve(const_c_array<ivec2> pcontrol_pts):
        m_control_pts(pcontrol_pts.begin(), pcontrol_pts.end())
      {
        process_control_pts();
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

      /*
        \param pt fixed value of line
        \param line_type if line if vertical (x-fixed) or horizontal (y-fixed)
        \param out_value location to which to put values
        \param solution_types_accepted bit mask using enums of solution_type
                                       of what solutions are accepted
      */
      void
      compute_line_intersection(int pt, enum coordinate_type line_type,
                                std::vector<solution_pt> *out_value,
                                uint32_t solution_types_accepted) const;

    private:
      void
      process_control_pts(void);

      void
      compute_derivatives_cancel_pts(void);

      void
      compute_derivative_zero_pts(void);

      std::vector<ivec2> m_control_pts;
      vecN<std::vector<int>, 2> m_as_polynomial;

      // [0] --> dx/dt is 0
      // [1] --> dy/dt is 0
      vecN<std::vector<solution_pt>, 2> m_derivative_zero;

      // where dx/dt +- dy/dt = 0
      std::vector<solution_pt> m_derivatives_cancel;

      BoundingBox<int> m_bb;
    };

    class IntContour
    {
    public:
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

      const_c_array<reference_counted_ptr<const IntBezierCurve> >
      curves(void) const
      {
        return fastuidraw::make_c_array(m_curves);
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
        enum
          {
            from_pt_to_x_negative_infinity,
            from_pt_to_x_positive_infinity,
            from_pt_to_y_negative_infinity,
            from_pt_to_y_positive_infinity,
          };

        distance_value(void):
          m_distance(-1.0f),
          m_winding_numbers(0, 0, 0, 0)
        {}

        /* unsigned distance in IntPath coordinates,
           a negative value indicates value is not
           assigned
        */
        float m_distance;

        // winding numbers computed from rays
        vecN<int, 4> m_winding_numbers;
      };

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

      /*
        Compute distance_value for the domain
             D = { (x(i), y(j)) : 0 <= i < count.x(), 0 <= j < count.y() }
        where
             x(i) = bounding_box().min_point().x() + step.x() * i
             y(j) = bounding_box().min_point().y() + step.y() * j
      */
      void
      compute_distance_values(const ivec2 &step, const ivec2 &count,
                              array2d<distance_value> &out_values);

    private:
      fastuidraw::ivec2 m_last_pt;
      std::vector<IntContour> m_contours;
      BoundingBox<int> m_bb;
    };

  } //namespace fastuidraw
} //namespace detail
