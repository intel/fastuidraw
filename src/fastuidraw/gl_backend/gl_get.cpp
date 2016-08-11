#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>

namespace fastuidraw {
namespace gl {

void
context_get(GLenum v, GLint *ptr)
{
  glGetIntegerv(v, ptr);
}

void
context_get(GLenum v, GLboolean *ptr)
{
  glGetBooleanv(v, ptr);
}

void
context_get(GLenum v, bool *ptr)
{
  GLboolean bptr( *ptr?GL_TRUE:GL_FALSE);
  glGetBooleanv(v, &bptr);
  *ptr=(bptr==GL_FALSE)?
    false:
    true;
}

void
context_get(GLenum v, GLfloat *ptr)
{
  glGetFloatv(v, ptr);
}


} //namespace gl
} //namespace fastuidraw
