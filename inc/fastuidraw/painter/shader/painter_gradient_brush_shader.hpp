/*!
 * \file painter_gradient_brush_shader.hpp
 * \brief file painter_gradient_brush_shader.hpp
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

#include <fastuidraw/painter/shader_data/painter_gradient_brush_shader_data.hpp>
#include <fastuidraw/painter/painter_custom_brush.hpp>
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

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
        spread_type_bit0 = 0,

        /*!
         * first bit used to encode the \ref PainterBrushEnums::gradient_type_t
         */
        gradient_type_bit0 = spread_type_bit0 + spread_type_num_bits,

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
        number_sub_shaders_of_specific_gradient = 1u << spread_type_num_bits
      };

    /*!
     * Various bit-mask values derived from \ref sub_shader_bits
     */
    enum sub_shader_masks
      {
        /*!
         * mask generated from \ref spread_type_bit0 and \ref spread_type_num_bits
         */
        gradient_spread_type_mask = FASTUIDRAW_MASK(spread_type_bit0, spread_type_num_bits),

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
                 enum spread_type_t spread) const;

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
