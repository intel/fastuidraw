/*!
 * \file painter_shader_set.cpp
 * \brief file painter_shader_set.cpp
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


#include <fastuidraw/painter/painter_shader_set.hpp>

namespace
{
  class PainterShaderSetPrivate
  {
  public:
    fastuidraw::PainterGlyphShader m_glyph_shader, m_glyph_shader_anisotropic;
    fastuidraw::PainterStrokeShader m_stroke_shader;
    fastuidraw::PainterStrokeShader m_pixel_width_stroke_shader;
    fastuidraw::PainterDashedStrokeShaderSet m_dashed_stroke_shader;
    fastuidraw::PainterDashedStrokeShaderSet m_pixel_width_dashed_stroke_shader;
    fastuidraw::PainterFillShader m_fill_shader;
    fastuidraw::PainterBlendShaderSet m_blend_shaders;
  };
}

//////////////////////////////////////////////
// fastuidraw::PainterShaderSet methods
fastuidraw::PainterShaderSet::
PainterShaderSet(void)
{
  m_d = FASTUIDRAWnew PainterShaderSetPrivate();
}

fastuidraw::PainterShaderSet::
PainterShaderSet(const PainterShaderSet &obj)
{
  PainterShaderSetPrivate *d;
  d = reinterpret_cast<PainterShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterShaderSetPrivate(*d);
}

fastuidraw::PainterShaderSet::
~PainterShaderSet()
{
  PainterShaderSetPrivate *d;
  d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterShaderSet&
fastuidraw::PainterShaderSet::
operator=(const PainterShaderSet &rhs)
{
  if(this != &rhs)
    {
      PainterShaderSetPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterShaderSetPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                             \
  fastuidraw::PainterShaderSet&                                  \
  fastuidraw::PainterShaderSet::                                 \
  name(const type &v)                                            \
  {                                                              \
    PainterShaderSetPrivate *d;                                  \
    d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);         \
    d->m_##name = v;                                             \
    return *this;                                                \
  }                                                              \
                                                                 \
  const type&                                                    \
  fastuidraw::PainterShaderSet::                                 \
  name(void) const                                               \
  {                                                              \
    PainterShaderSetPrivate *d;                                  \
    d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);         \
    return d->m_##name;                                          \
  }

setget_implement(fastuidraw::PainterGlyphShader, glyph_shader)
setget_implement(fastuidraw::PainterGlyphShader, glyph_shader_anisotropic)
setget_implement(fastuidraw::PainterStrokeShader, stroke_shader)
setget_implement(fastuidraw::PainterStrokeShader, pixel_width_stroke_shader)
setget_implement(fastuidraw::PainterDashedStrokeShaderSet, dashed_stroke_shader)
setget_implement(fastuidraw::PainterDashedStrokeShaderSet, pixel_width_dashed_stroke_shader)
setget_implement(fastuidraw::PainterFillShader, fill_shader)
setget_implement(fastuidraw::PainterBlendShaderSet, blend_shaders)

#undef setget_implement
