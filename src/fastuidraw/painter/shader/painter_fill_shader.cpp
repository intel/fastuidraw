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
#include <fastuidraw/painter/shader/painter_fill_shader.hpp>
#include <private/util_private.hpp>

namespace
{
  class PainterFillShaderPrivate
  {
  public:
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_item_shader;
    enum fastuidraw::PainterEnums::immediate_coverage_support_t m_immediate_coverage_support;
    enum fastuidraw::PainterEnums::shader_anti_alias_t m_fastest_anti_alias_mode;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_aa_fuzz_shader;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_aa_fuzz_hq_deferred_coverage;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_aa_fuzz_immediate_coverage_pass1;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_aa_fuzz_immediate_coverage_pass2;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_aa_fuzz_hq_action_pass1;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_aa_fuzz_hq_action_pass2;
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

assign_swap_implement(fastuidraw::PainterFillShader)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, item_shader)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 enum fastuidraw::PainterEnums::immediate_coverage_support_t,
                 immediate_coverage_support)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 enum fastuidraw::PainterEnums::shader_anti_alias_t,
                 fastest_anti_alias_mode);
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, aa_fuzz_shader)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, aa_fuzz_hq_deferred_coverage)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, aa_fuzz_immediate_coverage_pass1)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>&, aa_fuzz_immediate_coverage_pass2)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 aa_fuzz_hq_action_pass1)
setget_implement(fastuidraw::PainterFillShader, PainterFillShaderPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 aa_fuzz_hq_action_pass2)
