/*!
 * \file glyph_source.hpp
 * \brief file glyph_source.hpp
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

#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/glyph.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * Class to specify the source for a glyph.
   */
  class GlyphSource
  {
  public:
    /*!
     * Ctor. Initialize \ref m_glyph_code as 0 and \ref m_font
     * as nullptr.
     */
    GlyphSource(void):
      m_glyph_code(0)
    {}

    /*!
     * Ctor.
     * \param f value to assign to \ref m_font
     * \param g value to assign to \ref m_glyph_code
     */
    GlyphSource(const reference_counted_ptr<const FontBase> &f, uint32_t g):
      m_glyph_code(g),
      m_font(f)
    {}

    /*!
     * Ctor.
     * \param f value to assign to \ref m_font
     * \param g value to assign to \ref m_glyph_code
     */
    GlyphSource(uint32_t g, const reference_counted_ptr<const FontBase> &f):
      m_glyph_code(g),
      m_font(f)
    {}

    /*!
     * Comparison operator
     * \param rhs value to compare against
     */
    bool
    operator<(const GlyphSource &rhs) const
    {
      return (m_font < rhs.m_font)
        || (m_font == rhs.m_font && m_glyph_code < rhs.m_glyph_code);
    }

    /*!
     * Glyph code of a \ref Glyph
     */
    uint32_t m_glyph_code;

    /*!
     * Font of a \ref Glyph
     */
    reference_counted_ptr<const FontBase> m_font;
  };
/*! @} */
}
