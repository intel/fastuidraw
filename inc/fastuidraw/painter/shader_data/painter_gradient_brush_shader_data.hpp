/*!
 * \file painter_gradient_brush_shader_data.hpp
 * \brief file painter_gradient_brush_shader_data.hpp
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


#pragma once

#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/shader_data/painter_brush_shader_data.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaderData
 * @{
 */

  /*!
   * \brief
   * A \ref PainterGradientBrushShaderData defines the \ref
   * PainterBrushShaderData that the shaders of a \ref
   * PainterGradientBrushShader consume. It specifies what
   * \ref ColorStopSequence to use together with the
   * geometric properties of the gradient.
   */
  class PainterGradientBrushShaderData:
    public PainterBrushShaderData,
    public PainterBrushEnums
  {
  public:
    /*!
     * \brief
     * Bit encoding for packing ColorStopSequence::texel_location()
     */
    enum color_stop_xy_encoding
      {
        color_stop_x_num_bits = 16, /*!< number bits to encode ColorStopSequence::texel_location().x() */
        color_stop_y_num_bits = 16, /*!< number bits to encode ColorStopSequence::texel_location().y() */

        color_stop_x_bit0 = 0, /*!< where ColorStopSequence::texel_location().x() is encoded */
        color_stop_y_bit0 = color_stop_x_num_bits /*!< where ColorStopSequence::texel_location().y() is encoded */
      };

    /*!
     * \brief
     * Enumeration that provides offset, in units of
     * uint32_t, of the packing of the gradient data
     */
    enum gradient_offset_t
      {
        /*!
         * Offset to x-coordinate of starting point of both
         * linear and radial gradients (packed at float)
         */
        p0_x_offset = 0,

        /*!
         * Offset to y-coordinate of starting point of both
         * linear and radial gradients (packed at float)
         */
        p0_y_offset,

        /*!
         * Offset to x-coordinate of ending point of both
         * linear and radial gradients (packed at float)
         */
        p1_x_offset,

        /*!
         * Offset to y-coordinate of ending point of both
         * linear and radial gradients (packed at float)
         */
        p1_y_offset,

        /*!
         * Offset to the x and y-location of the color stops.
         * The offset is stored as a uint32 packed as according
         * in the enumeration \ref color_stop_xy_encoding
         */
        color_stop_xy_offset,

        /*!
         * Offset to the length of the color stop in -texels-, i.e.
         * ColorStopSequence::width(), packed as a uint32
         */
        color_stop_length_offset,

        /*!
         * Size of the data for linear gradients.
         */
        linear_data_size,

        /*!
         * Offset to starting radius of gradient
         * (packed at float) (radial gradient only)
         */
        start_radius_offset = linear_data_size,

        /*!
         * Offset to ending radius of gradient
         * (packed at float) (radial gradient only)
         */
        end_radius_offset,

        /*!
         * Size of the data for radial gradients.
         */
        radial_data_size,

        /*!
         * Offset to the x-coordinate of the point
         * of a sweep gradient.
         */
        sweep_p_x_offset = p0_x_offset,

        /*!
         * Offset to the y-coordinate of the point
         * of a sweep gradient.
         */
        sweep_p_y_offset = p0_y_offset,

        /*!
         * Offset to the angle of a sweep gradient.
         */
        sweep_angle_offset = p1_x_offset,

        /*!
         * Offset to the sign-factor of the sweep gradient.
         * The sign of the value is the sign of the sweep
         * gradient and the magnitude is the repeat factor
         * of the gradient.
         */
        sweep_sign_factor_offset = p1_y_offset,

        /*!
         * Size of the data for sweep gradients.
         */
        sweep_data_size = linear_data_size,
      };

    /*!
     * Ctor. Initializes the brush to have no gradient.
     */
    PainterGradientBrushShaderData(void)
    {}

    /*!
     * Copy ctor.
     */
    PainterGradientBrushShaderData(const PainterGradientBrushShaderData &obj):
      m_data(obj.m_data)
    {}

    /*!
     * Assignment operator.
     * \param obj value from which to copy
     */
    PainterGradientBrushShaderData&
    operator=(const PainterGradientBrushShaderData &obj)
    {
      m_data = obj.m_data;
      return *this;
    }

    /*!
     * Reset the brush to initial conditions.
     */
    PainterGradientBrushShaderData&
    reset(void)
    {
      m_data.m_cs.clear();
      m_data.m_type = gradient_non;
      return *this;
    }

    /*!
     * Returns the type of gradient the data specifies.
     */
    enum gradient_type_t
    type(void) const
    {
      return m_data.m_type;
    }

    /*!
     * Returns the value of the handle to the
     * ColorStopSequence that the
     * brush is set to use.
     */
    const reference_counted_ptr<const ColorStopSequence>&
    color_stops(void) const
    {
      return m_data.m_cs;
    }

    /*!
     * Sets the brush to have a linear gradient.
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param start_p start position of gradient
     * \param end_p end position of gradient.
     */
    PainterGradientBrushShaderData&
    linear_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                    const vec2 &start_p, const vec2 &end_p)
    {
      m_data.m_cs = cs;
      m_data.m_grad_start = start_p;
      m_data.m_grad_end = end_p;
      m_data.m_type = cs ? gradient_linear : gradient_non;
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
     */
    PainterGradientBrushShaderData&
    radial_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                    const vec2 &start_p, float start_r,
                    const vec2 &end_p, float end_r)
    {
      m_data.m_cs = cs;
      m_data.m_grad_start = start_p;
      m_data.m_grad_start_r = start_r;
      m_data.m_grad_end = end_p;
      m_data.m_grad_end_r = end_r;
      m_data.m_type = cs ? gradient_radial : gradient_non;
      return *this;
    }

    /*!
     * Sets the brush to have a radial gradient. Provided as
     * a conveniance, equivalent to
     * \code
     * radial_gradient(cs, p, 0.0f, p, r);
     * \endcode
     * \param cs color stops for gradient. If handle is invalid,
     *           then sets brush to not have a gradient.
     * \param p start and end position of gradient
     * \param r ending radius of radial gradient
     */
    PainterGradientBrushShaderData&
    radial_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                    const vec2 &p, float r)
    {
      return radial_gradient(cs, p, 0.0f, p, r);
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
     */
    PainterGradientBrushShaderData&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                   const vec2 &p, float theta, float F = 1.0f)
    {
      m_data.m_cs = cs;
      m_data.m_grad_start = p;
      m_data.m_grad_end = vec2(theta, F);
      m_data.m_type = cs ? gradient_sweep : gradient_non;
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
     */
    PainterGradientBrushShaderData&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                   const vec2 &p, float theta,
                   enum PainterEnums::screen_orientation orientation,
                   enum PainterEnums::rotation_orientation_t rotation_orientation,
                   float F)
    {
      float S;
      bool b1(orientation == PainterEnums::y_increases_upwards);
      bool b2(rotation_orientation == PainterEnums::counter_clockwise);

      S = (b1 == b2) ? 1.0f : -1.0f;
      return sweep_gradient(cs, p, theta, S * F);
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
     */
    PainterGradientBrushShaderData&
    sweep_gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
                   const vec2 &p, float theta,
                   enum PainterEnums::screen_orientation orientation,
                   enum PainterEnums::rotation_orientation_t rotation_orientation)
    {
      return sweep_gradient(cs, p, theta, orientation, rotation_orientation, 1.0f);
    }

    unsigned int
    data_size(void) const override;

    void
    pack_data(c_array<uvec4> dst) const override;

    void
    save_resources(c_array<reference_counted_ptr<const resource_base> > dst) const override;

    unsigned int
    number_resources(void) const override;

    c_array<const reference_counted_ptr<const Image> >
    bind_images(void) const override
    {
      return c_array<const reference_counted_ptr<const Image> >();
    }

  private:
    class data
    {
    public:
      data(void):
        m_grad_start(0.0f, 0.0f),
        m_grad_end(1.0f, 1.0f),
        m_grad_start_r(0.0f),
        m_grad_end_r(1.0f),
        m_type(gradient_non)
      {}

      reference_counted_ptr<const ColorStopSequence> m_cs;
      vec2 m_grad_start, m_grad_end;
      float m_grad_start_r, m_grad_end_r;
      enum gradient_type_t m_type;
    };

    data m_data;
  };

/*! @} */
}
