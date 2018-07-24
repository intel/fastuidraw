/*!
 * \file painter_shader_registrar_gl.cpp
 * \brief file painter_shader_registrar_gl.cpp
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

#include "painter_shader_registrar_gl.hpp"

//////////////////////////////////////
// fastuidraw::gl::detail::PainterShaderRegistrarGL methods
fastuidraw::gl::detail::PainterShaderRegistrarGL::
PainterShaderRegistrarGL(const PainterBackendGL::ConfigurationGL &P):
  fastuidraw::glsl::PainterShaderRegistrarGLSL(),
  m_break_on_shader_change(P.break_on_shader_change()),
  m_separate_program_for_discard(P.separate_program_for_discard())
{}

uint32_t
fastuidraw::gl::detail::PainterShaderRegistrarGL::
compute_blend_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterBlendShader> &shader)
{
  FASTUIDRAWunused(shader);
  return m_break_on_shader_change ?
    tag.m_ID:
    0u;
}

uint32_t
fastuidraw::gl::detail::PainterShaderRegistrarGL::
compute_item_shader_group(PainterShader::Tag tag,
                          const reference_counted_ptr<PainterItemShader> &shader)
{
  uint32_t return_value;

  return_value = m_break_on_shader_change ? tag.m_ID : 0u;
  return_value |= (shader_group_discard_mask & tag.m_group);

  if (m_separate_program_for_discard)
    {
      const glsl::PainterItemShaderGLSL *sh;
      sh = dynamic_cast<const glsl::PainterItemShaderGLSL*>(shader.get());
      if (sh && sh->uses_discard())
        {
          return_value |= shader_group_discard_mask;
        }
    }
  return return_value;
}
