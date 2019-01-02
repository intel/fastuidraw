/*!
 * \file painter_header.cpp
 * \brief file painter_header.cpp
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

#include <fastuidraw/painter/backend/painter_header.hpp>

void
fastuidraw::PainterHeader::
pack_data(c_array<generic_data> dst) const
{
  dst[clip_equations_location_offset].u        = m_clip_equations_location;
  dst[item_matrix_location_offset].u           = m_item_matrix_location;
  dst[brush_shader_data_location_offset].u     = m_brush_shader_data_location;
  dst[item_shader_data_location_offset].u      = m_item_shader_data_location;
  dst[composite_shader_data_location_offset].u = m_composite_shader_data_location;
  dst[blend_shader_data_location_offset].u     = m_blend_shader_data_location;
  dst[item_shader_offset].u                    = m_item_shader;
  dst[brush_shader_offset].u                   = m_brush_shader;
  dst[composite_shader_offset].u               = m_composite_shader;
  dst[blend_shader_offset].u                   = m_blend_shader;
  dst[z_offset].i                              = m_z;
  dst[flags_offset].u                          = m_flags;
}
