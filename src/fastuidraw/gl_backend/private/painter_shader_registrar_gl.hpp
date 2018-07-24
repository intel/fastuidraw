/*!
 * \file painter_shader_registrar_gl.hpp
 * \brief file painter_shader_registrar_gl.hpp
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

#pragma once

#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace fastuidraw { namespace gl { namespace detail {

class PainterShaderRegistrarGL:public glsl::PainterShaderRegistrarGLSL
{
public:
  enum
    {
      shader_group_discard_bit = 31u,
      shader_group_discard_mask = (1u << 31u)
    };

  explicit
  PainterShaderRegistrarGL(const PainterBackendGL::ConfigurationGL &P);

protected:
  uint32_t
  compute_blend_shader_group(PainterShader::Tag tag,
                             const reference_counted_ptr<PainterBlendShader> &shader);

  uint32_t
  compute_item_shader_group(PainterShader::Tag tag,
                            const reference_counted_ptr<PainterItemShader> &shader);

private:
  bool m_break_on_shader_change;
  bool m_separate_program_for_discard;
};

}}}
