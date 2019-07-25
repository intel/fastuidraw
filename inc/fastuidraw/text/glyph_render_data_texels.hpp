/*!
 * \file glyph_render_data_texels.hpp
 * \brief file glyph_render_data_texels.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef FASTUIDRAW_GLYPH_RENDER_DATA_TEXELS_HPP
#define FASTUIDRAW_GLYPH_RENDER_DATA_TEXELS_HPP

#include <fastuidraw/text/glyph_render_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

  /*!
   * \brief
   * A GlyphRenderDataTexels holds texel data for
   * rendering a glyph. Such texel data can be for
   * both distance field and coverage glyph rendeding.
   */
  class GlyphRenderDataTexels:public GlyphRenderData
  {
  public:
    /*!
     * This enumeration describes the meaning of the
     * attributes.
     */
    enum attribute_values_t
      {
        /*!
         * Dimensions of the glyph as packed by
         * \ref GlyphAttribute::rect_glyph_layout
         */
        glyph_size_xy,

        /*!
         * Location of the texel data within the
         * \ref GlyphAtlas
         */
        glyph_texel_data_offset,
      };

    /*!
     * Ctor, initialized the resolution as (0,0).
     */
    GlyphRenderDataTexels(void);

    ~GlyphRenderDataTexels();

    /*!
     * Returns the resolution of the glyph.
     */
    ivec2
    resolution(void) const;

    /*!
     * Returns the texel data for rendering.
     * The texel (x,y) is located at I where I is
     * given by I = x + y * resolution().x(). Value
     * is an 8-bit value.
     */
    c_array<const uint8_t>
    texel_data(void) const;

    /*!
     * Returns the coverage values for rendering.
     * The texel (x,y) is located at I where I is
     * given by I = x + y * resolution().x(). Value
     * is an 8-bit value.
     */
    c_array<uint8_t>
    texel_data(void);

    /*!
     * Change the resolution
     * \param sz new resolution
     */
    void
    resize(ivec2 sz);

    virtual
    c_array<const c_string>
    render_info_labels(void) const;

    virtual
    enum fastuidraw::return_code
    upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                    GlyphAttribute::Array &attributes,
                    c_array<float> render_costs) const;

  private:
    void *m_d;
  };
/*! @} */
}

#endif
