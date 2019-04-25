/*!
 * \file c_array.hpp
 * \brief file c_array.hpp
 *
 * Adapted from: c_array.hpp of WRATH:
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

#include <iterator>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>

namespace fastuidraw
{

/*!\addtogroup Utility
 * @{
 */

/*!
 * \brief
 * A c_array is a wrapper over a
 * C pointer with a size parameter
 * to facilitate bounds checking
 * and provide an STL-like iterator
 * interface.
 */
template<typename T>
class c_array
{
public:

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef T* pointer;

  /*!
   * \brief
   * STL compliant typedef; notice that const_pointer
   * is type T* and not const T*. This is because
   * a c_array is just a HOLDER of a pointer and
   * a length and thus the contents of the value
   * behind the pointer are not part of the value
   * of a c_array.
   */
  typedef T* const_pointer;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef T& reference;

  /*!
   * \brief
   * STL compliant typedef; notice that const_pointer
   * is type T& and not const T&. This is because
   * a c_array is just a HOLDER of a pointer and
   * a length and thus the contents of the value
   * behind the pointer are not part of the value
   * of a c_array.
   */
  typedef T& const_reference;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef T value_type;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef size_t size_type;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef ptrdiff_t difference_type;

  /*!
   * \brief
   * iterator typedef to pointer
   */
  typedef pointer iterator;

  /*!
   * \brief
   * iterator typedef to const_pointer
   */
  typedef const_pointer const_iterator;

  /*!
   * \brief
   * iterator typedef using std::reverse_iterator.
   */
  typedef std::reverse_iterator<const_iterator>  const_reverse_iterator;

  /*!
   * \brief
   * iterator typedef using std::reverse_iterator.
   */
  typedef std::reverse_iterator<iterator>        reverse_iterator;

  /*!
   * Default ctor, initializing the pointer as nullptr
   * with size 0.
   */
  c_array(void):
    m_size(0),
    m_ptr(nullptr)
  {}

  /*!
   * Ctor initializing the pointer and size
   * \param pptr pointer value
   * \param sz size, must be no more than the number of elements that pptr points to.
   */
  template<typename U>
  c_array(U *pptr, size_type sz):
    m_size(sz),
    m_ptr(pptr)
  {
    FASTUIDRAWstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Ctor from a vecN, size is the size of the fixed size array
   * \param pptr fixed size array that c_array references, must be
   *             in scope as until c_array is changed
   */
  template<typename U, size_type N>
  c_array(vecN<U, N> &pptr):
    m_size(N),
    m_ptr(pptr.c_ptr())
  {
    FASTUIDRAWstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Ctor from a vecN, size is the size of the fixed size array
   * \param pptr fixed size array that c_array references, must be
   *             in scope as until c_array is changed
   */
  template<typename U, size_type N>
  c_array(const vecN<U, N> &pptr):
    m_size(N),
    m_ptr(pptr.c_ptr())
  {
    FASTUIDRAWstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Ctor from another c_array object.
   * \tparam U type U* must be convertible to type T* AND
   *           the size of U must be the same as the size
   *           of T
   * \param obj value from which to copy
   */
  template<typename U>
  c_array(const c_array<U> &obj):
    m_size(obj.m_size),
    m_ptr(obj.m_ptr)
  {
    FASTUIDRAWstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Ctor from a range of pointers.
   * \param R R.m_begin will be the pointer and R.m_end - R.m_begin the size.
   */
  c_array(range_type<iterator> R):
    m_size(R.m_end - R.m_begin),
    m_ptr((m_size > 0) ? &*R.m_begin : nullptr)
  {}

  /*!
   * Resets the \ref c_array object to be equivalent to
   * a nullptr, i.e. c_ptr() will return nullptr and size()
   * will return 0.
   */
  void
  reset(void)
  {
    m_size = 0;
    m_ptr = nullptr;
  }

  /*!
   * Reinterpret style cast for c_array. It is required
   * that the sizeof(T)*size() evenly divides sizeof(S).
   * \tparam S type to which to be reinterpreted casted
   */
  template<typename S>
  c_array<S>
  reinterpret_pointer(void) const
  {
    S *ptr;
    size_type num_bytes(size() * sizeof(T));
    FASTUIDRAWassert(num_bytes % sizeof(S) == 0);
    ptr = reinterpret_cast<S*>(c_ptr());
    return c_array<S>(ptr, num_bytes / sizeof(S));
  }

  /*!
   * Const style cast for c_array. It is required
   * that the sizeof(T) is the same as sizeof(S).
   * \tparam S type to which to be const casted
   */
  template<typename S>
  c_array<S>
  const_cast_pointer(void) const
  {
    S *ptr;
    FASTUIDRAWstatic_assert(sizeof(S) == sizeof(T));
    ptr = const_cast<S*>(c_ptr());
    return c_array<S>(ptr, m_size);
  }

  /*!
   * Pointer of the c_array.
   */
  T*
  c_ptr(void) const
  {
    return m_ptr;
  }

  /*!
   * Pointer to the element one past
   * the last element of the c_array.
   */
  T*
  end_c_ptr(void) const
  {
    return m_ptr + m_size;
  }

  /*!
   * Access named element of c_array, under
   * debug build also performs bounds checking.
   * \param j index
   */
  reference
  operator[](size_type j) const
  {
    FASTUIDRAWassert(c_ptr() != nullptr);
    FASTUIDRAWassert(j < m_size);
    return c_ptr()[j];
  }

  /*!
   * STL compliant function.
   */
  bool
  empty(void) const
  {
    return m_size == 0;
  }

  /*!
   * STL compliant function.
   */
  size_type
  size(void) const
  {
    return m_size;
  }

  /*!
   * STL compliant function.
   */
  iterator
  begin(void) const
  {
    return iterator(c_ptr());
  }

  /*!
   * STL compliant function.
   */
  iterator
  end(void) const
  {
    return iterator(c_ptr() + static_cast<difference_type>(size()));
  }

  /*!
   * STL compliant function.
   */
  reverse_iterator
  rbegin(void) const
  {
    return reverse_iterator(end());
  }

  /*!
   * STL compliant function.
   */
  reverse_iterator
  rend(void) const
  {
    return reverse_iterator(begin());
  }

  /*!
   * Returns the range of the c_array as an
   * iterator range.
   */
  range_type<iterator>
  range(void) const
  {
    return range_type<iterator>(begin(), end());
  }

  /*!
   * Returns the range of the c_array as a
   * reverse_iterator range
   */
  range_type<reverse_iterator>
  reverse_range(void) const
  {
    return range_type<reverse_iterator>(rbegin(), rend());
  }

  /*!
   * Equivalent to
   * \code
   * operator[](size()-1-I)
   * \endcode
   * \param I index from the back to retrieve, I=0
   *          corrseponds to the back of the array.
   */
  reference
  back(size_type I) const
  {
    FASTUIDRAWassert(I < size());
    return (*this)[size() - 1 - I];
  }

  /*!
   * STL compliant function.
   */
  reference
  back(void) const
  {
    return (*this)[size() - 1];
  }

  /*!
   * STL compliant function.
   */
  reference
  front(void) const
  {
    return (*this)[0];
  }

  /*!
   * Returns a sub-array
   * \param pos position of returned sub-array to start,
   *            i.e. returned c_array's c_ptr() will return
   *            this->c_ptr()+pos. It is an error if pos
   *            is negative.
   * \param length length of sub array to return, note
   *               that it is an error if length+pos>size()
   *               or if length is negative.
   */
  c_array
  sub_array(size_type pos, size_type length) const
  {
    FASTUIDRAWassert(pos + length <= m_size);
    return c_array(m_ptr + pos, length);
  }

  /*!
   * Returns a sub-array, equivalent to
   * \code
   * sub_array(pos, size() - pos)
   * \endcode
   * \param pos position of returned sub-array to start,
   *            i.e. returned c_array's c_ptr() will return
   *            this->c_ptr() + pos. It is an error is pos
   *            is negative.
   */
  c_array
  sub_array(size_type pos) const
  {
    FASTUIDRAWassert(pos <= m_size);
    return c_array(m_ptr + pos, m_size - pos);
  }

  /*!
   * Returns a sub-array, equivalent to
   * \code
   * sub_array(R.m_begin, R.m_end - R.m_begin)
   * \endcode
   * \tparam I type convertible to size_type
   * \param R range of returned sub-array
   */
  template<typename I>
  c_array
  sub_array(range_type<I> R) const
  {
    return sub_array(R.m_begin, R.m_end - R.m_begin);
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * *this = this->sub_array(0, size() - 1);
   * \endcode
   * It is an error to call this when size() is 0.
   */
  void
  pop_back(void)
  {
    FASTUIDRAWassert(m_size > 0);
    --m_size;
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * *this = this->sub_array(1, size() - 1);
   * \endcode
   * It is an error to call this when size() is 0.
   */
  void
  pop_front(void)
  {
    FASTUIDRAWassert(m_size > 0);
    --m_size;
    ++m_ptr;
  }

  /*!
   * Returns true if and only if the passed
   * the c_array references exactly the same
   * data as this c_array.
   * \param rhs c_array to which to compare
   */
  template<typename U>
  bool
  same_data(const c_array<U> &rhs) const
  {
    return m_ptr == rhs.m_ptr
      && m_size * sizeof(T) == rhs.m_size * sizeof(U);
  }

private:
  template<typename>
  friend class c_array;

  size_type m_size;
  T *m_ptr;
};


/*!
 * Convert an array of 32-bit floating point values
 * to an array of 16-bit half values
 * \param src fp32 value to convert
 * \param dst location to which to write fp16 values
 */
void
convert_to_fp16(c_array<const float> src, c_array<uint16_t> dst);

/*!
 * Convert an array of 16-bit floating point values
 * to an array of 32-bit half values
 * \param src fp16 value to convert
 * \param dst location to which to write fp32 values
 */
void
convert_to_fp32(c_array<const uint16_t> src, c_array<float> dst);

/*!
 * Conveniance function to pack a single \ref vec2
 * into an fp16-pair returned as a uint32_t.
 * \param src float-pair to pack as an fp16-pair.
 */
inline
uint32_t
pack_as_fp16(vec2 src)
{
  uint32_t return_value;
  uint16_t *ptr(reinterpret_cast<uint16_t*>(&return_value));
  convert_to_fp16(src, c_array<uint16_t>(ptr, 2));
  return return_value;
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 *  pack_as_fp16(vec2(x, y));
 * \endcode
 */
inline
uint32_t
pack_as_fp16(float x, float y)
{
  return pack_as_fp16(vec2(x, y));
}

/*! @} */

} //namespace
