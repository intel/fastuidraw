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
           enum image_filter image_filter,
           enum mipmap_t mip_mapping)
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
