// -*- C++ -*-

/*!
 * \file gluniform_implement.hpp
 * \brief file gluniform_implement.hpp
 *
 * Adapted from: WRATHgluniform_implement.tcc of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#pragma once

#include <fastuidraw/gl_backend/gl_header.hpp>

namespace fastuidraw
{
namespace gl
{

/*
 * Implement Uniform{1,2,3,4}v overloads to correct
 * glUniform{1,2,3,4}{d,f,i,ui}v calls and provide overloads
 * for vecN types too.
 *
 * GLFN one of {d,f,i,ui}
 * TYPE GL type, such as GLfloat
 * COUNT one of {1,2,3,4}
 */
#define MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, COUNT)          \
  void Uniform##COUNT##v(int location, GLsizei count, const TYPE *v);   \
  inline void Uniform(int location, const vecN<TYPE, COUNT> &v)         \
  {                                                                     \
    Uniform##COUNT##v(location, 1, &v[0]);                              \
  }                                                                     \
  inline void Uniform(int location, GLsizei count, const vecN<TYPE, COUNT> *v) \
  {                                                                     \
    Uniform##COUNT##v(location, count, reinterpret_cast<const TYPE*>(v)); \
  }                                                                     \
  void ProgramUniform##COUNT##v(GLuint program, int location, GLsizei count, const TYPE *v); \
  inline void ProgramUniform(GLuint program, int location, const vecN<TYPE, COUNT> &v) \
  {                                                                     \
    ProgramUniform##COUNT##v(program, location, 1, &v[0]);              \
  }                                                                     \
  inline void ProgramUniform(GLuint program, int location, GLsizei count, const vecN<TYPE, COUNT> *v) \
  {                                                                     \
    ProgramUniform##COUNT##v(program, location, count, reinterpret_cast<const TYPE*>(v)); \
  }

/*
 * Use MACRO_IMPEMENT_GL_UNIFORM_IMPL_CNT to implement
 * all for a given type. In addition array Uniform
 * without vecN.
 *
 * GLFN one of {d,f,i,ui}
 * TYPE GL type, such as GLfloat
 */
#define MACRO_IMPLEMENT_GL_UNIFORM_IMPL(GLFN, TYPE)                     \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 1)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 2)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 3)                    \
  MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 4)                    \
  void Uniform(int location, TYPE v);                                   \
  inline void Uniform(int location, GLsizei count, const TYPE *v)       \
  {                                                                     \
    Uniform1v(location, count, v);                                      \
  }                                                                     \
  void ProgramUniform(GLuint program, int location, TYPE v);            \
  inline void Uniform(GLuint program, int location, GLsizei count, const TYPE *v) \
  {                                                                     \
    ProgramUniform1v(program, location, count, v);                      \
  }

/*
 * Implement square matrices uniform setting
 * A: dimension of matrix, one of  {2,3,4}
 * GLFN: one of {f,d}
 * TYPE: one of {GLfloat, GLdouble}
 */
#define MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A) \
  void Uniform(int location, GLsizei count, const matrixNxM<A,A,TYPE> *matrices, bool transposed=false); \
  inline void Uniform(int location, const matrixNxM<A,A,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    Uniform(location, 1, &matrix, transposed);                          \
  }                                                                     \
  void ProgramUniform(GLuint program, int location, GLsizei count, const matrixNxM<A,A,TYPE> *matrices, bool transposed=false); \
  inline void ProgramUniform(GLuint program, int location, const matrixNxM<A,A,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    ProgramUniform(program, location, 1, &matrix, transposed);          \
  }


  /*
   * Implement non-square matrices uniform setting
   * A: height of matrix, one of  {2,3,4}
   * B: width of matrix, one of  {2,3,4}
   * GLFN: one of {f,d}
   * TYPE: one of {GLfloat, GLdouble}
   */
#define MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A, B) \
  void Uniform(int location, GLsizei count, const matrixNxM<A,B,TYPE> *matrices, bool transposed=false); \
  inline void Uniform(int location, const matrixNxM<A,B,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    Uniform(location, 1, &matrix, transposed);                          \
  }                                                                     \
  void ProgramUniform(GLuint program, int location, GLsizei count, const matrixNxM<A,B,TYPE> *matrices, bool transposed=false); \
  inline void ProgramUniform(GLuint program, int location, const matrixNxM<A,B,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    ProgramUniform(program, location, 1, &matrix, transposed);          \
  }



/*
 * Implement square matrices uniform setting
 * GLFN: one of {f,d}
 * TYPE: one of {GLfloat, GLdouble}
 */
#define MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL(GLFN, TYPE)  \
  MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 2) \
  MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 3) \
  MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 4)


/*
 * Implement non-square matrices uniform setting
 * GLFN: one of {f,d}
 * TYPE: one of {GLfloat, GLdouble}
 */
#define MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL(GLFN, TYPE)   \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,2,3)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,2,4)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,3,2)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,3,4)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,4,2)  \
  MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,4,3)


/*
 * Implement all matrix uniform setting
 * GLFN: one of {f,d}
 * TYPE: one of {GLfloat, GLdouble}
 */
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

#undef MACRO_IMPLEMENT_GL_UNIFORM_IMPL
#undef MACRO_IMPLEMENT_GL_UNIFORM_IMPL_CNT
#undef MACRO_IMPLEMENT_GL_UNIFORM_MATRIX_IMPL
#undef MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL
#undef MACRO_IMPLEMENT_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM
#undef MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL
#undef MACRO_IMPLEMENT_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM

}
}
