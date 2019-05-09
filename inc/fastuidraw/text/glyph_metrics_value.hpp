/*!
 * \file glyph_metrics.hpp
 * \brief file glyph_metrics.hpp
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
/*!\addtogroup Glyph
 * @{
 */
  class FontBase;
  class GlyphCache;
  class Glyph;

  /*!
   * \brief
   * A GlyphMetricsValue is to be used by a \ref FontBase
   * derived object to specify the values of a \ref
   * GlyphMetrics object.
   */
  class GlyphMetricsValue
  {
  public:
    /*!
     * Set the value returned by GlyphMetrics::horizontal_layout_offset().
     * Default value is (0, 0).
     */
    GlyphMetricsValue&
    horizontal_layout_offset(vec2);

    /*!
     * Set the value returned by GlyphMetrics::vertical_layout_offset().
     * Default value is (0, 0).
     */
    GlyphMetricsValue&
    vertical_layout_offset(vec2);

    /*!
     * Set the value returned by GlyphMetrics::size().
     * Default value is (0, 0).
     */
    GlyphMetricsValue&
    size(vec2);

    /*!
     * Set the value returned by GlyphMetrics::advance().
     * Default value is (0, 0).
     */
    GlyphMetricsValue&
    advance(vec2);

    /*!
     * Set the value returned by GlyphMetrics::units_per_EM().
     * Default value is 0.
     */
    GlyphMetricsValue&
    units_per_EM(float);

  private:
    friend class GlyphCache;
    friend class Glyph;

    GlyphMetricsValue(void *d):
      m_d(d)
    {}

    void *m_d;
  };
/*! @} */
}
