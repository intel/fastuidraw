/*!
 * \file painter_gradient_brush_shader.hpp
 * \brief file painter_gradient_brush_shader.hpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */


#pragma once

#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_brush_shader_data.hpp>
#include <fastuidraw/painter/painter_custom_brush.hpp>
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A \ref PainterGradientBrushShaderData defines the \ref
   * \ref PainterBrushShaderData that the shaders of a \ref
   * PainterGradientBrushShader consume. It specifies what
   * \ref ColorStopSequenceOnAtlas to use together with the
   * geometric properties of the gradient.
   */
  class PainterGradientBrushShaderData:
    public PainterBrushShaderData,
    public PainterBrushEnums
  {
  public:
    /*!
     * \brief
     * Bit encoding for packing ColorStopSequenceOnAtlas::texel_location()
     */
    enum color_stop_xy_encoding
      {
        color_stop_x_num_bits = 16, /*!< number bits to encode ColorStopSequenceOnAtlas::texel_location().x() */
        color_stop_y_num_bits = 16, /*!< number bits to encode ColorStopSequenceOnAtlas::texel_location().y() */

        color_stop_x_bit0 = 0, /*!< where ColorStopSequenceOnAtlas::texel_location().x() is encoded */
        color_stop_y_bit0 = color_stop_x_num_bits /*!< where ColorStopSequenceOnAtlas::texel_location().y() is encoded */
      };

    /*!
     * \brief
     * Enumeration that provides offset, in units of \ref
     * generic_data, of the packing of the gradient data
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
         * in the enumeration \ref gradient_color_stop_xy_encoding
         */
        color_stop_xy_offset,

        /*!
         * Offset to the length of the color stop in -texels-, i.e.
         * ColorStopSequenceOnAtlas::width(), packed as a uint32
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
     * ColorStopSequenceOnAtlas that the
     * brush is set to use.
     */
    const reference_counted_ptr<const ColorStopSequenceOnAtlas>&
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
    linear_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
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
    radial_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
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
    radial_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
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
    sweep_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
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
    sweep_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
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
    sweep_gradient(const reference_counted_ptr<const ColorStopSequenceOnAtlas> &cs,
                   const vec2 &p, float theta,
                   enum PainterEnums::screen_orientation orientation,
                   enum PainterEnums::rotation_orientation_t rotation_orientation)
    {
      return sweep_gradient(cs, p, theta, orientation, rotation_orientation, 1.0f);
    }

    unsigned int
    data_size(void) const override;

    void
    pack_data(c_array<vecN<generic_data, 4> > dst) const override;

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

      reference_counted_ptr<const ColorStopSequenceOnAtlas> m_cs;
      vec2 m_grad_start, m_grad_end;
      float m_grad_start_r, m_grad_end_r;
      enum gradient_type_t m_type;
    };

    data m_data;
  };

  /*!
   * \brief
   * A \ref PainterGradientBrushShader represents a set of brush shaders to
   * to perform a gradient. Internally it containts four generic parent shaders:
   *  - a parent shader able to handle any gradient and spread type
   *  - a parent shader only for linear gradients able to handle any spread type
   *  - a parent shader only for radial gradients able to handle any spread type
   *  - a parent shader only for sweep gradients able to handle any spread type
   */
  class PainterGradientBrushShader:
    public reference_counted<PainterGradientBrushShader>::concurrent,
    public PainterBrushEnums
  {
  public:
    /*!
     * \brief
     * Enumeration describing the roles of the bits for
     * the sub-shader ID's.
     */
    enum sub_shader_bits
      {
        /*!
         * Number of bits used to encode a \ref spread_type_t
         */
        spread_type_num_bits = 2,

        /*!
         * Number of bits used to encode the gradient type,
         * see \ref PainterBrushEnums::gradient_type_t
         */
        gradient_type_num_bits = 2,

        /*!
         * first bit used to encode the \ref spread_type_t
         */
        gradient_spread_type_bit0 = 0,

        /*!
         * first bit used to encode the \ref PainterBrushEnums::gradient_type_t
         */
        gradient_type_bit0 = gradient_spread_type_bit0 + spread_type_num_bits,

        /*!
         * The total number of bits needed to specify the sub-shader IDs.
         */
        number_bits = gradient_type_bit0 + gradient_type_num_bits,

        /*!
         * the total number of sub-shaders that the generic parent
         * shader has.
         */
        number_sub_shaders_of_generic_gradient = 1u << number_bits,

        /*!
         * the total number of sub-shaders that a parent shader for
         * a specific gradient type has.
         */
        number_sub_shaders_of_specific_gradient = 1u << gradient_type_num_bits
      };

    /*!
     * Various bit-mask values derived from \ref sub_shader_bits
     */
    enum sub_shader_masks
      {
        /*!
         * mask generated from \ref gradient_spread_type_bit0 and \ref spread_type_num_bits
         */
        gradient_spread_type_mask = FASTUIDRAW_MASK(gradient_spread_type_bit0, spread_type_num_bits),

        /*!
         * mask generated from \ref gradient_type_bit0 and \ref gradient_type_num_bits
         */
        gradient_type_mask = FASTUIDRAW_MASK(gradient_type_bit0, gradient_type_num_bits),
      };

    /*!
     * Ctor.
     * \param generic \ref PainterBrushShader that supports all gradient and
     *                sweep types via its sub-shaders which are indexed by \ref
     *                sub_shader_id(enum spread_type_t, enum gradient_type_t)
     * \param linear \ref PainterBrushShader that performs linear gradient
     *               that supports all sweep types via its sub-shaders which
     *               are indexed by \ref sub_shader_id(enum spread_type_t)
     * \param radial \ref PainterBrushShader that performs radial gradient
     *               that supports all sweep types via its sub-shaders which
     *               are indexed by \ref sub_shader_id(enum spread_type_t)
     * \param sweep \ref PainterBrushShader that performs sweep gradient
     *               that supports all sweep types via its sub-shaders which
     *               are indexed by \ref sub_shader_id(enum spread_type_t)
     * \param white \ref PainterBrushShader that applied solid white for the brush
     */
    PainterGradientBrushShader(const reference_counted_ptr<PainterBrushShader> &generic,
                               const reference_counted_ptr<PainterBrushShader> &linear,
                               const reference_counted_ptr<PainterBrushShader> &radial,
                               const reference_counted_ptr<PainterBrushShader> &sweep,
                               const reference_counted_ptr<PainterBrushShader> &white);

    ~PainterGradientBrushShader();

    /*!
     * Returns the sub-shader of the generic parent shader for
     * specified \ref gradient_type_t and \ref spread_type_t
     * values.
     */
    const reference_counted_ptr<PainterBrushShader>&
    sub_shader(enum spread_type_t, enum gradient_type_t) const;

    /*!
     * Returns the sub-shader of the linear gradient parent shader
     * for a specified \ref spread_type_t value.
     */
    const reference_counted_ptr<PainterBrushShader>&
    linear_sub_shader(enum spread_type_t) const;

    /*!
     * Returns the sub-shader of the radial gradient parent shader
     * for a specified \ref spread_type_t value.
     */
    const reference_counted_ptr<PainterBrushShader>&
    radial_sub_shader(enum spread_type_t) const;

    /*!
     * Returns the sub-shader of the sweep gradient parent shader
     * for a specified \ref spread_type_t value.
     */
    const reference_counted_ptr<PainterBrushShader>&
    sweep_sub_shader(enum spread_type_t) const;

    /*!
     * Returns the white shader, i.e. the shader that is
     * solid white to the brush.
     */
    const reference_counted_ptr<PainterBrushShader>&
    white_shader(void) const;

    /*!
     * Create a \ref PainterCustomBrush from a \ref
     * PainterGradientBrushShaderData
     * \param pool \ref PainterPackedValuePool used to create the packed value
     * \param brush_data \ref PainterGradientBrushShaderData specifing gradient
     * \param spread spread pattern for gradient
     */
    PainterCustomBrush
    create_brush(PainterPackedValuePool &pool,
                 const PainterGradientBrushShaderData &brush_data,
                 enum spread_type_t spread);

    /*!
     * The sub-shader to take from the generic parent shader
     * for specified \ref gradient_type_t and \ref spread_type_t
     * values.
     */
    static
    uint32_t
    sub_shader_id(enum spread_type_t, enum gradient_type_t);

    /*!
     * The sub-shader to take from the linear, radial or sweep
     * parent shader for a specified \ref spread_type_t value.
     */
    static
    uint32_t
    sub_shader_id(enum spread_type_t);

  private:
    void *m_d;
  };

/*! @} */
}
