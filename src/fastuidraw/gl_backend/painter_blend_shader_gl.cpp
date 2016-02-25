/*!
 * \file painter_blend_shader_gl.cpp
 * \brief file painter_blend_shader_gl.cpp
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


#include <fastuidraw/gl_backend/painter_blend_shader_gl.hpp>

namespace
{
  class PainterBlendShaderGLPrivate
  {
  public:
    PainterBlendShaderGLPrivate(const fastuidraw::gl::Shader::shader_source &src,
                                const fastuidraw::gl::BlendMode &mode):
      m_src(src),
      m_blend_mode(mode)
    {}

    fastuidraw::gl::Shader::shader_source m_src;
    fastuidraw::gl::BlendMode m_blend_mode;

  };
}

///////////////////////////////////////////////
// fastuidraw::gl::PainterBlendShaderGL methods
fastuidraw::gl::PainterBlendShaderGL::
PainterBlendShaderGL(const Shader::shader_source &src, const BlendMode &mode)
{
  m_d = FASTUIDRAWnew PainterBlendShaderGLPrivate(src, mode);
}

fastuidraw::gl::PainterBlendShaderGL::
~PainterBlendShaderGL(void)
{
  PainterBlendShaderGLPrivate *d;
  d = reinterpret_cast<PainterBlendShaderGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::gl::BlendMode&
fastuidraw::gl::PainterBlendShaderGL::
blend_mode(void) const
{
  PainterBlendShaderGLPrivate *d;
  d = reinterpret_cast<PainterBlendShaderGLPrivate*>(m_d);
  return d->m_blend_mode;
}

const fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::PainterBlendShaderGL::
src(void) const
{
  PainterBlendShaderGLPrivate *d;
  d = reinterpret_cast<PainterBlendShaderGLPrivate*>(m_d);
  return d->m_src;
}
