/*!
 * \file math.hpp
 * \brief file math.hpp
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

#include <math.h>
#include <stdlib.h>

namespace fastuidraw
{
/*!\addtogroup Utility
 * @{
 */

/*!\def FASTUIDRAW_PI
 * Macro giving the value of pi as a float
 */
#define FASTUIDRAW_PI 3.14159265358979323846f

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  template<typename T>
  inline
  const T&
  t_min(const T &a, const T &b)
  {
    return (a < b) ? a : b;
  }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  template<typename T>
  inline
  const T&
  t_max(const T &a, const T &b)
  {
    return (a < b) ? b : a;
  }

  /*!
   * Return the sign of a value.
   */
  template<typename T>
  inline
  T
  t_sign(const T &a)
  {
    return (a < T(0)) ? T(-1) : T(1);
  }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_sin(float x) { return ::sinf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_cos(float x) { return ::cosf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_sqrt(float x) { return ::sqrtf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_asin(float x) { return ::asinf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_acos(float x) { return ::acosf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_atan(float x) { return ::atanf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_atan2(float y, float x) { return ::atan2f(y, x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_sin(double x) { return ::sin(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_cos(double x) { return ::cos(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_sqrt(double x) { return ::sqrt(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_asin(double x) { return ::asin(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_acos(double x) { return ::acos(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_atan(double x) { return ::atan(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_atan2(double y, double x) { return ::atan2(y, x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_sin(long double x) { return ::sinl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_cos(long double x) { return ::cosl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_sqrt(long double x) { return ::sqrtl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_asin(long double x) { return ::asinl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_acos(long double x) { return ::acosl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_atan(long double x) { return ::atanl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_atan2(long double y, long double x) { return ::atan2l(y, x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  int
  t_abs(int x) { return ::abs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long
  t_abs(long x) { return ::labs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long long
  t_abs(long long x) { return ::llabs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_abs(float x) { return fabsf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_abs(double x) { return fabs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_abs(long double x) { return fabsl(x); }

/*! @} */
}
