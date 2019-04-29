/*!
 * \file glyph.cpp
 * \brief file glyph.cpp
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

#include <algorithm>
#include <fastuidraw/text/glyph.hpp>

void
fastuidraw::Glyph::
glyph_attribute_dst_write(int glyph_attribute_index,
                          PainterAttribute::pointer_to_field *out_attribute,
                          int *out_index_into_attribute)
{
  if (glyph_attribute_index < 4)
    {
      *out_attribute = &PainterAttribute::m_attrib1;
      *out_index_into_attribute = glyph_attribute_index;
    }
  else if (glyph_attribute_index < 8)
    {
      *out_attribute = &PainterAttribute::m_attrib2;
      *out_index_into_attribute = glyph_attribute_index - 4;
    }
  else
    {
      *out_attribute = nullptr;
      *out_index_into_attribute = -1;
    }
}
