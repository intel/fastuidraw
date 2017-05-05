/*!
 * \file array2d.hpp
 * \brief file array2d.hpp
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
  @{
 */

/*!
  A generic array2d class:

 If FASTUIDRAW_VECTOR_BOUND_CHECK is defined,
 will perform bounds checking.
 \tparam T array2d entry type
*/
template<typename T>
class array2d
{
private:
  std::vector<T> m_data;
#ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
  size_t m_M;
#endif
  size_t m_N;

public:

  /*!
    Ctor.
    Initializes an MxN array2d.
  */
  array2d(size_t M, size_t N)
    : m_data(M * N, T())
#ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
    , m_M(M)
#endif
    , m_N(N)
  {
  }

  /*!
    Resizes the array3d.
   */
  void
  resize(size_t M, size_t N)
  {
#ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
    m_M = M;
#endif
    m_N = N;
    m_data.resize(M * N, T());
  }

  /*!
    Fills values with the parameter value.
    \param value value to fill values with
   */
  void
  fill(const T& value)
  {
    std::fill(m_data.begin(), m_data.end(), value);

  }

  /*!
    Returns the value in the vector corresponding M[i,j]
    \param row row(vertical coordinate) in the array2d
    \param col column(horizontal coordinate) in the array2d
   */
  T&
  operator()(unsigned int row, unsigned int col)
  {
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      FASTUIDRAWassert(row<m_M);
      FASTUIDRAWassert(col<m_N);
    #endif
    return m_data[m_N*row+col];
  }

  /*!
    Returns the value in the vector corresponding M[i,j]
    \param row row(vertical coordinate) in the array2d
    \param col column(horizontal coordinate) in the array2d
   */
  const T&
  operator()(unsigned int row, unsigned int col) const
  {
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      FASTUIDRAWassert(row<m_M);
      FASTUIDRAWassert(col<m_N);
    #endif
    return m_data[m_N*row+col];
  }
};

} //namespace

/*! @} */
