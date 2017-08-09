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
#include <fastuidraw/text/glyph_render_data_curve_pair.hpp>
#include <fastuidraw/text/glyph_render_data_distance_field.hpp>

#include "array2d.hpp"
#include "bounding_box.hpp"
#include "util_private.hpp"

namespace fastuidraw
{
  namespace detail
  {
    class IntBezierCurve
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

      class ID_t
      {
      public:
        ID_t(void):
          m_contourID(-1),
          m_curveID(-1)
        {}

        unsigned int m_contourID;
        unsigned int m_curveID;
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
        ID_t m_src;
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

      IntBezierCurve(const ID_t &pID, const IntBezierCurve &curve):
        m_ID(pID),
        m_control_pts(curve.m_control_pts),
        m_num_control_pts(curve.m_num_control_pts),
        m_as_polynomial_fcn(curve.m_as_polynomial_fcn),
        m_derivatives_cancel(curve.m_derivatives_cancel),
        m_bb(curve.m_bb)
      {
      }

      IntBezierCurve(const ID_t &pID, const ivec2 &pt0, const ivec2 &pt1):
        m_ID(pID),
        m_control_pts(pt0, pt1, ivec2(), ivec2()),
        m_num_control_pts(2)
      {
        process_control_pts();
      }

      IntBezierCurve(const ID_t &pID, const ivec2 &pt0, const ivec2 &ct,
                     const ivec2 &pt1):
        m_ID(pID),
        m_control_pts(pt0, ct, pt1, ivec2()),
        m_num_control_pts(3)
      {
        process_control_pts();
      }

      IntBezierCurve(const ID_t &pID, const ivec2 &pt0, const ivec2 &ct0,
                     const ivec2 &ct1, const ivec2 &pt1):
        m_ID(pID),
        m_control_pts(pt0, ct0, ct1, pt1),
        m_num_control_pts(4)
      {
        process_control_pts();
      }

      ~IntBezierCurve()
      {}

      const ID_t&
      ID(void) const
      {
        return m_ID;
      }

      const_c_array<ivec2>
      control_pts(void) const
      {
        return const_c_array<ivec2>(m_control_pts.c_ptr(), m_num_control_pts);
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
        return curve0.control_pts().back() == curve1.control_pts().front();
      }

      int
      degree(void) const
      {
        FASTUIDRAWassert(m_num_control_pts > 0);
        return m_num_control_pts - 1;
      }

      const_c_array<solution_pt>
      derivatives_cancel(void) const
      {
        return const_c_array<solution_pt>(m_derivatives_cancel.c_ptr(),
                                          m_num_derivatives_cancel);
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

      c_array<int>
      as_polynomial(int coord)
      {
        return c_array<int>(m_as_polynomial_fcn[coord].c_ptr(), m_num_control_pts);
      }

      const_c_array<int>
      as_polynomial(int coord) const
      {
        return const_c_array<int>(m_as_polynomial_fcn[coord].c_ptr(), m_num_control_pts);
      }

      vecN<const_c_array<int>, 2>
      as_polynomial(void) const
      {
        return vecN<const_c_array<int>, 2>(as_polynomial(0), as_polynomial(1));
      }

      ID_t m_ID;
      vecN<ivec2, 4> m_control_pts;
      int m_num_control_pts;
      vecN<ivec4, 2> m_as_polynomial_fcn;

      // where dx/dt +- dy/dt = 0
      vecN<solution_pt, 6> m_derivatives_cancel;
      int m_num_derivatives_cancel;
      BoundingBox<int> m_bb;
    };

    class IntContour
    {
    public:
      IntContour(void)
      {}

      void
      add_curve(const IntBezierCurve &curve)
      {
        FASTUIDRAWassert(m_curves.empty() || IntBezierCurve::are_ordered_neighbors(m_curves.back(), curve));
        m_curves.push_back(curve);
      }

      bool
      closed(void)
      {
        return !m_curves.empty()
          && IntBezierCurve::are_ordered_neighbors(m_curves.back(), m_curves.front());
      }

      const std::vector<IntBezierCurve>&
      curves(void) const
      {
        return m_curves;
      }

      const IntBezierCurve&
      curve(unsigned int curveID) const
      {
        FASTUIDRAWassert(curveID < m_curves.size());
        return m_curves[curveID];
      }

      /* Filter the Contour as follows:
          1. Collapse any curves that are within a texel
          2. Curves of tiny curvature are realized as a line
          3. Cubics are broken into quadratics
         The transformation tr is -NOT- applied to the contour,
         it is used as the transformation from IntContour
         coordinates to texel coordinates. The value of texel_size
         gives the size of a texel with the texel at (0, 0)
         starting at (0, 0) [in texel coordinates].
       */
      void
      filter(float curvature_collapse,
             const IntBezierCurve::transformation<int> &tr, ivec2 texel_size);

      void
      add_to_path(const IntBezierCurve::transformation<float> &tr, Path *dst) const;

    private:
      std::vector<IntBezierCurve> m_curves;
    };

    class IntPath
    {
    public:
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

      const IntContour&
      contour(unsigned int contourID) const
      {
        FASTUIDRAWassert(contourID < m_contours.size());
        return m_contours[contourID];
      }

      const IntBezierCurve&
      curve(IntBezierCurve::ID_t id) const
      {
        return contour(id.m_contourID).curve(id.m_curveID);
      }

      IntBezierCurve::ID_t
      prev_neighbor(IntBezierCurve::ID_t id) const
      {
        FASTUIDRAWassert(id.m_contourID < m_contours.size());
        FASTUIDRAWassert(id.m_curveID < m_contours[id.m_contourID].curves().size());
        id.m_curveID = (id.m_curveID == 0) ?
          m_contours[id.m_contourID].curves().size() - 1 :
          id.m_curveID - 1;
        return id;
      }

      IntBezierCurve::ID_t
      next_neighbor(IntBezierCurve::ID_t id) const
      {
        FASTUIDRAWassert(id.m_contourID < m_contours.size());
        FASTUIDRAWassert(id.m_curveID < m_contours[id.m_contourID].curves().size());
        id.m_curveID = (id.m_curveID == m_contours[id.m_contourID].curves().size() - 1) ?
          0:
          id.m_curveID + 1;
        return id;
      }

      void
      add_to_path(const IntBezierCurve::transformation<float> &tr, Path *dst) const;

      /* Filter the Path as follows:
          1. Collapse any curves that are within a texel
          2. Curves of tiny curvature are realized as a line
          3. Cubics are broken into quadratics
         The transformation tr is -NOT- applied to the contour,
         it is used as the transformation from IntContour
         coordinates to texel coordinates. The value of texel_size
         gives the size of a texel with the texel at (0, 0)
         starting at (0, 0) [in texel coordinates].
       */
      void
      filter(float curvature_collapse,
             const IntBezierCurve::transformation<int> &tr, ivec2 texel_size);


      /* extract render data the location of pixel(i, j) is at
         (step.x() * i, step.y() * j). The resolution is (count.x(), count.y())
         and the values extracted are those values from the IntPath
         transformed by tr. The max-distance values is in units
         of the IntPath with the transformation tr applied
       */
      void
      extract_render_data(const ivec2 &step, const ivec2 &count,
                          float max_distance,
                          const IntBezierCurve::transformation<int> &tr,
                          GlyphRenderDataDistanceField *dst) const;


      /* extract render data the location of pixel(i, j) is at
         (step.x() * i, step.y() * j). The resolution is (count.x(), count.y())
         and the values extracted are those values from the IntPath
         transformed by tr.

         The path should have filter() applied to it so that there
         are no cubic paths and that small paths are dropped
       */
      void
      extract_render_data(const ivec2 &step, const ivec2 &count,
                          const IntBezierCurve::transformation<int> &tr,
                          GlyphRenderDataCurvePair *dst) const;

    private:
      IntBezierCurve::ID_t
      computeID(void);

      fastuidraw::ivec2 m_last_pt;
      std::vector<IntContour> m_contours;
      BoundingBox<int> m_bb;
    };

  } //namespace fastuidraw
} //namespace detail
