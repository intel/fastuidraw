/*!
 * \file util_private_math.cpp
 * \brief file util_private_math.cpp
 *
 * Copyright 2018 by Intel.
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

#include <private/util_private_math.hpp>

fastuidraw::vecN<float, 2>
fastuidraw::detail::
compute_singular_values(const float2x2 &M)
{
  /* The SVD of a matrix M is defined as
   *
   *   M = U * D * tr(V)
   *
   * where U and V are orthogonormal matrices (i.e.
   * their transpose is their inverse) and D is a
   * diagonal matrix with all entries non-negative
   * in non-decreasing order along the diaganol.
   * Those value are te singular values. Consider
   *
   *  S = M * tr(M)
   *    = U * D * tr(v) * V * D * tr(U)
   *    = U * D^2 * tr(U)
   *
   * thus the Eigen values of S give the squares
   * of the singular values of M. When M is a 2x2,
   * we can solve the degree two polynomial for the
   * Eigen values of S easily with the quadratic
   * formulat
   */

  float2x2 S;
  float B, C, D, s1, s2;

  S = M * M.transpose();

  B = S(0, 0) + S(1, 1);
  C = S(0, 0) * S(1, 1) - S(0, 1) * S(1, 0);

  /* in exact arithmatic, D >= 0, but floating point
   * accuracy might not hold true, so we max the value
   * with 0,
   */
  D = B * B - 4.0 * C;
  D = t_sqrt(t_max(0.0f, D));

  s1 = (B + D) / 2.0f;
  s2 = (B - D) / 2.0f;

  /* again max with 0 for rounding issues and then take sqrt */
  s1 = t_sqrt(t_max(0.0f, s1));
  s2 = t_sqrt(t_max(0.0f, s2));

  return vecN<float, 2>(s1, s2);
}
