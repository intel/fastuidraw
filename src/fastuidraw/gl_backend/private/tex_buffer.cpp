#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include "tex_buffer.hpp"

enum fastuidraw::gl::detail::tex_buffer_support_t
fastuidraw::gl::detail::
compute_tex_buffer_support(void)
{
  ContextProperties ctx;
  if(ctx.is_es())
    {
      if(ctx.version() >= ivec2(3, 2))
        {
          return tex_buffer_no_extension;
        }

      if(ctx.has_extension("GL_OES_texture_buffer"))
        {
          return tex_buffer_oes_extension;
        }

      if(ctx.has_extension("GL_EXT_texture_buffer"))
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

void
fastuidraw::gl::detail::
tex_buffer(enum tex_buffer_support_t md, GLenum target, GLenum format, GLuint bo)
{
  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      assert(md == tex_buffer_no_extension);
      FASTUIDRAWunused(md);
      glTexBuffer(target, format, bo);
    }
  #else
    {
      switch(md)
        {
        case tex_buffer_no_extension:
          glTexBuffer(target, format, bo);
          break;
        case tex_buffer_oes_extension:
          glTexBufferOES(target, format, bo);
          break;
        case tex_buffer_ext_extension:
          glTexBufferEXT(target, format, bo);
          break;

        default:
          assert(!"glTexBuffer not supported!");
        }
    }
  #endif
}
