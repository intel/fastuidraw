/*!
 * \file glyph_cache.hpp
 * \brief file glyph_cache.hpp
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
#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/glyph_layout_data.hpp>
#include <fastuidraw/text/glyph.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * A GlyphCache represents a cache of glyphs and manages the uploading
   * of the data to a GlyphAtlas. The methods of GlyphAtlas are thread
   * safe because it maintains an internal mutex lock for the durations
   * of its methods.
   */
  class GlyphCache:public reference_counted<GlyphCache>::default_base
  {
  public:
    /*!
     * Ctor
     * \param patlas GlyphAtlas to store glyph data
     */
    explicit
    GlyphCache(reference_counted_ptr<GlyphAtlas> patlas);

    ~GlyphCache();

    /*!
     * Fetch, and if necessay create and store, a glyph given a
     * glyph code of a font and a GlyphRender specifying how
     * to render the glyph.
     * \param render renderer of fetched Glyph
     * \param font font from which to take the glyph
     * \param glyph_code glyph code
     * \param upload_to_atlas if true, upload to atlas
     */
    Glyph
    fetch_glyph(GlyphRender render,
                const reference_counted_ptr<const FontBase> &font,
                uint32_t glyph_code, bool upload_to_atlas = true);

    /*!
     * Fetch, and if necessay create and store, a sequence of
     * glyphs given a sequence of glyph codes of a font and a
     * GlyphRender specifying how to render the glyph.
     * \param render renderer of fetched Glyph
     * \param font font from which to take the glyph
     * \param glyph_codes sequence of glyph codes
     * \param[out] out_glyphs location to which to write the glyphs;
     *                        the size must be the same as glyph_codes
     * \param upload_to_atlas if true, upload glyphs to atlas
     */
    void
    fetch_glyphs(GlyphRender render,
                 const reference_counted_ptr<const FontBase> &font,
                 c_array<const uint32_t> glyph_codes,
                 c_array<Glyph> out_glyphs,
                 bool upload_to_atlas = true);

    /*!
     * Add a Glyph created with Glyph::create_glyph() to
     * this GlyphCache. Will fail if a Glyph with the
     * same glyph_code (GlyphLayoutData::m_glyph_code),
     * font (GlyphLayoutData::m_font) and renderer
     * (Glyph::renderer()) is already present in the
     * GlyphCache.
     * \param glyph Glyph to add to cache
     * \param upload_to_atlas if true, upload to atlas
     */
    enum return_code
    add_glyph(Glyph glyph, bool upload_to_atlas = true);

    /*!
     * Deletes and removes a glyph from the GlyphCache,
     * thus to use that glyph again requires calling fetch_glyph()
     * (and thus fetching a new value for Glyph). The underlying
     * memory of the Glyph will be reused by a later glyph,
     * thus the Glyph value passed should be discarded.
     * \param glyph Glyph to delete and remove from cache
     */
    void
    delete_glyph(Glyph glyph);

    /*!
     * Call to clear the backing GlyphAtlas. In doing so, the glyphs
     * will no longer be uploaded to the GlyphAtlas and will need
     * to be re-uploaded (see Glyph::upload_to_atlas()). The glyphs
     * however are NOT removed from this GlyphCache. Thus, the return
     * values of previous calls to create_glyph() are still valie, but
     * they need to be re-uploaded to the GlyphAtlas with
     * Glyph::upload_to_atlas().
     */
    void
    clear_atlas(void);

    /*!
     * Clear this GlyphCache and the GlyphAtlas. Essentially NUKE.
     */
    void
    clear_cache(void);

  private:
    void *m_d;
  };
/*! @} */
} //namespace fastuidraw
