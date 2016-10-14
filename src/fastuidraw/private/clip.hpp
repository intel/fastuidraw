/*!
 * \file clip.hpp
 * \brief file clip.hpp
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

#include <vector>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
  namespace detail
  {
    /* Clip a polygon against a single plane. The clip equation
       clip_eq and the polygon pts are both in the same coordinate
       system (likely local). Returns true if the polygon is
       completely unclipped.
     */
    bool
    clip_against_plane(const vec3 &clip_eq, const_c_array<vec2> pts,
                       std::vector<vec2> &out_pts, std::vector<float> &scratch_space);

    /* Clip a polygon against several planes. The clip equations
       clip_eq and the polygon pts are both in the same coordinate
       system (likely local). Returns true if the polygon is
       completely unclipped.
     */
    bool
    clip_against_planes(const_c_array<vec3> clip_eq, const_c_array<vec2> in_pts,
                        std::vector<vec2> &out_pts,
                        std::vector<float> &scratch_space_floats,
                        vecN<std::vector<vec2>, 2> &scratch_space_vec2s);
  }
}
