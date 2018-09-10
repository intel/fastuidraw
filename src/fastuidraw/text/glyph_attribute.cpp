/*!
 * \file glyph_attribute.cpp
 * \brief file glyph_attribute.cpp
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

#include <fastuidraw/text/glyph_attribute.hpp>

void
fastuidraw::GlyphAttribute::
pack_location(GlyphLocation G)
{
  if (G.valid())
    {
      ivec2 p(G.location()), sz(G.size());
      int layer(G.layer());

      for (unsigned int c = 0; c < 4; ++c)
        {
          uint32_t xbits, ybits, zbits;
          ivec2 q(p);

          if (c & right_corner_mask)
            {
              q.x() += sz.x();
            }

          if (c & top_corner_mask)
            {
              q.y() += sz.y();
            }

          xbits = pack_bits(bit0_x_texel, num_texel_coord_bits, q.x());
          ybits = pack_bits(bit0_y_texel, num_texel_coord_bits, q.y());
          zbits = pack_bits(bit0_z_texel, num_texel_coord_bits, layer);

          m_data[c] = xbits | ybits| zbits;
        }
    }
  else
    {
      for (int c = 0; c < 4; ++c)
        {
          m_data[c] = invalid_mask;
        }
    }
}
