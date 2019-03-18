/*!
 * \file array3d.hpp
 * \brief file array3d.hpp
 *
 * Copyright 2017 by Yusuke Suzuki.
 *
 * Contact: utatane.tea@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Yusuke Suzuki <utatane.tea@gmail.com>
 *
 */


#pragma once

#include <algorithm>
#include <vector>

namespace fastuidraw {

/*!\addtogroup Utility
 * @{
 */

/*!
 * A generic array3d class:
 *
 *If FASTUIDRAW_VECTOR_BOUND_CHECK is defined,
 *will perform bounds checking.
 *\tparam T array3d entry type
 */
template<typename T>
class array3d
{
private:
  std::vector<T> m_data;
#ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
  size_t m_A;
#endif
  size_t m_B;
  size_t m_C;

public:

  /*!
   * Ctor.
   * Initializes an AxBxC array3d.
   */
  array3d(size_t A, size_t B, size_t C)
    : m_data(A * B * C, T())
#ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
    , m_A(A)
#endif
    , m_B(B)
    , m_C(C)
  {
  }

  /*!
   * Resizes the array3d.
   */
  void
  resize(size_t A, size_t B, size_t C)
  {
#ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
    m_A = A;
#endif
    m_B = B;
    m_C = C;
    m_data.resize(A * B * C, T());
  }

  /*!
   * Fills values with the parameter value.
   * \param value value to fill values with
   */
  void
  fill(const T& value)
  {
    std::fill(m_data.begin(), m_data.end(), value);
  }

  /*!
   * Returns the value in the vector corresponding M[i,j,k]
   * \param a first index in the array3d
   * \param b second index in the array3d
   * \param c third index in the array3d
   */
  T&
  operator()(unsigned int a, unsigned int b, unsigned int c)
  {
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      FASTUIDRAWassert(a<m_A);
      FASTUIDRAWassert(b<m_B);
      FASTUIDRAWassert(c<m_C);
    #endif
    return m_data[(m_B * m_C)*a+m_C*b+c];
  }

  /*!
   * Returns the value in the vector corresponding M[i,j,k]
   * \param a first index in the array3d
   * \param b second index in the array3d
   * \param c third index in the array3d
   */
  const T&
  operator()(unsigned int a, unsigned int b, unsigned int c) const
  {
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      FASTUIDRAWassert(a<m_A);
      FASTUIDRAWassert(b<m_B);
      FASTUIDRAWassert(c<m_C);
    #endif
    return m_data[(m_B * m_C)*a+m_C*b+c];
  }
};

} //namespace

/*! @} */
