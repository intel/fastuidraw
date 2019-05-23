/*!
 * \file vecN.hpp
 * \brief file vecN.hpp
 *
 * Adapted from: vecN.hpp of WRATH:
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/math.hpp>

namespace fastuidraw
{
/*!\addtogroup Utility
 * @{
 */

/*!
 * \brief
 * vecN is a simple static array class with no virtual
 * functions and no memory overhead. Supports runtim array
 * index checking and STL style iterators via pointer iterators.
 *
 * \param T typename with a constructor that takes no arguments.
 * \param N size of array
 */
template<typename T, size_t N>
class vecN
{
public:

  enum
    {
      /*!
       * Enumeration value for length of array.
       */
      array_size = N
    };

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef T* pointer;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef const T* const_pointer;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef T& reference;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef const T& const_reference;

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
   * Ctor, no intiliaztion on POD types.
   */
  vecN(void)
  {}

  /*!
   * Ctor.
   * Calls T::operator= on each element of the array.
   * \param value constant reference to a T value.
   */
  explicit
  vecN(const T &value)
  {
    for(size_type i = 0; i < N; ++i)
      {
        operator[](i) = T(value);
      }
  }

  /*!
   * Copy constructor from array of different size.
   * Calls T::operator= on each array element, if M<N then
   * for each element beyond M, T::operator=
   * is called with the parameter value.
   * \tparam M size of other array.
   * \param obj constant reference to copy elements from.
   * \param value constant reference for value to use beyond index M
   */
  template<size_t M>
  explicit
  vecN(const vecN<T, M> &obj, const T &value = T())
  {
    size_type i;

    for(i = 0; i < t_min(N, M); ++i)
      {
        operator[](i) = obj[i];
      }

    for(; i < N; ++i)
      {
        operator[](i) = value;
      }
  }

  /*!
   * Ctor
   * Calls T::operator= on each array element, if M<N then
   * for each element beyond M, T::operator=
   * is called with the parameter value.
   * \tparam M size of other array.
   * \param obj constant reference to copy elements from.
   * \param value constant reference for value to use beyond index M
   */
  template<typename S, size_type M>
  explicit
  vecN(const vecN<S, M> &obj, const T &value = T())
  {
    size_type i;

    for(i = 0; i < t_min(N, M); ++i)
      {
        operator[](i) = T(obj[i]);
      }

    for(; i < N; ++i)
      {
        operator[](i) = value;
      }
  }

  /*!
   * Copy constructor from array of different size specifying the range
   * Copies every stride'th value stored at obj starting at index start
   * to this. For elements of this which are not assigned in this
   * fashion, they are assigned as default_value.
   * \tparam M size of other array.
   * \param obj constant reference to copy elements from.
   * \param start first index of M to use
   * \param stride stride of copying
   * \param default_value
   */
  template<size_t M>
  vecN(const vecN<T, M> &obj,
       size_type start, size_type stride = 1,
       const T &default_value = T())
  {
    size_type i,j;

    for(i = 0, j = start; i < N && j < M; ++i, j += stride)
      {
        operator[](i) = obj[i];
      }

    for(; i < N; ++i)
      {
        operator[](i) = default_value;
      }
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=2.
   * \param px value to which to assing the return value of x().
   * \param py value to which to assing the return value of y().
   */
  vecN(const T &px, const T &py)
  {
    FASTUIDRAWstatic_assert(N == 2);
    operator[](0) = px;
    operator[](1) = py;
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=3.
   * \param px value to which to assing the return value of x().
   * \param py value to which to assing the return value of y().
   * \param pz value to which to assing the return value of z().
   */
  vecN(const T &px, const T &py, const T &pz)
  {
    FASTUIDRAWstatic_assert(N == 3);
    operator[](0) = px;
    operator[](1) = py;
    operator[](2) = pz;
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=4.
   * \param px value to which to assing the return value of x().
   * \param py value to which to assing the return value of y().
   * \param pz value to which to assing the return value of z().
   * \param pw value to which to assing the return value of w().
   */
  vecN(const T &px, const T &py, const T &pz, const T &pw)
  {
    FASTUIDRAWstatic_assert(N == 4);
    operator[](0) = px;
    operator[](1) = py;
    operator[](2) = pz;
    operator[](3) = pw;
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=5
   * \param p0 value to which to assing the return value of operator[](0)
   * \param p1 value to which to assing the return value of operator[](1)
   * \param p2 value to which to assing the return value of operator[](2)
   * \param p3 value to which to assing the return value of operator[](3)
   * \param p4 value to which to assing the return value of operator[](4)
   */
  vecN(const T &p0, const T &p1, const T &p2,
       const T &p3, const T &p4)
  {
    FASTUIDRAWstatic_assert(N == 5);
    operator[](0) = p0;
    operator[](1) = p1;
    operator[](2) = p2;
    operator[](3) = p3;
    operator[](4) = p4;
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=6
   * \param p0 value to which to assing the return value of operator[](0)
   * \param p1 value to which to assing the return value of operator[](1)
   * \param p2 value to which to assing the return value of operator[](2)
   * \param p3 value to which to assing the return value of operator[](3)
   * \param p4 value to which to assing the return value of operator[](4)
   * \param p5 value to which to assing the return value of operator[](5)
   */
  vecN(const T &p0, const T &p1, const T &p2,
       const T &p3, const T &p4, const T &p5)
  {
    FASTUIDRAWstatic_assert(N == 6);
    operator[](0) = p0;
    operator[](1) = p1;
    operator[](2) = p2;
    operator[](3) = p3;
    operator[](4) = p4;
    operator[](5) = p5;
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=7
   * \param p0 value to which to assing the return value of operator[](0)
   * \param p1 value to which to assing the return value of operator[](1)
   * \param p2 value to which to assing the return value of operator[](2)
   * \param p3 value to which to assing the return value of operator[](3)
   * \param p4 value to which to assing the return value of operator[](4)
   * \param p5 value to which to assing the return value of operator[](5)
   * \param p6 value to which to assing the return value of operator[](6)
   */
  vecN(const T &p0, const T &p1, const T &p2,
       const T &p3, const T &p4, const T &p5,
       const T &p6)
  {
    FASTUIDRAWstatic_assert(N == 7);
    operator[](0) = p0;
    operator[](1) = p1;
    operator[](2) = p2;
    operator[](3) = p3;
    operator[](4) = p4;
    operator[](5) = p5;
    operator[](6) = p6;
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=8
   * \param p0 value to which to assing the return value of operator[](0)
   * \param p1 value to which to assing the return value of operator[](1)
   * \param p2 value to which to assing the return value of operator[](2)
   * \param p3 value to which to assing the return value of operator[](3)
   * \param p4 value to which to assing the return value of operator[](4)
   * \param p5 value to which to assing the return value of operator[](5)
   * \param p6 value to which to assing the return value of operator[](6)
   * \param p7 value to which to assing the return value of operator[](7)
   */
  vecN(const T &p0, const T &p1, const T &p2,
       const T &p3, const T &p4, const T &p5,
       const T &p6, const T &p7)
  {
    FASTUIDRAWstatic_assert(N == 8);
    operator[](0) = p0;
    operator[](1) = p1;
    operator[](2) = p2;
    operator[](3) = p3;
    operator[](4) = p4;
    operator[](5) = p5;
    operator[](6) = p6;
    operator[](7) = p7;
  }

  /*!
   * Conveniance ctor, will fail to compile unless N=9
   * \param p0 value to which to assing the return value of operator[](0)
   * \param p1 value to which to assing the return value of operator[](1)
   * \param p2 value to which to assing the return value of operator[](2)
   * \param p3 value to which to assing the return value of operator[](3)
   * \param p4 value to which to assing the return value of operator[](4)
   * \param p5 value to which to assing the return value of operator[](5)
   * \param p6 value to which to assing the return value of operator[](6)
   * \param p7 value to which to assing the return value of operator[](7)
   * \param p8 value to which to assing the return value of operator[](8)
   */
  vecN(const T &p0, const T &p1, const T &p2,
       const T &p3, const T &p4, const T &p5,
       const T &p6, const T &p7, const T &p8)
  {
    FASTUIDRAWstatic_assert(N == 9);
    operator[](0) = p0;
    operator[](1) = p1;
    operator[](2) = p2;
    operator[](3) = p3;
    operator[](4) = p4;
    operator[](5) = p5;
    operator[](6) = p6;
    operator[](7) = p7;
    operator[](8) = p8;
  }

  /*!
   * Conveniance function.
   * \param p gives valeus for array indices 0 to N-2 inclusive
   * \param d gives value for array index N-1
   */
  vecN(const vecN<T, N-1> &p, const T &d)
  {
    FASTUIDRAWstatic_assert(N>1);
    for(size_type i=0;i<N-1;++i)
      {
        operator[](i) = p[i];
      }
    operator[](N - 1) = d;
  }

  /*!
   * Returns a C-style pointer to the array.
   */
  T*
  c_ptr(void) { return m_data; }

  /*!
   * Returns a constant C-style pointer to the array.
   */
  const T*
  c_ptr(void) const { return m_data; }

  /*!
   * Return a constant refernce to the j'th element.
   * \param j index of element to return.
   */
  const_reference
  operator[](size_type j) const
  {
    FASTUIDRAWassert(j < N);
    return c_ptr()[j];
  }

  /*!
   * Return a refernce to the j'th element.
   * \param j index of element to return.
   */
  reference
  operator[](size_type j)
  {
    FASTUIDRAWassert(j < N);
    return c_ptr()[j];
  }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](0).
   */
  reference
  x(void) { FASTUIDRAWstatic_assert(N >= 1); return c_ptr()[0]; }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](1). Fails to compile
   * if N is not atleast 2.
   */
  reference
  y(void) { FASTUIDRAWstatic_assert(N >= 2); return c_ptr()[1]; }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](2). Fails to compile
   * if N is not atleast 3.
   */
  reference
  z(void) { FASTUIDRAWstatic_assert(N >= 3); return c_ptr()[2]; }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](3). Fails to compile
   * if N is not atleast 4.
   */
  reference
  w(void) { FASTUIDRAWstatic_assert(N >= 4); return c_ptr()[3]; }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](0).
   */
  const_reference
  x(void) const { FASTUIDRAWstatic_assert(N >= 1); return c_ptr()[0]; }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](1). Fails to compile
   * if N is not atleast 2.
   */
  const_reference
  y(void) const { FASTUIDRAWstatic_assert(N >= 2); return c_ptr()[1]; }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](2). Fails to compile
   * if N is not atleast 3.
   */
  const_reference
  z(void) const { FASTUIDRAWstatic_assert(N >= 3); return c_ptr()[2]; }

  /*!
   * Conveniance readability member function,
   * equivalent to operator[](3). Fails to compile
   * if N is not atleast 4.
   */
  const_reference
  w(void) const { FASTUIDRAWstatic_assert(N >= 4); return c_ptr()[3]; }

  /*!
   * Assignment operator, performs operator=(T&, const T&)
   * on each element.
   * \param obj constant reference to a same sized array.
   */
  const vecN&
  operator=(const vecN &obj)
  {
    if (&obj != this)
      {
        for(size_type i = 0; i < N; ++i)
          {
            operator[](i) = obj[i];
          }
      }
    return *this;
  }

  /*!
   * Set all values of array, performs operator=(T&, const T&)
   * on each element of the array agains obj.
   * \param obj Value to set all objects as.
   */
  const vecN&
  fill(const T &obj)
  {
    for(size_type i = 0; i < N; ++i)
      {
        operator[](i) = obj;
      }

    return *this;
  }

  /*!
   * Component-wise negation operator.
   * returns the componenet-wise negation
   */
  vecN
  operator-(void) const
  {
    vecN retval;
    for(size_type i = 0; i < N; ++i)
      {
        retval[i] = -operator[](i);
      }
    return retval;
  }

  /*!
   * Compoenent wise addition operator
   * returns the componenet wise addition of two
   * arrays.
   * \param obj right hand side of + operator
   */
  vecN
  operator+(const vecN &obj) const
  {
    vecN retval(*this);
    retval += obj;
    return retval;
  }

  /*!
   * Component-wise subtraction operator
   * returns the componenet-wise subtraction of two
   * arrays.
   * \param obj right hand side of - operator
   */
  vecN
  operator-(const vecN &obj) const
  {
    vecN retval(*this);
    retval -= obj;
    return retval;
  }

  /*!
   * Component-wise multiplication operator,
   * returns the componenet-wise multiplication of
   * two arrays.
   * \param obj right hand side of * operator
   */
  vecN
  operator*(const vecN &obj) const
  {
    vecN retval(*this);
    retval *= obj;
    return retval;
  }

  /*!
   * Component-wise division operator,
   * returns the componenet-wise division of
   * two arrays.
   * \param obj right hand side of / operator
   */
  vecN
  operator/(const vecN &obj) const
  {
    vecN retval(*this);
    retval /= obj;
    return retval;
  }

  /*!
   * Component-wise modulas operator
   * returns the componenet-wise modulas of
   * two arrays.
   * \param obj right hand side of % operator
   */
  vecN
  operator%(const vecN &obj) const
  {
    vecN retval(*this);
    retval %= obj;
    return retval;
  }

  /*!
   * Component-wise multiplication operator against a singleton
   * \param obj right hand side of * operator
   */
  vecN
  operator*(const T &obj) const
  {
    vecN retval(*this);
    retval *= obj;
    return retval;
  }

  /*!
   * Component-wise division against a singleton
   * \param obj right hand side of / operator
   */
  vecN
  operator/(const T &obj) const
  {
    vecN retval(*this);
    retval /= obj;
    return retval;
  }

  /*!
   * Component-wise modulas against a singleton
   * \param obj right hand side of % operator
   */
  vecN
  operator%(const T &obj) const
  {
    vecN retval(*this);
    retval %= obj;
    return retval;
  }

  /*!
   * Component-wise addition increment operator against an
   * array of possibly different size, if M is smaller
   * then only those indexes less than M are affected.
   * \param obj right hand side of += operator
   */
  template<size_t M>
  void
  operator+=(const vecN<T, M> &obj)
  {
    for(size_type i = 0;i < t_min(M, N); ++i)
      {
        operator[](i) += obj[i];
      }
  }

  /*!
   * Component-wise subtraction increment operator against an
   * array of possibly different size, if M is smaller
   * then only those indexes less than M are affected.
   * \param obj right hand side of -= operator
   */
  template<size_t M>
  void
  operator-=(const vecN<T, M> &obj)
  {
    for(size_type i = 0; i < t_min(M, N); ++i)
      {
        operator[](i) -= obj[i];
      }
  }

  /*!
   * Component-wise multiplication increment operator against an
   * array of possibly different size, if M is smaller
   * then only those indexes less than M are affected.
   * \param obj right hand side of *= operator
   */
  template<size_t M>
  void
  operator*=(const vecN<T, M> &obj)
  {
    for(size_type i = 0; i < t_min(N, M); ++i)
      {
        operator[](i) *= obj[i];
      }
  }

  /*!
   * Component-wise division increment operator against an
   * array of possibly different size, if M is smaller
   * then only those indexes less than M are affected.
   * \param obj right hand side of /= operator
   */
  template<size_t M>
  void
  operator/=(const vecN<T, M> &obj)
  {
    for(size_type i = 0; i < t_min(M, N); ++i)
      {
        operator[](i) /= obj[i];
      }
  }

  /*!
   * Component-wise modulas increment operator against an
   * array of possibly different size, if M is smaller
   * then only those indexes less than M are affected.
   * \param obj right hand side of /= operator
   */
  template<size_t M>
  void
  operator%=(const vecN<T, M> &obj)
  {
    for(size_type i = 0; i < t_min(M, N); ++i)
      {
        operator[](i) %= obj[i];
      }
  }

  /*!
   * Increment multiply operator against a singleton,
   * i.e. increment multiple each element of this against
   * the passed T value.
   * \param obj right hind side of operator*=
   */
  void
  operator*=(const T &obj)
  {
    for(size_type i = 0; i < N; ++i)
      {
        operator[](i) *= obj;
      }
  }

  /*!
   * Increment divide operator against a singleton,
   * i.e. increment divide each element of this against
   * the passed T value.
   * \param obj right hind side of operator/=
   */
  void
  operator/=(const T &obj)
  {
    for(size_type i = 0; i < N; ++i)
      {
        operator[](i) /= obj;
      }
  }

  /*!
   * Increment divide operator against a singleton,
   * i.e. increment divide each element of this against
   * the passed T value.
   * \param obj right hind side of operator/=
   */
  void
  operator%=(const T &obj)
  {
    for(size_type i = 0; i < N; ++i)
      {
        operator[](i) %= obj;
      }
  }

  /*!
   * Component-wise multiplication against a singleton
   * \param obj left hand side of * operator
   * \param vec right hand side of * operator
   */
  friend
  vecN operator*(const T &obj, const vecN &vec)
  {
    vecN retval(obj);
    retval *= vec;
    return retval;
  }

  /*!
   * Component-wise divition against a singleton
   * \param obj left hand side of / operator
   * \param vec right hand side of / operator
   */
  friend
  vecN operator/(const T &obj, const vecN &vec)
  {
    vecN retval(obj);
    retval /= vec;
    return retval;
  }

  /*!
   * Performs inner product against another vecN.
   * uses operator+=(T&, const T&) and operator*(T, T)
   * \param obj vecN to perform inner product against
   */
  T
  dot(const vecN &obj) const
  {
    T retval(operator[](0) * obj[0]);

    for(size_type i = 1; i < N; ++i)
      {
        retval += operator[](i) * obj[i];
      }
    return retval;
  }

  /*!
   * Conveninace function, equivalent to
   * \code dot(*this) \endcode
   */
  T
  magnitudeSq(void) const
  {
    return dot(*this);
  }

  /*!
   * Conveninace function, equivalent to
   * \code t_sqrt(dot(*this)) \endcode
   */
  T
  magnitude(void) const
  {
    return t_sqrt(magnitudeSq());
  }

  /*!
   * Computes the sum of t_abs() of each
   * of the elements of the vecN.
   */
  T
  L1norm(void) const
  {
    T retval(t_abs(operator[](0)));

    for(size_type i = 1; i < N; ++i)
      {
        retval += t_abs(operator[](i));
      }
    return retval;
  }

  /*!
   * Conveniance function, increments
   * this vecN by dood*mult by doing so
   * on each component individually,
   * slighly more efficient than
   * operator+=(dood*mult).
   */
  void
  AddMult(const vecN &dood, const T &mult)
  {
    for(size_type i = 0; i < N; ++i)
      {
        operator[](i) += mult * dood[i];
      }
  }

  /*!
   * Conveniance geometryic function; if dot(*this, referencePt)
   * is negative, negates the elements of this
   * \param referencePt refrence point to face forward to.
   */
  void
  face_forward(const vecN &referencePt)
  {
    T val;

    val = dot(*this, referencePt);
    if (val < T(0))
      {
        for(size_type i = 0; i < N; ++i)
          {
            operator[](i) = -operator[](i);
          }
      }
  }

  /*!
   * Equality operator by checking each array index individually.
   * \param obj vecN to which to compare.
   */
  bool
  operator==(const vecN &obj) const
  {
    if (this == &obj)
      {
        return true;
      }
    for(size_type i = 0; i < N; ++i)
      {
        if (obj[i] != operator[](i))
          {
            return false;
          }
      }
    return true;
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * !operator==(obj)
   * \endcode
   * \param obj vecN to which to compare.
   */
  bool
  operator!=(const vecN &obj) const
  {
    return !operator==(obj);
  }

  /*!
   * Returns lexographical operator< by performing
   * on individual elements.
   * \param obj vecN to which to compare.
   */
  bool
  operator<(const vecN &obj) const
  {
    if (this == &obj)
      {
        return false;
      }
    for(size_type i = 0; i < N; ++i)
      {
        if (operator[](i) < obj[i])
          {
            return true;
          }
        if (obj[i] < operator[](i))
          {
            return false;
          }
      }
    return false;
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * operator<(obj) || operator==(obj);
   * \endcode
   * \param obj vecN to which to compare.
   */
  bool
  operator<=(const vecN &obj) const
  {
    return operator<(obj) || operator==(obj);
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * obj.operator<(*this)
   * \endcode
   * \param obj vecN to which to compare.
   */
  bool
  operator>(const vecN &obj) const
  {
    return obj.operator<(*this);
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * !operator<(obj)
   * \endcode
   * \param obj vecN to which to compare.
   */
  bool
  operator>=(const vecN &obj) const
  {
    return !operator<(obj);
  }

  /*!
   * Normalize this vecN up to a tolerance,
   * equivalent to
   * \code
   * operator/=(t_sqrt(t_max(tol, magnitudeSq())))
   * \endcode
   * \param tol tolerance to avoid dividing by too small values
   */
  void
  normalize(T tol)
  {
    T denom;
    denom = magnitudeSq();
    denom = t_sqrt(t_max(denom, tol));
    (*this) /= denom;
  }

  /*!
   * Normalize this vecN to a default tolerancee,
   * equivalent to
   * \code
   * normalize( T(0.00001*0.00001)
   * \endcode
   */
  void
  normalize(void)
  {
    normalize(T(0.00001*0.00001) );
  }

  /*!
   * Returns the vector that would be made
   * by calling normalize(void).
   */
  vecN
  unit_vector(void) const
  {
    vecN retval(*this);
    retval.normalize();
    return retval;
  }

  /*!
   * Returns the vector that would be made
   * by calling normalize(T).
   */
  vecN
  unit_vector(T tol) const
  {
    vecN retval(*this);
    retval.normalize(tol);
    return retval;
  }

  /*!
   * Conveniance function only for N = 2, equivalent to
   * \code
   *  t_atan(y(), x())
   * \endcode
   */
  T
  atan(void) const
  {
    FASTUIDRAWstatic_assert(N == 2);
    return t_atan2(y(), x());
  }

  /*!
   * STL compliand size function, note that it is static
   * since the size of the array is determined by the template
   * parameter N.
   */
  static
  size_t
  size(void) { return static_cast<size_type>(N); }

  /*!
   * STL compliant iterator function.
   */
  iterator
  begin(void) { return iterator(c_ptr()); }

  /*!
   * STL compliant iterator function.
   */
  const_iterator
  begin(void) const { return const_iterator(c_ptr()); }

  /*!
   * STL compliant iterator function.
   */
  iterator
  end(void) { return iterator(c_ptr() + static_cast<difference_type>(size()) ); }

  /*!
   * STL compliant iterator function.
   */
  const_iterator
  end(void) const { return const_iterator(c_ptr() + static_cast<difference_type>(size()) ); }

  /*!
   * STL compliant back() function.
   */
  reference
  back(void) { return (*this)[size() - 1]; }

  /*!
   * STL compliant back() function.
   */
  const_reference
  back(void) const { return (*this)[size() - 1]; }

  /*!
   * STL compliant front() function.
   */
  reference
  front(void) { return (*this)[0]; }

  /*!
   * STL compliant front() function.
   */
  const_reference
  front(void) const { return (*this)[0]; }

private:
  T m_data[N];
};

/*!
 * conveniance function, equivalent to
 * \code
 * a.dot(b)
 * \endcode
 * \param a object to perform dot
 * \param b object passed as parameter to dot.
 */
template<typename T, size_t N>
T
dot(const vecN<T, N> &a, const vecN<T, N> &b)
{
  return a.dot(b);
}

/*!
 * conveniance function, equivalent to
 * \code
 * in.magnitudeSq()
 * \endcode
 * \param in object to perform magnitudeSq
 */
template<typename T, size_t N>
inline
T
magnitudeSq(const vecN<T, N> &in)
{
  return in.magnitudeSq();
}

/*!
 * conveniance function, equivalent to
 * \code
 * in.magnitude()
 * \endcode
 * \param in object to perform magnitude
 */
template<typename T, size_t N>
inline
T
magnitude(const vecN<T, N> &in)
{
  return in.magnitude();
}

/*!
 * conveniance function to compare magnitude squares, equivalent to
 * \code
 * a.magnitudeSq()<b.magnitudeSq();
 * \endcode
 * \param a left hand side of comparison
 * \param b right hand side of comparison
 */
template<typename T, size_t N>
bool
magnitude_compare(const vecN<T, N> &a,
                  const vecN<T, N> &b)
{
  return a.magnitudeSq()<b.magnitudeSq();
}

  /*!
   * unvecN extracts from a type T \ref vecN::array_size
   * and \ref vecN::value_type if T is a vecN<S, N> for
   * some type S and N.
   */
  template<typename T>
  class unvecN
  {
  public:
    enum
      {
        /*!
         * If T is the type vecN<S, N> for some type S,
         * then is the value of N, otherwise is 1.
         */
        array_size = 1,
      };

    /*!
     * If T is the type vecN<S, N> for some type S,
     * then is the type S, otherwise is the type T.
     */
    typedef T type;
  };

  ///@cond
  template<typename T, size_t N>
  class unvecN<vecN<T, N> >
  {
  public:
    enum
      {
        array_size = N,
      };
    typedef T type;
  };

  template<typename T, size_t N>
  class unvecN<vecN<T, N> const >
  {
  public:
    enum
      {
        array_size = N,
      };
    typedef T const type;
  };
  ///@endcond

  /*!
   * Conveniance typedef
   */
  typedef vecN<float, 1> vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<float, 2> vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<float, 3> vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<float, 4> vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<double, 1> dvec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<double, 2> dvec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<double, 3> dvec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<double, 4> dvec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 1> ivec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 2> ivec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 3> ivec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 4> ivec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 1> uvec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 2> uvec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 3> uvec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 4> uvec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<int8_t, 1> i8vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int8_t, 2> i8vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int8_t, 3> i8vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int8_t, 4> i8vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<int16_t, 1> i16vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int16_t, 2> i16vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int16_t, 3> i16vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int16_t, 4> i16vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 1> i32vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 2> i32vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 3> i32vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int32_t, 4> i32vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<int64_t, 1> i64vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int64_t, 2> i64vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int64_t, 3> i64vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<int64_t, 4> i64vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<uint8_t, 1> u8vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint8_t, 2> u8vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint8_t, 3> u8vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint8_t, 4> u8vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<uint16_t, 1> u16vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint16_t, 2> u16vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint16_t, 3> u16vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint16_t, 4> u16vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 1> u32vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 2> u32vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 3> u32vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint32_t, 4> u32vec4;

  /*!
   * Conveniance typedef
   */
  typedef vecN<uint64_t, 1> u64vec1;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint64_t, 2> u64vec2;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint64_t, 3> u64vec3;
  /*!
   * Conveniance typedef
   */
  typedef vecN<uint64_t, 4> u64vec4;

/*!
 * Pack 4 float values into a vecN<uint32_t, 4>
 * values via pack_float().
 * \param x x-value
 * \param y y-value
 * \param z z-value
 * \param w w-value
 */
inline
uvec4
pack_vec4(float x, float y, float z, float w)
{
  uvec4 return_value;
  return_value.x() = pack_float(x);
  return_value.y() = pack_float(y);
  return_value.z() = pack_float(z);
  return_value.w() = pack_float(w);
  return return_value;
}

  /*!
   * Compute the area of a triangle using Heron's rule.
   */
  template<typename T, size_t N>
  T
  triangle_area(const vecN<T, N> &p0,
                const vecN<T, N> &p1,
                const vecN<T, N> &p2)
  {
    T d, d0, d1, d2;
    d0 = p0.magnitude();
    d1 = p1.magnitude();
    d2 = p2.magnitude();
    d = (d0 + d1 + d2) / T(2);
    return t_sqrt(d * (d - d0) * (d - d1) * (d - d2));
  }

  /*!
   * Compute the cross produces of two vectors
   */
  template<typename T>
  vecN<T, 3>
  cross_product(const vecN<T, 3> &a, const vecN<T, 3> &b)
  {
    return vecN<T, 3>(a.y() * b.z() - a.z() * b.y(),
                      a.z() * b.x() - a.x() * b.z(),
                      a.x() * b.y() - a.y() * b.x());
  }

/*! @} */

} //namespace
