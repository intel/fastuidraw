/*!
 * \file glyph_render_data_distance_field.hpp
 * \brief file glyph_render_data_distance_field.hpp
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

#include <fastuidraw/text/glyph_render_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/

  /*!
    Represents a signed distance field of a glyph.
    This data can be rendered scaled, but the rendering
    distorts the corners of the glyph making them
    appeared rounded.
   */
  class GlyphRenderDataDistanceField:public GlyphRenderData
  {
  public:
    /*!
      Ctor, initialized the resolution as (0,0).
     */
    GlyphRenderDataDistanceField(void);
    ~GlyphRenderDataDistanceField(void);

    /*!
      Returns the resolution of the glyph with padding.
      The padding is to be 1 pixel wide on the bottom
      and on the right, i.e.
      GlyphAtlas::Padding::m_right = GlyphAtlas::Padding::m_bottom = 1
      and GlyphAtlas::Padding::m_left = GlyphAtlas::Padding::m_top = 0.
     */
    ivec2
    resolution(void) const;

    /*!
      Returns the distance values for rendering.
      The texel (x,y) is located at I where I is
      given by I = x + y * resolution().x(). Value
      is an 8-bit coverage value.
     */
    const_c_array<uint8_t>
    distance_values(void) const;

    /*!
      Returns the distance values for rendering.
      The texel (x,y) is located at I where I is
      given by I = x + y * resolution().x(). Value
      is an 8-bit coverage value.
     */
    c_array<uint8_t>
    distance_values(void);

    /*!
      Change the resolution
      \param sz new resolution
     */
    void
    resize(ivec2 sz);

    virtual
    enum fastuidraw::return_code
    upload_to_atlas(const GlyphAtlas::handle &atlas,
                    GlyphLocation &atlas_location,
                    GlyphLocation &secondary_atlas_location,
                    int &geometry_offset,
                    int &geometry_length) const;

  private:
    void *m_d;
  };
/*! @} */

} //namespace fastuidraw
