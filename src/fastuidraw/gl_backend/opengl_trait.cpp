#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>

namespace fastuidraw {
namespace gl {

  void
  VertexAttribPointer(GLint index, const opengl_trait_value &v, GLboolean normalized)
  {
    glVertexAttribPointer(index, v.m_count, v.m_type, normalized, v.m_stride, v.m_offset);
  }

  void
  VertexAttribIPointer(GLint index, const opengl_trait_value &v)
  {
    glVertexAttribIPointer(index, v.m_count, v.m_type, v.m_stride, v.m_offset);
  }

} //namespace gl
} //namespace fastuidraw
