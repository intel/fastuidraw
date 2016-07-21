/*!
 * \file glyph_layout.hpp
 * \brief file glyph_layout.hpp
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

#include <stdint.h>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/text/font.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/
  /*!
    A GlyphLayoutData provides information on how to
    layout text using a glyph.
   */
  class GlyphLayoutData
  {
  public:
    GlyphLayoutData(void):
      m_glyph_code(0),
      m_font(),
      m_horizontal_layout_offset(0.0f, 0.0f),
      m_vertical_layout_offset(0.0f, 0.0f),
      m_size(0.0f, 0.0f),
      m_advance(0.0f, 0.0f),
      m_pixel_size(0)
    {}

    /*!
      The index of the glyph into the -font- of the glyph
     */
    uint32_t m_glyph_code;

    /*!
      Font of the glyph;
     */
    reference_counted_ptr<const FontBase> m_font;

    /*!
      The offset from the pen at which to
      display the glyph when performing horizontal
      text layout.
     */
    vec2 m_horizontal_layout_offset;

    /*!
      The offset from the pen at which to
      display the glyph when performing horizontal
      text layout.
     */
    vec2 m_vertical_layout_offset;

    /*!
      Size at which to draw the glyph
     */
    vec2 m_size;

    /*!
      How much to advance the pen after
      drawing the glyph. The x-coordinate
      holds the advance when performing
      layout horizontally and y-coordinate
      when performing layout vertically.
     */
    vec2 m_advance;

    /*!
      Pixel size of glyph, all the data
      of GlyphLayoutData is relative to
      \ref m_pixel_size .
     */
    int m_pixel_size;
  };
/*! @} */
}
