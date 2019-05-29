/*!
 * \file gluniform_implement.cpp
 * \brief file gluniform_implement.cpp
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
    fastuidraw_glUniform##COUNT##GLFN##v(location, count, v);           \
  }                                                                     \
  void ProgramUniform##COUNT##v(GLuint program, int location, GLsizei count, const TYPE *v) \
  {                                                                     \
    fastuidraw_glProgramUniform##COUNT##GLFN##v(program, location, count, v); \
  }

#define MACRO_IMPLEMENT_GL_UNIFORM_IMPL(GLFN, TYPE)                     \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 1)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 2)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 3)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 4)                    \
  void Uniform(int location, TYPE v)                                    \
  {                                                                     \
    fastuidraw_glUniform1##GLFN(location, v);                           \
  }                                                                     \
  void ProgramUniform(GLuint program, int location, TYPE v)             \
  {                                                                     \
    fastuidraw_glProgramUniform1##GLFN(program, location, v);           \
  }

#define MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A) \
  void Uniform(int location, GLsizei count, const matrixNxM<A,A,TYPE> *matrices, bool transposed) \
  {                                                                     \
    fastuidraw_glUniformMatrix##A##GLFN##v(location, count, transposed?GL_TRUE:GL_FALSE, reinterpret_cast<const TYPE*>(matrices)); \
  }                                                                     \
  void ProgramUniform(GLuint program, int location, GLsizei count, const matrixNxM<A,A,TYPE> *matrices, bool transposed) \
  {                                                                     \
    fastuidraw_glProgramUniformMatrix##A##GLFN##v(program, location, count, transposed?GL_TRUE:GL_FALSE, reinterpret_cast<const TYPE*>(matrices)); \
  }

#define MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A, B) \
  void Uniform(int location, GLsizei count, const matrixNxM<A,B,TYPE> *matrices, bool transposed) \
  {                                                                     \
    fastuidraw_glUniformMatrix##A##x##B##GLFN##v(location, count, transposed?GL_TRUE:GL_FALSE, reinterpret_cast<const TYPE*>(matrices)); \
  }                                                                     \
  void ProgramUniform(GLuint program, int location, GLsizei count, const matrixNxM<A,B,TYPE> *matrices, bool transposed) \
  {                                                                     \
    fastuidraw_glProgramUniformMatrix##A##x##B##GLFN##v(program, location, count, transposed?GL_TRUE:GL_FALSE, reinterpret_cast<const TYPE*>(matrices)); \
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
