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


#include <fastuidraw/painter/shader/painter_shader_set.hpp>
#include <private/util_private.hpp>

namespace
{
  class PainterShaderSetPrivate
  {
  public:
    fastuidraw::PainterGlyphShader m_glyph_shader;
    fastuidraw::PainterStrokeShader m_stroke_shader;
    fastuidraw::PainterDashedStrokeShaderSet m_dashed_stroke_shader;
    fastuidraw::PainterFillShader m_fill_shader;
    fastuidraw::PainterBlendShaderSet m_blend_shaders;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader> m_brush_shader;
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
  d = static_cast<PainterShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterShaderSetPrivate(*d);
}

fastuidraw::PainterShaderSet::
~PainterShaderSet()
{
  PainterShaderSetPrivate *d;
  d = static_cast<PainterShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterShaderSet)
setget_implement(fastuidraw::PainterShaderSet, PainterShaderSetPrivate,
                 const fastuidraw::PainterGlyphShader&, glyph_shader)

setget_implement(fastuidraw::PainterShaderSet, PainterShaderSetPrivate,
                 const fastuidraw::PainterStrokeShader&, stroke_shader)

setget_implement(fastuidraw::PainterShaderSet, PainterShaderSetPrivate,
                 const fastuidraw::PainterDashedStrokeShaderSet&, dashed_stroke_shader)

setget_implement(fastuidraw::PainterShaderSet, PainterShaderSetPrivate,
                 const fastuidraw::PainterFillShader&, fill_shader)

setget_implement(fastuidraw::PainterShaderSet, PainterShaderSetPrivate,
                 const fastuidraw::PainterBlendShaderSet&, blend_shaders)

setget_implement(fastuidraw::PainterShaderSet, PainterShaderSetPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&,
                 brush_shader)
