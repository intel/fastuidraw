/*!
 * \file painter_dashed_stroke_shader_set.cpp
 * \brief file painter_dashed_stroke_shader_set.cpp
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_shader_set.hpp>

namespace
{

  class PainterDashedStrokeShaderSetPrivate
  {
  public:
    typedef fastuidraw::PainterStrokeShader PainterStrokeShader;
    enum { count = fastuidraw::PainterEnums::number_cap_styles };

    fastuidraw::vecN<PainterStrokeShader, count> m_shaders;
    fastuidraw::reference_counted_ptr<const fastuidraw::DashEvaluatorBase> m_dash_evaluator;
  };
}


///////////////////////////////////////////////////
// fastuidraw::PainterDashedStrokeShaderSet methods
fastuidraw::PainterDashedStrokeShaderSet::
PainterDashedStrokeShaderSet(void)
{
  m_d = FASTUIDRAWnew PainterDashedStrokeShaderSetPrivate();
}

fastuidraw::PainterDashedStrokeShaderSet::
PainterDashedStrokeShaderSet(const PainterDashedStrokeShaderSet &obj)
{
  PainterDashedStrokeShaderSetPrivate *d;
  d = static_cast<PainterDashedStrokeShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterDashedStrokeShaderSetPrivate(*d);
}

fastuidraw::PainterDashedStrokeShaderSet::
~PainterDashedStrokeShaderSet()
{
  PainterDashedStrokeShaderSetPrivate *d;
  d = static_cast<PainterDashedStrokeShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterDashedStrokeShaderSet&
fastuidraw::PainterDashedStrokeShaderSet::
operator=(const PainterDashedStrokeShaderSet &rhs)
{
  if(this != &rhs)
    {
      PainterDashedStrokeShaderSetPrivate *d, *rhs_d;
      d = static_cast<PainterDashedStrokeShaderSetPrivate*>(m_d);
      rhs_d = static_cast<PainterDashedStrokeShaderSetPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

const fastuidraw::PainterStrokeShader&
fastuidraw::PainterDashedStrokeShaderSet::
shader(enum PainterEnums::cap_style st) const
{
  PainterDashedStrokeShaderSetPrivate *d;
  d = static_cast<PainterDashedStrokeShaderSetPrivate*>(m_d);
  return d->m_shaders[st];
}

fastuidraw::PainterDashedStrokeShaderSet&
fastuidraw::PainterDashedStrokeShaderSet::
shader(enum PainterEnums::cap_style st,
       const PainterStrokeShader &sh)
{
  PainterDashedStrokeShaderSetPrivate *d;
  d = static_cast<PainterDashedStrokeShaderSetPrivate*>(m_d);
  d->m_shaders[st] = sh;
  return *this;
}

fastuidraw::PainterDashedStrokeShaderSet&
fastuidraw::PainterDashedStrokeShaderSet::
dash_evaluator(const reference_counted_ptr<const DashEvaluatorBase>& v)
{
  PainterDashedStrokeShaderSetPrivate *d;
  d = static_cast<PainterDashedStrokeShaderSetPrivate*>(m_d);
  d->m_dash_evaluator = v;
  return *this;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::DashEvaluatorBase>&
fastuidraw::PainterDashedStrokeShaderSet::
dash_evaluator(void) const
{
  PainterDashedStrokeShaderSetPrivate *d;
  d = static_cast<PainterDashedStrokeShaderSetPrivate*>(m_d);
  return d->m_dash_evaluator;
}
