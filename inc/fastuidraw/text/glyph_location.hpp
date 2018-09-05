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
  class Glyph;
  class GlyphCache;

  /*!
   * \brief
   * A GlyphLocation represents the location of a glyph
   * within a GlyphAtlas.
   */
  class GlyphLocation
  {
  public:
    /*!
     * \brief
     * Represents an opaque array of \ref GlyphLocation
     * values.
     */
    class Array:noncopyable
    {
    public:
      /*!
       * Return the size of Array
       */
      unsigned int
      size(void) const;

      /*!
       * change the size of Array
       */
      void
      resize(unsigned int);

      /*!
       * Equivalent to resize(0).
       */
      void
      clear(void)
      {
        resize(0);
      }

      /*!
       * Returns a reference to an element of the Array
       */
      GlyphLocation&
      operator[](unsigned int);

      /*!
       * Returns a reference to an element of the Array
       */
      const GlyphLocation&
      operator[](unsigned int) const;

      /*!
       * Return the backing store of the Array; valid
       * until resize() is called.
       */
      c_array<const GlyphLocation>
      data(void) const;

      /*!
       * Return the backing store of the Array; valid
       * until resize() is called.
       */
      c_array<GlyphLocation>
      data(void);

    private:
      friend class fastuidraw::Glyph;
      friend class fastuidraw::GlyphCache;

      Array(void *d):
        m_d(d)
      {}

      void *m_d;
    };

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
