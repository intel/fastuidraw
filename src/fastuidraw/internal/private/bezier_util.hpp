/*!
 * \file bezier_util.hpp
 * \brief file bezier_util.hpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#pragma once

#include <cmath>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/math.hpp>

namespace fastuidraw
{
  namespace detail
  {
    template<typename T>
    class DefaultIntermediateType
    {};

    template<>
    class DefaultIntermediateType<int>
    {
    public:
      typedef int64_t int_type;
      typedef float float_type;
    };

    template<>
    class DefaultIntermediateType<float>
    {
    public:
      typedef float int_type;
      typedef float float_type;
    };

    template<>
    class DefaultIntermediateType<double>
    {
    public:
      typedef double int_type;
      typedef double float_type;
    };

    template<typename InputType,
             typename IntermediateType = typename DefaultIntermediateType<InputType>::int_type,
             typename OutputType = InputType>
    vecN<vecN<vecN<OutputType, 2>, 4>, 2>
    split_cubicT(c_array<const vecN<InputType, 2> > pts)
    {
      FASTUIDRAWassert(pts.size() == 4);

      vecN<vecN<vecN<OutputType, 2>, 4>, 2> return_value;
      vecN<IntermediateType, 2> p0(pts[0]), p1(pts[1]), p2(pts[2]), p3(pts[3]);
      vecN<IntermediateType, 2> p01, p23, pA, pB, pC;
      const IntermediateType two(2), three(3), four(4), eight(8);

      p01 = (p0 + p1) / two;
      p23 = (p2 + p3) / two;
      pA = (p0 + two * p1 + p2) / four;
      pB = (p1 + two * p2 + p3) / four;
      pC = (p0 + three * p1 + three * p2 + p3) / eight;

      vecN<OutputType, 2> q0(pts[0]), q01(p01), qA(pA), qC(pC);
      vecN<OutputType, 2> qB(pB), q23(p23), q3(pts[3]);

      return_value[0] = vecN<vecN<OutputType, 2>, 4>(q0, q01, qA, qC);
      return_value[1] = vecN<vecN<OutputType, 2>, 4>(qC, qB, q23, q3);

      return return_value;
    }

    template<typename InputType,
             typename IntermediateType = typename DefaultIntermediateType<InputType>::int_type,
             typename OutputType = InputType>
    vecN<vecN<vecN<OutputType, 2>, 4>, 2>
    split_cubicT(const vecN<vecN<InputType, 2>, 4> &pts)
    {
      c_array<const vecN<InputType, 2> > q(pts.c_ptr(), 4);
      return split_cubicT<InputType, IntermediateType, OutputType>(q);
    }

    template<typename InputType,
             typename IntermediateType = typename DefaultIntermediateType<InputType>::int_type,
             typename IntermediateFloatType = typename DefaultIntermediateType<InputType>::float_type,
             typename OutputType = InputType>
    vecN<vecN<OutputType, 2>, 3>
    quadratic_from_cubicT(c_array<const vecN<InputType, 2> > pts)
    {
      FASTUIDRAWassert(pts.size() == 4);

      vecN<IntermediateType, 2> p0(pts[0]), p1(pts[1]), p2(pts[2]), p3(pts[3]);
      IntermediateFloatType three(3), four(4);
      vecN<IntermediateFloatType, 2> q0(pts[0]), q1(pts[1]), q2(pts[2]), q3(pts[3]);
      vecN<IntermediateFloatType, 2> C;

      /* see the discussion at compute_quadratic_cubic_approximate_error()
       * for what the error between the returned quadratic and original
       * cubic is.
       */
      C = (three * q2 - q3 + three * q1 - q0) / four;

      vecN<vecN<OutputType, 2>, 3> return_value;
      return_value[0] = vecN<OutputType, 2>(pts[0]);
      return_value[1] = vecN<OutputType, 2>(C);
      return_value[2] = vecN<OutputType, 2>(pts[3]);

      return return_value;
    }

    template<typename InputType,
             typename IntermediateType = typename DefaultIntermediateType<InputType>::int_type,
             typename IntermediateFloatType = typename DefaultIntermediateType<InputType>::float_type,
             typename OutputType = InputType>
    vecN<vecN<OutputType, 2>, 3>
    quadratic_from_cubicT(const vecN<vecN<InputType, 2>, 4> &pts)
    {
      c_array<const vecN<InputType, 2> > q(pts.c_ptr(), 4);
      return quadratic_from_cubicT<InputType, IntermediateType, IntermediateFloatType, OutputType>(q);
    }

    inline
    float
    compute_quadratic_cubic_approximate_error(fastuidraw::c_array<const fastuidraw::vec2> p)
    {
      /* This is derivied fromt the article at
       *     http://caffeineowl.com/graphics/2d/vectorial/cubic2quad01.html
       *
       * The derivation for the error comes from the following:
       *
       * Let p(t) = (1-t)^3 p0 + 3t(1-t)^2 p1 + 3t^2(1-t) p2 + t^3 p3
       *
       * Set A = 3p1 - p0, B = 3p2 - p1
       * q0 = p0, q1 = (A + B) / 4 and q2 = p2
       *
       * then, after lots of algebra,
       *
       * p(t) - q(t) = (A - B) (t^3 - 1.5t^2 + 0.5t)
       *
       * then the maximum error in each coordinate occurs when
       * the polynomial Z(t) =  t^3 - 1.5t^2 + 0.5t has maixmal
       * absolute value the critical points; thus we need to only
       * check the critical points which are when Z'(t) = 0 which
       * are t1 = 0.5 * (1 - sqrt(3)) and t2 = 0.5 * (1 + sqrt(3)).
       * Z(t1) = sqrt(3) / 36 and Z(t2) = -sqrt(3) / 36. Hence,
       * the maximal error of max ||p(t) - q(t) || over 0 <= t < 1
       * is ||A - B|| * sqrt(3) / 36
       */
      const float error_coeff(0.04811252243f);
      fastuidraw::vec2 error_vec(p[3] - 3.0f * p[2] + 3.0f * p[1] - p[0]);
      float error(error_coeff * error_vec.magnitude());
      return error;
    }

    template<typename Builder>
    unsigned int
    add_cubic_adaptive(int max_recursion, Builder *B,
                       c_array<const vec2> p, float tol)
    {
      FASTUIDRAWassert(p.size() == 4);
      if (max_recursion == 0 || compute_quadratic_cubic_approximate_error(p) < tol)
        {
          vec2 q1;
          q1 = (3.0f * p[2] - p[3] + 3.0f * p[1] - p[0]) * 0.25f;
          B->quadratic_to(q1, p[3]);
          return 1;
        }
      else
        {
          /* split the cubic */
          vecN<vecN<vec2, 4>, 2> split;
          unsigned int v0, v1;

          split = split_cubicT<float>(p);
          v0 = add_cubic_adaptive(max_recursion - 1, B, split[0], tol);
          v1 = add_cubic_adaptive(max_recursion - 1, B, split[1], tol);
          return v0 + v1;
        }
    }

    template<typename Builder>
    unsigned int
    add_arc_as_single_cubic(int max_recursion, Builder *B, float tol,
                            vec2 from_pt, vec2 to_pt, float angle)
    {
      vec2 vp(to_pt - from_pt);
      vec2 jp(-vp.y(), vp.x());
      float d(t_tan(angle * 0.25f));
      vec2 c0, c1;

      vp *= ((1.0f - d * d) / 3.0f);
      jp *= (2.0f * d / 3.0f);
      c0 = from_pt + vp - jp;
      c1 = to_pt - vp - jp;

      return add_cubic_adaptive(max_recursion, B,
                                vecN<vec2, 4>(from_pt, c0, c1, to_pt),
                                tol);
    }

    template<typename Builder>
    unsigned int
    add_arc_as_cubics(int max_recursion, Builder *B, float tol,
                      vec2 start_pt, vec2 end_pt, vec2 center,
                      float radius, float start_angle, float angle)
    {
      /* One way to approximate an arc with a cubic-bezier is as
       * follows (taken from GLyphy which is likely take its
       * computations from Cairo).
       *
       *   D = tan(angle / 4)
       *   p0 = start of arc
       *   p3 = end of arc
       *   vp = p3 - p1
       *   jp = J(vp)
       *   A = (1 - D^2) / 3
       *   B = (2 * D) / 3
       *   p1 = p0 + A * vp - B * jp
       *   p2 = p1 - A * vp - B * jp
       *
       * and the error between the arc and [p0, p1, p2, p3] is given by
       *
       *   error <= |vp| * |D|^5 / (54(1 + D^2))
       *
       * Now, |angle| < 2 * PI, implies |angle| / 4 < PI / 4 thus |D| < 1, giving
       *
       *   error <= |vp| * |D|^5 / 27
       *
       * thus we need:
       *
       *  |tan(|angle| / 4)|^5 < (27 * tol) / |vp|
       *
       * since, tan() is increasing function,
       *
       *   |angle| < 4 * pow(atan((27 * tol) / |vp|), 0.20f)
       *
       */

      vec2 vp(end_pt - start_pt);
      vec2 jp(-vp.y(), vp.x());
      float mag_vp(vp.magnitude());
      float goal, angle_max;

      if (mag_vp < tol)
        {
          B->line_to(end_pt);
          return 1;
        }

      /* half the tolerance for the cubic to quadratic and half
       * the tolerance for arc to cubic.
       */
      tol *= 0.5f;

      goal = t_min(1.0f, (27.0f * tol) / vp.magnitude());
      angle_max = t_min(FASTUIDRAW_PI, 4.0f * std::pow(fastuidraw::t_atan(goal), 0.20f));

      float angle_remaining(angle);
      float current_angle(start_angle);
      float angle_direction(t_sign(angle));
      float angle_advance(angle_max * angle_direction);
      vec2 last_pt(start_pt);
      unsigned int return_value(0);

      while (t_abs(angle_remaining) > angle_max)
        {
          float next_angle(current_angle + angle_advance);
          float cos_next_angle(t_cos(next_angle));
          float sin_next_angle(t_sin(next_angle));
          vec2 next_delta, next_pt;

          next_pt.x() = center.x() + cos_next_angle * radius;
          next_pt.y() = center.y() + sin_next_angle * radius;
          return_value += add_arc_as_single_cubic(max_recursion, B, tol, last_pt, next_pt, angle_advance);

          current_angle += angle_advance;
          angle_remaining -= angle_advance;
          last_pt = next_pt;
        }

      return_value += add_arc_as_single_cubic(max_recursion, B, tol, last_pt, end_pt, angle_remaining);
      return return_value;
    }

  }
}
