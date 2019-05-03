/*!
 * \file painter_image_brush_shader.cpp
 * \brief file painter_image_brush_shader.cpp
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/painter/shader/painter_image_brush_shader.hpp>

namespace
{
  class PainterImageBrushShaderPrivate
  {
  public:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader> shader_ref;

    shader_ref m_parent_shader;
    fastuidraw::vecN<shader_ref, fastuidraw::PainterImageBrushShader::number_sub_shaders> m_sub_shaders;
  };
}

/////////////////////////////////////////////
// fastuidraw::PainterImageBrushShader methods
fastuidraw::PainterImageBrushShader::
PainterImageBrushShader(const reference_counted_ptr<PainterBrushShader> &parent_shader)
{
  PainterImageBrushShaderPrivate *d;

  m_d = d = FASTUIDRAWnew PainterImageBrushShaderPrivate();
  d->m_parent_shader = parent_shader;

  for (unsigned int i = 0; i < number_sub_shaders; ++i)
    {
      d->m_sub_shaders[i] = FASTUIDRAWnew PainterBrushShader(parent_shader, i);
    }
}

fastuidraw::PainterImageBrushShader::
~PainterImageBrushShader()
{
  PainterImageBrushShaderPrivate *d;
  d = static_cast<PainterImageBrushShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&
fastuidraw::PainterImageBrushShader::
sub_shader(const Image *image,
           enum filter_t image_filter,
           enum mipmap_t mip_mapping) const
{
  PainterImageBrushShaderPrivate *d;
  d = static_cast<PainterImageBrushShaderPrivate*>(m_d);

  uint32_t sub_shader_id(0);
  if (image)
    {
      uint32_t filter_bits, type_bits, mip_bits, format_bits;

      filter_bits = pack_bits(filter_bit0, filter_num_bits, image_filter);
      mip_bits = pack_bits(mipmap_bit0, mipmap_num_bits,
                           (mip_mapping == apply_mipmapping) ?
                           image->number_mipmap_levels() : 0u);
      type_bits = pack_bits(type_bit0, type_num_bits, image->type());
      format_bits =  pack_bits(format_bit0, format_num_bits, image->format());
      sub_shader_id = filter_bits | mip_bits | type_bits | format_bits;
    }
  return d->m_sub_shaders[sub_shader_id];
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader> >
fastuidraw::PainterImageBrushShader::
sub_shaders(void) const
{
  PainterImageBrushShaderPrivate *d;
  d = static_cast<PainterImageBrushShaderPrivate*>(m_d);
  return d->m_sub_shaders;
}

fastuidraw::PainterCustomBrush
fastuidraw::PainterImageBrushShader::
create_brush(PainterPackedValuePool &pool,
             const reference_counted_ptr<const Image> &image,
             uvec2 xy, uvec2 wh,
             enum filter_t image_filter,
             enum mipmap_t mip_mapping) const
{
  PainterData::value<PainterBrushShaderData> packed_data;
  PainterImageBrushShaderData data;

  data.sub_image(image, xy, wh);
  packed_data = pool.create_packed_value(data);
  return PainterCustomBrush(packed_data, sub_shader(image.get(), image_filter, mip_mapping).get());
}

fastuidraw::PainterCustomBrush
fastuidraw::PainterImageBrushShader::
create_brush(PainterPackedValuePool &pool,
             const reference_counted_ptr<const Image> &image,
             enum filter_t image_filter,
             enum mipmap_t mip_mapping) const
{
  PainterData::value<PainterBrushShaderData> packed_data;
  PainterImageBrushShaderData data;

  data.image(image);
  packed_data = pool.create_packed_value(data);
  return PainterCustomBrush(packed_data, sub_shader(image.get(), image_filter, mip_mapping).get());
}

////////////////////////////////////////////////
// fastuidraw::PainterImageBrushShaderData methods
fastuidraw::PainterImageBrushShaderData::
PainterImageBrushShaderData(void)
{
  reference_counted_ptr<const Image> null_image;
  image(null_image);
}

void
fastuidraw::PainterImageBrushShaderData::
pack_data(c_array<generic_data> dst) const
{
  std::copy(m_packed_data.begin(), m_packed_data.end(), dst.begin());
}

unsigned int
fastuidraw::PainterImageBrushShaderData::
data_size(void) const
{
  return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(shader_data_size);
}

void
fastuidraw::PainterImageBrushShaderData::
save_resources(c_array<reference_counted_ptr<const resource_base> > dst) const
{
  if (m_image)
    {
      dst[0] = m_image;
    }
}

unsigned int
fastuidraw::PainterImageBrushShaderData::
number_resources(void) const
{
  return (m_image) ? 1 : 0;
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::Image> >
fastuidraw::PainterImageBrushShaderData::
bind_images(void) const
{
  unsigned int sz;
  const reference_counted_ptr<const Image> *im;
  sz = (m_image && m_image->type() == Image::context_texture2d) ? 1 : 0;
  im = (sz != 0) ? &m_image : nullptr;
  return c_array<const reference_counted_ptr<const Image> >(im, sz);
}

void
fastuidraw::PainterImageBrushShaderData::
image(const reference_counted_ptr<const Image> &im)
{
  sub_image(im, uvec2(0, 0),
            (im) ? uvec2(im->dimensions()) : uvec2(0, 0));
}

void
fastuidraw::PainterImageBrushShaderData::
sub_image(const reference_counted_ptr<const Image> &im,
          uvec2 xy, uvec2 wh)
{
  generic_data zero;

  zero.u = 0u;
  std::fill(m_packed_data.begin(), m_packed_data.end(), zero);
  m_image = im;

  if (m_image)
    {
      m_packed_data[size_xy_offset].u =
        pack_bits(size_x_bit0, size_x_num_bits, xy.x())
        | pack_bits(size_y_bit0, size_y_num_bits, xy.y());

      m_packed_data[start_xy_offset].u =
        pack_bits(size_x_bit0, size_x_num_bits, wh.x())
        | pack_bits(size_y_bit0, size_y_num_bits, wh.y());

      switch (m_image->type())
        {
        case Image::on_atlas:
          {
            uvec3 loc(m_image->master_index_tile());
            uint32_t lookups(m_image->number_index_lookups());

            m_packed_data[atlas_location_xyz_offset].u =
              pack_bits(atlas_location_x_bit0, atlas_location_x_num_bits, loc.x())
              | pack_bits(atlas_location_y_bit0, atlas_location_y_num_bits, loc.y())
              | pack_bits(atlas_location_z_bit0, atlas_location_z_num_bits, loc.z());
            m_packed_data[number_lookups_offset].u = lookups;
          }
          break;

        case Image::bindless_texture2d:
          {
            uint64_t v, hi, low;

            v = m_image->bindless_handle();
            hi = uint64_unpack_bits(32, 32, v);
            low = uint64_unpack_bits(0, 32, v);
            m_packed_data[bindless_handle_hi_offset].u = hi;
            m_packed_data[bindless_handle_low_offset].u = low;
          }
          break;

        default:
          break;
        }

    }
}
