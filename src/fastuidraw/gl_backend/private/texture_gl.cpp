#include <iostream>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include "texture_gl.hpp"

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

    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
      return GL_DEPTH_STENCIL;
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
      if(ctx.version() >= ivec2(3, 2))
        {
          return unextended_function;
        }

      if(ctx.has_extension("GL_OES_copy_image"))
        {
          return oes_function;
        }

      if(ctx.has_extension("GL_EXT_copy_image"))
        {
          return ext_function;
        }

      return emulate_function;
    }
  #else
    {
      if(ctx.version() >= ivec2(4,3) || ctx.has_extension("GL_ARB_copy_image"))
        {
          return unextended_function;
        }

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
  if(m_type == uninited)
    {
      m_type = compute_type();
    }

  switch(m_type)
    {
    case unextended_function:
      glCopyImageSubData(srcName, srcTarget, srcLevel,
                         srcX, srcY, srcZ,
                         dstName, dstTarget, dstLevel,
                         dstX, dstY, dstZ,
                         width, height, depth);
      break;

#ifdef FASTUIDRAW_GL_USE_GLES
    case oes_function:
      glCopyImageSubDataOES(srcName, srcTarget, srcLevel,
                         srcX, srcY, srcZ,
                         dstName, dstTarget, dstLevel,
                         dstX, dstY, dstZ,
                         width, height, depth);
      break;
    case ext_function:
      glCopyImageSubDataEXT(srcName, srcTarget, srcLevel,
                            srcX, srcY, srcZ,
                            dstName, dstTarget, dstLevel,
                            dstX, dstY, dstZ,
                            width, height, depth);
      break;
#endif

    default:
      assert(m_type == emulate_function);
      /* TODO:
          Use FBO's and glBlitFramebuffer to grab each layer. Ick.
       */
    }
}
