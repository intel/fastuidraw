/*!
 * \file pack_texels.hpp
 * \brief file pack_texels.hpp
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

#ifndef FASTUIDRAW_PACK_TEXELS_HPP
#define FASTUIDRAW_PACK_TEXELS_HPP

#include <vector>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>

namespace fastuidraw
{
  namespace detail
  {
    /*!
     * Pack 8-bit texel values into 32-bit values where each
     * 32-bit value holds a 2x2 block of texels.
     */
    void
    pack_texels(const uvec2 &in_dims,
                c_array<const uint8_t> texels,
                std::vector<uint32_t> *out_packed_texels);
  }
}

#endif
