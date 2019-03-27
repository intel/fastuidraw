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

#include <fastuidraw/painter/shader/painter_stroke_shader.hpp>
#include <private/util_private.hpp>

namespace
{
  class PainterStrokeShaderPrivate
  {
  public:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> shader;
    typedef fastuidraw::vecN<shader, fastuidraw::PainterStrokeShader::number_shader_types> shader_set;

    PainterStrokeShaderPrivate(void):
      m_fastest_anti_aliased_stroking_method(fastuidraw::PainterEnums::stroking_method_arc),
      m_fastest_non_anti_aliased_stroking_method(fastuidraw::PainterEnums::stroking_method_linear)
    {}

    fastuidraw::vecN<shader_set, fastuidraw::PainterEnums::stroking_method_number_precise_choices> m_shaders;
    fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> m_stroking_data_selector;

    enum fastuidraw::PainterEnums::stroking_method_t m_fastest_anti_aliased_stroking_method;
    enum fastuidraw::PainterEnums::stroking_method_t m_fastest_non_anti_aliased_stroking_method;
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
  d = static_cast<PainterStrokeShaderPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterStrokeShaderPrivate(*d);
}

fastuidraw::PainterStrokeShader::
~PainterStrokeShader()
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterStrokeShader)

const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&
fastuidraw::PainterStrokeShader::
shader(enum PainterEnums::stroking_method_t tp, enum shader_type_t sh) const
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  return d->m_shaders[tp][sh];
}

fastuidraw::PainterStrokeShader&
fastuidraw::PainterStrokeShader::
shader(enum PainterEnums::stroking_method_t tp, enum shader_type_t sh,
       const reference_counted_ptr<PainterItemShader> &v)
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  d->m_shaders[tp][sh] = v;
  return *this;
}

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 enum fastuidraw::PainterEnums::stroking_method_t,
                 fastest_anti_aliased_stroking_method)

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 enum fastuidraw::PainterEnums::stroking_method_t,
                 fastest_non_anti_aliased_stroking_method)

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>&,
                 stroking_data_selector)
