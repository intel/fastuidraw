/*!
 * \file painter_stroke_shader.cpp
 * \brief file painter_stroke_shader.cpp
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

#include <fastuidraw/painter/painter_stroke_shader.hpp>

namespace
{

  class PainterStrokeShaderPrivate
  {
  public:
    PainterStrokeShaderPrivate(void):
      m_aa_type(fastuidraw::PainterStrokeShader::draws_solid_then_fuzz)
    {}

    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_aa_shader_pass1;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_aa_shader_pass2;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_non_aa_shader;
    enum fastuidraw::PainterStrokeShader::type_t m_aa_type;
  };
}

//////////////////////////////////////////
// fastuidraw::PainterStrokeShader methods
fastuidraw::PainterStrokeShader::
PainterStrokeShader(void)
{
  m_d = FASTUIDRAWnew PainterStrokeShaderPrivate();
}

fastuidraw::PainterStrokeShader::
PainterStrokeShader(const PainterStrokeShader &obj)
{
  PainterStrokeShaderPrivate *d;
  d = reinterpret_cast<PainterStrokeShaderPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterStrokeShaderPrivate(*d);
}

fastuidraw::PainterStrokeShader::
~PainterStrokeShader()
{
  PainterStrokeShaderPrivate *d;
  d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterStrokeShader&
fastuidraw::PainterStrokeShader::
operator=(const PainterStrokeShader &rhs)
{
  if(this != &rhs)
    {
      PainterStrokeShaderPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterStrokeShaderPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                                \
  fastuidraw::PainterStrokeShader&                                  \
  fastuidraw::PainterStrokeShader::                                 \
  name(type v)                                                      \
  {                                                                 \
    PainterStrokeShaderPrivate *d;                                  \
    d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);         \
    d->m_##name = v;                                                \
    return *this;                                                   \
  }                                                                 \
                                                                    \
  type                                                              \
  fastuidraw::PainterStrokeShader::                                 \
  name(void) const                                                  \
  {                                                                 \
    PainterStrokeShaderPrivate *d;                                  \
    d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);         \
    return d->m_##name;                                             \
  }

setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, aa_shader_pass1)
setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, aa_shader_pass2)
setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, non_aa_shader)
setget_implement(enum fastuidraw::PainterStrokeShader::type_t, aa_type);

#undef setget_implement
