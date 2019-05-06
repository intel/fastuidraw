/*!
 * \file painter_gradient_brush_shader.cpp
 * \brief file painter_gradient_brush_shader.cpp
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

#include <fastuidraw/painter/shader/painter_gradient_brush_shader.hpp>

/////////////////////////////////////////////////////
// fastuidraw::PainterGradientBrushShaderData methods
unsigned int
fastuidraw::PainterGradientBrushShaderData::
data_size(void) const
{
  switch (m_data.m_type)
    {
    case linear:
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(linear_data_size);
    case sweep:
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(sweep_data_size);
    case radial:
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(radial_data_size);
    default:
      return 0;
    }
}

void
fastuidraw::PainterGradientBrushShaderData::
pack_data(c_array<vecN<generic_data, 4> > dst) const
{
  if (!m_data.m_cs || m_data.m_type == non)
    {
      return;
    }

  uint32_t x, y;
  c_array<generic_data> sub_dest;

  sub_dest = dst.flatten_array();
  x = static_cast<uint32_t>(m_data.m_cs->texel_location().x());
  y = static_cast<uint32_t>(m_data.m_cs->texel_location().y());

  sub_dest[color_stop_xy_offset].u =
    pack_bits(color_stop_x_bit0, color_stop_x_num_bits, x)
    | pack_bits(color_stop_y_bit0, color_stop_y_num_bits, y);

  sub_dest[color_stop_length_offset].u = m_data.m_cs->width();
  sub_dest[p0_x_offset].f = m_data.m_grad_start.x();
  sub_dest[p0_y_offset].f = m_data.m_grad_start.y();
  sub_dest[p1_x_offset].f = m_data.m_grad_end.x();
  sub_dest[p1_y_offset].f = m_data.m_grad_end.y();

  if (m_data.m_type == radial)
    {
      sub_dest[start_radius_offset].f = m_data.m_grad_start_r;
      sub_dest[end_radius_offset].f = m_data.m_grad_end_r;
    }
}

void
fastuidraw::PainterGradientBrushShaderData::
save_resources(c_array<reference_counted_ptr<const resource_base> > dst) const
{
  if (m_data.m_cs)
    {
      dst[0] = m_data.m_cs;
    }
}

unsigned int
fastuidraw::PainterGradientBrushShaderData::
number_resources(void) const
{
  return (m_data.m_cs) ? 1 : 0;
}
