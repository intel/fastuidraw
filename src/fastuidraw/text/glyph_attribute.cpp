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
pack_texel_rect(unsigned int width, unsigned int height)
{
  for (unsigned int c = 0; c < 4; ++c)
    {
      unsigned int x, y;

      x = (c & right_corner_mask) ? width : 0u;
      y = (c & top_corner_mask) ? height : 0u;

      m_data[c] = pack_bits(rect_width_bit0, rect_width_num_bits, width)
        | pack_bits(rect_height_bit0, rect_height_num_bits, height)
        | pack_bits(rect_x_bit0, rect_x_num_bits, x)
        | pack_bits(rect_y_bit0, rect_y_num_bits, y);
    }
}
