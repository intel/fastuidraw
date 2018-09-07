/*!
 * \file font.hpp
 * \brief file font.hpp
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
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/text/font_properties.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include <fastuidraw/text/glyph_metrics.hpp>
#include <fastuidraw/text/glyph_metrics_value.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * FontBase provides an interface for a font
   * to generate glyph rendering data.
   */
  class FontBase:
    public reference_counted<FontBase>::default_base
  {
  public:
    /*!
     * Ctor.
     * \param pprops font properties describing the font
     */
    explicit
    FontBase(const FontProperties &pprops);

    virtual
    ~FontBase();

    /*!
     * Returns the properties of the font.
     */
    const FontProperties&
    properties(void) const
    {
      return m_props;
    }

    /*!
     * To be implemented by a derived class to return an
     * index value (glyph code) from a character code
     * (typically a unicode). A return value of 0 indicates
     * that the character code is not contained in the font.
     */
    virtual
    uint32_t
    glyph_code(uint32_t pcharacter_code) const = 0;

    /*!
     * To be implemented by a derived class to indicate
     * that it will return non-nullptr in
     * compute_rendering_data() when passed a
     * GlyphRender whose GlyphRender::m_type
     * is a specified value.
     */
    virtual
    bool
    can_create_rendering_data(enum glyph_type tp) const = 0;

    /*!
     * To be implemented by a derived class to provide the metrics
     * data for the named glyph.
     * \param glyph_code glyph code of glyph to compute the metric values
     * \param[out] metrics location to which to place the metric values for the glyph
     */
    virtual
    void
    compute_metrics(uint32_t glyph_code, GlyphMetricsValue &metrics) const = 0;

    /*!
     * To be implemented by a derived class to generate glyph
     * rendering data given a glyph code and GlyphRender.
     * \param render specifies object to return via GlyphRender::type(),
     *               it is guaranteed by the caller that can_create_rendering_data()
     *               returns true on render.type()
     * \param glyph_metrics GlyphMetrics values as computed by compute_metrics()
     * \param[out] path Path of the glyph
     */
    virtual
    GlyphRenderData*
    compute_rendering_data(GlyphRender render,
                           GlyphMetrics glyph_metrics, Path &path) const = 0;

  private:
    FontProperties m_props;
  };
/*! @} */
}
