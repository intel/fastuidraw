/*!
 * \file font_metrics.hpp
 * \brief file font_metrics.hpp
 *
 * Copyright 2019 by Intel.
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

#ifndef FASTUIDRAW_FONT_METRICS_HPP
#define FASTUIDRAW_FONT_METRICS_HPP

#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

  /*!
   * \brief
   * Represents various metric values global to an entire font.
   */
  class FontMetrics
  {
  public:
    /*!
     * Ctor. Initializes all fields as zero.
     */
    FontMetrics(void);

    /*!
     * Copy ctor.
     * \param obj vaue from which to copy
     */
    FontMetrics(const FontMetrics &obj);

    ~FontMetrics();

    /*!
     * assignment operator
     * \param obj value from which to assign
     */
    FontMetrics&
    operator=(const FontMetrics &obj);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(FontMetrics &obj);

    /*!
     * The recommended distance, in font units, between lines of text.
     * This is typically greater than the difference between \ref
     * ascender() and \ref descender().
     */
    float
    height(void) const;

    /*!
     * Set the value returned by height(void) const
     */
    FontMetrics&
    height(float);

    /*!
     * The typographical ascender value for the font in
     * font units; this is essentially the maximum across
     * all glyphs of the distance from the baseline to top
     * of the glyph.
     */
    float
    ascender(void) const;

    /*!
     * Set the value returned by ascender(void) const
     */
    FontMetrics&
    ascender(float);

    /*!
     * The typographical descender value for the font in
     * font units; this is essentially the minumum across
     * all glyphs of the signed distance from the baseline
     * to bottom of the glyph. A negative value indicates
     * below the baseline and a positive value above the
     * baseline.
     */
    float
    descender(void) const;

    /*!
     * Set the value returned by descender(void) const
     */
    FontMetrics&
    descender(float);

    /*!
     * The number of font units per EM for the glyph.
     * The conversion from font coordinates to pixel
     * coordiantes is given by:
     * \f$PixelCoordinates = FontCoordinates * PixelSize / units_per_EM\f$
     */
    float
    units_per_EM(void) const;

    /*!
     * Set the value returned by units_per_EM(void) const
     */
    FontMetrics&
    units_per_EM(float);

    /*!
     * Returns the strike-through position.
     */
    float
    strikeout_position(void) const;

    /*!
     * Set the value returned by strikeout_position(void) const
     */
    FontMetrics&
    strikeout_position(float);

    /*!
     * Returns the strike-through thickness.
     */
    float
    strikeout_thickness(void) const;

    /*!
     * Set the value returned by strikeout_thickness(void) const
     */
    FontMetrics&
    strikeout_thickness(float);

    /*!
     * Returns the underline position.
     */
    float
    underline_position(void) const;

    /*!
     * Set the value returned by underline_position(void) const
     */
    FontMetrics&
    underline_position(float);

    /*!
     * Returns the underline thickness.
     */
    float
    underline_thickness(void) const;

    /*!
     * Set the value returned by underline_thickness(void) const
     */
    FontMetrics&
    underline_thickness(float);

  private:
    void *m_d;
  };
/*! @} */
}

#endif
