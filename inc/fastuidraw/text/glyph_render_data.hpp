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
#include <fastuidraw/text/glyph_renderer.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * A GlyphRenderInfo provides rendering const information
   * of a \ref Glyph.
   */
  class GlyphRenderCostInfo
  {
  public:
    GlyphRenderCostInfo(void):
      m_label(""),
      m_value(0.0f)
    {}

    /*!
     * Label of the value; the string is NOT owned by the
     * GlyphRenderInfo (it is expected that the c_string
     * is build-time constant string).
     */
    c_string m_label;

    /*!
     * Value of the information element.
     */
    float m_value;
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
    ~GlyphRenderData()
    {}

    /*!
     * To be implemented by a derived class to return
     * the strings used in \ref GlyphRenderCostInfo::m_value;
     * The pointer behind each of the string in the return
     * \ref c_array returned are required to stay valid even
     * after the GlyphRenderData is deleted. The expectation
     * is that the returned \ref c_array is just c_array
     * wrapping over a static constant array of string.
     */
    virtual
    c_array<const c_string>
    render_info_labels(void) const = 0;

    /*!
     * To be implemented by a derived class to upload data to a
     * GlyphAtlas.
     * \param atlas_proxy GlyphAtlasProxy to which to upload data
     * \param attributes (output) glyph attributes (see Glyph::attributes())
     * \param render_costs (output) an array of size render_info().size()
     *                     to which to write the render costs for each
     *                     entry in render_info().
     */
    virtual
    enum fastuidraw::return_code
    upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                    GlyphAttribute::Array &attributes,
                    c_array<float> render_costs) const = 0;

  };
/*! @} */
}
