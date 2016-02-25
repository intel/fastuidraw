/*!
 * \file matrix.hpp
 * \brief file matrix.hpp
 *
 * Adapted from: matrixGL.hpp of WRATH:
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

#include <fastuidraw/util/math.hpp>
#include <fastuidraw/util/vecN.hpp>

namespace fastuidraw {

/*!\addtogroup Utility
  @{
 */

/*!
  A generic matrix class whose
  entries are packed in a form
  suitable for OpenGL:

  \verbatim
  data[ 0 ] data[ N  ] data[2N  ]  .. data[ N(M-1)  ]
  data[ 1 ] data[ N+1] data[2N+1]  .. data[ N(M-1)+1]
  .
  .
  data[N-1] data[2N-1] data[3N-1]  .. data[  N*M - 1]
  \endverbatim

 i.e. data[ row + col*N] --> matrix(row,col),
 with 0<=row<N, 0<=col<M
 operator()(row,col) --> data[row+col*N] <--> matrix(row,col)

 If FASTUIDRAW_VECTOR_BOUND_CHECK is defined,
 will perform bounds checking.
 \tparam N hieght of matrix
 \tparam M width of matrix
 \tparam T matrix entry type
*/
template<size_t N, size_t M, typename T=float>
class matrixNxM
{
private:
  vecN<T,N*M> m_data;

public:

  /*!
    Copy-constructor for a NxN matrix.
    \param obj matrix to copy
   */
  matrixNxM(const matrixNxM &obj):
    m_data(obj.m_data)
  {}

  /*!
    Ctor.
    Initializes an NxN matrix as diagnols are 1
    and other values are 0, for square matrices,
    that is the identity matrix.
  */
  matrixNxM(void)
  {
    reset();
  }

  /*!
    Set matrix as identity.
   */
  void
  reset(void)
  {
    for(unsigned int i=0;i<M;++i)
      {
        for(unsigned int j=0;j<N;++j)
          {
            m_data[N*i+j]=
              (i==j)?
              T(1):
              T(0);
          }
      }
  }

  /*!
    Swaps the values between this and the parameter matrix.
    \param obj matrix to swap values with
   */
  void
  swap(matrixNxM &obj)
  {
    m_data.swap(obj.m_data);
  }

  /*!
    Returns a c-style pointer to the data.
   */
  T*
  c_ptr(void) { return m_data.c_ptr(); }

  /*!
    Returns a constant c-style pointer to the data.
   */
  const T*
  c_ptr(void) const { return m_data.c_ptr(); }

  /*!
    Returns a reference to raw data vector in the matrix.
   */
  vecN<T,N*M>&
  raw_data(void) { return m_data; }

  /*!
    Returns a const reference to the raw data vectors in the matrix.
    */
  const vecN<T,N*M>&
  raw_data(void) const { return m_data; }

  /*!
    Returns the value in the vector corresponding M[i,j]
    \param row row(vertical coordinate) in the matrix
    \param col column(horizontal coordinate) in the matrix
   */
  T&
  operator()(unsigned int row, unsigned int col)
  {
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      assert(row<N);
      assert(col<M);
    #endif
    return m_data[N*col+row];
  }

  /*!
    Returns the value in the vector corresponding M[i,j]
    \param row row(vertical coordinate) in the matrix
    \param col column(horizontal coordinate) in the matrix
   */
  const T&
  operator()(unsigned int row, unsigned int col) const
  {
    #ifdef FASTUIDRAW_VECTOR_BOUND_CHECK
      assert(row<N);
      assert(col<M);
    #endif
    return m_data[N*col+row];
  }

  /*!
    Compute the transpose of the matrix
    \param retval location to which to write the transpose
   */
  void
  transpose(matrixNxM<M,N,T> &retval) const
  {
    for(unsigned int i=0;i<N;++i)
      {
        for(unsigned int j=0;j<M;++j)
          {
            retval.operator()(i,j)=operator()(j,i);
          }
      }
  }

  /*!
    Returns a transpose of the matrix.
   */
  matrixNxM<M,N,T>
  transpose(void) const
  {
    matrixNxM<M,N,T>  retval;
    transpose(retval);
    return retval;
  }

  /*!
    Operator for adding matrices together.
    \param matrix target matrix
   */
  matrixNxM
  operator+(const matrixNxM &matrix) const
  {
    matrixNxM out;
    out.m_data= m_data+matrix.m_data;
    return out;
  }

  /*!
    Operator for substracting matrices from each other.
    \param matrix target matrix
   */
  matrixNxM
  operator-(const matrixNxM &matrix) const
  {
    matrixNxM out;
    out.m_data= m_data-matrix.m_data;
    return out;
  }

  /*!
    Multiplies the matrix with a given scalar.
    \param value scalar to multiply with
   */
  matrixNxM
  operator*(T value) const
  {
    matrixNxM out;
    out.m_data= m_data*value;
    return out;
  }

  /*!
    Multiplies the given matrix with the given scalar
    and returns the resulting matrix.
    \param value scalar to multiply with
    \param matrix target matrix to multiply
   */
  friend
  matrixNxM
  operator*(T value, const matrixNxM &matrix)
  {
    matrixNxM out;
    out.m_data=matrix.m_data*value;
    return out;
  }

  /*!
    Multiplies this matrix with the given matrix.
    \param matrix target matrix
   */
  template<size_t K>
  matrixNxM<N,K,T>
  operator*(const matrixNxM<M,K,T> &matrix) const
  {
    unsigned int i,j,k;
    matrixNxM<N,K,T> out;

    for(i=0;i<N;++i)
      {
        for(j=0;j<K;++j)
          {
            out.operator()(i,j)=T(0);
            for(k=0;k<M;++k)
              {
                out.operator()(i,j)+=operator()(i,k)*matrix.operator()(k,j);
              }
          }
      }
    return out;
  }

  /*!
    Multiplies the given vector with the matrix.
    \param in target vector
   */
  vecN<T,N>
  operator*(const vecN<T,M> &in) const
  {
    vecN<T,N> retval;

    for(unsigned int i=0;i<N;++i)
      {
        retval[i]=T(0);
        for(unsigned int j=0;j<M;++j)
          {
            retval[i]+=operator()(i,j)*in[j];
          }
      }

    return retval;
  }


  /*!
    Multiplies the target matrix with the given vector
    \param matrix target matrix
    \param in target vector
   */
  friend
  vecN<T,M>
  operator*(const vecN<T,N> &in, const matrixNxM &matrix)
  {
    vecN<T,M> retval;

    for(unsigned int i=0;i<M;++i)
      {
        retval[i]=T(0);
        for(unsigned int j=0;j<N;++j)
          {
            retval[i]+=in[j]*matrix.operator()(j,i);
          }
      }
    return retval;
  }

};



/*!
  Convenience typedef to matrixNxM\<2, float\>
 */
typedef matrixNxM<2, 2, float> float2x2;


/*!
  A projection_params holds the
  data to describe a projection
  matrix with and without perspective.
 */
template<typename T>
class projection_params
{
public:
  T m_top; ///< Top edge of the clipping plane
  T m_bottom; ///< Bottom edge of the clipping plane
  T m_left; ///< Left edge of the clipping plane
  T m_right; ///< Right edge of the clipping plane
  T m_near; ///< Near clipping plane distance
  T m_far; ///< Far clipping plane distance

  /*!
    True when the far clipping plane is not set (and is thus in infinity).
    */
  bool m_farAtinfinity;

  /*!
    Default constructor for projection parameters,
    values are unitialized except for
    m_farAtinfinity which is initialized
    as false
   */
  projection_params(void):
    m_farAtinfinity(false)
  {}

  /*!
    Creates the projection parameters instance according to the given parameters.
    \param l Left
    \param r Right
    \param t Top
    \param b Bottom
    \param n Near clipping plane
    \param f Far clipping plane
   */
  projection_params(T l, T r, T b, T t, T n,T f):
    m_top(t), m_bottom(b), m_left(l), m_right(r), m_near(n), m_far(f),
    m_farAtinfinity(false) {}

  /*!
    Creates the projection parameters instance according to the given parameters,
    where far clipping plane is not defined and is, thus, set to infinity.
    \param l Left
    \param r Right
    \param t Top
    \param b Bottom
    \param n Near clipping plane
   */
  projection_params(T l, T r, T b, T t, T n):
    m_top(t), m_bottom(b), m_left(l), m_right(r), m_near(n), m_far(100000.0f*n),
    m_farAtinfinity(true) {}
};

/*!
  An extension to projection parameters to provide projection parameters
  better suited for orthogonal projection.
  */
template<typename T>
class orthogonal_projection_params:public projection_params<T>
{
public:
  /*!
    Creates projection parameters defined by the given parameters.
    Equivalent to projection_params<T>(l, r, b, t, n, f).
    \param l Left
    \param r Right
    \param t Top
    \param b Bottom
    \param n Near clipping plane
    \param f Far clipping plane
   */
  orthogonal_projection_params(T l, T r, T b, T t, T n, T f):
    projection_params<T>(l, r, b, t, n, f)
  {}

  /*!
    Creates a projection parameters instance defined by the given plane
    size, assuming the clipping planes at Near=T(-1), Far=T(1).
    Equivalent to projection_params<T>(l, r, b, t, T(-1), T(1)).
    \param l Left
    \param r Right
    \param t Top
    \param b Bottom
   */
  orthogonal_projection_params(T l, T r, T b, T t):
    projection_params<T>(l, r, b, t, T(-1), T(1))
  {}
};

/*!
  A representation of a 3x3 matrix, that in addition to the NxN
  matrix functionality provides function for calculating the
  determinant.
 */
template<typename T>
class matrix3x3:public matrixNxM<3,3,T>
{
public:
  /*!
    Conveniance typedef to base class, matrixNxM<3,3,T>
   */
  typedef matrixNxM<3,3,T> base_class;

  /*!
    Initializes the 3x3 matrix as the identity,
    i.e. diagnols are 1 and other values are 0.
   */
  matrix3x3(void):base_class() {}

  /*!
    Copy-constructor for a 3x3 matrix.
    \param obj target matrix to copy.
   */
  matrix3x3(const base_class &obj):base_class(obj) {}

  /*!
    Construct a matrix3x3 M so that
     - M*vecN<T,3>(1,0,0)=T
     - M*vecN<T,3>(0,1,0)=B
     - M*vecN<T,3>(0,0,1)=N
    \param t first row vector
    \param b second row vector
    \param n third row vector
   */
  matrix3x3(const vecN<T,3> &t, const vecN<T,3> &b, const vecN<T,3> &n)
  {
    for(int i=0;i<3;++i)
      {
        base_class::operator()(i,0)=t[i];
        base_class::operator()(i,1)=b[i];
        base_class::operator()(i,2)=n[i];
      }
  }

  /*!
    Initialize the 3x3 matrix with the upper 2x2 corner
    coming from a 2x2 martix and the right column
    coming from a 2-vector. The bottom is initialized
    as 0 in first and second column and bottom right as 1.
   */
  matrix3x3(const matrixNxM<2, 2, T> &mat, const vecN<T, 2> &vec = vecN<T,2>(T(0)))
  {
    for(int i = 0; i < 2; ++i)
      {
        for(int j = 0; j < 2; ++j)
          {
            base_class::operator()(i,j) = mat(i, j);
          }
        base_class::operator()(2, i) = T(0);
        base_class::operator()(i, 2) = vec[i];
      }
    base_class::operator()(2, 2) = T(1);
  }

  /*!fn matrix3x3(const orthogonal_projection_params<T>&)
    Creates a 3x3 orthogonal projection matrix from the given projection
    parameters.
    \param P orthogonal projection parameters for the matrix
   */
  matrix3x3(const orthogonal_projection_params<T> &P):
    base_class()
  {
    orthogonal_projection_matrix(P);
  }

  /*!
    Apply shear on the right of the matrix, algebraicly equivalent to
    \code
    matrix3x3 M;
    M(0, 0) = sx;
    M(1, 1) = sy;
    *this = *this * M;
    \endcode
   */
  void
  shear(T sx, T sy)
  {
    base_class::operator()(0, 0) *= sx;
    base_class::operator()(1, 0) *= sx;
    base_class::operator()(2, 0) *= sx;

    base_class::operator()(0, 1) *= sy;
    base_class::operator()(1, 1) *= sy;
    base_class::operator()(2, 1) *= sy;
  }

  /*!
    Apply scale on the right of the matrix, algebraicly equivalent to
    \code
    matrix3x3 M;
    M(0, 0) = s;
    M(1, 1) = s;
    *this = *this * M;
    \endcode
   */
  void
  scale(T s)
  {
    shear(s, s);
  }

  /*!
    Apply translate on the right of the matrix, algebraicly equivalent to
    \code
    matrix3x3 M;
    M(0, 2) = x;
    M(1, 2) = y;
    *this = *this * M;
    \endcode
    \param x amount by which to translate horizontally
    \param y amount by which to translate vertically
   */
  void
  translate(T x, T y)
  {
    base_class::operator()(0, 2) += x * base_class::operator()(0, 0) + y * base_class::operator()(0, 1);
    base_class::operator()(1, 2) += x * base_class::operator()(1, 0) + y * base_class::operator()(1, 1);
    base_class::operator()(2, 2) += x * base_class::operator()(2, 0) + y * base_class::operator()(2, 1);
  }

  /*!
    Provided as an override conveniance, equivalent to
    \code
    translate(p.x(), p.y())
    \endcode
    \param p amount by which to translate
   */
  void
  translate(const vecN<T,2> &p)
  {
    translate(p.x(), p.y());
  }

  /*!
    Apply a rotation matrix on the right of the matrix, algebraicly equivalent to
    \code
    matrix3x3 M;
    s = t_sin(angle);
    c = t_cos(angle);
    M(0, 0) = c;
    M(1, 0) = s;
    M(0, 1) = -s;
    M(1, 1) = c;
    *this = *this * M;
    \endcode
    \param angle amount by which to rotate in radians.
   */
  void
  rotate(T angle)
  {
    matrix3x3 &me(*this);
    matrixNxM<2, 2, T> tr, temp;
    T s, c;

    s = t_sin(angle);
    c = t_cos(angle);

    tr(0, 0) = c;
    tr(1, 0) = s;

    tr(0, 1) = -s;
    tr(1, 1) = c;

    temp(0, 0) = me(0, 0);
    temp(0, 1) = me(0, 1);
    temp(1, 0) = me(1, 0);
    temp(1, 1) = me(1, 1);

    temp = temp * tr;

    me(0, 0) = temp(0, 0);
    me(0, 1) = temp(0, 1);
    me(1, 0) = temp(1, 0);
    me(1, 1) = temp(1, 1);
  }

  /*!
    Sets the matrix variables to correspond an orthogonal projection matrix
    determined by the given projection parameters.
    \param P orthogonal projection parameters for this matrix
   */
  void
  orthogonal_projection_matrix(const projection_params<T> &P)
  {
    base_class::operator()(0,0)=T(2)/(P.m_right-P.m_left);
    base_class::operator()(1,0)=T(0);
    base_class::operator()(2,0)=T(0);

    base_class::operator()(0,1)=T(0);
    base_class::operator()(1,1)=T(2)/(P.m_top-P.m_bottom);
    base_class::operator()(2,1)=T(0);

    base_class::operator()(0,2)=(P.m_right+P.m_left)/(P.m_left-P.m_right);
    base_class::operator()(1,2)=(P.m_top+P.m_bottom)/(P.m_bottom-P.m_top);
    base_class::operator()(2,2)=T(1);
  }

  /*!
    Convenience function to matrix3x3::orthogonal_projection_matrix(const project_params<T>&).
    \param l Left
    \param r Right
    \param b Bottom
    \param t Top
   */
  void
  orthogonal_projection_matrix(T l,  T r,  T b,  T t)
  {
    orthogonal_projection_matrix( projection_params<T>(l, r, b, t));
  }

  /*!
    Calculates the determinant for the matrix.
   */
  T
  determinate(void) const
  {
    const base_class &me(*this);
    return me(0,0) * (me(1,1) * me(2,2) - me(1,2) *me(2,1))
      - me(1,0) * (me(0,1) * me(2,2) - me(2,1) * me(0,2))
      + me(2,0) * (me(0,1) * me(1,2) - me(1,1) * me(0,2)) ;
  }

  /*!
    Compute the transpose of the cofactor matrix
    \param result location to which to place the cofactor matrx
   */
  void
  cofactor_transpose(matrix3x3 &result) const
  {
    const base_class &me(*this);
    result(0, 0) =  (me(1, 1) * me(2, 2) - me(2, 1) * me(1, 2));
    result(0, 1) = -(me(0, 1) * me(2, 2) - me(0, 2) * me(2, 1));
    result(0, 2) =  (me(0, 1) * me(1, 2) - me(0, 2) * me(1, 1));
    result(1, 0) = -(me(1, 0) * me(2, 2) - me(1, 2) * me(2, 0));
    result(1, 1) =  (me(0, 0) * me(2, 2) - me(0, 2) * me(2, 0));
    result(1, 2) = -(me(0, 0) * me(1, 2) - me(1, 0) * me(0, 2));
    result(2, 0) =  (me(1, 0) * me(2, 1) - me(2, 0) * me(1, 1));
    result(2, 1) = -(me(0, 0) * me(2, 1) - me(2, 0) * me(0, 1));
    result(2, 2) =  (me(0, 0) * me(1, 1) - me(1, 0) * me(0, 1));
  }

  /*!
    Compute the cofactor matrix
    \param result location to which to place the cofactor matrx
   */
  void
  cofactor(matrix3x3 &result) const
  {
    const base_class &me(*this);
    result(0, 0) =  (me(1, 1) * me(2, 2) - me(2, 1) * me(1, 2));
    result(1, 0) = -(me(0, 1) * me(2, 2) - me(0, 2) * me(2, 1));
    result(2, 0) =  (me(0, 1) * me(1, 2) - me(0, 2) * me(1, 1));
    result(0, 1) = -(me(1, 0) * me(2, 2) - me(1, 2) * me(2, 0));
    result(1, 1) =  (me(0, 0) * me(2, 2) - me(0, 2) * me(2, 0));
    result(2, 1) = -(me(0, 0) * me(1, 2) - me(1, 0) * me(0, 2));
    result(0, 2) =  (me(1, 0) * me(2, 1) - me(2, 0) * me(1, 1));
    result(1, 2) = -(me(0, 0) * me(2, 1) - me(2, 0) * me(0, 1));
    result(2, 2) =  (me(0, 0) * me(1, 1) - me(1, 0) * me(0, 1));
  }

  /*!
    Compute the inverse of a matrix.
    \param result location to which to place the inverse matrix
   */
  void
  inverse(matrix3x3 &result) const
  {
    T det, recipDet;
    det = determinate();
    recipDet = T(1) / det;

    cofactor_transpose(result);
    result.raw_data() *= recipDet;
  }

  /*!
    Compute the inverse transpose of a matrix.
    \param result location to which to place the inverse matrix
   */
  void
  inverse_transpose(matrix3x3 &result) const
  {
    T det, recipDet;
    det = determinate();
    recipDet = T(1) / det;

    cofactor(result);
    result.raw_data() *= recipDet;
  }

  /*!
    Checks whether the matrix reverses orientation. This check is equivalent
    to determinant < T(0).
   */
  bool
  reverses_orientation(void) const
  {
    return determinate() < T(0);
  }

};


/*!
  Convenience typedef to matrix3x3\<float\>
 */
typedef matrix3x3<float> float3x3;

/*!
  Convenience typedef for projection_params\<float\>
 */
typedef projection_params<float> float_projection_params;

/*!
  Convenience typedef for orthogonal_projection_params\<float\>
 */
typedef orthogonal_projection_params<float> float_orthogonal_projection_params;


/*!
  4x4 matrix class that provides functions for matrix math,
  such as scaling, translating and creating projection matrices.
  Special case of NxM matrix.
 */
template<typename T>
class matrix4x4:public matrixNxM<4, 4, T>
{
public:
  /*!
    Conveniance typedef to base class, matrixNxM<4,4,T>
   */
  typedef matrixNxM<4,4,T> base_class;

  /*!
    Initializes the 4x4 matrix as the identity,
    i.e. diagnols are 1 and other values are 0.
   */
  matrix4x4(void):base_class() {}

  /*!
    Copy-constructor for const 4x4 matrix.
    \param obj matrix to be copied from
   */
  matrix4x4(const base_class &obj):base_class(obj) {}

  /*!
                   const vecN<T,3>&,
                   const vecN<T,3>&,
                   const vecN<T,3>&)
   Constructs a matrix4x4 from the given vectors so that
   - M*vecN<T,4>(0,0,0,1)=origin
   - M*vecN<T,4>(1,0,0,0)=right
   - M*vecN<T,4>(0,1,0,0)=up
   - M*vecN<T,4>(0,0,1,0)=backwards
   \param origin M(0,0,0,1)
   \param right M(1,0,0,0)
   \param up M(0,1,0,0)
   \param backwards M(0,0,1,0)
   */
  matrix4x4(const vecN<T,3> &origin,
            const vecN<T,3> &right,
            const vecN<T,3> &up,
            const vecN<T,3> &backwards)
  {
    for(int i=0;i<3;++i)
      {
        base_class::operator()(i,0)=right[i];
        base_class::operator()(i,1)=up[i];
        base_class::operator()(i,2)=backwards[i];
        base_class::operator()(i,3)=origin[i];

        base_class::operator()(3,i)=T(0);
      }
    base_class::operator()(3,3)=T(1);
  }

  /*!
    Create a 4x4 matrix representing translation by translate.
    \param translate translation matrix
    */
  explicit
  matrix4x4(const vecN<T,3> &translate):
    base_class()
  {
    for(int i=0;i<3;++i)
      {
        base_class::operator()(i,3)=translate[i];
      }
  }

  /*!
    Creates a translated 4x4 matrix from a given matrix and
    translation.
    \param m source matrix
    \param translate translation matrix
    */
  explicit
  matrix4x4(const matrixNxM<3,3,T> &m,
            const vecN<T,3> &translate):
    base_class()
  {
    for(int i=0;i<3;++i)
      {
        base_class::operator()(i,3)=translate[i];
        base_class::operator()(3,i)=T(0);

        for(int j=0;j<3;++j)
          {
            base_class::operator()(i,j)=m.operator()(i,j);
          }
      }
    base_class::operator()(3,3)=T(1);
  }

  /*!
    Copy-constructor for a 4x4 matrix from
    a 3x3 matrix
    \param m source matrix to copy
    */
  explicit
  matrix4x4(const matrixNxM<3,3,T> &m):
    base_class()
  {
    for(int i=0;i<3;++i)
      {
        for(int j=0;j<3;++j)
          {
            base_class::operator()(i,j)=m.operator()(i,j);
          }
      }
  }

  /*!
    Creates a scaling 4x4 matrix for scaling point's x, y and z axis.
    \param scaleX X-axis scaling factor
    \param scaleY Y-axis scaling factor
    \param scaleZ Z-axis scaling factor
    */
  matrix4x4(T scaleX, T scaleY, T scaleZ)
  {
    base_class::operator()(0,0)=scaleX;
    base_class::operator()(1,1)=scaleY;
    base_class::operator()(2,2)=scaleZ;
  }

  /*!
    Creates a 4x4 projection matrix from the given projection parameters.
    \param P projection parameters for the matrix
   */
  explicit
  matrix4x4(const projection_params<T> &P):
    base_class()
  {
    projection_matrix(P);
  }

  /*!
    Creates an 4x4 orthogonal projection matrix from the given projection
    parameters.
    \param P orthogonal projection parameters for the matrix
    */
  explicit
  matrix4x4(const orthogonal_projection_params<T> &P):
    base_class()
  {
    orthogonal_projection_matrix(P);
  }

  /*!
    Sets the matrix values to correspond the projection matrix
    determined by the given projection parameters.
    \param P projection parameters for this matrix
   */
  void
  projection_matrix(const projection_params<T> &P)
  {
    base_class::operator()(0,0)=T(2)*P.m_near/(P.m_right-P.m_left);
    base_class::operator()(1,0)=T(0);
    base_class::operator()(2,0)=T(0);
    base_class::operator()(3,0)=T(0);

    base_class::operator()(0,1)=T(0);
    base_class::operator()(1,1)=T(2)*P.m_near/(P.m_top-P.m_bottom);
    base_class::operator()(2,1)=T(0);
    base_class::operator()(3,1)=T(0);


    base_class::operator()(0,2)=(P.m_right+P.m_left)/(P.m_right-P.m_left);
    base_class::operator()(1,2)=(P.m_top+P.m_bottom)/(P.m_top-P.m_bottom);
    base_class::operator()(3,2)=T(-1);

    base_class::operator()(0,3)=T(0);
    base_class::operator()(1,3)=T(0);
    base_class::operator()(3,3)=T(0);

    if(!P.m_farAtinfinity)
      {
        base_class::operator()(2,2)=(P.m_near+P.m_far)/(P.m_near-P.m_far);
        base_class::operator()(2,3)=T(2)*P.m_near*P.m_far/(P.m_near-P.m_far);
      }
    else
      {
        base_class::operator()(2,2)=T(-1);
        base_class::operator()(2,3)=T(-2)*P.m_near;
      }
  }

  /*!
    Sets the matrix values to correspond the inverse of the projection
    matrix determined by the given projection parameters.

    \param P projection parameters for this matrix
   */
  void
  inverse_projection_matrix(const projection_params<T> &P)
  {
    base_class::operator()(0,0)=(P.m_right-P.m_left)/( T(2)*P.m_near);
    base_class::operator()(1,0)=T(0);
    base_class::operator()(2,0)=T(0);
    base_class::operator()(3,0)=T(0);

    base_class::operator()(0,1)=T(0);
    base_class::operator()(1,1)=(P.m_top-P.m_bottom)/( T(2)*P.m_near);
    base_class::operator()(2,1)=T(0);
    base_class::operator()(3,1)=T(0);

    base_class::operator()(0,2)=T(0);
    base_class::operator()(1,2)=T(0);
    base_class::operator()(2,2)=T(0);

    base_class::operator()(0,3)=(P.m_right+P.m_left)/( T(2)*P.m_near);
    base_class::operator()(1,3)=(P.m_top+P.m_bottom)/( T(2)*P.m_near);
    base_class::operator()(2,3)=T(-1);

    if(!P.m_farAtinfinity)
      {
        base_class::operator()(3,2)=(P.m_near-P.m_far)/(P.m_far*P.m_near*T(2));
        base_class::operator()(3,3)=(P.m_near+P.m_far)/(P.m_far*P.m_near*T(-2));
      }
    else
      {
        base_class::operator()(3,2)=T(-1)/ (2.0f*P.m_near);
        base_class::operator()(3,3)=T(-1)/ (2.0f*P.m_near);
      }
  }

  /*!
    Sets the matrix variables to correspond an orthogonal projection matrix
    determined by the given projection parameters.
    \param P orthogonal projection parameters for this matrix
   */
  void
  orthogonal_projection_matrix(const projection_params<T> &P)
  {
    base_class::operator()(0,0)=T(2)/(P.m_right-P.m_left);
    base_class::operator()(1,0)=T(0);
    base_class::operator()(2,0)=T(0);
    base_class::operator()(3,0)=T(0);

    base_class::operator()(0,1)=T(0);
    base_class::operator()(1,1)=T(2)/(P.m_top-P.m_bottom);
    base_class::operator()(2,1)=T(0);
    base_class::operator()(3,1)=T(0);

    base_class::operator()(0,2)=T(0);
    base_class::operator()(1,2)=T(0);
    base_class::operator()(2,2)=T(2)/(P.m_near-P.m_far);
    base_class::operator()(3,2)=T(0);

    base_class::operator()(0,3)=(P.m_right+P.m_left)/(P.m_left-P.m_right);
    base_class::operator()(1,3)=(P.m_top+P.m_bottom)/(P.m_bottom-P.m_top);
    base_class::operator()(2,3)=(P.m_near+P.m_far)/(P.m_near-P.m_far);
    base_class::operator()(3,3)=T(1);
  }

  /*!
    Convenience function for matrix4x4::orthogonal_projection_matrix(const project_params<T>&).
    \param l Left
    \param r Right
    \param b Bottom
    \param t Top
    \param n Near clip plane
    \param f Far clip plane
   */
  void
  orthogonal_projection_matrix(T l,  T r,  T b,  T t,  T n,  T f)
  {
    orthogonal_projection_matrix( projection_params<T>(l,r,b,t,n,f));
  }

  /*!
    Convenience function for matrix4x4::orthogonal_projection_matrix(const project_params<T>&).
    Equivalent to
    \code
    orthogonal_projection_matrix(l, r, b, t, T(-1), T(1) );
    \endcode
    \param l Left
    \param r Right
    \param b Bottom
    \param t Top
   */
  void
  orthogonal_projection_matrix(T l,  T r,  T b,  T t)
  {
    orthogonal_projection_matrix(l,r,b,t, T(-1), T(1) );
  }

  /*!
    Compose this matrix with a tanslation matrix
    \param v translation by axis (x,y,z,1)
   */
  void
  translate_matrix(const vecN<T,3> &v)
  {
    matrix4x4 temp(v);

    (*this)=(*this) * temp;
  }

  /*!
    Scales the matrix based on the per-axis basis
    \param sx x-axis scaling
    \param sy y-axis scaling
    \param sz z-axis scaling
   */
  void
  scale_matrix(float sx, float sy, float sz)
  {
    matrix4x4 temp(sx, sy, sz);
    (*this)=(*this) * temp;
  }

  /*!
    Rotates the matrix the given amount in radians around axis
    determined by a vec3.
    \param angle_radians Angle to rotate in radians
    \param rotation_axis Axis to rotate around [x, y, z]
   */
  void
  rotate_matrix(T angle_radians, const vecN<T,3> &rotation_axis)
  {
    matrix4x4 temp(angle_radians, rotation_axis);

    (*this)=(*this) * temp;
  }

  /*!
    Multiplies the matrix with the given vecN<T,4>(x,y,z,1) and returns
    the resulting vecN<T,3>. This effectively transforms the point according
    to the matrix transforms.
    \param in target vector to apply matrix transform to
   */
  vecN<T,3>
  apply_to_point(const vecN<T,3> &in) const
  {
    vecN<T,4> temp;

    temp[0]=in[0];
    temp[1]=in[1];
    temp[2]=in[2];
    temp[3]=T(1);

    temp=(*this)*temp;

    return vecN<T,3>(temp);
  }

  /*!
    Multiplies the matrix with the given vecN<T,4>(x,y,z,0) and returns
    the resulting vecN<T,3>. This effectively transforms the direction
    defined by the vector according to the matrix transforms.
    \param in target direction vector to apply the transform to
   */
  vecN<T,3>
  apply_to_direction(const vecN<T,3> &in) const
  {
    vecN<T,4> temp;

    temp[0]=in[0];
    temp[1]=in[1];
    temp[2]=in[2];
    temp[3]=T(0);

    temp=(*this)*temp;

    return vecN<T,3>(temp);
  }

  /*!
    Returns a 3x3 matrix representing the upper-left part
    of the matrix:\n\n
      M11 M12 M13|M14\n
      M21 M22 M23|M24\n
      M31 M32 M33|M34\n
      ---------------\n
      M41 M42 M43|M44\n
   */
  matrixNxM<3,3,T>
  upper3x3_submatrix(void) const
  {
    matrixNxM<3,3,T> retval;
    for(int i=0;i<3;++i)
      {
        for(int j=0;j<3;++j)
          {
            retval.operator()(i,j)=base_class::operator()(i,j);
          }
      }
    return retval;
  }

  /*!
    Returns the translation vector of the matrix,
    i.e. vecN<T,3>(operator()(0,0), operator()(1,0), operator()(2,0)),
    i.e. the last column of the matrix truncating
    the last element of the last column.
    */
  vecN<T,3>
  translation_vector(void) const
  {
    vecN<T,3> retval;

    for(int i=0;i<3;++i)
      {
        retval[i]=base_class::operator()(i,3);
      }

    return retval;
  }

  /*!
    Sets the translation vector of the matrix,
    i.e. for each 0<=I<3, operator(i,3)=v[i].
    \param v new translation vector
   */
  void
  translation_vector(const vecN<T,3> &v)
  {
    for(int i=0;i<3;++i)
      {
        base_class::operator()(i,3)=v[i];
      }
  }

  /*!
    Calculate and return the determinant of the upper-left 3x3 block.
  */
  T
  upper3x3_determinate(void) const
  {
    return base_class::operator()(0,0)*
      ( base_class::operator()(1,1)*base_class::operator()(2,2)
        - base_class::operator()(1,2)*base_class::operator()(2,1) )

      - base_class::operator()(1,0)*
      ( base_class::operator()(0,1)*base_class::operator()(2,2)
        - base_class::operator()(2,1)*base_class::operator()(0,2) )

      + base_class::operator()(2,0)*
      ( base_class::operator()(0,1)*base_class::operator()(1,2)
        - base_class::operator()(1,1)*base_class::operator()(0,2) ) ;
  }

  /*!
    Returns true if the matrix reverses orientation (upper 3x3 determinant is
    lesser than 0).
   */
  bool
  reverses_orientation(void) const
  {
    return upper3x3_determinate()<T(0);
  }


};



/*!
  A convenience typedef to matrix4x4\<float\>
 */
typedef matrix4x4<float> float4x4;


} //namespace



/*! @} */
