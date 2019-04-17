/*!
 * \file painter_surface_gl_private.cpp
 * \brief file painter_surface_gl_private.cpp
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

#include <private/gl_backend/painter_surface_gl_private.hpp>
#include <private/gl_backend/tex_buffer.hpp>
#include <private/gl_backend/texture_gl.hpp>

/////////////////////////////
//PainterSurfaceGLPrivate methods
fastuidraw::gl::detail::PainterSurfaceGLPrivate::
PainterSurfaceGLPrivate(enum PainterSurface::render_type_t render_type,
                        GLuint texture, ivec2 dimensions,
                        bool allow_bindless):
  m_render_type(render_type),
  m_clear_color(0.0f, 0.0f, 0.0f, 0.0f),
  m_dimensions(dimensions),
  m_buffers(0),
  m_fbo(0),
  m_own_texture(texture == 0),
  m_allow_bindless(allow_bindless)
{
  m_buffers[buffer_color] = texture;
  m_viewport.m_dimensions = m_dimensions;
  m_viewport.m_origin = fastuidraw::ivec2(0, 0);
}

fastuidraw::gl::detail::PainterSurfaceGLPrivate::
~PainterSurfaceGLPrivate()
{
  if (!m_own_texture)
    {
      m_buffers[buffer_color] = 0;
    }
  fastuidraw_glDeleteFramebuffers(m_fbo.size(), m_fbo.c_ptr());
  fastuidraw_glDeleteTextures(m_buffers.size(), m_buffers.c_ptr());
}

fastuidraw::reference_counted_ptr<const fastuidraw::Image>
fastuidraw::gl::detail::PainterSurfaceGLPrivate::
image(const reference_counted_ptr<ImageAtlas> &atlas)
{
  if (!m_image)
    {
      GLuint texture;

      /* there is a risk that the image will go out of scope
       * after the Surface. To combat this, we let the created
       * Image own the texture (if the PainterSurfaceGL owned it).
       * The m_image is part of the PainterSurfaceGLPrivate, so it
       * won't release the texture until the PainterSurfaceGLPrivate
       * dtor is called. Note that the image is exposed as
       * Image::premultipied_rgba_format; this is because
       * the GL/GLSL painter shader emits pre-multiplied
       * RGBA values.
       */
      texture = buffer(buffer_color);
      m_image = TextureImage::create(atlas,
                                                   m_dimensions.x(),
                                                   m_dimensions.y(),
                                                   1,
                                                   texture,
                                                   m_own_texture,
                                                   Image::premultipied_rgba_format,
                                                   m_allow_bindless);
      m_own_texture = false;
    }

  FASTUIDRAWassert(m_image->atlas() == atlas);
  return m_image;
}

fastuidraw::gl::PainterSurfaceGL*
fastuidraw::gl::detail::PainterSurfaceGLPrivate::
surface_gl(const fastuidraw::reference_counted_ptr<fastuidraw::PainterSurface> &surface)
{
  PainterSurfaceGL *q;

  FASTUIDRAWassert(dynamic_cast<PainterSurfaceGL*>(surface.get()));
  q = static_cast<PainterSurfaceGL*>(surface.get());
  return q;
}

GLuint
fastuidraw::gl::detail::PainterSurfaceGLPrivate::
buffer(enum buffer_t tp)
{
  if (m_buffers[tp] == 0)
    {
      GLenum tex_target, tex_target_binding;
      GLenum internalFormat;
      GLint old_tex;

      tex_target = GL_TEXTURE_2D;
      tex_target_binding = GL_TEXTURE_BINDING_2D;

      if (tp == buffer_color)
        {
          internalFormat = (m_render_type == PainterSurface::color_buffer_type) ?
            GL_RGBA8 :
            GL_R8;
        }
      else
        {
          FASTUIDRAWassert(m_render_type == PainterSurface::color_buffer_type);
          internalFormat = GL_DEPTH24_STENCIL8;
        }

      fastuidraw_glGetIntegerv(tex_target_binding, &old_tex);
      fastuidraw_glGenTextures(1, &m_buffers[tp]);
      FASTUIDRAWassert(m_buffers[tp] != 0);
      fastuidraw_glBindTexture(tex_target, m_buffers[tp]);

      detail::tex_storage<GL_TEXTURE_2D>(true, internalFormat, m_dimensions);
      /* This is more than just good sanitation; For Intel GPU
       * drivers on MS-Windows, if we dont't to clear a texture
       * and derive a bindless handle afterwards, clears on
       * the surface will result in incorrect reads. The cause
       * is likely that an auxiliary (hidden) surface is attached
       * AFTER a clear is issued on the surface. If we don't do
       * the clear now, a bindless handle derived from the surface
       * will not have the auxiliary attached to it resulting in that
       * reads of the surface via bindless will produce garbage.
       */
      clear_texture_2d(m_buffers[tp], 0, internalFormat);
      fastuidraw_glTexParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      fastuidraw_glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      fastuidraw_glBindTexture(tex_target, old_tex);
    }

  return m_buffers[tp];
}

GLuint
fastuidraw::gl::detail::PainterSurfaceGLPrivate::
fbo(bool with_color_buffer)
{
  int tp(with_color_buffer);
  if (m_fbo[tp] == 0)
    {
      GLint old_fbo;
      GLenum tex_target;

      tex_target = GL_TEXTURE_2D;

      fastuidraw_glGenFramebuffers(1, &m_fbo[tp]);
      FASTUIDRAWassert(m_fbo[tp] != 0);

      fastuidraw_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo);
      fastuidraw_glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo[tp]);

      if (m_render_type == PainterSurface::color_buffer_type)
        {
          fastuidraw_glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                            tex_target, buffer(buffer_depth), 0);
        }

      if (with_color_buffer)
        {
          fastuidraw_glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            tex_target, buffer(buffer_color), 0);
        }

      fastuidraw_glBindFramebuffer(GL_READ_FRAMEBUFFER, old_fbo);
    }
  return m_fbo[tp];
}

fastuidraw::c_array<const GLenum>
fastuidraw::gl::detail::PainterSurfaceGLPrivate::
draw_buffers(bool with_color_buffer)
{
  int tp(with_color_buffer);
  if (m_draw_buffers[tp].empty())
    {
      m_draw_buffers[tp] = c_array<const GLenum>(m_draw_buffer_values[tp]);
      if (with_color_buffer)
        {
          m_draw_buffer_values[tp][0] = GL_COLOR_ATTACHMENT0;
        }
      else
        {
          m_draw_buffer_values[tp][0] = GL_NONE;
        }
    }

  return m_draw_buffers[tp];
}
