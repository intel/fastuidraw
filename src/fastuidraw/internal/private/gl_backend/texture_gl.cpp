/*!
 * \file texture_gl.cpp
 * \brief file texture_gl.cpp
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

#include <iostream>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <private/gl_backend/texture_gl.hpp>

namespace
{
  bool
  texture_is_layered(GLenum texTarget)
  {
    bool return_value(false);

    #ifdef GL_TEXTURE_1D_ARRAY
      {
        return_value = (texTarget == GL_TEXTURE_1D_ARRAY);
      }
    #endif

    return return_value || texTarget == GL_TEXTURE_2D_ARRAY
      || texTarget == GL_TEXTURE_3D;
  }


  void
  set_color_attachment(GLenum fbo,
               GLenum texTarget,
               GLuint texName,
               GLint layer,
               GLint level)
  {
    if (texture_is_layered(texTarget))
      {
        fastuidraw_glFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, texName, level, layer);
      }
    else
      {
        FASTUIDRAWassert(layer == 0);
        switch(texTarget)
          {
#ifdef GL_TEXTURE_1D
          case GL_TEXTURE_1D:
            fastuidraw_glFramebufferTexture1D(fbo, GL_COLOR_ATTACHMENT0, texTarget, texName, level);
            break;
#endif
          default:
            // we do not need to worry about GL_TEXTURE_3D, because that target
            // is layered
            fastuidraw_glFramebufferTexture2D(fbo, GL_COLOR_ATTACHMENT0, texTarget, texName, level);
            break;
          }
      }
  }

}

GLenum
fastuidraw::gl::detail::
format_from_internal_format(GLenum fmt)
{
  switch(fmt)
    {
    default:
    case GL_RGB:
    case GL_RGB8:
    case GL_RGB32F:
    case GL_RGB16F:
      return GL_RGB;

    case GL_RGBA:
    case GL_RGBA8:
    case GL_RGBA32F:
    case GL_RGBA16F:
      return GL_RGBA;

      //integer formats:
    case GL_RGBA32UI:
    case GL_RGBA32I:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA8UI:
    case GL_RGBA8I:
      //GL_BRGA_INTEGER also ok
      return GL_RGBA_INTEGER;

    case GL_RGB32UI:
    case GL_RGB32I:
    case GL_RGB16UI:
    case GL_RGB16I:
    case GL_RGB8UI:
    case GL_RGB8I:
      //GL_BGR_INTEGER also ok
      return GL_RGB_INTEGER;

    case GL_RG8:
    case GL_RG16F:
    case GL_RG32F:
      return GL_RG;

    case GL_R8:
    case GL_R16F:
    case GL_R32F:
      return GL_RED;

    case GL_RG8I:
    case GL_RG16I:
    case GL_RG32I:
    case GL_RG8UI:
    case GL_RG16UI:
    case GL_RG32UI:
      return GL_RG_INTEGER;

    case GL_R8I:
    case GL_R16I:
    case GL_R32I:
    case GL_R8UI:
    case GL_R16UI:
    case GL_R32UI:
      return GL_RED_INTEGER;

    case GL_DEPTH24_STENCIL8:
      return GL_DEPTH_STENCIL;

    case GL_DEPTH32F_STENCIL8:
      return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
      return GL_DEPTH_COMPONENT;
    }
}

GLenum
fastuidraw::gl::detail::
type_from_internal_format(GLenum fmt)
{
  switch(fmt)
    {
    default:
    case GL_R8:
    case GL_R8UI:
    case GL_RG8:
    case GL_RG8UI:
    case GL_RGB:
    case GL_RGB8:
    case GL_RGB8UI:
    case GL_RGBA:
    case GL_RGBA8:
    case GL_RGBA8UI:
      return GL_UNSIGNED_BYTE;

    case GL_R8I:
    case GL_RG8I:
    case GL_RGB8I:
    case GL_RGBA8I:
      return GL_BYTE;

    case GL_R16UI:
    case GL_RG16UI:
    case GL_RGB16UI:
    case GL_RGBA16UI:
      return GL_UNSIGNED_SHORT;

    case GL_R16I:
    case GL_RG16I:
    case GL_RGB16I:
    case GL_RGBA16I:
      return GL_SHORT;

    case GL_R32UI:
    case GL_RG32UI:
    case GL_RGB32UI:
    case GL_RGBA32UI:
      return GL_UNSIGNED_INT;

    case GL_R32I:
    case GL_RG32I:
    case GL_RGB32I:
    case GL_RGBA32I:
      return GL_INT;

    case GL_R16F:
    case GL_RG16F:
    case GL_RGB16F:
    case GL_RGBA16F:
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F:
      return GL_FLOAT;

    case GL_DEPTH24_STENCIL8:
      return GL_UNSIGNED_INT_24_8;

    case GL_DEPTH32F_STENCIL8:
      return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
      return GL_UNSIGNED_BYTE;
    }
}

////////////////////////////////
// CopyImageSubData methods
fastuidraw::gl::detail::CopyImageSubData::
CopyImageSubData(void):
  m_type(uninited)
{}

enum fastuidraw::gl::detail::CopyImageSubData::type_t
fastuidraw::gl::detail::CopyImageSubData::
compute_type(void)
{
  ContextProperties ctx;
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (ctx.version() >= ivec2(3, 2))
        {
          return unextended_function;
        }

      if (ctx.has_extension("GL_OES_copy_image"))
        {
          return oes_function;
        }

      if (ctx.has_extension("GL_EXT_copy_image"))
        {
          return ext_function;
        }

      return emulate_function;
    }
  #else
    {
      #ifndef __APPLE__
        {
          if (ctx.version() >= ivec2(4,3) || ctx.has_extension("GL_ARB_copy_image"))
            {
              return unextended_function;
            }
        }
      #endif

      return emulate_function;
    }
  #endif
}


void
fastuidraw::gl::detail::CopyImageSubData::
operator()(GLuint srcName, GLenum srcTarget, GLint srcLevel,
           GLint srcX, GLint srcY, GLint srcZ,
           GLuint dstName, GLenum dstTarget, GLint dstLevel,
           GLint dstX, GLint dstY, GLint dstZ,
           GLsizei width, GLsizei height, GLsizei depth) const
{
  if (m_type == uninited)
    {
      m_type = compute_type();
    }

  switch(m_type)
    {
#ifndef __APPLE__
    case unextended_function:
      fastuidraw_glCopyImageSubData(srcName, srcTarget, srcLevel,
                                    srcX, srcY, srcZ,
                                    dstName, dstTarget, dstLevel,
                                    dstX, dstY, dstZ,
                                    width, height, depth);
      break;
#endif

#ifdef FASTUIDRAW_GL_USE_GLES
    case oes_function:
      fastuidraw_glCopyImageSubDataOES(srcName, srcTarget, srcLevel,
                                       srcX, srcY, srcZ,
                                       dstName, dstTarget, dstLevel,
                                       dstX, dstY, dstZ,
                                       width, height, depth);
      break;
    case ext_function:
      fastuidraw_glCopyImageSubDataEXT(srcName, srcTarget, srcLevel,
                                       srcX, srcY, srcZ,
                                       dstName, dstTarget, dstLevel,
                                       dstX, dstY, dstZ,
                                       width, height, depth);
      break;
#endif

    default:
      {
        FASTUIDRAWassert(m_type == emulate_function);
        /* Use FBO's and glBlitFramebuffer to grab each layer. Ick.
         */
        enum { fbo_draw, fbo_read };

        GLuint new_fbos[2] = { 0 }, old_fbos[2] = { 0 };
        fastuidraw_glGenFramebuffers(2, new_fbos);
        FASTUIDRAWassert(new_fbos[fbo_draw] != 0 && new_fbos[fbo_read] != 0);

        old_fbos[fbo_draw] = context_get<GLint>(GL_DRAW_FRAMEBUFFER_BINDING);
        old_fbos[fbo_read] = context_get<GLint>(GL_READ_FRAMEBUFFER_BINDING);
        fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, new_fbos[fbo_draw]);
        fastuidraw_glBindFramebuffer(GL_READ_FRAMEBUFFER, new_fbos[fbo_read]);
        for(int layer = 0, src_layer = srcZ, dst_layer = dstZ;
            layer < depth; ++layer, ++src_layer, ++dst_layer)
          {
            /* TODO: handle depth, stencil and depth/stencil textures
             * correctly.
             */
            FASTUIDRAWassert(src_layer == 0 || texture_is_layered(srcTarget));
            FASTUIDRAWassert(dst_layer == 0 || texture_is_layered(dstTarget));
            set_color_attachment(GL_DRAW_FRAMEBUFFER, dstTarget, dstName, dst_layer, dstLevel);
            set_color_attachment(GL_READ_FRAMEBUFFER, srcTarget, srcName, src_layer, srcLevel);
            fastuidraw_glBlitFramebuffer(srcX, srcY, srcX + width, srcY + height,
                                         dstX, dstY, dstX + width, dstY + height,
                                         GL_COLOR_BUFFER_BIT, GL_NEAREST);
          }
        fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_fbos[fbo_draw]);
        fastuidraw_glBindFramebuffer(GL_READ_FRAMEBUFFER, old_fbos[fbo_read]);
        fastuidraw_glDeleteFramebuffers(2, new_fbos);
      }
      break;
    }
}

///////////////////////////////////
// non-class methods
enum fastuidraw::gl::detail::texture_type_t
fastuidraw::gl::detail::
compute_texture_type(GLenum format, GLenum type)
{
  FASTUIDRAWunused(type);
  switch(format)
    {
    default:
      return decimal_color_texture_type;
    case GL_DEPTH_STENCIL:
      return depth_stencil_texture_type;
    case GL_DEPTH:
      return depth_texture_type;
#ifndef FASTUIDRAW_GL_USE_GLES
     case GL_GREEN_INTEGER:
     case GL_BLUE_INTEGER:
     case GL_BGR_INTEGER:
     case GL_BGRA_INTEGER:
#endif
     case GL_RED_INTEGER:
     case GL_RGB_INTEGER:
     case GL_RGBA_INTEGER:
     case GL_RG_INTEGER:
       return integer_color_texture_type;
    }
}

void
fastuidraw::gl::detail::
clear_texture_2d(GLuint texture, GLint level, enum texture_type_t type)
{
  GLuint fbo(0), old_fbo;
  GLenum attach_pt;

  old_fbo = context_get<GLint>(GL_DRAW_FRAMEBUFFER_BINDING);

  fastuidraw_glGenFramebuffers(1, &fbo);
  FASTUIDRAWassert(fbo != 0);
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

  switch(type)
    {
    default:
      FASTUIDRAWassert(!"Invalid texture type passed to clear_texture_2d");
      return;
    case decimal_color_texture_type:
    case integer_color_texture_type:
      attach_pt = GL_COLOR_ATTACHMENT0;
      break;
    case depth_stencil_texture_type:
      attach_pt = GL_DEPTH_STENCIL_ATTACHMENT;
      break;
    case depth_texture_type:
      attach_pt = GL_DEPTH_ATTACHMENT;
      break;
    }

  fastuidraw_glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attach_pt, GL_TEXTURE_2D, texture, level);
  switch (type)
    {
    case depth_stencil_texture_type:
      fastuidraw_glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0f, 0);
      break;

    case depth_texture_type:
      fastuidraw_glClearBufferfv(GL_DEPTH, 0, vec4(0.0f, 0.0f, 0.0f, 0.0f).c_ptr());
      break;

    case decimal_color_texture_type:
      fastuidraw_glClearBufferfv(GL_COLOR, 0, vec4(0.0f, 0.0f, 0.0f, 0.0f).c_ptr());
      break;

    case integer_color_texture_type:
      fastuidraw_glClearBufferiv(GL_COLOR, 0, ivec4(0, 0, 0, 0).c_ptr());
      break;
    }
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_fbo);
  fastuidraw_glDeleteFramebuffers(1, &fbo);
}
