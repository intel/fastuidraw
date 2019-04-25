/*!
 * \file painter_packer_data.hpp
 * \brief file painter_packer_data.hpp
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

#include <fastuidraw/painter/painter_data.hpp>
#include <fastuidraw/painter/backend/painter_brush_adjust.hpp>
#include <private/painter_backend/painter_packed_value_pool_private.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A PainterPackerData is the data parameters for drawing
   * commands of PainterPacker.
   */
  class PainterPackerData:public PainterData
  {
  public:
    /*!
     * Ctor. Intitializes all fields as default nothings.
     */
    PainterPackerData(void)
    {}

    /*!
     * Initializes those fiels coming from PainterData from
     * a PainterData value.
     * \param obj PainterData object from which to take value
     */
    explicit
    PainterPackerData(const PainterData &obj):
      PainterData(obj)
    {}

    /*!
     * value for the clip equations.
     */
    detail::PackedValuePool<fastuidraw::PainterClipEquations>::ElementHandle m_clip;

    /*!
     * value for the transformation matrix.
     */
    detail::PackedValuePool<fastuidraw::PainterItemMatrix>::ElementHandle m_matrix;

    /*!
     * value for the brush adjust
     */
    detail::PackedValuePool<fastuidraw::PainterBrushAdjust>::ElementHandle m_brush_adjust;
  };

/*! @} */

}
