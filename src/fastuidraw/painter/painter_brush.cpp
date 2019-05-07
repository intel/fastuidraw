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

#include <algorithm>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/backend/painter_header.hpp>

////////////////////////////////////
// fastuidraw::PainterBrush methods
unsigned int
fastuidraw::PainterBrush::
data_size(void) const
{
  unsigned int return_value(0);
  uint32_t pfeatures = features();

  return_value += FASTUIDRAW_NUMBER_BLOCK4_NEEDED(header_data_size);

  if (pfeatures & repeat_window_mask)
    {
      return_value += FASTUIDRAW_NUMBER_BLOCK4_NEEDED(repeat_window_data_size);
    }

  return_value += m_data.m_gradient.data_size();

  if (pfeatures & transformation_translation_mask)
    {
      return_value += FASTUIDRAW_NUMBER_BLOCK4_NEEDED(transformation_translation_data_size);
    }

  if (pfeatures & transformation_matrix_mask)
    {
      return_value += FASTUIDRAW_NUMBER_BLOCK4_NEEDED(transformation_matrix_data_size);
    }

  if (pfeatures & image_mask)
    {
      return_value += m_data.m_image.data_size();
    }

  return return_value;
}

void
fastuidraw::PainterBrush::
pack_data(c_array<vecN<generic_data, 4> > dst) const
{
  unsigned int current(0);
  unsigned int sz;
  c_array<generic_data> sub_dest;
  uint32_t pfeatures = features();

  sz = FASTUIDRAW_NUMBER_BLOCK4_NEEDED(header_data_size);
  sub_dest = dst.sub_array(current, sz).flatten_array();
  current += sz;
  sub_dest[features_offset].u = pfeatures;
  sub_dest[header_red_green_offset].u = pack_as_fp16(m_data.m_color.x(), m_data.m_color.y());
  sub_dest[header_blue_alpha_offset].u = pack_as_fp16(m_data.m_color.z(), m_data.m_color.w());

  if (pfeatures & repeat_window_mask)
    {
      sz = FASTUIDRAW_NUMBER_BLOCK4_NEEDED(repeat_window_data_size);
      sub_dest = dst.sub_array(current, sz).flatten_array();
      current += sz;

      sub_dest[repeat_window_x_offset].f = m_data.m_window_position.x();
      sub_dest[repeat_window_y_offset].f = m_data.m_window_position.y();
      sub_dest[repeat_window_width_offset].f = m_data.m_window_size.x();
      sub_dest[repeat_window_height_offset].f = m_data.m_window_size.y();
    }

  enum gradient_type_t tp(gradient_type());
  if (tp != gradient_non)
    {
      sz = m_data.m_gradient.data_size();
      m_data.m_gradient.pack_data(dst.sub_array(current, sz));
      current += sz;
    }

  if (pfeatures & transformation_matrix_mask)
    {
      sz = FASTUIDRAW_NUMBER_BLOCK4_NEEDED(transformation_matrix_data_size);
      sub_dest = dst.sub_array(current, sz).flatten_array();
      current += sz;

      sub_dest[transformation_matrix_row0_col0_offset].f = m_data.m_transformation_matrix(0, 0);
      sub_dest[transformation_matrix_row0_col1_offset].f = m_data.m_transformation_matrix(0, 1);
      sub_dest[transformation_matrix_row1_col0_offset].f = m_data.m_transformation_matrix(1, 0);
      sub_dest[transformation_matrix_row1_col1_offset].f = m_data.m_transformation_matrix(1, 1);
    }

  if (pfeatures & transformation_translation_mask)
    {
      sz = FASTUIDRAW_NUMBER_BLOCK4_NEEDED(transformation_translation_data_size);
      sub_dest = dst.sub_array(current, sz).flatten_array();
      current += sz;

      sub_dest[transformation_translation_x_offset].f = m_data.m_transformation_p.x();
      sub_dest[transformation_translation_y_offset].f = m_data.m_transformation_p.y();
    }

  if (pfeatures & image_mask)
    {
      sz = m_data.m_image.data_size();
      m_data.m_image.pack_data(dst.sub_array(current, sz));
      current += sz;
    }

  FASTUIDRAWassert(current == dst.size());
}

fastuidraw::PainterBrush&
fastuidraw::PainterBrush::
sub_image(const reference_counted_ptr<const Image> &im,
          uvec2 xy, uvec2 wh,
          enum filter_t f,
          enum mipmap_t mipmap_filtering)
{
  uint32_t image_bits;

  image_bits = PainterImageBrushShader::sub_shader_id(im.get(), f, mipmap_filtering);
  m_data.m_image.sub_image(im, xy, wh);

  m_data.m_features_raw &= ~image_mask;
  m_data.m_features_raw |= pack_bits(image_bit0, image_num_bits, image_bits);

  return *this;
}

fastuidraw::PainterBrush&
fastuidraw::PainterBrush::
image(const reference_counted_ptr<const Image> &im,
      enum filter_t f,
      enum mipmap_t mipmap_filtering)
{
  uvec2 sz(0, 0);
  if (im)
    {
      sz = uvec2(im->dimensions());
    }
  return sub_image(im, uvec2(0,0), sz, f, mipmap_filtering);
}

uint32_t
fastuidraw::PainterBrush::
features(void) const
{
  uint32_t return_value;

  FASTUIDRAWstatic_assert(number_feature_bits <= 32u);

  return_value = m_data.m_features_raw;
  if (!m_data.m_image.image() && !m_data.m_gradient.color_stops())
    {
      /* lacking an image or gradient means the brush does
       * nothing and so all bits should be down.
       */
      return_value = 0;
    }
  return return_value;
}

fastuidraw::PainterBrush&
fastuidraw::PainterBrush::
reset(void)
{
  color(1.0, 1.0, 1.0, 1.0);
  m_data.m_features_raw = 0u;
  m_data.m_image.image(nullptr);
  m_data.m_gradient.reset();
  m_data.m_transformation_p = vec2(0.0f, 0.0f);
  m_data.m_transformation_matrix = float2x2();
  return *this;
}
