/*!
 * \file painter_brush_adjust.cpp
 * \brief file painter_brush_adjust.cpp
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

#include <fastuidraw/painter/backend/painter_brush_adjust.hpp>

//////////////////////////////////////////////
// fastuidraw::PainterBrushAdjust methods
void
fastuidraw::PainterBrushAdjust::
pack_data(c_array<uvec4> pdst) const
{
  c_array<uint32_t> dst(pdst.flatten_array());

  dst[shear_x_offset] = pack_float(m_shear.x());
  dst[shear_y_offset] = pack_float(m_shear.y());
  dst[translation_x_offset] = pack_float(m_translate.x());
  dst[translation_y_offset] = pack_float(m_translate.y());
}
