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

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/painter_blend_shader_gl.hpp>

namespace
{
  class BlendShaderSourceCodePrivate
  {
  public:
    BlendShaderSourceCodePrivate(const fastuidraw::glsl::ShaderSource &psrc,
                                 unsigned int num_sub_shaders):
      m_src(psrc),
      m_number_sub_shaders(num_sub_shaders),
      m_shader_id(0),
      m_registered_to(NULL)
    {}

    fastuidraw::glsl::ShaderSource m_src;
    unsigned int m_number_sub_shaders;
    uint32_t m_shader_id;
    const fastuidraw::PainterBackend *m_registered_to;
  };

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

//////////////////////////////////////////////
// fastuidraw::gl::BlendShaderSourceCode
fastuidraw::gl::BlendShaderSourceCode::
BlendShaderSourceCode(const glsl::ShaderSource &psrc,
                      unsigned int num_sub_shaders)
{
  m_d = FASTUIDRAWnew BlendShaderSourceCodePrivate(psrc, num_sub_shaders);
}

fastuidraw::gl::BlendShaderSourceCode::
~BlendShaderSourceCode()
{
  BlendShaderSourceCodePrivate *d;
  d = reinterpret_cast<BlendShaderSourceCodePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::glsl::ShaderSource&
fastuidraw::gl::BlendShaderSourceCode::
shader_src(void) const
{
  BlendShaderSourceCodePrivate *d;
  d = reinterpret_cast<BlendShaderSourceCodePrivate*>(m_d);
  return d->m_src;
}

unsigned int
fastuidraw::gl::BlendShaderSourceCode::
number_sub_shaders(void) const
{
  BlendShaderSourceCodePrivate *d;
  d = reinterpret_cast<BlendShaderSourceCodePrivate*>(m_d);
  return d->m_number_sub_shaders;
}

uint32_t
fastuidraw::gl::BlendShaderSourceCode::
ID(void) const
{
  BlendShaderSourceCodePrivate *d;
  d = reinterpret_cast<BlendShaderSourceCodePrivate*>(m_d);
  assert(d->m_registered_to != NULL);
  return d->m_shader_id;
}

const fastuidraw::PainterBackend*
fastuidraw::gl::BlendShaderSourceCode::
registered_to(void) const
{
  BlendShaderSourceCodePrivate *d;
  d = reinterpret_cast<BlendShaderSourceCodePrivate*>(m_d);
  return d->m_registered_to;
}

void
fastuidraw::gl::BlendShaderSourceCode::
register_shader_code(uint32_t pshader_id,
                     const PainterBackend *backend)
{
  BlendShaderSourceCodePrivate *d;
  d = reinterpret_cast<BlendShaderSourceCodePrivate*>(m_d);
  assert(d->m_registered_to == NULL);
  d->m_registered_to = backend;
  d->m_shader_id = pshader_id;
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
