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
    PainterBlendShaderGLPrivate(const fastuidraw::gl::SingleSourceBlenderShader &psingle_src_blender,
                                const fastuidraw::gl::DualSourceBlenderShader &pdual_src_blender,
                                const fastuidraw::gl::FramebufferFetchBlendShader &pfetch_blender):
      m_single_src_blender(psingle_src_blender),
      m_dual_src_blender(pdual_src_blender),
      m_fetch_blender(pfetch_blender)
    {}

    fastuidraw::gl::SingleSourceBlenderShader m_single_src_blender;
    fastuidraw::gl::DualSourceBlenderShader m_dual_src_blender;
    fastuidraw::gl::FramebufferFetchBlendShader m_fetch_blender;
  };
}

///////////////////////////////////////////////
// fastuidraw::gl::PainterBlendShaderGL methods
fastuidraw::gl::PainterBlendShaderGL::
PainterBlendShaderGL(const SingleSourceBlenderShader &psingle_src_blender,
                     const DualSourceBlenderShader &pdual_src_blender,
                     const FramebufferFetchBlendShader &pfetch_blender)
{
  m_d = FASTUIDRAWnew PainterBlendShaderGLPrivate(psingle_src_blender,
                                                  pdual_src_blender,
                                                  pfetch_blender);
}

fastuidraw::gl::PainterBlendShaderGL::
~PainterBlendShaderGL(void)
{
  PainterBlendShaderGLPrivate *d;
  d = reinterpret_cast<PainterBlendShaderGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::gl::SingleSourceBlenderShader&
fastuidraw::gl::PainterBlendShaderGL::
single_src_blender(void) const
{
  PainterBlendShaderGLPrivate *d;
  d = reinterpret_cast<PainterBlendShaderGLPrivate*>(m_d);
  return d->m_single_src_blender;
}

const fastuidraw::gl::DualSourceBlenderShader&
fastuidraw::gl::PainterBlendShaderGL::
dual_src_blender(void) const
{
  PainterBlendShaderGLPrivate *d;
  d = reinterpret_cast<PainterBlendShaderGLPrivate*>(m_d);
  return d->m_dual_src_blender;
}

const fastuidraw::gl::FramebufferFetchBlendShader&
fastuidraw::gl::PainterBlendShaderGL::
fetch_blender(void) const
{
  PainterBlendShaderGLPrivate *d;
  d = reinterpret_cast<PainterBlendShaderGLPrivate*>(m_d);
  return d->m_fetch_blender;
}
