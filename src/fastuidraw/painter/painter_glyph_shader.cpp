/*!
 * \file painter_glyph_shader.cpp
 * \brief file painter_glyph_shader.cpp
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


#include <vector>
#include <fastuidraw/painter/painter_glyph_shader.hpp>
#include "../private/util_private.hpp"

namespace
{
  class PainterGlyphShaderPrivate
  {
  public:
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> > m_shaders;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_null;
  };
}

///////////////////////////////////////
// fastuidraw::PainterGlyphShader methods
fastuidraw::PainterGlyphShader::
PainterGlyphShader(void)
{
  m_d = FASTUIDRAWnew PainterGlyphShaderPrivate();
}

fastuidraw::PainterGlyphShader::
PainterGlyphShader(const PainterGlyphShader &obj)
{
  PainterGlyphShaderPrivate *d;
  d = static_cast<PainterGlyphShaderPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterGlyphShaderPrivate(*d);
}

fastuidraw::PainterGlyphShader::
~PainterGlyphShader()
{
  PainterGlyphShaderPrivate *d;
  d = static_cast<PainterGlyphShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterGlyphShader)

const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&
fastuidraw::PainterGlyphShader::
shader(enum glyph_type tp) const
{
  PainterGlyphShaderPrivate *d;
  d = static_cast<PainterGlyphShaderPrivate*>(m_d);
  return (tp < d->m_shaders.size()) ? d->m_shaders[tp] : d->m_null;
}


fastuidraw::PainterGlyphShader&
fastuidraw::PainterGlyphShader::
shader(enum glyph_type tp,
       const fastuidraw::reference_counted_ptr<PainterItemShader> &sh)
{
  PainterGlyphShaderPrivate *d;
  d = static_cast<PainterGlyphShaderPrivate*>(m_d);
  if (tp >= d->m_shaders.size())
    {
      d->m_shaders.resize(tp + 1);
    }
  d->m_shaders[tp] = sh;
  return *this;
}

unsigned int
fastuidraw::PainterGlyphShader::
shader_count(void) const
{
  PainterGlyphShaderPrivate *d;
  d = static_cast<PainterGlyphShaderPrivate*>(m_d);
  return d->m_shaders.size();
}
