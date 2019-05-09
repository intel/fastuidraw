/*!
 * \file glyph.hpp
 * \brief file glyph.hpp
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

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/text/glyph_attribute.hpp>
#include <fastuidraw/text/glyph_metrics.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

  class GlyphCache;

  /*!
   * \brief
   * A Glyph is essentially an opaque pointer to
   * data for rendering and performing layout of a
   * glyph.
   */
  class Glyph
  {
  public:
    /*!
     * Ctor. Initializes the Glyph to be invalid,
     * i.e. essentially a nullptr pointer
     */
    Glyph(void):
      m_opaque(nullptr)
    {}

    /*!
     * Returns true if the Glyph refers to actual
     * glyph data
     */
    bool
    valid(void) const
    {
      return m_opaque != nullptr;
    }

    /*!
     * Returns the glyph's rendering type, valid()
     * must return true. If not, debug builds FASTUIDRAWassert
     * and release builds crash.
     */
    enum glyph_type
    type(void) const;

    /*!
     * Returns the glyph's renderer, valid() must
     * return true. If not, debug builds FASTUIDRAWassert
     * and release builds crash.
     */
    GlyphRenderer
    renderer(void) const;

    /*!
     * Returns ths rendering size of the glyph (in
     * font coordinates). This value is similair in
     * value to GlyphMetrics:size() but not necessarily
     * idential (differnces come from discretization to
     * pixels for example).
     */
    vec2
    render_size(void) const;

    /*!
     * Returns the glyph's layout data, valid()
     * must return true. If not, debug builds FASTUIDRAWassert
     * and release builds crash.
     */
    GlyphMetrics
    metrics(void) const;

    /*!
     * Returns the glyph's per-corner attribute data.
     */
    c_array<const GlyphAttribute>
    attributes(void) const;

    /*!
     * Returns the GlyphCache on which the glyph
     * resides. The return value of valid() must be
     * true. If not, debug builds FASTUIDRAWassert
     * and release builds crash.
     */
    GlyphCache*
    cache(void) const;

    /*!
     * Returns the index location into the GlyphCache
     * of the glyph. The return value of valid() must be
     * true. If not, debug builds FASTUIDRAWassert and release
     * builds crash.
     */
    unsigned int
    cache_location(void) const;

    /*!
     * If returns \ref routine_fail, then the GlyphCache
     * on which the glyph resides needs to be cleared
     * first. If the glyph is already uploaded returns
     * immediately with \ref routine_success.
     */
    enum return_code
    upload_to_atlas(void) const;

    /*!
     * Returns true if and only if the Glyph is
     * already uploaded to a GlyphAtlas.
     */
    bool
    uploaded_to_atlas(void) const;

    /*!
     * Returns the path of the Glyph; the path is in
     * coordinates of the glyph with the convention
     * that the y-coordinate increases upwards. If one
     * is rendering the path (for example stroking it),
     * together with drawing of glyphs via a \ref Painter,
     * then one needs to reverse the y-coordinate (for
     * example by Painter::shear(1.0, -1.0)) if the
     * glyphs are rendered with data packed by
     * pack_glyph() with PainterEnums::y_increases_downwards.
     */
    const Path&
    path(void) const;

    /*!
     * Create a Glyph WITHOUT placing it on a \ref GlyphCache.
     * Such a Glyph needs to be destroyed manually with
     * delete_glyph() or placed on a GlyphCache (via GlyphCache::add_glyph()).
     * Glyph values that are NOT on a GlyphCache will always fail
     * in their call to upload_to_atlas().
     * \param render the nature of the render data to give to the Glyph
     * \param font the font used to generate the Glyph
     * \param glyph_code the glyph code to generate the Glyph
     */
    static
    Glyph
    create_glyph(GlyphRenderer render,
                 const reference_counted_ptr<const FontBase> &font,
                 uint32_t glyph_code);

    /*!
     * Destroy a Glyph that is NOT in a \ref GlyphCache,
     * i.e. cache() returns a nullptr. On success the underlying
     * data of the passed Glyph is no longer valid and the
     * Glyph value passed should be discarded (i.e. like a freed
     * pointer).
     * \param G Glyph to delete
     */
    static
    enum return_code
    delete_glyph(Glyph G);

    /*!
     * Given an index into an array of \ref GlyphAttribute values,
     * as used by pack_raw() or returned by \ref attributes()),
     * return the point to member variable of which attribute is
     * written to from the glyph attribute value and what component
     * of it.
     * \param glyph_attribute_index into an array of \ref GlyphAttribute
     *                              values, for example the array returned
     *                              by \ref attributes().
     * \param out_attribute location to which to write the pointer
     *                      member of \ref PainterAttribute that
     *                      the glyph attribute value is written to
     *                      in pack_raw() and/or pack_glyph().
     * \param out_index_into_attribute location to which to write the
     *                                 array-index element of the
     *                                 member of \ref PainterAttribute
     *                                 that the glyph attribute value
     *                                 is written to in pack_raw()
     *                                 and/or pack_glyph().
     */
    static
    void
    glyph_attribute_dst_write(int glyph_attribute_index,
                              PainterAttribute::pointer_to_field *out_attribute,
                              int *out_index_into_attribute);

    /*!
     * Provides information on the rendering cost of the Glyph,
     * entirely dependent on the \ref GlyphRenderData that generated
     * the data.
     */
    c_array<const GlyphRenderCostInfo>
    render_cost(void) const;

  private:
    friend class GlyphCache;

    explicit
    Glyph(void *p):
      m_opaque(p)
    {}

    void *m_opaque;
  };
/*! @} */
}
