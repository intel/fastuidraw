#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
namespace gl
{

#define MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, COUNT)          \
  void Uniform##COUNT##v(int location, GLsizei count, const TYPE *v)    \
  {                                                                     \
    glUniform##COUNT##GLFN##v(location, count, v);                      \
  }

#define MACRO_IMPLEMENT_GL_UNIFORM_IMPL(GLFN, TYPE)                     \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 1)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 2)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 3)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 4)                    \
  void Uniform(int location, TYPE v)                                    \
  {                                                                     \
    glUniform1##GLFN(location, v);                                      \
  }

#define MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A) \
  void Uniform(int location, GLsizei count, const matrixNxM<A,A,TYPE> *matrices, bool transposed) \
  {                                                                     \
    glUniformMatrix##A##GLFN##v(location, count, transposed?GL_TRUE:GL_FALSE, reinterpret_cast<const TYPE*>(matrices)); \
  }

#define MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A, B) \
  void Uniform(int location, GLsizei count, const matrixNxM<A,B,TYPE> *matrices, bool transposed) \
  {                                                                     \
    glUniformMatrix##A##x##B##GLFN##v(location, count, transposed?GL_TRUE:GL_FALSE, reinterpret_cast<const TYPE*>(matrices)); \
  }


#define MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL(GLFN, TYPE)  \
  MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 2) \
  MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 3) \
  MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 4)

#define MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL(GLFN, TYPE)   \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,2,3)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,2,4)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,3,2)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,3,4)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,4,2)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,4,3)


#define MACRO_IMPLEMENT_GL_UNIFORM_MATRIX_IMPL(GLFN, TYPE)        \
  MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL(GLFN, TYPE)       \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL(GLFN, TYPE)


MACRO_IMPLEMENT_GL_UNIFORM_IMPL(f, GLfloat)
MACRO_IMPLEMENT_GL_UNIFORM_IMPL(i, GLint)
MACRO_IMPLEMENT_GL_UNIFORM_IMPL(ui, GLuint)
MACRO_IMPLEMENT_GL_UNIFORM_MATRIX_IMPL(f, GLfloat)


#ifndef FASTUIDRAW_GL_USE_GLES
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL(d, GLdouble)
  MACRO_IMPLEMENT_GL_UNIFORM_MATRIX_IMPL(d, GLdouble)
#endif

}
}
