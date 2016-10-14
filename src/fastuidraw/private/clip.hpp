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
    void
    clip_against_plane(const vec3 &clip_eq, const_c_array<vec2> pts,
                       std::vector<vec2> &out_pts, std::vector<float> &scratch_space);

    void
    clip_against_planes(const_c_array<vec3> clip_eq, const_c_array<vec2> in_pts,
                        std::vector<vec2> &out_pts,
                        std::vector<float> &scratch_space_floats,
                        vecN<std::vector<vec2>, 2> &scratch_space_vec2s);
  }
}
