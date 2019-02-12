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
#include <fastuidraw/painter/painter_attribute.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
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
     * Returne ths rendering size of the glpyh (in
     * font coordinates). This value is similair in
     * value to GlyphMetrics:size() but not necesaarily
     * idential (differnces come from discreitzation to
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
     * Pack a single glyph into attribute and index data. A
     * single glyph takes exactly 4 attributes and 6 indices.
     * The data is packed as follows:
     *   - PainterAttribute::m_attrib0 .xy -> position in item coordinates of the
     *                                        vertex of the quad to draw the glyph (float)
     *   - PainterAttribute::m_attrib0 .zw -> the difference in item coordinates
     *                                        between the bottom-left vertex position
     *                                        and the top-right vertex position.
     *   - PainterAttribute::m_attrib1 .x  -> Glyph::attribute()[0]
     *   - PainterAttribute::m_attrib1 .y  -> Glyph::attribute()[1]
     *   - PainterAttribute::m_attrib1. z  -> Glyph::attribute()[2]
     *   - PainterAttribute::m_attrib1 .w  -> Glyph::attribute()[3]
     *   - PainterAttribute::m_attrib2 .x  -> Glyph::attribute()[4]
     *   - PainterAttribute::m_attrib2 .y  -> Glyph::attribute()[5]
     *   - PainterAttribute::m_attrib2 .z  -> Glyph::attribute()[6]
     *   - PainterAttribute::m_attrib2 .w  -> Glyph::attribute()[7]
     * \param attrib_loc index into dst_attrib to which to write data
     * \param dst_attrib location to which to pack attributes
     * \param index_loc index into dst_index to which to write data
     * \param dst_index location to which to pack indices
     * \param position position of the bottom left corner of the glyph
     * \param scale_factor scale factor to apply to the glyph
     * \param orientation orientation of drawing the glyph
     * \param layout if glyph position is for horizontal or vertical layout
     */
    void
    pack_glyph(unsigned int attrib_loc, c_array<PainterAttribute> dst_attrib,
               unsigned int index_loc, c_array<PainterIndex> dst_index,
               const vec2 position, float scale_factor,
               enum fastuidraw::PainterEnums::screen_orientation orientation,
               enum fastuidraw::PainterEnums::glyph_layout_type layout) const;

    /*!
     * Pack a single glyph into attribute and index data. A
     * single glyph takes exactly 4 attributes and 6 indices.
     * The data is packed as follows:
     *   - PainterAttribute::m_attrib0 .xy -> position in item coordinates of the
     *                                        vertex of the quad to draw the glyph (float)
     *   - PainterAttribute::m_attrib0 .zw -> the difference in item coordinates
     *                                        between the bottom-left vertex position
     *                                        and the top-right vertex position.
     *   - PainterAttribute::m_attrib1 .x  -> Glyph::attribute()[0]
     *   - PainterAttribute::m_attrib1 .y  -> Glyph::attribute()[1]
     *   - PainterAttribute::m_attrib1. z  -> Glyph::attribute()[2]
     *   - PainterAttribute::m_attrib1 .w  -> Glyph::attribute()[3]
     *   - PainterAttribute::m_attrib2 .x  -> Glyph::attribute()[4]
     *   - PainterAttribute::m_attrib2 .y  -> Glyph::attribute()[5]
     *   - PainterAttribute::m_attrib2 .z  -> Glyph::attribute()[6]
     *   - PainterAttribute::m_attrib2 .w  -> Glyph::attribute()[7]
     * \param glyph_attributes glyph attribute data
     * \param attrib_loc index into dst_attrib to which to write data
     * \param dst_attrib location to which to pack attributes
     * \param index_loc index into dst_index to which to write data
     * \param dst_index location to which to pack indices
     * \param p_bl position of bottom left of glyph
     * \param p_tr position of top right of glyph
     */
    static
    void
    pack_raw(c_array<const GlyphAttribute> glyph_attributes,
             unsigned int attrib_loc, c_array<PainterAttribute> dst_attrib,
             unsigned int index_loc, c_array<PainterIndex> dst_index,
             const vec2 p_bl, const vec2 p_tr);

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
