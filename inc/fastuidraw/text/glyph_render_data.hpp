/*!
 * \file glyph_render_data.hpp
 * \brief file glyph_render_data.hpp
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

#include <stdint.h>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/text/glyph_attribute.hpp>
#include <fastuidraw/text/glyph_atlas_proxy.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * Provides an enumeration of the rendering data for
   * a glyph.
   */
  enum glyph_type
    {
      /*!
       * Glyph is a coverage glyph. Glyph is not
       * scalable.
       */
      coverage_glyph,

      /*!
       * Glyph is a distance field glyph. Glyph
       * is scalable.
       */
      distance_field_glyph,

      /*!
       * Glyph is a Restricted rays glyph, generated
       * from a GlyphRenderDataRestrictedRays. Glyph
       * is scalable.
       */
      restricted_rays_glyph,

      /*!
       * Tag to indicate invalid glyph type; the value is much
       * larger than the last glyph type to allow for later ABI
       * compatibility as more glyph types are added. Value is
       * also to indicate to Painter::draw_glyphs() to draw glyphs
       * adaptively (i.e. choose a renderer based from the size
       * of the rendered glyphs).
       */
      invalid_glyph = 0x1000,

      /*!
       * Tag to indicate invalid glyph type; the value is much
       * larger than the last glyph type to allow for later ABI
       * compatibility as more glyph types are added. Value is
       * also to indicate to Painter::draw_glyphs() to draw glyphs
       * adaptively (i.e. choose a renderer based from the size
       * of the rendered glyphs).
       */
      adaptive_rendering = invalid_glyph,
    };

  /*!
   * \brief
   * Specifies how to render a glyph
   */
  class GlyphRender
  {
  public:
    /*!
     * Ctor. Initializes m_type as coverage_glyph
     * \param pixel_size value to which to initialize m_pixel_size
     */
    explicit
    GlyphRender(int pixel_size);

    /*!
     * Ctor.
     * \param t value to which to initialize \ref m_type, value must be so
     *          that scalable() returns true
     */
    explicit
    GlyphRender(enum glyph_type t);

    /*!
     * Ctor. Initializes \ref m_type to \ref invalid_glyph (which is the
     * same value as \ref adaptive_rendering).
     */
    GlyphRender(void);

    /*!
     * How to render glyph.
     */
    enum glyph_type m_type;

    /*!
     * Pixel size observed only if scalable()
     * when passed m_type returns false.
     */
    int m_pixel_size;

    /*!
     * Returns true if and only if the data for a glyph type
     * is scalable, for example \ref distance_field_glyph and
     * \ref restricted_rays_glyph are scalable
     */
    static
    bool
    scalable(enum glyph_type tp);

    /*!
     * Comparison operator.
     * \param rhs value to which to compare against
     */
    bool
    operator<(const GlyphRender &rhs) const;

    /*!
     * Comparison operator.
     * \param rhs value to which to compare against
     */
    bool
    operator==(const GlyphRender &rhs) const;

    /*!
     * Returns true if and only if this GlyphRender is valid
     * to specify how to render a glyph.
     */
    bool
    valid(void) const;
  };

  /*!
   * \brief
   * GlyphRenderData provides an interface to specify
   * data used for rendering glyphs and to pack that data
   * onto a GlyphAtlas.
   */
  class GlyphRenderData:noncopyable
  {
  public:
    virtual
    ~GlyphRenderData();

    /*!
     * To be implemented by a derived class to upload data to a
     * GlyphAtlas.
     * \param atlas_proxy GlyphAtlasProxy to which to upload data
     * \param attributes (output) glyph attributes (see Glyph::attributes())
     */
    virtual
    enum fastuidraw::return_code
    upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                    GlyphAttribute::Array &attributes) const = 0;

  };
/*! @} */
}
