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
  @{
 */

template<typename T>
class c_array;

template<typename T>
class const_c_array;


/*!
  A c_array is a wrapper over a
  C pointer with a size parameter
  to facilitate bounds checking
  and provide an STL-like iterator
  interface.
  If FASTUIDRAW_VECTOR_BOUND_CHECK is defined,
  will perform bounds checking.
 */
template<typename T>
class c_array
{
public:

  /*!
    STL compliant typedef
  */
  typedef T* pointer;

  /*!
    STL compliant typedef
  */
  typedef const T* const_pointer;

  /*!
    STL compliant typedef
  */
  typedef T& reference;

  /*!
    STL compliant typedef
  */
  typedef const T& const_reference;

  /*!
    STL compliant typedef
  */
  typedef T value_type;

  /*!
    STL compliant typedef
  */
  typedef size_t size_type;

  /*!
    STL compliant typedef
  */
  typedef ptrdiff_t difference_type;

  /*!
    iterator typedef to pointer
   */
  typedef pointer iterator;

  /*!
    iterator typedef to const_pointer
   */
  typedef const_pointer const_iterator;

  /*!
    iterator typedef using std::reverse_iterator.
   */
  typedef std::reverse_iterator<const_iterator>  const_reverse_iterator;

  /*!
    iterator typedef using std::reverse_iterator.
   */
  typedef std::reverse_iterator<iterator>        reverse_iterator;

  /*!
    Default ctor, initializing the pointer as NULL
    with size 0.
   */
  c_array(void):
    m_size(0),
    m_ptr(NULL)
  {}

  /*!
    Ctor initializing the pointer and size
    \param pptr pointer value
    \param sz size, must be no more than the number of elements that pptr points to.
   */
  c_array(T *pptr, size_type sz):
    m_size(sz),
    m_ptr(pptr)
  {}

  /*!
    Ctor from a vecN, size is the size of the fixed size array
    \param pptr fixed size array that c_array references, must be
                in scope as until c_array is changed
   */
  template<size_type N>
  c_array(vecN<T,N> &pptr):
    m_size(N),
    m_ptr(pptr.c_ptr())
  {}

  /*!
    Ctor from a range of pointers.
    \param R R.m_begin will be the pointer and R.m_end-R.m_begin the size.
   */
  c_array(range_type<iterator> R):
    m_size(R.m_end-R.m_begin),
    m_ptr((m_size > 0) ? &*R.m_begin : NULL)
  {}

  /*!
    Reinterpret style cast for c_array. It is required
    that the sizeof(T)*size() evenly divides sizeof(S).
    \tparam S type to which to be reinterpreted casted
   */
  template<typename S>
  c_array<S>
  reinterpret_pointer(void) const
  {
    S *ptr;
    size_type num_bytes(size() * sizeof(T));
    assert(num_bytes % sizeof(S) == 0);
    ptr = reinterpret_cast<S*>(c_ptr());
    return c_array<S>(ptr, num_bytes / sizeof(S));
  }

  /*!
    Pointer of the c_array.
   */
  T*
  c_ptr(void) const
  {
    return m_ptr;
  }

  /*!
    Pointer to the element one past
    the last element of the c_array.
   */
  T*
  end_c_ptr(void) const
  {
    return m_ptr+m_size;
  }

  /*!
    Access named element of c_array, under
    debug build also performs bounds checking.
    \param j index
   */
  T&
  operator[](size_type j) const
  {
    assert(c_ptr() != NULL);
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      assert(j < m_size);
    #endif
    return c_ptr()[j];
  }

  /*!
    STL compliant function.
   */
  bool
  empty(void) const
  {
    return m_size == 0;
  }

  /*!
    STL compliant function.
   */
  size_type
  size(void) const
  {
    return m_size;
  }

  /*!
    STL compliant function.
   */
  iterator
  begin(void) const
  {
    return iterator(c_ptr());
  }

  /*!
    STL compliant function.
   */
  iterator
  end(void) const
  {
    return iterator(c_ptr() + static_cast<difference_type>(size()));
  }

  /*!
    STL compliant function.
   */
  reverse_iterator
  rbegin(void) const
  {
    return reverse_iterator(end());
  }

  /*!
    STL compliant function.
   */
  reverse_iterator
  rend(void) const
  {
    return reverse_iterator(begin());
  }

  /*!
    Returns the range of the c_array as an
    iterator range.
   */
  range_type<iterator>
  range(void)
  {
    return range_type<iterator>(begin(), end());
  }

  /*!
    Returns the range of the c_array as a
    reverse_iterator range
   */
  range_type<reverse_iterator>
  reverse_range(void)
  {
    return range_type<reverse_iterator>(rbegin(), rend());
  }

  /*!
    Equivalent to
    \code
    operator[](size()-1-I)
    \endcode
    \param I index from the back to retrieve, I=0
             corrseponds to the back of the array.
   */
  T&
  back(size_type I) const
  {
    assert(I<size());
    return (*this)[size() - 1 - I];
  }

  /*!
    STL compliant function.
   */
  T&
  back(void) const
  {
    return (*this)[size() - 1];
  }

  /*!
    STL compliant function.
   */
  T&
  front(void) const
  {
    return (*this)[0];
  }

  /*!
    Returns a sub-array
    \param pos position of returned sub-array to start,
               i.e. returned c_array's c_ptr() will return
               this->c_ptr()+pos. It is an error if pos
               is negative.
    \param length length of sub array to return, note
                  that it is an error if length+pos>size()
                  or if length is negative.
   */
  c_array
  sub_array(size_type pos, size_type length) const
  {
    assert(pos + length <= m_size);
    return c_array(m_ptr + pos, length);
  }

  /*!
    Returns a sub-array, equivalent to
    \code
    sub_array(pos, size() - pos)
    \endcode
    \param pos position of returned sub-array to start,
               i.e. returned c_array's c_ptr() will return
               this->c_ptr() + pos. It is an error is pos
               is negative.
   */
  c_array
  sub_array(size_type pos) const
  {
    assert(pos <= m_size);
    return c_array(m_ptr + pos, m_size - pos);
  }

  /*!
    Returns a sub-array, equivalent to
    \code
    sub_array(R.m_begin, R.m_end - R.m_begin)
    \endcode
    \tparam I type convertible to size_type
    \param R range of returned sub-array
   */
  template<typename I>
  c_array
  sub_array(range_type<I> R) const
  {
    return sub_array(R.m_begin, R.m_end - R.m_begin);
  }

  /*!
    Returns true if and only if the passed
    the c_array references exactly the same
    data as this c_array.
    \param rhs c_array to which to compare
   */
  bool
  same_data(const c_array &rhs) const
  {
    return m_size == rhs.m_size
      and m_ptr == rhs.m_ptr;
  }

  /*!
    Returns true if and only if the passed
    the c_array references exactly the same
    data as this c_array.
    \param rhs c_array to which to compare
   */
  bool
  same_data(const const_c_array<T> &rhs) const;

private:
  size_type m_size;
  T *m_ptr;

};





/*!
  A const_c_array is a wrapper over a
  const C pointer with a size parameter
  to facilitate bounds checking
  and provide an STL-like iterator
  interface.
  If FASTUIDRAW_VECTOR_BOUND_CHECK is defined,
  will perform bounds checking.
 */
template<typename T>
class const_c_array
{
public:

  /*!
    STL compliant typedef
  */
  typedef T* pointer;

  /*!
    STL compliant typedef
  */
  typedef const T* const_pointer;

  /*!
    STL compliant typedef
  */
  typedef T& reference;

  /*!
    STL compliant typedef
  */
  typedef const T& const_reference;

  /*!
    STL compliant typedef
  */
  typedef T value_type;

  /*!
    STL compliant typedef
  */
  typedef size_t size_type;

  /*!
    STL compliant typedef
  */
  typedef ptrdiff_t difference_type;

  /*!
    iterator typedef
   */
  typedef const_pointer iterator;

  /*!
    iterator typedef
   */
  typedef const_pointer const_iterator;

  /*!
    iterator typedef using std::reverse_iterator.
   */
  typedef std::reverse_iterator<const_iterator>  const_reverse_iterator;

  /*!
    iterator typedef using std::reverse_iterator.
   */
  typedef std::reverse_iterator<iterator>        reverse_iterator;

  /*!
    Default ctor, initializing the pointer as NULL
    with size 0.
   */
  const_c_array(void):
    m_size(0),
    m_ptr(NULL)
  {}

  /*!
    Ctor initializing the pointer and size
    \param pptr pointer value
    \param sz size, must be no more than the number of elements that pptr points to.
   */
  const_c_array(const T *pptr, size_type sz):
    m_size(sz),
    m_ptr(pptr)
  {}

  /*!
    Contruct a const_c_array from a c_array
    \param v c_array from which to copy size and pointer value
   */
  const_c_array(c_array<T> v):
    m_size(v.size()),
    m_ptr(v.c_ptr())
  {}

  /*!
    Ctor from a vecN, size is the size of the fixed size array
    \param v fixed size array that const_c_array references, must be
             in scope as until c_array is changed
   */
  template<size_type N>
  const_c_array(const vecN<T,N> &v):
    m_size(v.size()),
    m_ptr(v.c_ptr())
  {}

  /*!
    Ctor from a range of pointers.
    \param R R.m_begin will be the pointer and R.m_end-R.m_begin the size.
   */
  const_c_array(range_type<iterator> R):
    m_size(R.m_end - R.m_begin),
    m_ptr((m_size > 0) ? &*R.m_begin : NULL)
  {}


  /*!
    Reinterpret style cast for c_array. It is required
    that the sizeof(T)*size() evenly divides sizeof(S).
    \tparam S type to which to be reinterpreted casted
   */
  template<typename S>
  const_c_array<S>
  reinterpret_pointer(void) const
  {
    const S *ptr;
    size_type num_bytes(size() * sizeof(T));
    assert(num_bytes % sizeof(S) == 0);
    ptr = reinterpret_cast<const S*>(c_ptr());
    return const_c_array<S>(ptr, num_bytes / sizeof(S));
  }

  c_array<T>
  const_cast_pointer(void) const
  {
    T *q;
    q = const_cast<T*>(c_ptr());
    return c_array<T>(q, size());
  }

  /*!
    Pointer of the const_c_array.
   */
  const T*
  c_ptr(void) const
  {
    return m_ptr;
  }

  /*!
    Pointer to the element one past
    the last element of the const_c_array.
   */
  const T*
  end_c_ptr(void) const
  {
    return m_ptr + m_size;
  }

  /*!
    Access named element of const_c_array, under
    debug build also performs bounds checking.
    \param j index
   */
  const T&
  operator[](size_type j) const
  {
    assert(c_ptr()!=NULL);
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      assert(j < m_size);
    #endif
    return c_ptr()[j];
  }

  /*!
     STL compliant function.
   */
  bool
  empty(void) const
  {
    return m_size == 0;
  }

  /*!
    STL compliant function.
   */
  size_type
  size(void) const
  {
    return m_size;
  }

  /*!
    STL compliant function.
   */
  const_iterator
  begin(void) const
  {
    return const_iterator(c_ptr());
  }

  /*!
    STL compliant function.
   */
  const_reverse_iterator
  rbegin(void) const
  {
    return const_reverse_iterator(end());
  }

  /*!
    STL compliant function.
   */
  const_iterator
  end(void) const
  {
    return const_iterator(c_ptr() + static_cast<difference_type>(size()) );
  }

  /*!
    STL compliant function.
   */
  const_reverse_iterator
  rend(void) const
  {
    return const_reverse_iterator(begin());
  }

  /*!
    Returns the range of the const_c_array as an
    const_iterator range.
   */
  range_type<const_iterator>
  range(void) const
  {
    return range_type<const_iterator>(begin(), end());
  }

  /*!
    Returns the range of the const_c_array as a
    const_reverse_iterator range
   */
  range_type<const_reverse_iterator>
  reverse_range(void) const
  {
    return range_type<const_reverse_iterator>(rbegin(), rend());
  }

  /*!
    Equivalent to
    \code
    operator[](size()-1-I)
    \endcode
    \param I index from the back to retrieve, I=0
             corrseponds to the back of the array.
   */
  const T&
  back(size_type I) const
  {
    assert(I < size());
    return (*this)[size() - 1 - I];
  }

  /*!
    STL compliant function.
   */
  const T&
  back(void) const
  {
    return (*this)[size() - 1];
  }

  /*!
    STL compliant function.
   */
  const T&
  front(void) const
  {
    return (*this)[0];
  }

  /*!
    Returns a sub-array
    \param pos position of returned sub-array to start,
               i.e. returned c_array's c_ptr() will return
               this->c_ptr()+pos. It is an error is pos
               is negative.
    \param length length of sub array to return, note
                  that it is an error if length+pos>size()
                  or if length is negative.
   */
  const_c_array
  sub_array(size_type pos, size_type length) const
  {
    assert(pos + length <= m_size);
    return const_c_array(m_ptr + pos, length);
  }

  /*!
    Returns a sub-array, equivalent to
    \code
    sub_array(pos, size()-pos)
    \endcode
    \param pos position of returned sub-array to start,
               i.e. returned c_array's c_ptr() will return
               this->c_ptr()+pos. It is an error is pos
               is negative.
   */
  const_c_array
  sub_array(size_type pos) const
  {
    assert(pos <= m_size);
    return const_c_array(m_ptr + pos, m_size - pos);
  }

  /*!
    Returns a sub-array, equivalent to
    \code
    sub_array(R.m_begin, R.m_end-R.m_begin)
    \endcode
    \param R range of returned sub-array
   */
  template<typename I>
  const_c_array
  sub_array(range_type<I> R) const
  {
    return sub_array(R.m_begin, R.m_end - R.m_begin);
  }

  /*!
    Returns true if and only if the passed
    the const_c_array references exactly the same
    data as this const_c_array.
    \param rhs const_c_array to which to compare
   */
  bool
  same_data(const const_c_array &rhs) const
  {
    return m_size == rhs.m_size
      and m_ptr == rhs.m_ptr;
  }

private:
  size_type m_size;
  const T *m_ptr;
};

template<typename T>
bool
c_array<T>::
same_data(const const_c_array<T> &rhs) const
{
  return rhs.same_data(*this);
}

/*! @} */

} //namespace
