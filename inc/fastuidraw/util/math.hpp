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

namespace fastuidraw
{
/*!\addtogroup Utility
  @{
 */

  /*!
    Conveniance overload avoiding to rely on std::
   */
  template<typename T>
  const T&
  t_min(const T &a, const T &b)
  {
    return (a < b) ? a : b;
  }

  /*!
    Conveniance overload avoiding to rely on std::
   */
  template<typename T>
  const T&
  t_max(const T &a, const T &b)
  {
    return (a < b) ? b : a;
  }

  /*!
    Conveniance overload avoiding to rely on std::
   */
  float
  t_sin(float x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  float
  t_cos(float x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  float
  t_sqrt(float x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  double
  t_sin(double x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  double
  t_cos(double x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  double
  t_sqrt(double x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  long double
  t_sin(long double x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  long double
  t_cos(long double x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  long double
  t_sqrt(long double x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  int
  t_abs(int x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  long
  t_abs(long x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  long long
  t_abs(long long x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  float
  t_abs(float x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  double
  t_abs(double x);

  /*!
    Conveniance overload avoiding to rely on std::
   */
  long double
  t_abs(long double x);

/*! @} */
}
