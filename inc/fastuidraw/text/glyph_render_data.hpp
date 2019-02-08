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
       * Glyph is a Banded rays glyph, generated
       * from a GlyphRenderDataBandedRays. Glyph
       * is scalable.
       */
      banded_rays_glyph,

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
  class GlyphRenderer
  {
  public:
    /*!
     * Ctor. Initializes m_type as coverage_glyph
     * \param pixel_size value to which to initialize m_pixel_size
     */
    explicit
    GlyphRenderer(int pixel_size);

    /*!
     * Ctor.
     * \param t value to which to initialize \ref m_type, value must be so
     *          that scalable() returns true
     */
    explicit
    GlyphRenderer(enum glyph_type t);

    /*!
     * Ctor. Initializes \ref m_type to \ref invalid_glyph (which is the
     * same value as \ref adaptive_rendering).
     */
    GlyphRenderer(void);

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
    operator<(const GlyphRenderer &rhs) const;

    /*!
     * Comparison operator.
     * \param rhs value to which to compare against
     */
    bool
    operator==(const GlyphRenderer &rhs) const;

    /*!
     * Returns true if and only if this GlyphRenderer is valid
     * to specify how to render a glyph.
     */
    bool
    valid(void) const;
  };

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
    ~GlyphRenderData();

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
