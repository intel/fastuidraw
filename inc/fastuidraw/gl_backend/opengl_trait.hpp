/*!
 * \file opengl_trait.hpp
 * \brief file opengl_trait.hpp
 *
 * Adapted from: opengl_trait.hpp and opengl_trait_implement of WRATH:
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/gl_backend/gl_header.hpp>

namespace fastuidraw {
namespace gl {

/*!\addtogroup GLUtility
  @{
 */

/*!
  Converts an offset given in bytes to a const void* value
  as expected by GL functions (for example glVertexAttribPointer)
  \param offset offset in bytes
 */
inline
const void*
offset_as_void_pointer(unsigned int offset)
{
  const char *return_value(0);
  return_value += offset;
  return return_value;
}

/*!
  \brief
  Type trait struct that provides type information to feed GL commands.
  \tparam T type from which to extract values. The struct is specicialized for
            each of the GL types: GLubyte, GLbyte, GLuint, GLint, GLushort, GLshort, GLfloat
            and recursively for vecN
 */
template<typename T>
struct opengl_trait
{
  /*!
    \brief
    Typedef to template parameter
   */
  typedef T data_type;

  /*!
    \brief
    For an array type, such as vecN,
    element type of the array, otherwise
    is same as data_type. Note,
    for vecN types of vecN's reports
    the element type of inner array type,
    for example
    \code vecN<vecN<something,N>, M> \endcode
    gives something for basic_type
   */
  typedef T basic_type;

  enum
    {
      /*!
        GL type label, for example if basic_type
        is GLuint, then type is GL_UNSIGNED_INT
       */
      type = GL_INVALID_ENUM
    };

  enum
    {
      /*!
        The number of basic_type
        elements packed into one data_type
       */
      count = 1
    };

  enum
    {
      /*!
        The space between adjacent
        data_type elements in an array
       */
      stride = sizeof(T)
    };
};

///@cond
template<>
struct opengl_trait<GLbyte>
{
  enum { type = GL_BYTE };
  enum { count = 1 };
  enum { stride = sizeof(GLbyte) };
  typedef GLbyte basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<GLubyte>
{
  enum { type = GL_UNSIGNED_BYTE };
  enum { count = 1 };
  enum { stride = sizeof(GLubyte) };
  typedef GLubyte basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<GLshort>
{
  enum { type = GL_SHORT };
  enum { count = 1 };
  enum { stride = sizeof(GLshort) };
  typedef GLshort basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<GLushort>
{
  enum { type = GL_UNSIGNED_SHORT };
  enum { count = 1 };
  enum { stride = sizeof(GLushort) };
  typedef GLushort basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<GLint>
{
  enum { type = GL_INT };
  enum { count = 1 };
  enum { stride = sizeof(GLint) };
  typedef GLint basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<GLuint>
{
  enum { type = GL_UNSIGNED_INT };
  enum { count = 1 };
  enum { stride = sizeof(GLuint) };
  typedef GLuint basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<GLfloat>
{
  enum { type = GL_FLOAT };
  enum { count = 1 };
  enum { stride = sizeof(GLfloat) };
  typedef GLfloat basic_type;
  typedef basic_type data_type;
};

#ifdef GL_DOUBLE
template<>
struct opengl_trait<GLdouble>
{
  enum { type = GL_DOUBLE };
  enum { count = 1 };
  enum { stride = sizeof(GLdouble) };
  typedef GLfloat basic_type;
  typedef basic_type data_type;
};
#endif

template<typename T, size_t N>
struct opengl_trait< vecN<T,N> >
{
  enum { type = opengl_trait<T>::type };
  enum { count = N * opengl_trait<T>::count };
  enum { stride = sizeof(vecN<T,N>) };

  typedef typename opengl_trait<T>::basic_type basic_type;
  typedef vecN<T,N> data_type;
};
///@endcond


/*!
  \brief
  Class the bundles up count, size and type parameters
  for the GL API function glVertexAttribPointer
*/
class opengl_trait_value
{
public:
  /*!
    The number of elements, \sa opengl_trait::count
   */
  GLint m_count;

  /*!
    The element type, \sa opengl_train::type
   */
  GLenum m_type;

  /*!
    The stride to the next element in the buffer
    from which to source the attribute data
   */
  GLsizei m_stride;

  /*!
    The <i>offset</i> of the location of the
    attribute data in the buffer from which to
    source the attribute data
   */
  const void *m_offset;
};

/*!
  Template function that initializes the members of
  opengl_trait_value_base class from
  the enums of a opengl_trait<>.
 */
template<typename T>
opengl_trait_value
opengl_trait_values(void)
{
  opengl_trait_value R;
  R.m_type = opengl_trait<T>::type;
  R.m_count = opengl_trait<T>::count;
  R.m_stride = opengl_trait<T>::stride;
  R.m_offset = nullptr;
  return R;
}

/*!
  Template function that initializes the members of
  opengl_trait_value_base class from
  the enums of a opengl_trait<>.
  \param stride stride argument
  \param offset offset in bytes
 */
template<typename T>
opengl_trait_value
opengl_trait_values(GLsizei stride, GLsizei offset)
{
  opengl_trait_value R;
  R.m_type = opengl_trait<T>::type;
  R.m_count = opengl_trait<T>::count;
  R.m_stride = stride;
  R.m_offset = offset_as_void_pointer(offset);
  return R;
}

/*!
  Template function that initializes the members of
  opengl_trait_value_base class from
  the enums of a opengl_trait<>, equivalent to
  \code
  opengl_trait_values<T>(sizeof(C), offset)
  \endcode
 */
template<typename C, typename T>
opengl_trait_value
opengl_trait_values(GLsizei offset)
{
  return opengl_trait_values<T>(sizeof(C), offset);
}

/*!
  Provided as a conveniance, equivalent to
  \code
  glVertexAttribPointer(index, v.m_count, v.m_type, normalized, v.m_stride, v.m_offset);
  \endcode
  \param index which attribute
  \param v traits for attribute
  \param normalized if attribute data should be normalized
 */
void
VertexAttribPointer(GLint index, const opengl_trait_value &v, GLboolean normalized = GL_FALSE);

/*!
  Provided as a conveniance, equivalent to
  \code
  glVertexAttribIPointer(index, v.m_count, v.m_type, v.m_stride, v.m_offset);
  \endcode
  \param index which attribute
  \param v traits for attribute
 */
void
VertexAttribIPointer(GLint index, const opengl_trait_value &v);
/*! @} */

} //namespace gl
} //namespace fastuidraw
