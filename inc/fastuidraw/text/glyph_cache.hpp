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
#include <fastuidraw/text/glyph_metrics.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/text/glyph_source.hpp>

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
     * An AllocationHandle represents a handle to data allocated
     * on the underlying GlyphAtlas of a GlyphCache. The handle
     * is to be used to deallocate from the GlyphAtlas. Note that
     * all data on the GlyphCache is deallocated when \ref
     * GlyphCache::clear_atlas() or \ref GlyphCache::clear_cache()
     * is called.
     */
    class AllocationHandle
    {
    public:
      AllocationHandle(void):
        m_location(0),
        m_size(0)
      {}

      /*!
       * Returns true if the AllocationHandle refers
       * to a successful allocation.
       */
      bool
      valid(void) const
      {
        return m_size > 0;
      }

      /*!
       * Returns the location within the GlyphAtlas of the
       * allocated data.
       */
      unsigned int
      location(void) const
      {
        return m_location;
      }

    private:
      friend class GlyphCache;
      unsigned int m_location, m_size;
    };

    /*!
     * Ctor
     * \param patlas GlyphAtlas to store glyph data
     */
    explicit
    GlyphCache(reference_counted_ptr<GlyphAtlas> patlas);

    ~GlyphCache();

    /*!
     * Fetch, and if necessay create and store, the metrics
     * of given a glyph code of a font.
     * \param font font from which to take the glyph
     * \param glyph_code glyph code
     */
    GlyphMetrics
    fetch_glyph_metrics(const FontBase *font, uint32_t glyph_code);

    /*!
     * Fetch, and if necessay create and store, the metrics
     * of given a set of glyph codes of a font.
     * \param font font from which to take the glyph
     * \param glyph_codes glyph codes to fetch
     * \param out_metrics location to which to write the Glyph
     */
    void
    fetch_glyph_metrics(const FontBase *font,
                        c_array<const uint32_t> glyph_codes,
                        c_array<GlyphMetrics> out_metrics);

    /*!
     * Fetch, and if necessay create and store, the metrics
     * of given a set of glyph codes of a font.
     * \param glyph_sources sequence of \ref GlyphSource values
     * \param out_metrics location to which to write the Glyph
     */
    void
    fetch_glyph_metrics(c_array<const GlyphSource> glyph_sources,
                        c_array<GlyphMetrics> out_metrics);

    /*!
     * Fetch, and if necessay create and store, a glyph given a
     * glyph code of a font and a GlyphRenderer specifying how
     * to render the glyph.
     * \param render renderer of fetched Glyph
     * \param font font from which to take the glyph
     * \param glyph_code glyph code
     * \param upload_to_atlas if true, upload to atlas
     */
    Glyph
    fetch_glyph(GlyphRenderer render, const FontBase *font,
                uint32_t glyph_code, bool upload_to_atlas = true);

    /*!
     * Fetch, and if necessay create and store, a sequence of
     * glyphs given a sequence of glyph codes of a font and a
     * GlyphRenderer specifying how to render the glyph.
     * \param render renderer of fetched Glyph
     * \param font font from which to take the glyph
     * \param glyph_codes sequence of glyph codes
     * \param[out] out_glyphs location to which to write the glyphs;
     *                        the size must be the same as glyph_codes
     * \param upload_to_atlas if true, upload glyphs to atlas
     */
    void
    fetch_glyphs(GlyphRenderer render, const FontBase *font,
                 c_array<const uint32_t> glyph_codes,
                 c_array<Glyph> out_glyphs,
                 bool upload_to_atlas = true);

    /*!
     * Fetch, and if necessay create and store, a sequence of
     * glyphs given a sequence of glyph codes of a font and a
     * GlyphRenderer specifying how to render the glyph.
     * \param render renderer of fetched Glyph
     * \param glyph_sources sequence of \ref GlyphSource values
     * \param[out] out_glyphs location to which to write the glyphs;
     *                        the size must be the same as glyph_codes
     * \param upload_to_atlas if true, upload glyphs to atlas
     */
    void
    fetch_glyphs(GlyphRenderer render,
                 c_array<const GlyphSource> glyph_sources,
                 c_array<Glyph> out_glyphs,
                 bool upload_to_atlas = true);

    /*!
     * Fetch, and if necessay create and store, a sequence of
     * glyphs given a sequence of \ref GlyphMetrics values and
     * a \ref GlyphRenderer specifying how to render the glyph.
     * \param render renderer of fetched Glyph
     * \param glyph_metrics sequence of \ref GlyphMetrics values
     * \param[out] out_glyphs location to which to write the glyphs;
     *                        the size must be the same as glyph_codes
     * \param upload_to_atlas if true, upload glyphs to atlas
     */
    void
    fetch_glyphs(GlyphRenderer render,
                 c_array<const GlyphMetrics> glyph_metrics,
                 c_array<Glyph> out_glyphs,
                 bool upload_to_atlas = true);

    /*!
     * Add a Glyph created with Glyph::create_glyph() to
     * this GlyphCache. Will fail if a Glyph with the
     * same glyph_code (GlyphMetrics::glyph_code()),
     * font (GlyphMetrics::font()) and renderer
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
     * will lose their backing store in the GlyphAtlas and will need
     * to be re-uploaded (see Glyph::upload_to_atlas()). The glyphs
     * however are NOT removed from this GlyphCache. Thus, the return
     * values of previous calls to create_glyph() are still valie, but
     * they need to be re-uploaded to the GlyphAtlas with
     * Glyph::upload_to_atlas().
     */
    void
    clear_atlas(void);

    /*!
     * Returns the number of times that this GlyphCache cleared
     * its GlyphAtlas (i.e. the number of times clear_atlas() or
     * clear_cache() have been called).
     */
    unsigned int
    number_times_atlas_cleared(void);

    /*!
     * Clear this GlyphCache and the GlyphAtlas backing the glyphs.
     * Thus all previous \ref Glyph and \ref GlyphMetrics values
     * returned are no longer valid. In addition, as a side-effect
     * of clearing all Glyph and GlyphMetrics values, all references
     * to \ref FontBase objects are also released.
     */
    void
    clear_cache(void);

    /*!
     * Allocate and set data in the GlyphAtlas of this GlyphCache.
     */
    AllocationHandle
    allocate_data(c_array<const generic_data> pdata);

    /*!
     * Deallocate data in the GlyphAtlas of this GlyphCache
     * previously allocated with allocate_data().
     */
    void
    deallocate_data(AllocationHandle h);

  private:
    void *m_d;
  };
/*! @} */
} //namespace fastuidraw
