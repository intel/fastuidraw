/*!
 * \file painter_brush.cpp
 * \brief file painter_brush.cpp
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


#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_brush.hpp>

////////////////////////////////////
// fastuidraw::PainterBrush methods
unsigned int
fastuidraw::PainterBrush::
data_size(unsigned int alignment) const
{
  using namespace PainterPacking::Brush;
  unsigned int return_value(0);
  uint32_t pshader = shader();

  return_value += round_up_to_multiple(pen_data_size, alignment);

  if(pshader & image_mask)
    {
      return_value += round_up_to_multiple(image_data_size, alignment);
    }

  if(pshader & radial_gradient_mask)
    {
      assert(pshader & gradient_mask);
      return_value += round_up_to_multiple(radial_gradient_data_size, alignment);
    }
  else if(pshader & gradient_mask)
    {
      return_value += round_up_to_multiple(linear_gradient_data_size, alignment);
    }

  if(pshader & repeat_window_mask)
    {
      return_value += round_up_to_multiple(repeat_window_data_size, alignment);
    }

  if(pshader & transformation_translation_mask)
    {
      return_value += round_up_to_multiple(transformation_translation_data_size, alignment);
    }

  if(pshader & transformation_matrix_mask)
    {
      return_value += round_up_to_multiple(transformation_matrix_data_size, alignment);
    }

  return return_value;
}

void
fastuidraw::PainterBrush::
pack_data(unsigned int alignment, c_array<generic_data> dst) const
{
  using namespace PainterPacking::Brush;

  unsigned int current(0);
  unsigned int sz;
  c_array<generic_data> sub_dest;
  uint32_t pshader = shader();

  {
    sz = round_up_to_multiple(pen_data_size, alignment);
    sub_dest = dst.sub_array(current, sz);
    current += sz;

    sub_dest[pen_red_offset].f = m_data.m_pen.x();
    sub_dest[pen_green_offset].f = m_data.m_pen.y();
    sub_dest[pen_blue_offset].f = m_data.m_pen.z();
    sub_dest[pen_alpha_offset].f = m_data.m_pen.w();
  }

  if(pshader & image_mask)
    {
      sz = round_up_to_multiple(image_data_size, alignment);
      sub_dest = dst.sub_array(current, sz);
      current += sz;

      assert(m_data.m_image);
      uvec3 loc(m_data.m_image->master_index_tile());

      sub_dest[image_atlas_location_xyz_offset].u =
        pack_bits(image_atlas_location_x_bit0, image_atlas_location_x_num_bits, loc.x())
        | pack_bits(image_atlas_location_y_bit0, image_atlas_location_y_num_bits, loc.y())
        | pack_bits(image_atlas_location_z_bit0, image_atlas_location_z_num_bits, loc.z());

      sub_dest[image_size_xy_offset].u =
        pack_bits(image_size_x_bit0, image_size_x_num_bits, m_data.m_image_size.x())
        | pack_bits(image_size_y_bit0, image_size_y_num_bits, m_data.m_image_size.y());

      sub_dest[image_start_xy_offset].u =
        pack_bits(image_size_x_bit0, image_size_x_num_bits, m_data.m_image_start.x())
        | pack_bits(image_size_y_bit0, image_size_y_num_bits, m_data.m_image_start.y());
    }

  if(pshader & gradient_mask)
    {
      if(pshader & radial_gradient_mask)
        {
          sz = round_up_to_multiple(radial_gradient_data_size, alignment);
        }
      else
        {
          sz = round_up_to_multiple(linear_gradient_data_size, alignment);
        }

      sub_dest = dst.sub_array(current, sz);
      current += sz;

      assert(m_data.m_cs);
      assert(m_data.m_cs->texel_location().x() >= 0);
      assert(m_data.m_cs->texel_location().y() >= 0);

      uint32_t x, y;
      x = static_cast<uint32_t>(m_data.m_cs->texel_location().x());
      y = static_cast<uint32_t>(m_data.m_cs->texel_location().y());

      sub_dest[gradient_color_stop_xy_offset].u =
        pack_bits(gradient_color_stop_x_bit0, gradient_color_stop_x_num_bits, x)
        | pack_bits(gradient_color_stop_y_bit0, gradient_color_stop_y_num_bits, y);

      sub_dest[gradient_color_stop_length_offset].u = m_data.m_cs->width();

      sub_dest[gradient_p0_x_offset].f = m_data.m_grad_start.x();
      sub_dest[gradient_p0_y_offset].f = m_data.m_grad_start.y();
      sub_dest[gradient_p1_x_offset].f = m_data.m_grad_end.x();
      sub_dest[gradient_p1_y_offset].f = m_data.m_grad_end.y();

      if(pshader & radial_gradient_mask)
        {
          sub_dest[gradient_start_radius_offset].f = m_data.m_grad_start_r;
          sub_dest[gradient_end_radius_offset].f = m_data.m_grad_end_r;
        }
    }

  if(pshader & repeat_window_mask)
    {
      sz = round_up_to_multiple(repeat_window_data_size, alignment);
      sub_dest = dst.sub_array(current, sz);
      current += sz;

      sub_dest[repeat_window_x_offset].f = m_data.m_window_position.x();
      sub_dest[repeat_window_y_offset].f = m_data.m_window_position.y();
      sub_dest[repeat_window_width_offset].f = m_data.m_window_size.x();
      sub_dest[repeat_window_height_offset].f = m_data.m_window_size.y();
    }

  if(pshader & transformation_matrix_mask)
    {
      sz = round_up_to_multiple(transformation_matrix_data_size, alignment);
      sub_dest = dst.sub_array(current, sz);
      current += sz;

      sub_dest[transformation_matrix_m00_offset].f = m_data.m_transformation_matrix(0, 0);
      sub_dest[transformation_matrix_m01_offset].f = m_data.m_transformation_matrix(0, 1);
      sub_dest[transformation_matrix_m10_offset].f = m_data.m_transformation_matrix(1, 0);
      sub_dest[transformation_matrix_m11_offset].f = m_data.m_transformation_matrix(1, 1);
    }

  if(pshader & transformation_translation_mask)
    {
      sz = round_up_to_multiple(transformation_translation_data_size, alignment);
      sub_dest = dst.sub_array(current, sz);
      current += sz;

      sub_dest[transformation_translation_x_offset].f = m_data.m_transformation_p.x();
      sub_dest[transformation_translation_y_offset].f = m_data.m_transformation_p.y();
    }

  assert(current == dst.size());
}

fastuidraw::PainterBrush&
fastuidraw::PainterBrush::
sub_image(const Image::const_handle im, uvec2 xy, uvec2 wh, enum image_filter f)
{
  uint32_t slack, lookups;
  uint32_t filter_bits;

  filter_bits = im ? f : 0;

  m_data.m_image = im;
  m_data.m_image_start = xy;
  m_data.m_image_size = wh;

  m_data.m_shader_raw &= ~(filter_bits << image_filter_bit0);
  m_data.m_shader_raw |= (filter_bits << image_filter_bit0);

  slack = im ? im->slack() : 0;
  assert(slack <= image_slack_max);
  m_data.m_shader_raw &= ~(image_slack_max << image_slack_bit0);
  m_data.m_shader_raw |= (slack << image_slack_bit0);

  lookups = im ? im->number_index_lookups() : 0;
  assert(lookups <= image_number_index_lookups_max);
  m_data.m_shader_raw &= ~(image_number_index_lookups_max << image_number_index_lookups_bit0);
  m_data.m_shader_raw |= (lookups << image_number_index_lookups_bit0);

  return *this;
}

fastuidraw::PainterBrush&
fastuidraw::PainterBrush::
image(const Image::const_handle im, enum image_filter f)
{
  uvec2 sz(0, 0);
  if(im)
    {
      sz = uvec2(im->dimensions());
    }
  return sub_image(im, uvec2(0,0), sz, f);
}

uint32_t
fastuidraw::PainterBrush::
shader(void) const
{
  uint32_t return_value;

  return_value = m_data.m_shader_raw;
  /* lacking an image or gradient means the brush does
     nothing and so all bits should be down.
   */
  if(!m_data.m_image && !m_data.m_cs)
    {
      return_value = 0;
    }
  return return_value;
}

void
fastuidraw::PainterBrush::
reset(void)
{
  pen(1.0, 1.0, 1.0, 1.0);
  m_data.m_shader_raw = 0u;
  m_data.m_image = NULL;
  m_data.m_cs = NULL;
}

fastuidraw::PainterBrush&
fastuidraw::PainterBrush::
operator=(const PainterBrush &rhs)
{
  m_data = rhs.m_data;
  return *this;
}

enum fastuidraw::PainterBrush::image_filter
fastuidraw::PainterBrush::
best_filter_for_image(const Image::const_handle &im)
{
  return im ?
    static_cast<enum image_filter>(std::min(im->slack() + 1,
                                            static_cast<unsigned int>(image_filter_cubic))) :
    image_filter_nearest;
}

bool
fastuidraw::PainterBrush::
filter_suitable_for_image(const Image::const_handle &im,
                          enum image_filter f)
{
  assert(f >= image_filter_nearest);
  assert(f <= image_filter_cubic);
  return im && im->slack() >= static_cast<unsigned int>(f) - 1;
}

int
fastuidraw::PainterBrush::
slack_requirement(enum image_filter f)
{
  assert(f >= image_filter_nearest);
  assert(f <= image_filter_cubic);
  return f - 1;
}
