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
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#pragma once

#include <fastuidraw/util/math.hpp>
#include <fastuidraw/util/vecN.hpp>

namespace fastuidraw {

/*!\addtogroup Utility
 * @{
 */

/*!
 * \brief
 * A generic matrix class. The operator() is overloaded
 * to access elements of the matrix as follows:
 * \verbatim
 * matrix(0    , 0) matrix(0    , 1) matrix(0    , 2) ... matrix(0    , M - 1)
 * matrix(1    , 0) matrix(1    , 1) matrix(1    , 2) ... matrix(1    , M - 1)
 * .
 * .
 * .
 * matrix(N - 2, 0) matrix(N - 2, 1) matrix(N - 1, 2) ... matrix(N - 2, M - 1)
 * matrix(N - 1, 0) matrix(N - 1, 1) matrix(N - 1, 2) ... matrix(N - 1, M - 1)
 * \endverbatim
 *
 * The data is represented internally with a 1-dimensional array with the
 * packing order
 *
 * \code
 * matrix(row, color) <--> raw_data()[row + N * col]
 * \endcode
 *
 *\tparam N height of matrix
 *\tparam M width of matrix
 *\tparam T matrix entry type
 */
template<size_t N, size_t M, typename T=float>
class matrixNxM
{
private:
  vecN<T, N * M> m_data;

public:
  /*!
   * \brief
   * Typedef to underlying vecN that holds
   * the matrix data.
   */
  typedef vecN<T, N * M> raw_data_type;

  enum
    {
      /*!
       * Enumeration value for size of the matrix.
       */
      number_rows = N,

      /*!
       * Enumeration value for size of the matrix.
       */
      number_cols = M,
    };

  /*!
   * Copy-constructor for a NxN matrix.
   * \param obj matrix to copy
   */
  matrixNxM(const matrixNxM &obj):
    m_data(obj.m_data)
  {}

  /*!
   * Ctor.
   * Initializes an NxN matrix as diagnols are 1
   * and other values are 0, for square matrices,
   * that is the identity matrix.
   */
  matrixNxM(void)
  {
    reset();
  }

  /*!
   * Set matrix as identity.
   */
  void
  reset(void)
  {
    for(unsigned int i = 0; i < M; ++i)
      {
        for(unsigned int j = 0; j < N; ++j)
          {
            m_data[N * i + j] = (i == j) ? T(1): T(0);
          }
      }
  }

  /*!
   * Swaps the values between this and the parameter matrix.
   * \param obj matrix to swap values with
   */
  void
  swap(matrixNxM &obj)
  {
    m_data.swap(obj.m_data);
  }

  /*!
   * Returns a c-style pointer to the data.
   */
  T*
  c_ptr(void) { return m_data.c_ptr(); }

  /*!
   * Returns a constant c-style pointer to the data.
   */
  const T*
  c_ptr(void) const { return m_data.c_ptr(); }

  /*!
   * Returns a reference to raw data vector in the matrix.
   */
  vecN<T, N * M>&
  raw_data(void) { return m_data; }

  /*!
   * Returns a const reference to the raw data vectors in the matrix.
   */
  const vecN<T, N * M>&
  raw_data(void) const { return m_data; }

  /*!
   * Returns the named entry of the matrix
   * \param row row(vertical coordinate) in the matrix
   * \param col column(horizontal coordinate) in the matrix
   */
  T&
  operator()(unsigned int row, unsigned int col)
  {
    FASTUIDRAWassert(row < N);
    FASTUIDRAWassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Returns the named entry of the matrix
   * \param row row(vertical coordinate) in the matrix
   * \param col column(horizontal coordinate) in the matrix
   */
  const T&
  operator()(unsigned int row, unsigned int col) const
  {
    FASTUIDRAWassert(row < N);
    FASTUIDRAWassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Returns the named entry of the matrix; provided
   * as a conveniance to interface with systems where
   * access of elements is column then row or to more
   * easily access the transpose of the matrix.
   * \param col column(horizontal coordinate) in the matrix
   * \param row row(vertical coordinate) in the matrix
   */
  T&
  col_row(unsigned int col, unsigned row)
  {
    FASTUIDRAWassert(row < N);
    FASTUIDRAWassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Returns the named entry of the matrix; provided
   * as a conveniance to interface with systems where
   * access of elements is column then row or to more
   * easily access the transpose of the matrix.
   * \param col column(horizontal coordinate) in the matrix
   * \param row row(vertical coordinate) in the matrix
   */
  const T&
  col_row(unsigned int col, unsigned row) const
  {
    FASTUIDRAWassert(row < N);
    FASTUIDRAWassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Compute the transpose of the matrix
   * \param retval location to which to write the transpose
   */
  void
  transpose(matrixNxM<M, N, T> &retval) const
  {
    for(unsigned int i = 0; i < N; ++i)
      {
        for(unsigned int j = 0; j < M; ++j)
          {
            retval.operator()(i,j) = operator()(j,i);
          }
      }
  }

  /*!
   * Returns a transpose of the matrix.
   */
  matrixNxM<M, N, T>
  transpose(void) const
  {
    matrixNxM<M, N, T>  retval;
    transpose(retval);
    return retval;
  }

  /*!
   * Operator for adding matrices together.
   * \param matrix target matrix
   */
  matrixNxM
  operator+(const matrixNxM &matrix) const
  {
    matrixNxM out;
    out.m_data = m_data+matrix.m_data;
    return out;
  }

  /*!
   * Operator for substracting matrices from each other.
   * \param matrix target matrix
   */
  matrixNxM
  operator-(const matrixNxM &matrix) const
  {
    matrixNxM out;
    out.m_data = m_data-matrix.m_data;
    return out;
  }

  /*!
   * Multiplies the matrix with a given scalar.
   * \param value scalar to multiply with
   */
  matrixNxM
  operator*(T value) const
  {
    matrixNxM out;
    out.m_data = m_data*value;
    return out;
  }

  /*!
   * Multiplies the given matrix with the given scalar
   * and returns the resulting matrix.
   * \param value scalar to multiply with
   * \param matrix target matrix to multiply
   */
  friend
  matrixNxM
  operator*(T value, const matrixNxM &matrix)
  {
    matrixNxM out;
    out.m_data = matrix.m_data*value;
    return out;
  }

  /*!
   * Multiplies this matrix with the given matrix.
   * \param matrix target matrix
   */
  template<size_t K>
  matrixNxM<N, K, T>
  operator*(const matrixNxM<M, K, T> &matrix) const
  {
    unsigned int i,j,k;
    matrixNxM<N,K, T> out;

    for(i = 0; i < N; ++i)
      {
        for(j = 0; j < K; ++j)
          {
            out.operator()(i,j) = T(0);
            for(k = 0; k < M; ++k)
              {
                out.operator()(i,j) += operator()(i,k) * matrix.operator()(k,j);
              }
          }
      }
    return out;
  }

  /*!
   * Computes the value of \ref matrixNxM * \ref vecN
   * \param in right operand of multiply
   */
  vecN<T, N>
  operator*(const vecN<T, M> &in) const
  {
    vecN<T, N> retval;

    for(unsigned int i = 0; i < N;++i)
      {
        retval[i] = T(0);
        for(unsigned int j = 0; j < M; ++j)
          {
            retval[i] += operator()(i,j) * in[j];
          }
      }

    return retval;
  }

  /*!
   * Computes the value of \ref vecN * \ref matrixNxM
   * \param in left operand of multiply
   * \param matrix right operand of multiply
   */
  friend
  vecN<T, M>
  operator*(const vecN<T, N> &in, const matrixNxM &matrix)
  {
    vecN<T, M> retval;

    for(unsigned int i = 0; i < M; ++i)
      {
        retval[i] = T(0);
        for(unsigned int j = 0; j < N; ++j)
          {
            retval[i] += in[j] * matrix.operator()(j,i);
          }
      }
    return retval;
  }
};

/*!
 * \brief
 * Convenience typedef to matrixNxM\<2, float\>
 */
typedef matrixNxM<2, 2, float> float2x2;

/*!
 * \brief
 * An orthogonal_projection_params holds the data to describe an
 * orthogonal projection matrix without perspective.
 */
template<typename T>
class orthogonal_projection_params
{
public:
  T m_top; ///< Top edge of the clipping plane
  T m_bottom; ///< Bottom edge of the clipping plane
  T m_left; ///< Left edge of the clipping plane
  T m_right; ///< Right edge of the clipping plane

  /*!
   * Default constructor for projection parameters,
   * values are unitialized.
   */
  orthogonal_projection_params(void)
  {}

  /*!
   * Creates the projection parameters instance according to the given parameters.
   * \param l Left
   * \param r Right
   * \param t Top
   * \param b Bottom
   */
  orthogonal_projection_params(T l, T r, T b, T t):
    m_top(t), m_bottom(b), m_left(l), m_right(r)
  {}
};

/*!
 * \brief
 * A projection_params holds the data to describe a projection matrix with
 * perspective.
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

  /*!
   * Default constructor for projection parameters,
   * values are unitialized.
   */
  projection_params(void)
  {}

  /*!
   * Creates the projection parameters instance according to the given parameters.
   * \param l Left
   * \param r Right
   * \param t Top
   * \param b Bottom
   * \param n Near clipping plane
   */
  projection_params(T l, T r, T b, T t, T n):
    m_top(t), m_bottom(b), m_left(l), m_right(r),
    m_near(n)
  {}
};

/*!
 * \brief
 * A representation of a 3x3 matrix, that in addition to the NxN
 * matrix functionality provides function for calculating the
 * determinant.
 */
template<typename T>
class matrix3x3:public matrixNxM<3, 3, T>
{
public:
  /*!
   * \brief
   * Conveniance typedef to base class, matrixNxM<3, 3, T>
   */
  typedef matrixNxM<3, 3, T> base_class;

  /*!
   * Initializes the 3x3 matrix as the identity,
   * i.e. diagnols are 1 and other values are 0.
   */
  matrix3x3(void):base_class() {}

  /*!
   * Copy-constructor for a 3x3 matrix.
   * \param obj target matrix to copy.
   */
  matrix3x3(const base_class &obj):base_class(obj) {}

  /*!
   * Construct a matrix3x3 M so that
   *  - M*vecN<T, 3>(1, 0, 0)=T
   *  - M*vecN<T, 3>(0, 1, 0)=B
   *  - M*vecN<T, 3>(0, 0, 1)=N
   * \param t first row vector
   * \param b second row vector
   * \param n third row vector
   */
  matrix3x3(const vecN<T, 3> &t, const vecN<T, 3> &b, const vecN<T, 3> &n)
  {
    for(int i=0;i<3;++i)
      {
        this->operator()(i, 0) = t[i];
        this->operator()(i, 1) = b[i];
        this->operator()(i, 2) = n[i];
      }
  }

  /*!
   * Initialize the 3x3 matrix with the upper 2x2 corner
   * coming from a 2x2 martix and the right column
   * coming from a 2-vector. The bottom is initialized
   * as 0 in first and second column and bottom right as 1.
   */
  matrix3x3(const matrixNxM<2, 2, T> &mat, const vecN<T, 2> &vec = vecN<T, 2>(T(0)))
  {
    for(int i = 0; i < 2; ++i)
      {
        for(int j = 0; j < 2; ++j)
          {
            this->operator()(i,j) = mat(i, j);
          }
        this->operator()(2, i) = T(0);
        this->operator()(i, 2) = vec[i];
      }
    this->operator()(2, 2) = T(1);
  }

  /*!fn matrix3x3(const orthogonal_projection_params<T>&)
   * Creates a 3x3 orthogonal projection matrix from the given projection
   * parameters.
   * \param P orthogonal projection parameters for the matrix
   */
  matrix3x3(const orthogonal_projection_params<T> &P):
    base_class()
  {
    orthogonal_projection_matrix(P);
  }

  /*!fn matrix3x3(const orthogonal_projection_params<T>&)
   * Creates a 3x3 projection matrix from the given projection
   * parameters.
   * \param P projection parameters for the matrix
   */
  matrix3x3(const projection_params<T> &P):
    base_class()
  {
    projection_matrix(P);
  }

  /*!
   * Apply shear on the right of the matrix, algebraicly equivalent to
   * \code
   * matrix3x3 M;
   * M(0, 0) = sx;
   * M(1, 1) = sy;
   * *this = *this * M;
   * \endcode
   */
  void
  shear(T sx, T sy)
  {
    this->operator()(0, 0) *= sx;
    this->operator()(1, 0) *= sx;
    this->operator()(2, 0) *= sx;

    this->operator()(0, 1) *= sy;
    this->operator()(1, 1) *= sy;
    this->operator()(2, 1) *= sy;
  }

  /*!
   * Apply scale on the right of the matrix, algebraicly equivalent to
   * \code
   * matrix3x3 M;
   * M(0, 0) = s;
   * M(1, 1) = s;
   * *this = *this * M;
   * \endcode
   */
  void
  scale(T s)
  {
    shear(s, s);
  }

  /*!
   * Apply translate on the right of the matrix, algebraicly equivalent to
   * \code
   * matrix3x3 M;
   * M(0, 2) = x;
   * M(1, 2) = y;
   * *this = *this * M;
   * \endcode
   * \param x amount by which to translate horizontally
   * \param y amount by which to translate vertically
   */
  void
  translate(T x, T y)
  {
    this->operator()(0, 2) += x * this->operator()(0, 0) + y * this->operator()(0, 1);
    this->operator()(1, 2) += x * this->operator()(1, 0) + y * this->operator()(1, 1);
    this->operator()(2, 2) += x * this->operator()(2, 0) + y * this->operator()(2, 1);
  }

  /*!
   * Provided as an override conveniance, equivalent to
   * \code
   * translate(p.x(), p.y())
   * \endcode
   * \param p amount by which to translate
   */
  void
  translate(const vecN<T, 2> &p)
  {
    translate(p.x(), p.y());
  }

  /*!
   * Apply a rotation matrix on the right of the matrix, algebraicly equivalent to
   * \code
   * matrix3x3 M;
   * s = t_sin(angle);
   * c = t_cos(angle);
   * M(0, 0) = c;
   * M(1, 0) = s;
   * M(0, 1) = -s;
   * M(1, 1) = c;
   * *this = *this * M;
   * \endcode
   * \param angle amount by which to rotate in radians.
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
   * Sets the matrix variables to correspond an orthogonal projection matrix
   * determined by the given projection parameters.
   * \param P orthogonal projection parameters for this matrix
   */
  void
  orthogonal_projection_matrix(const orthogonal_projection_params<T> &P)
  {
    this->operator()(0, 0) = T(2) / (P.m_right - P.m_left);
    this->operator()(1, 0) = T(0);
    this->operator()(2, 0) = T(0);

    this->operator()(0, 1) = T(0);
    this->operator()(1, 1) = T(2) / (P.m_top - P.m_bottom);
    this->operator()(2, 1) = T(0);

    this->operator()(0, 2) = (P.m_right + P.m_left) / (P.m_left - P.m_right);
    this->operator()(1, 2) = (P.m_top + P.m_bottom) / (P.m_bottom - P.m_top);
    this->operator()(2, 2) = T(1);
  }

  /*!
   * Sets the matrix variables to correspond the inverse to an
   * orthogonal projection matrix determined by the given projection
   * parameters.
   * \param P orthogonal projection parameters for this matrix
   */
  void
  inverse_orthogonal_projection_matrix(const orthogonal_projection_params<T> &P)
  {
    this->operator()(0, 0) = (P.m_right - P.m_left) / T(2);
    this->operator()(1, 0) = T(0);
    this->operator()(2, 0) = T(0);

    this->operator()(0, 1) = T(0);
    this->operator()(1, 1) = (P.m_top - P.m_bottom) / T(2);
    this->operator()(2, 1) = T(0);

    this->operator()(0, 2) = (P.m_right + P.m_left) / T(2);
    this->operator()(1, 2) = (P.m_top + P.m_bottom) / T(2);
    this->operator()(2, 2) = T(1);
  }

  /*!
   * Convenience function to matrix3x3::orthogonal_projection_matrix(const project_params<T>&).
   * \param l Left
   * \param r Right
   * \param b Bottom
   * \param t Top
   */
  void
  orthogonal_projection_matrix(T l, T r, T b, T t)
  {
    orthogonal_projection_matrix(orthogonal_projection_params<T>(l, r, b, t));
  }

  /*!
   * Sets the matrix values to correspond the projection matrix
   * determined by the given projection parameters.
   * \param P projection parameters for this matrix
   */
  void
  projection_matrix(const projection_params<T> &P)
  {
    this->operator()(0, 0) = T(2) * P.m_near / (P.m_right - P.m_left);
    this->operator()(1, 0) = T(0);
    this->operator()(2, 0) = T(0);

    this->operator()(0, 1) = T(0);
    this->operator()(1, 1) = T(2) * P.m_near / (P.m_top - P.m_bottom);
    this->operator()(2, 1) = T(0);

    this->operator()(0, 2) = T(0);
    this->operator()(1, 2) = T(0);
    this->operator()(2, 2) = T(-1);
  }

  /*!
   * Calculates the determinant for the matrix.
   */
  T
  determinate(void) const
  {
    const base_class &me(*this);
    return me(0, 0) * (me(1, 1) * me(2, 2) - me(1, 2) *me(2, 1))
      - me(1, 0) * (me(0, 1) * me(2, 2) - me(2, 1) * me(0, 2))
      + me(2, 0) * (me(0, 1) * me(1, 2) - me(1, 1) * me(0, 2)) ;
  }

  /*!
   * Compute the transpose of the cofactor matrix
   * \param result location to which to place the cofactor matrx
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
   * Compute the cofactor matrix
   * \param result location to which to place the cofactor matrx
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
   * Compute the inverse of a matrix.
   * \param result location to which to place the inverse matrix
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
   * Compute the inverse transpose of a matrix.
   * \param result location to which to place the inverse matrix
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
   * Checks whether the matrix reverses orientation. This check is equivalent
   * to determinant < T(0).
   */
  bool
  reverses_orientation(void) const
  {
    return determinate() < T(0);
  }

};

/*!
 * \brief
 * Convenience typedef to matrix3x3\<float\>
 */
typedef matrix3x3<float> float3x3;

/*!
 * \brief
 * Convenience typedef for projection_params\<float\>
 */
typedef projection_params<float> float_projection_params;

/*!
 * \brief
 * Convenience typedef for orthogonal_projection_params\<float\>
 */
typedef orthogonal_projection_params<float> float_orthogonal_projection_params;

/*! @} */
} //namespace
