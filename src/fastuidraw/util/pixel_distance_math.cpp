/*!
 * \file pixel_distance_math.cpp
 * \brief file pixel_distance_math.cpp
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

#include <fastuidraw/util/pixel_distance_math.hpp>
#include <fastuidraw/util/math.hpp>

float
fastuidraw::
local_distance_from_pixel_distance(float pixel_distance,
                                   const vec2 &resolution,
                                   const float3x3 &matrix,
                                   const vec2 &p, const vec2 &v)
{
  vec3 clip_p(p.x(), p.y(), 1.0f), clip_offset(v.x(), v.y(), 0.0f);
  vec2 zeta;
  float length_zeta, return_value;

  clip_p = vec3(0.5f * resolution.x(), 0.5f * resolution.y(), 1.0f) * (matrix * clip_p);
  clip_offset = vec3(0.5f * resolution.x(), 0.5f * resolution.y(), 1.0f) * (matrix * clip_offset);

  /* From where this formula comes. Our challenge is to give a stroking width
     in local (pre-projection) coordiantes from a stroking width of pixels.
     We have a point p (given by position) and a direction v (given by offset)
     and we wish to find an L so that after projection p + L*v is W pixels
     from p. Now for some math.

     Let m>0, and let q(m) = a + m*v. Once m>0 is large enough, we can
     project q(m) and p to the screen, find the screen space barycentric
     W pixels from p.

     Let P = projection(T(p,1))
     Let Q(m) = projection(T(q(m),1))

     where

     T = K * matrix
     K = (0.5 * resolution.x, 0.5 * resolution.y, 1)
     projection(x,y,z) = (x/z, y/z)

     Let R = P + W * DELTA where DELTA = (Q(m) - P)/||Q(m)-P||
     Let s = W / ||Q(m) - P|| gives that P + s * (Q(m) - P) = R.
     Let t = ( s/Q(m)_z ) / ( (1-s)/P_z + s/Q(m)_z). Then
     projection( T(p,1) + t * (T(q(m),1) - T(p,1)) ) = R

     t * (T(q(m),1) - T(p,1)) = t * (T(p,1) + m*T(v,0) - T(p,1)) = t * m * T(v, 0)

     Thus the coefficient we want is given by (t*m).

     t * m = ( m * s * P_z) / ( (1-s)Q(m)_z + s * P_z)

     which simplifies to (after lots of algebra) to

     t * m = (W * P_z * P_z) / (- W * P_z * |T(v,0)_z| + ||zeta||)

     where zeta = (T(v,0)_x * T(p,1)_z - T(v,0)_z * T(p,1)_x,
                   T(v,0)_y * T(p,1)_z - T(v,0)_z * T(p,1)_y)
  */
  zeta = vec2(clip_offset.x() * clip_p.z() - clip_offset.z() * clip_p.x(),
              clip_offset.y() * clip_p.z() - clip_offset.z() * clip_p.y());
  length_zeta = zeta.magnitude();

  return_value = pixel_distance * clip_p.z() * clip_p.z();
  return_value /= (-pixel_distance * t_abs(clip_p.z() * clip_offset.z()) + length_zeta);

  return return_value;
}
