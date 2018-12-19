/*!
 * \file glyph_run.hpp
 * \brief file glyph_run.hpp
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

#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_source.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/packing/painter_packer.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!\brief
   * A GlyphRun represents a sequence of glyph codes with positions.
   * A GlyphRun provides an interface to grab the glyph-codes realized
   * as different renderers for the purpose of rendering text in response
   * to the transformation that a Painter currently has. In contrast to
   * a \ref GlyphSequence, a GlyphRun does NOT maintain a hierarchy
   * to perform culling. However, it does provide an interface to select
   * continuous subsets of the glyphs for the purporse of rendering.
   * In addition, since it does not carry a hierarchy for culling, it
   * is also a lighter wieght object than \ref GlyphSequence.
   */
  class GlyphRun:fastuidraw::noncopyable
  {
  public:
    /*!
     * Ctor.
     * \param pixel_size pixel size at which glyphs added via
     *                   add_glyphs() or add_glyph() are formatted
     * \param orientation screen orientation at which glyphs added by
     *                    add_glyphs() or add_glyph() are formatted
     * \param cache \ref GlyphCache used to fetch \ref Glyph values
     * \param layout specifies if glyphs added by add_glyphs()
     *               or add_glyph() will be layed out horizontally
     *               or vertically
     */
    explicit
    GlyphRun(float pixel_size,
             enum PainterEnums::screen_orientation orientation,
             const reference_counted_ptr<GlyphCache> &cache,
             enum PainterEnums::glyph_layout_type layout
             = PainterEnums::glyph_layout_horizontal);

    ~GlyphRun();

    /*!
     * Add glyphs passing an array of positions and GlyphMetric values;
     * values are -copied-.
     * \param glyph_metrics specifies what glyphs to add
     * \param positions specifies the positions of each glyph added
     */
    void
    add_glyphs(c_array<const GlyphMetrics> glyph_metrics,
               c_array<const vec2> positions);

    /*!
     * Add \ref GlyphSource values and positions; values are -copied-.
     * \param glyph_sources specifies what glyphs to add
     * \param positions specifies the positions of each glyph added
     */
    void
    add_glyphs(c_array<const GlyphSource> glyph_sources,
               c_array<const vec2> positions);

    /*!
     * Add glyphs from a specific font and positions; values are -copied-.
     * \param font font from which to fetch glyphs
     * \param glyph_codes specifies what glyphs to add
     * \param positions specifies the positions of each glyph added
     */
    void
    add_glyphs(const reference_counted_ptr<const FontBase> &font,
               c_array<const uint32_t> glyph_codes,
               c_array<const vec2> positions);

    /*!
     * Add a single \ref GlyphSource and position
     * \param glyph_source specifies what glyph to add
     * \param position specifies the position of the glyph added
     */
    void
    add_glyph(const GlyphSource &glyph_source, const vec2 &position)
    {
      c_array<const GlyphSource> glyph_sources(&glyph_source, 1);
      c_array<const vec2> positions(&position, 1);
      add_glyphs(glyph_sources, positions);
    }

    /*!
     * Returns the number of \ref GlyphSource values added via
     * add_glyph() and add_glyphs().
     */
    unsigned int
    number_glyphs(void) const;

    /*!
     * Returns the \ref GlyphSource and position value for
     * the i'th glyph added via add_glyph() or add_glyphs().
     * \param I index to select which glyph, must be that
     *          0 <= I < number_glyphs()
     * \param *out_glyph_metrics location to which to write
     *                           the \ref GlyphMetrics value
     *                           describing the glyph
     * \param *out_position location to which to write the
     *                      position of the glyph
     */
    void
    added_glyph(unsigned int I,
		GlyphMetrics *out_glyph_metrics,
		vec2 *out_position) const;

    /*!
     * Return the \ref GlyphCache used by this GlyphRun
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

    /*!
     * Orientation with which glyph sequences added by
     * add_glyphs() and add_glyph() are formatted.
     */
    enum PainterEnums::screen_orientation
    orientation(void) const;

    /*!
     * Layout with which glyph sequences added by
     * add_glyphs() and add_glyph() are formatted.
     */
    PainterEnums::glyph_layout_type
    layout(void) const;

    /*!
     * Returns a const-reference to PainterPacker::DataWrite
     * object for rendering a named range of glyphs for
     * a specified \ref GlyphRenderer. The returned object
     * is valid in value until this GlyphRun is destroyed or
     * one of add_glyph(), add_glyphs(), subsequence() is called.
     * \param renderer how to render the glyphs
     * \param begin index to select which is the first glyph
     * \param count number of glyphs to take starting at begin
     */
    const PainterPacker::DataWriter&
    subsequence(GlyphRenderer renderer, unsigned int begin, unsigned int count) const;

    /*!
     * Returns a const-reference to PainterPacker::DataWrite
     * object for rendering all glyphs from a starting point
     * for a specified \ref GlyphRenderer. The returned object
     * is valid in value until this GlyphRun is destroyed or
     * one of add_glyph(), add_glyphs(), subsequence() is called.
     * \param renderer how to render the glyphs
     * \param begin index to select which is the first glyph
     */
    const PainterPacker::DataWriter&
    subsequence(GlyphRenderer renderer, unsigned int begin) const;

    /*!
     * Returns a const-reference to PainterPacker::DataWrite
     * object for rendering the entire range of glyphs for
     * a specified \ref GlyphRenderer. The returned object
     * is valid in value until this GlyphRun is destroyed or
     * one of add_glyph(), add_glyphs(), subsequence() is called.
     * \param renderer how to render the glyphs
     */
    const PainterPacker::DataWriter&
    subsequence(GlyphRenderer renderer) const;

  private:
    void *m_d;
  };

/*! @} */
}
