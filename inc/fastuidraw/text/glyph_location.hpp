/*!
 * \file glyph_location.hpp
 * \brief file glyph_location.hpp
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

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * A GlyphLocation represents the location of a glyph
   * within a GlyphAtlas.
   */
  class GlyphLocation
  {
  public:
    GlyphLocation(void):
      m_opaque(nullptr)
    {}

    /*!
     * Returns true if and only if the GlyphLocation refers
     * to an actual location on a GlyphAtlas
     */
    bool
    valid(void) const
    {
      return m_opaque != nullptr;
    }

    /*!
     * If valid() returns true, returns the bottom left
     * corner of the location of the glyph on the texel
     * store on which it resides. If valid() returns false,
     * returns ivec2(-1, -1).
     */
    ivec2
    location(void) const;

    /*!
     * If valid() returns true, returns the layer of the
     * location glyph on the texel store on which it resides.
     * If valid() returns false, returns -1.
     */
    int
    layer(void) const;

    /*!
     * If valid() returns true, returns the size of the glyph
     * on the texel store on which it resides. If valid()
     * returns false, returns ivec2(-1, -1).
     */
    ivec2
    size(void) const;

  private:
    friend class GlyphAtlas;
    const void *m_opaque;
  };
/*! @} */
}
