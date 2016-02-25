/*!
 * \file math.cpp
 * \brief file math.cpp
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


#include <stdlib.h>
#include <math.h>
#include <fastuidraw/util/math.hpp>


namespace fastuidraw
{
  float t_sin(float x) { return ::sinf(x); }
  float t_cos(float x) { return ::cosf(x); }
  float t_sqrt(float x) { return ::sqrtf(x); }

  double t_sin(double x) { return ::sin(x); }
  double t_cos(double x) { return ::cos(x); }
  double t_sqrt(double x) { return ::sqrt(x); }

  long double t_sin(long double x) { return ::sinl(x); }
  long double t_cos(long double x) { return ::cosl(x); }
  long double t_sqrt(long double x) { return ::sqrtl(x); }

  int
  t_abs(int x) { return ::abs(x); }

  long
  t_abs(long x) { return ::labs(x); }

  long long
  t_abs(long long x)  { return ::llabs(x); }

  float
  t_abs(float x) { return fabsf(x); }

  double
  t_abs(double x) { return fabs(x); }

  long double
  t_abs(long double x) { return fabsl(x); }
}
