/*!
 * \file painter_shader_registrar.cpp
 * \brief file painter_shader_registrar.cpp
 *
 * Copyright 2018 by Intel.
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


#include <fastuidraw/painter/backend/painter_shader_registrar.hpp>
#include "../../private/util_private.hpp"


////////////////////////////////////
// fastuidraw::PainterShaderRegistrar methods
fastuidraw::PainterShaderRegistrar::
PainterShaderRegistrar(void)
{
  m_d = FASTUIDRAWnew Mutex();
}

fastuidraw::PainterShaderRegistrar::
~PainterShaderRegistrar()
{
  Mutex *d;
  d = static_cast<Mutex*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::Mutex&
fastuidraw::PainterShaderRegistrar::
mutex(void)
{
  Mutex *d;
  d = static_cast<Mutex*>(m_d);
  return *d;
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const reference_counted_ptr<PainterItemShader> &shader)
{
  if (!shader || shader->registered_to() == this)
    {
      return;
    }
  FASTUIDRAWassert(shader->registered_to() == nullptr);
  if (shader->registered_to() == nullptr)
    {
      if (shader->parent())
        {
          register_shader(shader->parent().static_cast_ptr<PainterItemShader>());

          /* activate the guard AFTER calling register_shader(),
           * otherwise we would attempt to double-lock the mutex
           */
          Mutex::Guard m(mutex());
          shader->set_group_of_sub_shader(compute_item_sub_shader_group(shader));
        }
      else
        {
          Mutex::Guard m(mutex());
          PainterShader::Tag tag;

          tag = absorb_item_shader(shader);
          shader->register_shader(tag, this);
        }
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const reference_counted_ptr<PainterCompositeShader> &shader)
{
  /* TODO: check shader->type() against what composite types the
   * PainterShaderRegistrar will accept.
   */
  if (!shader || shader->registered_to() == this)
    {
      return;
    }

  FASTUIDRAWassert(shader->registered_to() == nullptr);
  if (shader->registered_to() == nullptr)
    {
      if (shader->parent())
        {
          register_shader(shader->parent().static_cast_ptr<PainterCompositeShader>());

          /* activate the guard AFTER calling register_shader(),
           * otherwise we would attempt to double-lock the mutex
           */
          Mutex::Guard m(mutex());
          shader->set_group_of_sub_shader(compute_composite_sub_shader_group(shader));
        }
      else
        {
          Mutex::Guard m(mutex());
          PainterShader::Tag tag;

          tag = absorb_composite_shader(shader);
          shader->register_shader(tag, this);
        }
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const reference_counted_ptr<PainterBlendShader> &shader)
{
  if (!shader || shader->registered_to() == this)
    {
      return;
    }

  FASTUIDRAWassert(shader->registered_to() == nullptr);
  if (shader->registered_to() == nullptr)
    {
      if (shader->parent())
        {
          register_shader(shader->parent().static_cast_ptr<PainterBlendShader>());

          /* activate the guard AFTER calling register_shader(),
           * otherwise we would attempt to double-lock the mutex
           */
          Mutex::Guard m(mutex());
          shader->set_group_of_sub_shader(compute_blend_sub_shader_group(shader));
        }
      else
        {
          Mutex::Guard m(mutex());
          PainterShader::Tag tag;

          tag = absorb_blend_shader(shader);
          shader->register_shader(tag, this);
        }
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterGlyphShader &shader)
{
  for(unsigned int i = 0, endi = shader.shader_count(); i < endi; ++i)
    {
      register_shader(shader.shader(static_cast<enum glyph_type>(i)));
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterCompositeShaderSet &p)
{
  for(unsigned int i = 0, endi = p.shader_count(); i < endi; ++i)
    {
      enum PainterEnums::composite_mode_t tp;
      tp = static_cast<enum PainterEnums::composite_mode_t>(i);

      const reference_counted_ptr<PainterCompositeShader> &sh(p.shader(tp));
      register_shader(sh);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterBlendShaderSet &p)
{
  for(unsigned int i = 0, endi = p.shader_count(); i < endi; ++i)
    {
      enum PainterEnums::blend_w3c_mode_t tp;
      tp = static_cast<enum PainterEnums::blend_w3c_mode_t>(i);

      const reference_counted_ptr<PainterBlendShader> &sh(p.shader(tp));
      register_shader(sh);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterShaderSet &shaders)
{
  register_shader(shaders.stroke_shader());
  register_shader(shaders.dashed_stroke_shader());
  register_shader(shaders.fill_shader());
  register_shader(shaders.glyph_shader());
  register_shader(shaders.composite_shaders());
  register_shader(shaders.blend_shaders());
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterStrokeShader &p)
{
  for (unsigned int tp = 0; tp < PainterStrokeShader::number_stroke_types; ++tp)
    {
      enum PainterStrokeShader::stroke_type_t e_tp;
      e_tp = static_cast<enum PainterStrokeShader::stroke_type_t>(tp);
      for (unsigned int sh = 0; sh < PainterStrokeShader::number_shader_types; ++sh)
        {
          enum PainterStrokeShader::shader_type_t e_sh;
          e_sh = static_cast<enum PainterStrokeShader::shader_type_t>(sh);
          register_shader(p.shader(e_tp, e_sh));
        }
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterFillShader &p)
{
  register_shader(p.item_shader());
  register_shader(p.aa_fuzz_shader());
  register_shader(p.aa_fuzz_hq_shader_pass1());
  register_shader(p.aa_fuzz_hq_shader_pass2());
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterDashedStrokeShaderSet &p)
{
  for(int i = 0; i < PainterEnums::number_cap_styles; ++i)
    {
      enum PainterEnums::cap_style c;
      c = static_cast<enum PainterEnums::cap_style>(i);
      register_shader(p.shader(c));
    }
}
