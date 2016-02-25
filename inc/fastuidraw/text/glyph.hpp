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
#include <fastuidraw/text/glyph_location.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/

  class GlyphCache;
  class GlyphLayoutData;
  class GlyphAtlas;

  /*!
    A Glyph is essentially an opaque pointer to
    data for rendering and performing layout of a
    glyph.
   */
  class Glyph
  {
  public:
    /*!
      Ctor. Initializes the Glyph to be invalid,
      i.e. essentially a NULL pointer
     */
    Glyph(void):
      m_opaque(NULL)
    {}

    /*!
      Returns true if the Glyph refers to actual
      glyph data
     */
    bool
    valid(void) const
    {
      return m_opaque != NULL;
    }

    /*!
      Returns the glyph's rendering type, valid()
      must return true. If not, debug builds assert
      and release builds crash.
     */
    enum glyph_type
    type(void) const;

    /*!
      Returns the glyph's layout data, valid()
      must return true. If not, debug builds assert
      and release builds crash.
     */
    const GlyphLayoutData&
    layout(void) const;

    /*!
      Returns the glyph's location within the a
      GlyphAtlas. The size in the glyph atlas can be
      larger than the actual glyph rendering size,
      for example to pad for texture filtering.
      The return value of valid() must be true.
      If not, debug builds assert and release builds
      crash.
     */
    GlyphLocation
    atlas_location(void) const;

    /*!
      Returns the location in the glyph atlas for the
      secondary glyph store. Some forms of glyph
      rendering data require a second store of texels.
     */
    GlyphLocation
    secondary_atlas_location(void) const;

    /*!
      Returns the glyph's geometry data location within a
      GlyphAtlas, a negative value indicates that
      the glyph has no gometry data. The return value
      of valid() must be true. If not, debug builds
      assert and release builds crash.
    */
    int
    geometry_offset(void) const;

    /*!
      Returns the GlyphCache on which the glyph
      resides. The return value of valid() must be
      true. If not, debug builds assert and release
      builds crash.
     */
    reference_counted_ptr<GlyphCache>
    cache(void) const;

    /*!
      Returns the index location into the GlyphCache
      of the glyph. The return value of valid() must be
      true. If not, debug builds assert and release
      builds crash.
    */
    unsigned int
    cache_location(void) const;

    /*!
      If returns \ref routine_fail, then the GlyphCache
      on which the glyph resides needs to be cleared
      first. If the glyph is already uploaded returns
      immediately with \ref routine_success.
     */
    enum return_code
    upload_to_atlas(void) const;

    /* How to use: when printing a bunch of glyphs do this:
        for(each glyph G)
          {
            enum return_code R;
            R = G.upload();
            if(R == routine_fail)
              {
                 send_render_commands_to_graphics_api();
                 G.cache()->clear_atlas();
                 R = G.upload();
                 assert(R == routine_success);
              }
            append_render_command(G);
          }

     */

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
