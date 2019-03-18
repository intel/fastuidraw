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

    fastuidraw::vecN<shader_set, 2> m_shaders;
    enum fastuidraw::PainterEnums::hq_anti_alias_support_t m_hq_anti_alias_support;
    fastuidraw::vecN<enum fastuidraw::PainterEnums::shader_anti_alias_t, 2> m_fastest_anti_alias_mode;
    fastuidraw::vecN<bool, fastuidraw::PainterEnums::number_shader_anti_alias_enums> m_arc_stroking_is_fast;
    fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> m_stroking_data_selector;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_hq_aa_action_pass1;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_hq_aa_action_pass2;
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
shader(enum stroke_type_t tp, enum shader_type_t sh) const
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  return d->m_shaders[tp][sh];
}

fastuidraw::PainterStrokeShader&
fastuidraw::PainterStrokeShader::
shader(enum stroke_type_t tp, enum shader_type_t sh,
       const reference_counted_ptr<PainterItemShader> &v)
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  d->m_shaders[tp][sh] = v;
  return *this;
}

enum fastuidraw::PainterEnums::shader_anti_alias_t
fastuidraw::PainterStrokeShader::
fastest_anti_alias_mode(enum stroke_type_t tp) const
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  return d->m_fastest_anti_alias_mode[tp];
}

fastuidraw::PainterStrokeShader&
fastuidraw::PainterStrokeShader::
fastest_anti_alias_mode(enum stroke_type_t tp,
                        enum PainterEnums::shader_anti_alias_t sh)
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  d->m_fastest_anti_alias_mode[tp] = sh;
  return *this;
}

bool
fastuidraw::PainterStrokeShader::
arc_stroking_is_fast(enum PainterEnums::shader_anti_alias_t sh) const
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  return d->m_arc_stroking_is_fast[sh];
}

fastuidraw::PainterStrokeShader&
fastuidraw::PainterStrokeShader::
arc_stroking_is_fast(enum PainterEnums::shader_anti_alias_t sh, bool v)
{
  PainterStrokeShaderPrivate *d;
  d = static_cast<PainterStrokeShaderPrivate*>(m_d);
  d->m_arc_stroking_is_fast[sh] = v;
  return *this;
}

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 enum fastuidraw::PainterEnums::hq_anti_alias_support_t,
                 hq_anti_alias_support);

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>&,
                 stroking_data_selector);

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 hq_aa_action_pass1);

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 hq_aa_action_pass2);
