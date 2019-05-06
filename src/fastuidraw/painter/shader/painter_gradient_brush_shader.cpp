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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/painter/shader/painter_gradient_brush_shader.hpp>
#include <fastuidraw/painter/painter_packed_value_pool.hpp>

namespace
{
  typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader> shader_ref;
  enum
    {
      number_spread_types = fastuidraw::PainterBrushEnums::number_spread_types,
      number_gradient_types = fastuidraw::PainterBrushEnums::number_gradient_types,

      number_generic_sub_shaders = number_spread_types * number_gradient_types
    };

  class PerGradientType
  {
  public:
    PerGradientType(shader_ref parent);

    fastuidraw::vecN<shader_ref, number_spread_types> m_sub_shader;
  };

  class GenericGradientType
  {
  public:
    GenericGradientType(const shader_ref &parent);

    fastuidraw::vecN<shader_ref, number_generic_sub_shaders> m_sub_shaders;
  };

  class PainterGradientBrushShaderPrivate
  {
  public:
    PainterGradientBrushShaderPrivate(const shader_ref &generic,
                                      const shader_ref &linear,
                                      const shader_ref &radial,
                                      const shader_ref &sweep,
                                      const shader_ref &white):
      m_generic(generic),
      m_linear(linear),
      m_radial(radial),
      m_sweep(sweep),
      m_white(white)
    {
    }

    GenericGradientType m_generic;
    PerGradientType m_linear, m_radial, m_sweep;
    shader_ref m_white;
  };
}

/////////////////////////////////////////
// PerGradientType methods
PerGradientType::
PerGradientType(shader_ref parent)
{
  for (unsigned int i = 0; i < number_spread_types; ++i)
    {
      m_sub_shader[i] = FASTUIDRAWnew fastuidraw::PainterBrushShader(parent, i);
    }
}

/////////////////////////////////////////
// GenericGradientType methods
GenericGradientType::
GenericGradientType(const shader_ref &parent)
{
  using namespace fastuidraw;

  for (unsigned int i = 0; i < number_spread_types; ++i)
    {
      for (unsigned int j = 0; j < number_gradient_types; ++j)
        {
          unsigned int sub_shader_id;
          enum PainterBrushEnums::spread_type_t spread_type;
          enum PainterBrushEnums::gradient_type_t gradient_type;

          spread_type = static_cast<enum PainterBrushEnums::spread_type_t>(i);
          gradient_type = static_cast<enum PainterBrushEnums::gradient_type_t>(j);
          sub_shader_id = PainterGradientBrushShader::sub_shader_id(spread_type, gradient_type);
          m_sub_shaders[sub_shader_id] = FASTUIDRAWnew PainterBrushShader(parent, sub_shader_id);
        }
    }
}

//////////////////////////////////////////////////////
// fastuidraw::PainterGradientBrushShader methods
fastuidraw::PainterGradientBrushShader::
PainterGradientBrushShader(const reference_counted_ptr<PainterBrushShader> &generic,
                           const reference_counted_ptr<PainterBrushShader> &linear,
                           const reference_counted_ptr<PainterBrushShader> &radial,
                           const reference_counted_ptr<PainterBrushShader> &sweep,
                           const reference_counted_ptr<PainterBrushShader> &white)
{
  m_d = FASTUIDRAWnew PainterGradientBrushShaderPrivate(generic, linear, radial, sweep, white);
}

fastuidraw::PainterGradientBrushShader::
~PainterGradientBrushShader()
{
  PainterGradientBrushShaderPrivate *d;
  d = static_cast<PainterGradientBrushShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&
fastuidraw::PainterGradientBrushShader::
sub_shader(enum spread_type_t sp, enum gradient_type_t gt) const
{
  PainterGradientBrushShaderPrivate *d;
  d = static_cast<PainterGradientBrushShaderPrivate*>(m_d);
  return d->m_generic.m_sub_shaders[sub_shader_id(sp, gt)];
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&
fastuidraw::PainterGradientBrushShader::
linear_sub_shader(enum spread_type_t sp) const
{
  PainterGradientBrushShaderPrivate *d;
  d = static_cast<PainterGradientBrushShaderPrivate*>(m_d);
  return d->m_linear.m_sub_shader[sub_shader_id(sp)];
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&
fastuidraw::PainterGradientBrushShader::
radial_sub_shader(enum spread_type_t sp) const
{
  PainterGradientBrushShaderPrivate *d;
  d = static_cast<PainterGradientBrushShaderPrivate*>(m_d);
  return d->m_radial.m_sub_shader[sub_shader_id(sp)];
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&
fastuidraw::PainterGradientBrushShader::
sweep_sub_shader(enum spread_type_t sp) const
{
  PainterGradientBrushShaderPrivate *d;
  d = static_cast<PainterGradientBrushShaderPrivate*>(m_d);
  return d->m_sweep.m_sub_shader[sub_shader_id(sp)];
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&
fastuidraw::PainterGradientBrushShader::
white_shader(void) const
{
  PainterGradientBrushShaderPrivate *d;
  d = static_cast<PainterGradientBrushShaderPrivate*>(m_d);
  return d->m_white;
}

fastuidraw::PainterCustomBrush
fastuidraw::PainterGradientBrushShader::
create_brush(PainterPackedValuePool &pool,
             const PainterGradientBrushShaderData &data,
             enum spread_type_t spread)
{
  PainterDataValue<PainterBrushShaderData> packed_data;

  packed_data = pool.create_packed_value(data);
  switch (data.type())
    {
    case gradient_linear:
      return PainterCustomBrush(linear_sub_shader(spread).get(), packed_data);

    case gradient_radial:
      return PainterCustomBrush(radial_sub_shader(spread).get(), packed_data);

    case gradient_sweep:
      return PainterCustomBrush(sweep_sub_shader(spread).get(), packed_data);

    default:
      return PainterCustomBrush(white_shader().get(), packed_data);
    }
}

uint32_t
fastuidraw::PainterGradientBrushShader::
sub_shader_id(enum spread_type_t sp, enum gradient_type_t gt)
{
  FASTUIDRAWassert(sp >= 0 && sp < number_spread_types);
  FASTUIDRAWassert(gt >= 0 && gt < number_gradient_types);
  return sp + number_spread_types * gt;
}

uint32_t
fastuidraw::PainterGradientBrushShader::
sub_shader_id(enum spread_type_t sp)
{
  return sp;
}

/////////////////////////////////////////////////////
// fastuidraw::PainterGradientBrushShaderData methods
unsigned int
fastuidraw::PainterGradientBrushShaderData::
data_size(void) const
{
  switch (m_data.m_type)
    {
    case gradient_linear:
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(linear_data_size);
    case gradient_sweep:
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(sweep_data_size);
    case gradient_radial:
      return FASTUIDRAW_NUMBER_BLOCK4_NEEDED(radial_data_size);
    default:
      return 0;
    }
}

void
fastuidraw::PainterGradientBrushShaderData::
pack_data(c_array<vecN<generic_data, 4> > dst) const
{
  if (!m_data.m_cs || m_data.m_type == gradient_non)
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

  if (m_data.m_type == gradient_radial)
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
