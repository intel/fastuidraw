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
#include "../private/util_private.hpp"

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
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_arc_aa_shader_pass1;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_arc_aa_shader_pass2;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_arc_non_aa_shader;
    enum fastuidraw::PainterStrokeShader::type_t m_aa_type;
    fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase> m_stroking_data_selector;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_aa_action_pass1;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_aa_action_pass2;
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

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&,
                 aa_shader_pass1)
setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&,
                 aa_shader_pass2)
setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&,
                 non_aa_shader)

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&,
                 arc_aa_shader_pass1)
setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&,
                 arc_aa_shader_pass2)
setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&,
                 arc_non_aa_shader)

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 enum fastuidraw::PainterStrokeShader::type_t, aa_type);

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::StrokingDataSelectorBase>&,
                 stroking_data_selector);

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 aa_action_pass1);

setget_implement(fastuidraw::PainterStrokeShader, PainterStrokeShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 aa_action_pass2);
