/*!
 * \file painter_item_matrix.cpp
 * \brief file painter_item_matrix.cpp
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

#include <fastuidraw/painter/packing/painter_item_matrix.hpp>

//////////////////////////////////////////////
// fastuidraw::PainterItemMatrix methods
void
fastuidraw::PainterItemMatrix::
pack_data(c_array<generic_data> dst) const
{
  dst[matrix00_offset].f = m_item_matrix(0, 0);
  dst[matrix01_offset].f = m_item_matrix(0, 1);
  dst[matrix02_offset].f = m_item_matrix(0, 2);

  dst[matrix10_offset].f = m_item_matrix(1, 0);
  dst[matrix11_offset].f = m_item_matrix(1, 1);
  dst[matrix12_offset].f = m_item_matrix(1, 2);

  dst[matrix20_offset].f = m_item_matrix(2, 0);
  dst[matrix21_offset].f = m_item_matrix(2, 1);
  dst[matrix22_offset].f = m_item_matrix(2, 2);
}
