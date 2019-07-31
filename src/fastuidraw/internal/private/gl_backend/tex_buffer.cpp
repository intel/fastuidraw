/*!
 * \file tex_buffer.cpp
 * \brief file tex_buffer.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <private/gl_backend/tex_buffer.hpp>

enum fastuidraw::gl::detail::tex_buffer_support_t
fastuidraw::gl::detail::
compute_tex_buffer_support(void)
{
  #ifdef __EMSCRIPTEN__
    {
      return tex_buffer_not_supported;
    }
  #else
    {
      ContextProperties ctx;
      return compute_tex_buffer_support(ctx);
    }
  #endif
}

enum fastuidraw::gl::detail::tex_buffer_support_t
fastuidraw::gl::detail::
compute_tex_buffer_support(const ContextProperties &ctx)
{
  #ifdef __EMSCRIPTEN__
    {
      return tex_buffer_not_supported;
    }
  #else
    {
      if (ctx.is_es())
        {
          if (ctx.version() >= ivec2(3, 2))
            {
              return tex_buffer_no_extension;
            }

          if (ctx.has_extension("GL_OES_texture_buffer"))
            {
              return tex_buffer_oes_extension;
            }

          if (ctx.has_extension("GL_EXT_texture_buffer"))
            {
              return tex_buffer_ext_extension;
            }

          return tex_buffer_not_supported;
        }
      else
        {
          // FASTUIDRAW requires version 3.3 for GL, in which read
          // texture buffer objects are core.
          return tex_buffer_no_extension;
        }
    }
  #endif
}

void
fastuidraw::gl::detail::
tex_buffer(enum tex_buffer_support_t md, GLenum target, GLenum format, GLuint bo)
{
  #ifdef __EMSCRIPTEN__
    {
      FASTUIDRAWassert(!"Texbuffer supported");
    }
  #elif !defined(FASTUIDRAW_GL_USE_GLES)
    {
      FASTUIDRAWassert(md == tex_buffer_no_extension);
      FASTUIDRAWunused(md);
      fastuidraw_glTexBuffer(target, format, bo);
    }
  #else
    {
      switch(md)
        {
        case tex_buffer_no_extension:
          fastuidraw_glTexBuffer(target, format, bo);
          break;
        case tex_buffer_oes_extension:
          fastuidraw_glTexBufferOES(target, format, bo);
          break;
        case tex_buffer_ext_extension:
          fastuidraw_glTexBufferEXT(target, format, bo);
          break;

        default:
          FASTUIDRAWassert(!"glTexBuffer not supported!" || true);
        }
    }
  #endif
}
