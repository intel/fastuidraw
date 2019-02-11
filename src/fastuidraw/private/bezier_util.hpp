/*!
 * \file bezier_util.hpp
 * \brief file bezier_util.hpp
 *
 * Copyright 2019 by Intel.
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
             typename IntermediateFloatType = typename DefaultIntermediateType<InputType>::float_type,
             typename OutputType = InputType>
    vecN<vecN<OutputType, 2>, 3>
    quadratic_from_cubicT(c_array<const vecN<InputType, 2> > pts)
    {
      FASTUIDRAWassert(pts.size() == 4);

      IntermediateType det;
      vecN<IntermediateType, 2> p0(pts[0]), p1(pts[1]), p2(pts[2]), p3(pts[3]);
      vecN<IntermediateType, 2> d10(p1 - p0), d32(p3 - p2);
      vecN<IntermediateType, 2> Jd32(d32.y(), -d32.x());
      vecN<IntermediateType, 2> Jd10(d10.y(), -d10.x());
      vecN<IntermediateFloatType, 2> C;

      det = d10.x() * d32.y() - d10.y() * d32.x();
      if (det == IntermediateType(0))
        {
          C = IntermediateFloatType(0.5) * ( vecN<IntermediateFloatType, 2>(p1 + p2));
        }
      else
        {
          /* Compute where the lines [p0, p1] and [p2, p3]
           * intersect. If the intersection point goes
           * to far beyond p1 or p3, take the average
           * of clamping it going out upto 3 times the
           * length of [p0, p1] or [p2, p3].
           */
          vecN<IntermediateFloatType, 2> Cs, Ct;
          IntermediateFloatType s, t;

          s = IntermediateFloatType(dot(Jd32, p3 - p0)) / IntermediateFloatType(det);
          t = IntermediateFloatType(dot(Jd10, p0 - p3)) / IntermediateFloatType(det);

          s = t_max(IntermediateFloatType(-3.0), t_min(s, IntermediateFloatType(3.0)));
          t = t_max(IntermediateFloatType(-3.0), t_min(t, IntermediateFloatType(3.0)));

          Cs = vecN<IntermediateFloatType, 2>(p0) + s * vecN<IntermediateFloatType, 2>(d10);
          Ct = vecN<IntermediateFloatType, 2>(p3) - t * vecN<IntermediateFloatType, 2>(d32);

          C = IntermediateFloatType(0.5) * (Cs + Ct);
        }

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
  }
}
