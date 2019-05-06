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


#include <fastuidraw/painter/shader/painter_brush_shader_set.hpp>
#include <private/util_private.hpp>

namespace
{
  class PainterBrushShaderSetPrivate
  {
  public:
    fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader> m_standard_brush;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterImageBrushShader> m_image_brush;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterGradientBrushShader> m_gradient_brush;
  };
}

/////////////////////////////////////////////////
// fastuidraw::PainterBrushShaderSet methods
fastuidraw::PainterBrushShaderSet::
PainterBrushShaderSet(void)
{
  m_d = FASTUIDRAWnew PainterBrushShaderSetPrivate();
}

fastuidraw::PainterBrushShaderSet::
PainterBrushShaderSet(const PainterBrushShaderSet &obj)
{
  PainterBrushShaderSetPrivate *d;
  d = static_cast<PainterBrushShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterBrushShaderSetPrivate(*d);
}

fastuidraw::PainterBrushShaderSet::
~PainterBrushShaderSet()
{
  PainterBrushShaderSetPrivate *d;
  d = static_cast<PainterBrushShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterBrushShaderSet)
setget_implement(fastuidraw::PainterBrushShaderSet, PainterBrushShaderSetPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterBrushShader>&,
                 standard_brush)
setget_implement(fastuidraw::PainterBrushShaderSet, PainterBrushShaderSetPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterImageBrushShader>&,
                 image_brush)
setget_implement(fastuidraw::PainterBrushShaderSet, PainterBrushShaderSetPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterGradientBrushShader>&,
                 gradient_brush)
