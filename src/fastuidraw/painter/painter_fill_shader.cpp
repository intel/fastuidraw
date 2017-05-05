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

#include <utility>
#include <fastuidraw/painter/painter_fill_shader.hpp>

namespace
{
  class PainterFillShaderPrivate
  {
  public:
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_item_shader;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_aa_fuzz_shader;
  };
}

//////////////////////////////////////////
// fastuidraw::PainterFillShader methods
fastuidraw::PainterFillShader::
PainterFillShader(void)
{
  m_d = FASTUIDRAWnew PainterFillShaderPrivate();
}

fastuidraw::PainterFillShader::
PainterFillShader(const PainterFillShader &obj)
{
  PainterFillShaderPrivate *d;
  d = static_cast<PainterFillShaderPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterFillShaderPrivate(*d);
}

fastuidraw::PainterFillShader::
~PainterFillShader()
{
  PainterFillShaderPrivate *d;
  d = static_cast<PainterFillShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::PainterFillShader::
swap(PainterFillShader &obj)
{
  std::swap(obj.m_d, m_d);
}

fastuidraw::PainterFillShader&
fastuidraw::PainterFillShader::
operator=(const PainterFillShader &rhs)
{
  if(this != &rhs)
    {
      PainterFillShader v(rhs);
      swap(v);
    }
  return *this;
}

#define setget_implement(type, name)                                \
  fastuidraw::PainterFillShader&                                    \
  fastuidraw::PainterFillShader::                                   \
  name(type v)                                                      \
  {                                                                 \
    PainterFillShaderPrivate *d;                                    \
    d = static_cast<PainterFillShaderPrivate*>(m_d);                \
    d->m_##name = v;                                                \
    return *this;                                                   \
  }                                                                 \
                                                                    \
  type                                                              \
  fastuidraw::PainterFillShader::                                   \
  name(void) const                                                  \
  {                                                                 \
    PainterFillShaderPrivate *d;                                    \
    d = static_cast<PainterFillShaderPrivate*>(m_d);                \
    return d->m_##name;                                             \
  }

setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, item_shader)
setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, aa_fuzz_shader)
#undef setget_implement
