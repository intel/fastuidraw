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
#include <fastuidraw/text/glyph_layout.hpp>
#include <fastuidraw/text/glyph.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/

  /*!
    A GlyphCache represents a cache of glyphs and manages the uploading
    of the data to a GlyphAtlas. Methods are NOT thread safe.

    Data is uploaded as follows:
    - CoverageGlyphData::m_coverage_values is uploaded directly to
      the texel backing store of the GlyphAtlas and the location
      is fetched from Glyph::atlas_location().

    - DistanceFieldGyphData::m_distance_values is uploaded directly to
      the texel backing store of the GlyphAtlas and the location
      is fetched from Glyph::atlas_location().
   */
  class GlyphCache:public reference_counted<GlyphCache>::default_base
  {
  public:
    /*!
      Ctor
      \param patlas GlyphAtlas to store glyph data
     */
    explicit
    GlyphCache(GlyphAtlas::handle patlas);

    ~GlyphCache();

    /*!
      Create and store a glyph into the GlyphCache
      given a glyph code of a font and a glyph_type.
     */
    Glyph
    fetch_glyph(GlyphRender render, FontBase::const_handle font,
                uint32_t glyph_code);

    /*!
      Removes a glyph from the -CACHE-, i.e. the GlyphCache,
      thus to use that glyph again requires calling create_glyph()
      (and thus a new value for Glyph).
     */
    void
    delete_glyph(Glyph);

    /*!
      Call to clear the backing GlyphAtlas. In doing so, the glyphs
      will no longer be uploaded to the GlyphAtlas and will need
      to be re-uploaded (see Glyph::upload_to_atlas()). The glyphs
      however are NOT removed from this GlyphCache. Thus, the return
      values of previous calls to create_glyph() are still valie, but
      they need to be re-uploaded to the GlyphAtlas with
      Glyph::upload_to_atlas().
     */
    void
    clear_atlas(void);

    /*!
      Clear this GlyphCache and the GlyphAtlas. Essentially NUKE.
     */
    void
    clear_cache(void);

  private:
    void *m_d;
  };
/*! @} */
} //namespace fastuidraw
