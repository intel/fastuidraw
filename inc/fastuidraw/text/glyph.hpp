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
#include <fastuidraw/text/glyph_location.hpp>
#include <fastuidraw/text/glyph_attribute.hpp>
#include <fastuidraw/text/glyph_metrics.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>

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
    GlyphRender
    renderer(void) const;

    /*!
     * Returns the glyph's layout data, valid()
     * must return true. If not, debug builds FASTUIDRAWassert
     * and release builds crash.
     */
    GlyphMetrics
    metrics(void) const;

    /*!
     * Returns the glyph's locations within the
     * GlyphAtlas. The size in the glyph atlas can be
     * larger than the actual glyph rendering size,
     * for example to pad for texture filtering.
     * The return value of valid() must be true.
     * If not, debug builds FASTUIDRAWassert and
     * release builds crash.
     */
    c_array<const GlyphLocation>
    atlas_locations(void) const;

    /*!
     * Returns the glyph's per-corner attribute data.
     */
    c_array<const GlyphAttribute>
    attributes(void) const;

    /*!
     * Returns the glyph's geometry data location within a
     * GlyphAtlas, a negative value indicates that
     * the glyph has no gometry data. The return value
     * of valid() must be true. If not, debug builds
     * FASTUIDRAWassert and release builds crash.
     */
    int
    geometry_offset(void) const;

    /*!
     * Returns the GlyphCache on which the glyph
     * resides. The return value of valid() must be
     * true. If not, debug builds FASTUIDRAWassert and release
     * builds crash.
     */
    reference_counted_ptr<GlyphCache>
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
     * \ref PainterAttributeDataFillerGlyphs with
     * \ref PainterEnums::y_increases_downwards.
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
    create_glyph(GlyphRender render,
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
