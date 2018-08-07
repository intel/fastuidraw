/*!
 * \file painter_composite_shader_glsl.cpp
 * \brief file painter_composite_shader_glsl.cpp
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

#include <fastuidraw/glsl/painter_composite_shader_glsl.hpp>

namespace
{
  class PainterCompositeShaderGLSLPrivate
  {
  public:
    explicit
    PainterCompositeShaderGLSLPrivate(const fastuidraw::glsl::ShaderSource &src):
      m_src(src)
    {}

    fastuidraw::glsl::ShaderSource m_src;
  };
}


///////////////////////////////////////////////
// fastuidraw::glsl::PainterCompositeShaderGLSL methods
fastuidraw::glsl::PainterCompositeShaderGLSL::
PainterCompositeShaderGLSL(enum shader_type tp,
                       const ShaderSource &src,
                       unsigned int num_sub_shaders):
  PainterCompositeShader(tp, num_sub_shaders)
{
  m_d = FASTUIDRAWnew PainterCompositeShaderGLSLPrivate(src);
}

fastuidraw::glsl::PainterCompositeShaderGLSL::
~PainterCompositeShaderGLSL(void)
{
  PainterCompositeShaderGLSLPrivate *d;
  d = static_cast<PainterCompositeShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

const fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::PainterCompositeShaderGLSL::
composite_src(void) const
{
  PainterCompositeShaderGLSLPrivate *d;
  d = static_cast<PainterCompositeShaderGLSLPrivate*>(m_d);
  return d->m_src;
}
