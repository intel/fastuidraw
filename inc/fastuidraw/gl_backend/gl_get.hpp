/*!
 * \file gl_get.hpp
 * \brief file gl_get.hpp
 *
 * Adapted from: WRATHglGet.hpp of WRATH:
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

namespace fastuidraw {
namespace gl {

/*!\addtogroup GLUtility
 * @{
 */


/*!
 * Overloaded C++ version of glGet* family
 * of functions in GL. Equivalent to
 * glGetInteger(v, ptr).
 * \param v GL enumeration to fetch
 * \param ptr address to which to write values
 */
void
context_get(GLenum v, GLint *ptr);

/*!
 * Overloaded C++ version of glGet* family
 * of functions in GL. Equivalent to
 * glGetBooleanv(v, ptr).
 * \param v GL enumeration to fetch
 * \param ptr address to which to write values
 */
void
context_get(GLenum v, GLboolean *ptr);

/*!
 * Overloaded C++ version of glGet* family
 * of functions in GL. Equivalent to
 * glGetBooleanv(v, ptr).
 * \param v GL enumeration to fetch
 * \param ptr address to which to write values
 */
void
context_get(GLenum v, bool *ptr);

/*!
 * Overloaded C++ version of glGet* family
 * of functions in GL. Equivalent to
 * glGetFloatv(v, ptr).
 * \param v GL enumeration to fetch
 * \param ptr address to which to write values
 */
void
context_get(GLenum v, GLfloat *ptr);

/*!
 * Overloaded C++ version of glGet* family
 * of functions in GL, accepting the address
 * of a vecN, by rules of template recursion,
 * can take vecN's of other types.
 * \param v GL enumeration to fetch
 * \param p address to which to write values
 */
template<typename T, size_t N>
void
context_get(GLenum v, vecN<T,N> *p)
{
  context_get(v, p->c_ptr());
}


/*!
 * Overloaded C++ version of glGet* family of functions in GL. The
 * template parameter determines what glGet function is called.
 * The return value is initialized as 0 before calling glGet(),
 * thus if the GL implementation does not support that enum, the
 * return value is 0.
 * \param value GL enumeration to fetch
 */
template<typename T>
T
context_get(GLenum value)
{
  T return_value(0);
  context_get(value, &return_value);
  return return_value;
}
/*! @} */


} //namespace gl
} //namespace fastuidraw
