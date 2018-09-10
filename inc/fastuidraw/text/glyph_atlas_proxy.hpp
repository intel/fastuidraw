/*!
 * \file glyph_atlas_proxy.hpp
 * \brief file glyph_atlas_proxy.hpp
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


#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/text/glyph_atlas.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */
  class Glyph;
  class GlyphCache;

  /*!
   * !\brief
   * An \ref GlyphAtlasProxy is a proxy for a GlyphAtlas; one can allocate
   * through it both geometry data and texel data. Internally it tracks
   * all that was allocated with it.
   */
  class GlyphAtlasProxy:noncopyable
  {
  public:
    /*!
     * Conveniance typedef to GlyphAtlas::Padding
     */
    typedef GlyphAtlas::Padding Padding;

    /*!
     * Allocate a rectangular region. If allocation is not possible,
     * return a GlyphLocation where GlyphLocation::valid() is false.
     * \param size size of region to allocate
     * \param data data to which to set the region allocated
     * \param padding amount of padding the passed data has
     */
    GlyphLocation
    allocate(ivec2 size, c_array<const uint8_t> data, const Padding &padding);

    /*!
     * Negative return value indicates failure.
     */
    int
    allocate_geometry_data(c_array<const generic_data> pdata);

  private:
    friend class Glyph;
    friend class GlyphCache;

    GlyphAtlasProxy(void *d):
      m_d(d)
    {}

    void *m_d;
  };
/*! @} */
}
