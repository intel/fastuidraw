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

#include <fastuidraw/painter/backend/painter_item_matrix.hpp>

//////////////////////////////////////////////
// fastuidraw::PainterItemMatrix methods
void
fastuidraw::PainterItemMatrix::
pack_data(c_array<uvec4> pdst) const
{
  c_array<uint32_t> dst(pdst.flatten_array());

  dst[matrix_row0_col0_offset] = pack_float(m_item_matrix(0, 0));
  dst[matrix_row0_col1_offset] = pack_float(m_item_matrix(0, 1));
  dst[matrix_row0_col2_offset] = pack_float(m_item_matrix(0, 2));

  dst[matrix_row1_col0_offset] = pack_float(m_item_matrix(1, 0));
  dst[matrix_row1_col1_offset] = pack_float(m_item_matrix(1, 1));
  dst[matrix_row1_col2_offset] = pack_float(m_item_matrix(1, 2));

  dst[matrix_row2_col0_offset] = pack_float(m_item_matrix(2, 0));
  dst[matrix_row2_col1_offset] = pack_float(m_item_matrix(2, 1));
  dst[matrix_row2_col2_offset] = pack_float(m_item_matrix(2, 2));

  dst[normalized_translate_x] = pack_float(m_normalized_translate.x());
  dst[normalized_translate_y] = pack_float(m_normalized_translate.y());
}
