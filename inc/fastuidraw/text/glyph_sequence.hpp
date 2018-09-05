/*!
 * \file glyph_sequence.hpp
 * \brief file glyph_sequence.hpp
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

#pragma once

#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_source.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * A GlyphSequence represents a sequence of glyph codes with positions.
   * A GlyphSequence provides an interface to grab the glyph-codes realized
   * as different renderers for the pupose of rendering text in reponse
   * to the transformation that a Painter currently has. The methods of
   * GlyphSequence are re-entrant but not thread safe, i.e. if an application
   * uses the same GlyphSequence from multiple threads it needs to explicitely
   * lock the sequence when using it.
   */
  class GlyphSequence:fastuidraw::noncopyable
  {
  public:
    /*!
     * Ctor, initailizes as empty.
     * \param pixel_size pixel size at which glyphs added via
     *                   add_glyphs() or add_glyph() are formatted
     * \param cache \ref GlyphCache used to fetch Glyph values
     */
    explicit
    GlyphSequence(float pixel_size,
                  const reference_counted_ptr<GlyphCache> &cache);

    ~GlyphSequence();

    /*!
     * Add \ref GlyphSource values and positions; values are -copied-.
     */
    void
    add_glyphs(c_array<const GlyphSource> glyph_sources,
               c_array<const vec2> positions);

    /*!
     * Add a single \ref GlyphSource and position
     */
    void
    add_glyph(const GlyphSource &glyph_source, const vec2 &position)
    {
      c_array<const GlyphSource> glyph_sources(&glyph_source, 1);
      c_array<const vec2> positions(&position, 1);
      add_glyphs(glyph_sources, positions);
    }

    /*!
     * Returns the Glyph values of the glyph code sequence
     * realized with a specified \ref GlyphRender. This
     * function creates the sequence lazily on demand.
     * The return value is no longer valid if add_glyphs()
     * or add_glyph() is called.
     */
    c_array<const Glyph>
    glyph_sequence(GlyphRender render, bool upload_to_atlas = true);

    /*!
     * Return the \ref GlyphSource sequence. The return
     * value is no longer valid if add_glyphs() or
     * add_glyph() is called.
     */
    c_array<const GlyphSource>
    glyph_sources(void) const;

    /*!
     * Returns the glyph positions. The return
     * value is no longer valid if add_glyphs()
     * or add_glyph() is called.
     */
    c_array<const vec2>
    glyph_positions(void) const;

    /*!
     * Return the \ref GlyphCache used by this GlyphSequence
     * to fetch \ref Glyph values.
     */
    const reference_counted_ptr<GlyphCache>&
    glyph_cache(void) const;

    /*!
     * Pixel size with which glyph sequences added by
     * add_glyphs() and add_glyph() are formatted.
     */
    float
    pixel_size(void) const;

  private:
    void *m_d;
  };

/*! @} */
}
