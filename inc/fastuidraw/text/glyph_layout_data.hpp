/*!
 * \file glyph_layout_data.hpp
 * \brief file glyph_layout_data.hpp
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
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */
  class FontBase;
  class GlyphCache;

  /*!
   * \brief
   * A GlyphLayoutData provides information on how to
   * layout text using a glyph, all the values are in
   * units of the font glyph. The field \ref m_units_per_EM
   * gives the conversion factor to pixel coordinates via
   * \f$PixelCoordinates = FontCoordinates * PixelSize / m_units_per_EM\f$
   * where PixelSize is the pixel size in which one is
   * to render the text.
   */
  class GlyphLayoutData
  {
  public:
    /*!
     * Ctor.
     */
    GlyphLayoutData(void);

    /*!
     * Copy ctor.
       * \param obj value from which to copy
     */
    GlyphLayoutData(const GlyphLayoutData &obj);

    ~GlyphLayoutData();

    /*!
     * Assignment operator.
     * \param obj value from which to copy
     */
    GlyphLayoutData&
    operator=(const GlyphLayoutData &obj);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(GlyphLayoutData &obj);

    /*!
     * The index of the glyph into the -font- of the glyph
     */
    uint32_t
    glyph_code(void) const;

    /*!
     * Set the value returned by glyph_code(void) const.
     * Default value is 0.
     */
    GlyphLayoutData&
    glyph_code(uint32_t);

    /*!
     * Font of the glyph
     */
    const reference_counted_ptr<const FontBase>&
    font(void) const;

    /*!
     * Set the value returned by font(void) const.
     * Default value is nullptr.
     */
    GlyphLayoutData&
    font(const reference_counted_ptr<const FontBase>&);

    /*!
     * The offset (in font coordinates) from the pen
     * at which to display the glyph when performing
     * horizontal text layout.
     */
    vec2
    horizontal_layout_offset(void) const;

    /*!
     * Set the value returned by horizontal_layout_offset(void) const.
     * Default value is (0, 0).
     */
    GlyphLayoutData&
    horizontal_layout_offset(vec2);

    /*!
     * The offset (in font coordinates) from the pen
     * at which to display the glyph when performing
     * horizontal text layout.
     */
    vec2
    vertical_layout_offset(void) const;

    /*!
     * Set the value returned by vertical_layout_offset(void) const.
     * Default value is (0, 0).
     */
    GlyphLayoutData&
    vertical_layout_offset(vec2);

    /*!
     * Size (in font coordinates) at which to draw
     * the glyph.
     */
    vec2
    size(void) const;

    /*!
     * Set the value returned by size(void) const.
     * Default value is (0, 0).
     */
    GlyphLayoutData&
    size(vec2);

    /*!
     * How much (in font coordinates) to advance the pen
     * after drawing the glyph. The x-coordinate holds the
     * advance when performing layout horizontally and
     * the y-coordinate when performing layout vertically.
     */
    vec2
    advance(void) const;

    /*!
     * Set the value returned by advance(void) const.
     * Default value is (0, 0).
     */
    GlyphLayoutData&
    advance(vec2);

    /*!
     * The number of font units per EM for the glyph.
     * The conversion from font coordinates to pixel
     * coordiantes is given by:
     * \f$PixelCoordinates = FontCoordinates * PixelSize / units_per_EM\f$
     */
    float
    units_per_EM(void) const;

    /*!
     * Set the value returned by units_per_EM(void) const.
     * Default value is 0.
     */
    GlyphLayoutData&
    units_per_EM(float);

  private:
    void *m_d;
  };
/*! @} */
}
