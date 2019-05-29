/*!
 * \file ScaleTranslate.hpp
 * \brief file ScaleTranslate.hpp
 *
 * Adapted from: WRATHScaleTranslate.hpp of WRATH:
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

#include <cstdlib>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/math.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/painter/painter.hpp>

/*!\class ScaleTranslate
  A ScaleTranslate represents the composition
  of a scaling and a translation, i.e.
  \f[ f(x,y) = (sx,sy) + (A,B) \f]
  where \f$ s \f$ =scale() and \f$ (A,B) \f$ =translation().
 */
template<typename T>
class ScaleTranslate
{
public:
  /*!
    Ctor. Initialize a ScaleTranslate from a
    scaling factor and translation
    \param tr translation to use
    \param s scaling factor to apply to both x-axis and y-axis,
             _absolute_ value is stored, i.e. the sign of s
             is ignored
   */
  ScaleTranslate(const fastuidraw::vecN<T, 2> &tr = fastuidraw::vecN<T, 2>(T(0), T(0)),
                 T s = T(1)):
    m_scale(fastuidraw::t_abs(s)),
    m_translation(tr)
  {}

  /*!
    Ctor. Initialize a ScaleTranslate from a
    scaling factor
    \param s scaling factor to apply to both x-axis and y-axis,
             _absolute_ value is stored, i.e. the sign of s
             is ignored
   */
  ScaleTranslate(T s):
    m_scale(fastuidraw::t_abs(s)),
    m_translation(T(0), T(0))
  {}

  /*!
    Returns the inverse transformation to this.
   */
  ScaleTranslate
  inverse(void) const
  {
    ScaleTranslate r;

    r.m_scale = T(1) / m_scale;
    r.m_translation = -r.m_scale * m_translation;

    return r;
  }

  /*!
    Returns the translation of this
    ScaleTranslate.
   */
  const fastuidraw::vecN<T, 2>&
  translation(void) const
  {
    return m_translation;
  }

  /*!
    Sets the translation of this.
    \param tr value to set translation to.
   */
  ScaleTranslate&
  translation(const fastuidraw::vecN<T, 2> &tr)
  {
    m_translation = tr;
    return *this;
  }

  /*!
    Sets the x-coordinate of the translation of this.
    \param x value to set translation to.
   */
  ScaleTranslate&
  translation_x(T x)
  {
    m_translation.x() = x;
    return *this;
  }

  /*!
    Sets the y-coordinate of the translation of this.
    \param y value to set translation to.
   */
  ScaleTranslate&
  translation_y(T y)
  {
    m_translation.y() = y;
    return *this;
  }

  /*!
    Returns the scale of this.
    Scaling factor is _NEVER_
    negative.
   */
  T
  scale(void) const
  {
    return m_scale;
  }

  /*!
    Sets the scale of this.
    If a negative value is passed,
    it's absolute value is used.
    \param s value to set scale to.
   */
  ScaleTranslate&
  scale(T s)
  {
    m_scale = fastuidraw::t_abs(s);
    return *this;
  }

  /*!
    Returns the value of applying the transformation to a point.
    \param pt point to apply the transformation to.
   */
  fastuidraw::vecN<T, 2>
  apply_to_point(const fastuidraw::vecN<T, 2> &pt) const
  {
    return scale() * pt + translation();
  }

  /*!
    Returns the value of applying the inverse of the
    transformation to a point.
    \param pt point to apply the transformation to.
   */
  fastuidraw::vecN<T, 2>
  apply_inverse_to_point(const fastuidraw::vecN<T, 2> &pt) const
  {
    return (pt - translation()) / scale();
  }

  /*!
    Returns the transformation of this
    ScaleTranslate as a 3x3 matrix
   */
  fastuidraw::matrix3x3<T>
  matrix3(void) const
  {
    fastuidraw::matrix3x3<T> M;

    M(0,0) = M(1,1) = scale();
    M(0,2) = translation().x();
    M(1,2) = translation().y();

    return M;
  }

  void
  concat_to_painter(const fastuidraw::reference_counted_ptr<fastuidraw::Painter> &p) const
  {
    p->translate(fastuidraw::vec2(m_translation));
    p->scale(m_scale);
  }

  /*!
    Computes the interpolation between
    two ScaleTranslate objects.
    \param a0 begin value of interpolation
    \param a1 end value of interpolation
    \param t interpolate, t=0 returns a0, t=1 returns a1
   */
  static
  ScaleTranslate
  interpolate(const ScaleTranslate &a0,
              const ScaleTranslate &a1,
              T t)
  {
    ScaleTranslate R;

    R.translation( a0.translation() + t * (a1.translation()-a0.translation()) );
    R.scale( a0.scale() + t * (a1.scale() - a0.scale()) );
    return R;
  }

private:
  T m_scale;
  fastuidraw::vecN<T, 2> m_translation;
};

/*!
  Compose two ScaleTranslate so that:
  a*b.apply_to_point(p) "=" a.apply_to_point( b.apply_to_point(p)).
  \param a left hand side of composition
  \param b right hand side of composition
 */
template<typename T>
ScaleTranslate<T>
operator*(const ScaleTranslate<T> &a, const ScaleTranslate<T> &b)
{
  ScaleTranslate<T> c;

  //
  // c(p)= a( b(p) )
  //     = a.translation + a.scale*( b.scale*p + b.translation )
  //     = (a.translate + a.scale*b.translation) + (a.scale*b.scale)*p
  //
  //thus:
  //
  // c.scale=a.scale*b.scale
  // c.translation= a.apply_to_point(b.translation)
  c.scale( a.scale() * b.scale());
  c.translation( a.apply_to_point(b.translation()) );

  return c;
}

/*! @} */
