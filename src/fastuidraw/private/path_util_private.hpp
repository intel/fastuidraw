/*!
 * \file path_util_private.hpp
 * \brief file path_util_private.hpp
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

#include <fastuidraw/tessellated_path.hpp>

namespace fastuidraw
{
  namespace detail
  {
    unsigned int
    number_segments_for_tessellation(float radius, float arc_angle,
                                     const TessellatedPath::TessellationParams &P);
  }
}
