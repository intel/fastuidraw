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

#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_source.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * A GlyphSequence represents a sequence of glyph codes with positions.
   * A GlyphSequence provides an interface to grab the glyph-codes realized
   * as different renderers for the pupose of rendering text in response
   * to the transformation that a Painter currently has. The methods of
   * GlyphSequence are re-entrant but not thread safe, i.e. if an application
   * uses the same GlyphSequence from multiple threads it needs to explicitely
   * lock the sequence when using it.
   */
  class GlyphSequence:fastuidraw::noncopyable
  {
  public:
    /*!
     * A \ref SubSequence represents a subset of the glyphs
     * of a \ref GlyphSequence for the purpose of culling
     * when rendering. Different SubSequence values from the
     * same \ref GlyphSequence are guaranteed to have disjoint
     * glyphs.
     */
    class SubSequence
    {
    public:
      /*!
       * Given a \ref GlyphRender, returns \ref PainterAttribute
       * and \ref PainterIndex data for specified \ref GlyphRender
       * value. The data is constructed lazily on demand.
       */
      void
      attributes_and_indices(GlyphRender render,
			     c_array<const PainterAttribute> *out_attributes,
			     c_array<const PainterIndex> *out_indices);

      /*!
       * Returns indices into GlyphSequence::glyph_sequence() and
       * \ref GlyphSequence::glyph_sources() of the glyphs that
       * are in this SubSequence.
       */
      unsigned int
      glyphs(c_array<unsigned int> dst);

      /*!
       * Returns the bounding box of the glyphs of this
       * SubSequence object. Returns false if the
       * bounding box is empty.
       */
      bool
      bounding_box(vec2 *out_min_bb, vec2 *out_max_bb);

      /*!
       * Returns the \ref Path made from the bounding
       * box of the SubSequence.
       */
      const Path&
      path(void);

    private:
      friend class GlyphSequence;
      explicit
      SubSequence(void *d);
      void *m_d;
    };

    /*!
     * \brief
     * Opaque object to hold work room needed for functions
     * of GlyphSequence that require scratch space.
     */
    class ScratchSpace:fastuidraw::noncopyable
    {
    public:
      ScratchSpace(void);
      ~ScratchSpace();
    private:
      friend class GlyphSequence;
      void *m_d;
    };

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
    GlyphSequence(float pixel_size,
                  enum PainterEnums::screen_orientation orientation,
                  const reference_counted_ptr<GlyphCache> &cache,
                  enum PainterEnums::glyph_layout_type layout
                  = PainterEnums::glyph_layout_horizontal);

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
     * Returns the number of \ref GlyphSource values added via
     * add_glyph() and add_glyphs().
     */
    unsigned int
    number_glyphs(void) const;

    /*!
     * Returns the \ref GlyphSource and position value for
     * the i'th glyph added via add_glyph() or add_glyphs().
     */
    void
    added_glyph(unsigned int I,
		GlyphMetrics *out_glyph_metrics,
		vec2 *out_position) const;

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
     * Returns the total number of \ref SubSequence objects of this
     * \ref GlyphSequence. This value can change when add_glyph() or
     * add_glyphs() is called.
     */
    unsigned int
    number_sub_sequences(void) const;

    /*!
     * Fetch a \ref SubSequence of this \ref GlyphSequence.
     * The returned object may no longer be valid if add_glyph()
     * or add_glyphs() is called. In addition, any returned
     * object is no longer valid if the owning \ref GlyphSequence
     * goes out of scope.
     */
    SubSequence
    sub_sequence(unsigned int I) const;

    /*!
     * Fetch those SubSequence objects that intersect a region
     * specified by clip equations.
     * \param scratch_space scratch space for computations.
     * \param clip_equations array of clip equations
     * \param clip_matrix_local 3x3 transformation from local (x, y, 1)
     *                          coordinates to clip coordinates.
     * \param[out] dst location to which to write the \ref SubSequence
     *                 ID values
     * \returns the number of SubSequence object ID's written to dst, that
     *          number is guaranteed to be no more than number_sub_sequences().
     */
    unsigned int
    select_sub_sequences(ScratchSpace &scratch_space,
			 c_array<const vec3> clip_equations,
			 const float3x3 &clip_matrix_local,
			 c_array<unsigned int> dst) const;

  private:
    void *m_d;
  };

/*! @} */
}
