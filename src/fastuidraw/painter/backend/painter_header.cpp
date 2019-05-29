/*!
 * \file painter_header.cpp
 * \brief file painter_header.cpp
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

#include <fastuidraw/painter/backend/painter_header.hpp>
#include <fastuidraw/util/util.hpp>

void
fastuidraw::PainterHeader::
pack_data(c_array<uvec4> pdst) const
{
  c_array<uint32_t> dst(pdst.flatten_array());

  dst[clip_equations_location_offset]        = m_clip_equations_location;
  dst[item_matrix_location_offset]           = m_item_matrix_location;
  dst[brush_shader_data_location_offset]     = m_brush_shader_data_location;
  dst[item_shader_data_location_offset]      = m_item_shader_data_location;
  dst[blend_shader_data_location_offset]     = m_blend_shader_data_location;
  dst[item_shader_offset]                    = m_item_shader;
  dst[brush_shader_offset]                   = m_brush_shader;
  dst[blend_shader_offset]                   = m_blend_shader;
  dst[z_offset]                              = static_cast<uint32_t>(m_z);
  dst[brush_adjust_location_offset]          = m_brush_adjust_location;
  dst[offset_to_deferred_coverage_x_offset]  = static_cast<uint32_t>(m_offset_to_deferred_coverage.x());
  dst[offset_to_deferred_coverage_y_offset]  = static_cast<uint32_t>(m_offset_to_deferred_coverage.y());
  dst[deferred_coverage_min_x_offset]        = static_cast<uint32_t>(m_deferred_coverage_min.x());
  dst[deferred_coverage_min_y_offset]        = static_cast<uint32_t>(m_deferred_coverage_min.y());
  dst[deferred_coverage_max_x_offset]        = static_cast<uint32_t>(m_deferred_coverage_max.x());
  dst[deferred_coverage_max_y_offset]        = static_cast<uint32_t>(m_deferred_coverage_max.y());
}
