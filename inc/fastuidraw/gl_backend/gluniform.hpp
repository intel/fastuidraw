/*!
 * \file gluniform.hpp
 * \brief file gluniform.hpp
 *
 * Adapted from: WRATHgluniform.hpp of WRATH:
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
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */


#pragma once

#include <fastuidraw/gl_backend/gl_header.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/gl_backend/gluniform_implement.hpp>


namespace fastuidraw
{

namespace gl {

/*!\addtogroup GLUtility
  @{
 */


  /*
    NOTE:
      The overload Uniform(int loc, const vecN<T, N> &v) mapping to
      Uniform(loc, v.size(), &v[0]) because if T is a GL type
      (for example float) and N is 2,3 or 4 it is ambigous if
      we are setting an array of floats or a single vec2, vec3
      or vec4 (for the case T=float)
   */

/*!
  \brief
  Template version for setting array of uniforms.

  Equivalent to
  \code
  Uniform(location, count, v.c_ptr());
  \endcode
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use.
 */
template<typename T, size_t N>
void
Uniform(int location, GLsizei count, const vecN<T,N> &v)
{
  Uniform(location, count, v.c_ptr());
}

/*!
  \brief
  Template version for setting array of uniform matrices.

  Equivalent to
  \code
  Uniform(location, count, v.c_ptr(), transposed);
  \endcode
  This call is for when v is an array of matrices.
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use
  \param transposed flag to indicate if the matrices are to be transposed
 */
template<typename T, size_t N>
void
Uniform(int location, GLsizei count, const vecN<T,N> &v, bool transposed)
{
  Uniform(location, count, v.c_ptr(), transposed);
}


/*!
  \brief
  Template version for setting array of uniforms.

  Equivalent to
  \code
  Uniform(location, count, &v[0]);
  \endcode
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use.
 */
template<typename T>
void
Uniform(int location, GLsizei count, c_array<const T> v)
{
  if(!v.empty())
    {
      Uniform(location, count, &v[0]);
    }
}

/*!
  \brief
  Template version for setting array of uniforms.

  Equivalent to
  \code
  Uniform(location, count, &v[0]);
  \endcode
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use.
 */
template<typename T>
void
Uniform(int location, GLsizei count, c_array<T> v)
{
  if(!v.empty())
    {
      Uniform(location, count, &v[0]);
    }
}

/*!
  \brief
  Template version for setting array of uniforms.

  Equivalent to
  \code
  Uniform(location, count, &v[0], transposed);
  \endcode
  This call is for when v is an array of matrices.
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use
  \param transposed flag to indicate if the matrices are to be transposed
 */
template<typename T>
void
Uniform(int location, GLsizei count, c_array<const T> v, bool transposed)
{
  if(!v.empty())
    {
      Uniform(location, count, &v[0], transposed);
    }
}

/*!
  \brief
  Template version for setting array of uniforms,

  Equivalent to
  \code
  Uniform(location, v.size(), &v[0]);
  \endcode
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
 */
template<typename T>
void
Uniform(int location, c_array<const T> v)
{
  if(!v.empty())
    {
      Uniform(location, v.size(), &v[0]);
    }
}

/*!
  \brief
  Template version for setting array of uniforms,

  Equivalent to
  \code
  ProgramUniform(program, location, count, v.c_ptr());
  \endcode
  \param program GL name of program to which uniform(s) belong
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use.
 */
template<typename T, size_t N>
void
ProgramUniform(GLuint program, int location, GLsizei count, const vecN<T,N> &v)
{
  ProgramUniform(program, location, count, v.c_ptr());
}

/*!
  Template version for setting array of uniform matrices,
  equivalent to
  \code
  ProgramUniform(program, location, count, v.c_ptr(), transposed);
  \endcode
  This call is for when v is an array of matrices.
  \param program GL name of program to which uniform(s) belong
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use
  \param transposed flag to indicate if the matrices are to be transposed
 */
template<typename T, size_t N>
void
ProgramUniform(GLuint program, int location, GLsizei count, const vecN<T,N> &v, bool transposed)
{
  ProgramUniform(program, location, count, v.c_ptr(), transposed);
}


/*!
  \brief
  Template version for setting array of uniforms,

  Equivalent to
  \code
  ProgramUniform(program, location, count, &v[0]);
  \endcode
  \param program GL name of program to which uniform(s) belong
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use.
 */
template<typename T>
void
ProgramUniform(GLuint program, int location, GLsizei count, c_array<const T> v)
{
  if(!v.empty())
    {
      ProgramUniform(program, location, count, &v[0]);
    }
}

/*!
  \brief
  Template version for setting array of uniforms,

  Equivalent to
  \code
  ProgramUniform(program, location, count, &v[0]);
  \endcode
  \param program GL name of program to which uniform(s) belong
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use.
 */
template<typename T>
void
ProgramUniform(GLuint program, int location, GLsizei count, c_array<T> v)
{
  if(!v.empty())
    {
      ProgramUniform(program, location, count, &v[0]);
    }
}

/*!
  \brief
  Template version for setting array of uniforms.

  Equivalent to
  \code
  ProgramUniform(program, location, count, &v[0], transposed);
  \endcode
  This call is for when v is an array of matrices.
  \param program GL name of program to which uniform(s) belong
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
  \param count number of elements from the array v to use
  \param transposed flag to indicate if the matrices are to be transposed
 */
template<typename T>
void
ProgramUniform(GLuint program, int location, GLsizei count, c_array<const T> v, bool transposed)
{
  if(!v.empty())
    {
      ProgramUniform(program, location, count, &v[0], transposed);
    }
}

/*!
  \brief
  Template version for setting array of uniforms,

  Equivalent to
  \code
  ProgramUniform(program, location, v.size(), &v[0]);
  \endcode
  \param program GL name of program to which uniform(s) belong
  \param location location of uniform, i.e. return value
                  of glGetUniformLocation
  \param v array of values
 */
template<typename T>
void
ProgramUniform(GLuint program, int location, c_array<const T> v)
{
  if(!v.empty())
    {
      ProgramUniform(program, location, v.size(), &v[0]);
    }
}

/*! @} */


} //namespace gl
} //namespace fastuidraw
