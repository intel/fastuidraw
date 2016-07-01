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
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */




#pragma once

#include <cstdlib>
#include <QtGlobal>
#include <QPointF>


/*! \addtogroup Utility
 * @{
 */

/*!\class ScaleTranslate
  A ScaleTranslate represents the composition
  of a scaling and a translation, i.e.
  \f[ f(x,y) = (sx,sy) + (A,B) \f]
  where \f$ s \f$ =scale() and \f$ (A,B) \f$ =translation().
 */
class ScaleTranslate
{
public:
  /*!\fn ScaleTranslate(const vecN<T, 2>&, T)
    Ctor. Initialize a ScaleTranslate from a
    scaling factor and translation
    \param tr translation to use
    \param s scaling factor to apply to both x-axis and y-axis,
             _absolute_ value is stored, i.e. the sign of s
             is ignored
   */
  ScaleTranslate(const QPointF &tr = QPointF(0.0, 0.0),
                 qreal s=1.0):
    m_scale(qAbs(s)),
    m_translation(tr)
  {}

  /*!\fn ScaleTranslate(T)
    Ctor. Initialize a ScaleTranslate from a
    scaling factor
    \param s scaling factor to apply to both x-axis and y-axis,
             _absolute_ value is stored, i.e. the sign of s
             is ignored
   */
  ScaleTranslate(qreal s):
    m_scale(qAbs(s)),
    m_translation(0.0, 0.0)
  {}

  /*!\fn ScaleTranslate inverse(void) const
    Returns the inverse transformation to this.
   */
  ScaleTranslate
  inverse(void) const
  {
    ScaleTranslate r;

    r.m_scale = 1.0 / m_scale;
    r.m_translation = -r.m_scale * m_translation;

    return r;
  }

  /*!\fn const vecN<T, 2>& translation(void) const
    Returns the translation of this
    ScaleTranslate.
   */
  const QPointF&
  translation(void) const
  {
    return m_translation;
  }

  /*!\fn ScaleTranslate& translation(const vecN<T, 2>&)
    Sets the translation of this.
    \param tr value to set translation to.
   */
  ScaleTranslate&
  translation(const QPointF &tr)
  {
    m_translation=tr;
    return *this;
  }

  /*!\fn ScaleTranslate& translation_x(T)
    Sets the x-coordinate of the translation of this.
    \param x value to set translation to.
   */
  ScaleTranslate&
  translation_x(qreal x)
  {
    m_translation.rx()=x;
    return *this;
  }

  /*!\fn ScaleTranslate& translation_y(T)
    Sets the y-coordinate of the translation of this.
    \param y value to set translation to.
   */
  ScaleTranslate&
  translation_ry(qreal y)
  {
    m_translation.ry()=y;
    return *this;
  }

  /*!\fn T scale(void) const
    Returns the scale of this.
    Scaling factor is _NEVER_
    negative.
   */
  qreal
  scale(void) const
  {
    return m_scale;
  }

  /*!\fn ScaleTranslate& scale(T)
    Sets the scale of this.
    If a negative value is passed,
    it's absolute value is used.
    \param s value to set scale to.
   */
  ScaleTranslate&
  scale(qreal s)
  {
    m_scale=qAbs(s);
    return *this;
  }

  /*!\fn vecN<T, 2> apply_to_point
    Returns the value of applying the transformation to a point.
    \param pt point to apply the transformation to.
   */
  QPointF
  apply_to_point(const QPointF &pt) const
  {
    return scale()*pt + translation();
  }

  /*!\fn vecN<T, 2> apply_inverse_to_point
    Returns the value of applying the inverse of the
    transformation to a point.
    \param pt point to apply the transformation to.
   */
  QPointF
  apply_inverse_to_point(const QPointF &pt) const
  {
    return (pt - translation()) / scale();
  }

private:
  qreal m_scale;
  QPointF m_translation;
};

/*!\fn operator*(const ScaleTranslate&, const ScaleTranslate&)
  Compose two ScaleTranslate so that:
  a*b.apply_to_point(p) "=" a.apply_to_point( b.apply_to_point(p)).
  \param a left hand side of composition
  \param b right hand side of composition
 */
inline
ScaleTranslate
operator*(const ScaleTranslate &a, const ScaleTranslate &b)
{
  ScaleTranslate c;

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
