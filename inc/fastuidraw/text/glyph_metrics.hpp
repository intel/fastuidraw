/*!
 * \file glyph_metrics.hpp
 * \brief file glyph_metrics.hpp
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


#ifndef FASTUIDRAW_GLYPH_METRICS_HPP
#define FASTUIDRAW_GLYPH_METRICS_HPP

#include <stdint.h>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */
  class FontBase;

  /*!
   * \brief
   * A GlyphMetrics provides information on the metrics
   * of a glyph, all the values are in units of the font
   * glyph. The function units_per_EM() provides the
   * conversion factor to pixel coordinates via
   * \f$PixelCoordinates = FontCoordinates * PixelSize / units_per_EM()\f$
   * where PixelSize is the pixel size in which one is
   * to render the text.
   */
  class GlyphMetrics
  {
  public:
    /*!
     * Ctor.
     */
    GlyphMetrics(void):
      m_d(nullptr)
    {}

    /*!
     * Returns true if the Glyph refers to actual
     * glyph data
     */
    bool
    valid(void) const
    {
      return m_d != nullptr;
    }

    /*!
     * The index of the glyph into the -font- of the glyph
     */
    uint32_t
    glyph_code(void) const;

    /*!
     * Font of the glyph
     */
    const reference_counted_ptr<const FontBase>&
    font(void) const;

    /*!
     * The offset (in font coordinates) from the pen
     * at which to display the glyph when performing
     * horizontal text layout.
     */
    vec2
    horizontal_layout_offset(void) const;

    /*!
     * The offset (in font coordinates) from the pen
     * at which to display the glyph when performing
     * horizontal text layout.
     */
    vec2
    vertical_layout_offset(void) const;

    /*!
     * Size (in font coordinates) at which to draw
     * the glyph.
     */
    vec2
    size(void) const;

    /*!
     * How much (in font coordinates) to advance the pen
     * after drawing the glyph. The x-coordinate holds the
     * advance when performing layout horizontally and
     * the y-coordinate when performing layout vertically.
     */
    vec2
    advance(void) const;

    /*!
     * The number of font units per EM for the glyph.
     * The conversion from font coordinates to pixel
     * coordiantes is given by:
     * \f$PixelCoordinates = FontCoordinates * PixelSize / units_per_EM\f$
     */
    float
    units_per_EM(void) const;

    /*!
     * If the glyph has a strikeout thickness, returns true
     * and sets the value of *v to the strikeout thickness.
     */
    bool
    strikeout_thickness(float *v) const;

    /*!
     * If the glyph has a strikeout position, returns true
     * and sets the value of *v to the strikeout position.
     */
    bool
    strikeout_position(float *v) const;

  private:
    friend class GlyphCache;
    friend class Glyph;

    explicit
    GlyphMetrics(void *p):
      m_d(p)
    {}

    void *m_d;
  };
/*! @} */
}

#endif
