/*!
 * \file util_private_math.hpp
 * \brief file util_private_math.hpp
 *
 * Copyright 2018 by Intel.
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

#ifndef FASTUIDRAW_UTIL_PRIVATE_MATH_HPP
#define FASTUIDRAW_UTIL_PRIVATE_MATH_HPP

#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/math.hpp>

namespace fastuidraw
{
  namespace detail
  {
    vecN<float, 2>
    compute_singular_values(const float2x2 &matrix);
  }
}

#endif
