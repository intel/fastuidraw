/*!
 * \file painter_effect_brush.hpp
 * \brief file painter_effect_brush.hpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef FASTUIDRAW_PAINTER_EFFECT_BRUSH_HPP
#define FASTUIDRAW_PAINTER_EFFECT_BRUSH_HPP

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/painter/effects/painter_effect.hpp>
#include <fastuidraw/painter/painter_brush.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterEffectBrush represents applying a brush.
   * The \ref PainterEffectParams derived object to use
   * with a \ref PainterEffectBrush is \ref
   * PainterEffectBrushParams
   */
  class PainterEffectBrush:public PainterEffect
  {
  public:
    virtual
    unsigned int
    number_passes(void) const override;

    virtual
    PainterData::brush_value
    brush(unsigned pass,
          const reference_counted_ptr<const Image> &image,
	  const Rect &brush_rect,
          PainterEffectParams &params) const override;
  };

  /*!
   * The \ref PainterEffectParams derived object for
   * \ref PainterEffectBrush
   */
  class PainterEffectBrushParams:public PainterEffectParams
  {
  public:
    /*!
     * Set the modulate color, default value is
     * (1, 1, 1, 1), i.e. no modulation.
     */
    PainterEffectBrushParams&
    color(const vec4 &v)
    {
      m_brush.color(v);
      return *this;
    }

    /*!
     * Sets the brush to have a linear gradient.
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param start_p start position of gradient
     * \param end_p end position of gradient.
     * \param spread specifies the gradient spread type
     */
    PainterEffectBrushParams&
    linear_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                    const vec2 &start_p, const vec2 &end_p,
                    enum PainterBrush::spread_type_t spread)
    {
      m_brush.linear_gradient(cs, start_p, end_p, spread);
      return *this;
    }

    /*!
     * Sets the brush to have a radial gradient.
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param start_p start position of gradient
     * \param start_r starting radius of radial gradient
     * \param end_p end position of gradient.
     * \param end_r ending radius of radial gradient
     * \param spread specifies the gradient spread type
     */
    PainterEffectBrushParams&
    radial_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                    const vec2 &start_p, float start_r,
                    const vec2 &end_p, float end_r,
                    enum PainterBrush::spread_type_t spread)
    {
      m_brush.radial_gradient(cs, start_p, start_r, end_p, end_r, spread);
      return *this;
    }

    /*!
     * Sets the brush to have a radial gradient. Provided as
     * a conveniance, equivalent to
     * \code
     * radial_gradient(cs, p, 0.0f, p, r, repeat);
     * \endcode
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p start and end position of gradient
     * \param r ending radius of radial gradient
     * \param spread specifies the gradient spread type
     */
    PainterEffectBrushParams&
    radial_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                    const vec2 &p, float r, enum PainterBrush::spread_type_t spread)
    {
      m_brush.radial_gradient(cs, p, r, spread);
      return *this;
    }

    /*!
     * Sets the brush to have a sweep gradient (directly).
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p position of gradient
     * \param theta start angle of the sweep gradient, this value
     *              should be in the range [-PI, PI]
     * \param F the repeat factor applied to the interpolate, the
     *          sign of F is used to determine the sign of the
     *          sweep gradient.
     * \param spread specifies the gradient spread type
     */
    PainterEffectBrushParams&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                   const vec2 &p, float theta, float F,
                   enum PainterBrush::spread_type_t spread)
    {
      m_brush.sweep_gradient(cs, p, theta, F, spread);
      return *this;
    }

    /*!
     * Sets the brush to have a sweep gradient where the sign is
     * determined by a PainterEnums::screen_orientation and a
     * PainterEnums::rotation_orientation_t.
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p position of gradient
     * \param theta angle of the sweep gradient, this value
     *              should be in the range [-PI, PI]
     * \param F the repeat factor applied to the interpolate,
     *          a negative reverses the orientation of the sweep.
     * \param orientation orientation of the screen
     * \param rotation_orientation orientation of the sweep
     * \param spread specifies the gradient spread type
     */
    PainterEffectBrushParams&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                   const vec2 &p, float theta,
                   enum PainterEnums::screen_orientation orientation,
                   enum PainterEnums::rotation_orientation_t rotation_orientation,
                   float F, enum PainterBrush::spread_type_t spread)
    {
      m_brush.sweep_gradient(cs, p, theta, orientation,
                             rotation_orientation, F, spread);
      return *this;
    }

    /*!
     * Sets the brush to have a sweep gradient with a repeat factor
     * of 1.0 and where the sign is determined by a
     * PainterEnums::screen_orientation and a
     * PainterEnums::rotation_orientation_t. Equivalent to
     * \code
     * sweep_gradient(cs, p, theta, orientation, rotation_orientation, 1.0f, repeat);
     * \endcode
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p position of gradient
     * \param theta angle of the sweep gradient, this value
     *              should be in the range [-PI, PI]
     * \param orientation orientation of the screen
     * \param rotation_orientation orientation of the sweep
     * \param spread specifies the gradient spread type
     */
    PainterEffectBrushParams&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                   const vec2 &p, float theta,
                   enum PainterEnums::screen_orientation orientation,
                   enum PainterEnums::rotation_orientation_t rotation_orientation,
                   enum PainterBrush::spread_type_t spread)
    {
      m_brush.sweep_gradient(cs, p, theta, orientation,
                             rotation_orientation, spread);
      return *this;
    }

    /*!
     * Sets the brush to not have a gradient.
     */
    PainterEffectBrushParams&
    no_gradient(void)
    {
      m_brush.no_gradient();
      return *this;
    }

    /*!
     * Sets the brush to have a translation in its transformation.
     * \param p translation value for brush transformation
     */
    PainterEffectBrushParams&
    transformation_translate(const vec2 &p)
    {
      m_brush.transformation_translate(p);
      return *this;
    }

    /*!
     * Sets the brush to have a matrix in its transformation.
     * \param m matrix value for brush transformation
     */
    PainterEffectBrushParams&
    transformation_matrix(const float2x2 &m)
    {
      m_brush.transformation_matrix(m);
      return *this;
    }

    /*!
     * Apply a shear to the transformation of the brush.
     * \param m matrix to which to apply
     */
    PainterEffectBrushParams&
    apply_matrix(const float2x2 &m)
    {
      m_brush.apply_matrix(m);
      return *this;
    }

    /*!
     * Apply a shear to the transformation of the brush.
     * \param sx scale factor in x-direction
     * \param sy scale factor in y-direction
     */
    PainterEffectBrushParams&
    apply_shear(float sx, float sy)
    {
      m_brush.apply_shear(sx, sy);
      return *this;
    }

    /*!
     * Apply a rotation to the transformation of the brush.
     * \param angle in radians by which to rotate
     */
    PainterEffectBrushParams&
    apply_rotate(float angle)
    {
      m_brush.apply_rotate(angle);
      return *this;
    }

    /*!
     * Apply a translation to the transformation of the brush.
     */
    PainterEffectBrushParams&
    apply_translate(const vec2 &p)
    {
      m_brush.apply_translate(p);
      return *this;
    }

    /*!
     * Sets the brush to have a matrix and translation in its
     * transformation
     * \param p translation value for brush transformation
     * \param m matrix value for brush transformation
     */
    PainterEffectBrushParams&
    transformation(const vec2 &p, const float2x2 &m)
    {
      m_brush.transformation(p, m);
      return *this;
    }

    /*!
     * Sets the brush to have no translation in its transformation.
     */
    PainterEffectBrushParams&
    no_transformation_translation(void)
    {
      m_brush.no_transformation_translation();
      return *this;
    }

    /*!
     * Sets the brush to have no matrix in its transformation.
     */
    PainterEffectBrushParams&
    no_transformation_matrix(void)
    {
      m_brush.no_transformation_matrix();
      return *this;
    }

    /*!
     * Sets the brush to have no transformation.
     */
    PainterEffectBrushParams&
    no_transformation(void)
    {
      m_brush.no_transformation();
      return *this;
    }

    /*!
     * Sets the brush to have a repeat window
     * \param pos location of repeat window
     * \param size of repeat window
     * \param x_mode spread mode for x-coordinate
     * \param y_mode spread mode for y-coordinate
     */
    PainterEffectBrushParams&
    repeat_window(const vec2 &pos, const vec2 &size,
                  enum PainterBrush::spread_type_t x_mode = PainterBrush::spread_repeat,
                  enum PainterBrush::spread_type_t y_mode = PainterBrush::spread_repeat)
    {
      m_brush.repeat_window(pos, size, x_mode, y_mode);
      return *this;
    }

    /*!
     * Sets the brush to not have a repeat window
     */
    PainterEffectBrushParams&
    no_repeat_window(void)
    {
      m_brush.no_repeat_window();
      return *this;
    }

  private:
    friend class PainterEffectBrush;
    PainterBrush m_brush;
  };

/*! @} */
};

#endif
