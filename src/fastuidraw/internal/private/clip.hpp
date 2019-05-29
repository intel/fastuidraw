/*!
 * \file clip.hpp
 * \brief file clip.hpp
 *
 * Copyright 2016 by Intel.
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

#include <vector>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

#include <private/util_private.hpp>

namespace fastuidraw
{
  namespace detail
  {
    /* Clip a polygon against a single plane. The clip equation
     * clip_eq and the polygon pts are both in the same coordinate
     * system (likely local). Returns true if the polygon is
     * completely unclipped.
     */
    bool
    clip_against_plane(const vec3 &clip_eq, c_array<const vec2> pts,
                       std::vector<vec2> &out_pts);

    /* Clip a polygon against several planes. The clip equations
     * clip_eq and the polygon pts are both in the same coordinate
     * system (likely local). Returns true if the polygon is
     * completely unclipped.
     * \param clip_eq array of clip-equations
     * \param in_pts pts of input polygon
     * \param[out] out_idx location to which to write index into
     *                     scratch_space where clipped polygon is
     *                     written
     * \param scratch_space scratch spase for computation
     */
    bool
    clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec2> in_pts,
                        unsigned int *out_idx,
                        vecN<std::vector<vec2>, 2> &scratch_space);

    inline
    bool
    clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec2> in_pts,
                        c_array<const vec2> *out_pts,
                        vecN<std::vector<vec2>, 2> &scratch_space)
    {
      unsigned int idx;
      bool return_value;

      return_value = clip_against_planes(clip_eq, in_pts, &idx, scratch_space);
      *out_pts = make_c_array(scratch_space[idx]);
      return return_value;
    }

    /* Clip a polygon against a single plane. The clip equation
     * clip_eq and the polygon pts are both in the same coordinate
     * system (likely clip-coordinates). Returns true if the polygon
     * is completely unclipped.
     */
    bool
    clip_against_plane(const vec3 &clip_eq, c_array<const vec3> pts,
                       std::vector<vec3> &out_pts);

    /* Clip a polygon against several planes. The clip equations
     * clip_eq and the polygon pts are both in the same coordinate
     * system (likely clip-coordinates). Returns true if the polygon
     * is completely unclipped.
     * \param clip_eq array of clip-equations
     * \param in_pts pts of input polygon
     * \param[out] out_idx location to which to write index into
     *                     scratch_space where clipped polygon is
     *                     written
     * \param scratch_space scratch spase for computation
     */
    bool
    clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec3> in_pts,
                        unsigned int *out_idx,
                        vecN<std::vector<vec3>, 2> &scratch_space);

    inline
    bool
    clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec3> in_pts,
                        c_array<const vec3> *out_pts,
                        vecN<std::vector<vec3>, 2> &scratch_space)
    {
      unsigned int idx;
      bool return_value;

      return_value = clip_against_planes(clip_eq, in_pts, &idx, scratch_space);
      *out_pts = make_c_array(scratch_space[idx]);
      return return_value;
    }
  }
}
