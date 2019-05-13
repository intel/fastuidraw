/*!
 * \file painter_brush_adjust.hpp
 * \brief file painter_brush_adjust.hpp
 *
 * Copyright 2019 by Intel.
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A PainterBrushAdjust holds the value for applying a tranformation
   * to the brush position before it is handed off to brush shading.
   * The transformation is shearing composited with a translation,
   * i.e. the transformation is given by
   * \code
   * out_value = m_shear * in_value + m_translate
   * \endcode
   */
  class PainterBrushAdjust
  {
  public:
    /*!
     * \brief
     * Enumeration that provides offsets for the values
     * in a \ref PainterBrushAdjust in units of uint32_t.
     */
    enum data_offset_t
      {
        shear_x_offset, /*!< offset of m_shear.x() (packed as float) */
        shear_y_offset, /*!< offset of m_shear.y() (packed as float) */
        translation_x_offset, /*!< offset of m_translation.x() (packed as float) */
        translation_y_offset, /*!< offset of m_translation.y() (packed as float) */
        raw_data_size, /*!< size of the raw data before padding*/
      };

    /*!
     * Ctor, initializes the adjustment as identity
     */
    explicit
    PainterBrushAdjust(void):
      m_shear(1.0f, 1.0f),
      m_translate(0.0f, 0.0f)
    {}

    /*!
     * Returns the length of the data needed to encode the data.
     * Data is padded to be multiple of 4.
     */
    unsigned int
    data_size(void) const
    {
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(raw_data_size);
    }

    /*!
     * Pack the values of this PainterItemMatrix
     * \param dst place to which to pack data
     */
    void
    pack_data(c_array<uvec4> dst) const;

    /*!
     * The shearing to apply to the brush before the
     * translation.
     */
    vec2 m_shear;

    /*!
     * The translation to apply to the brush after
     * applying the shearing.
     */
    vec2 m_translate;
  };

/*! @} */
}
