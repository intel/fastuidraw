/*!
 * \file pack_texels.cpp
 * \brief file pack_texels.cpp
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

#include <private/pack_texels.hpp>

namespace
{
  inline
  uint32_t
  read_texel(const fastuidraw::uvec2 &dims,
         fastuidraw::c_array<const uint8_t> texels,
         unsigned int x, unsigned int y)
  {
    x = fastuidraw::t_min(x, dims.x() - 1u);
    y = fastuidraw::t_min(y, dims.y() - 1u);
    return texels[x + y * dims.x()];
  }
}

void
fastuidraw::detail::
pack_texels(const uvec2 &dims,
            c_array<const uint8_t> texels,
            std::vector<generic_data> *out_packed_texels)
{
  uvec2 wh(dims);
  if (wh.x() & 1u)
    {
      ++wh.x();
    }

  if (wh.y() & 1u)
    {
      ++wh.y();
    }

  std::vector<generic_data> &data(*out_packed_texels);
  uint32_t num_texels(wh.x() * wh.y());

  FASTUIDRAWassert((num_texels & 0x3u) == 0u);
  data.resize(num_texels >> 2u);
  for (unsigned int y = 0, dst = 0; y < wh.y(); y += 2u)
    {
      for (unsigned int x = 0; x < wh.x(); x += 2u, ++dst)
        {
          uint32_t v, p00, p01, p10, p11;

          p00 = read_texel(dims, texels, x + 0u, y + 0u);
          p10 = read_texel(dims, texels, x + 1u, y + 0u);
          p01 = read_texel(dims, texels, x + 0u, y + 1u);
          p11 = read_texel(dims, texels, x + 1u, y + 1u);

          v = pack_bits(0u, 8u, p00)
            | pack_bits(8u, 8u, p10)
            | pack_bits(16u, 8u, p01)
            | pack_bits(24u, 8u, p11);

          FASTUIDRAWassert(dst < data.size());
          data[dst].u = v;
        }
    }
}
