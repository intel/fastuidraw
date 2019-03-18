/*!
 * \file blend_mode.hpp
 * \brief file blend_mode.hpp
 *
 * Copyright 2016 by Intel.
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

#include <stdint.h>

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{
/*!\addtogroup Utility
 * @{
 */
  /*!
   * \brief
   * Class to hold the blend mode as exposed by typical
   * 3D APIs
   */
  class BlendMode
  {
  public:
    /*!
     * \brief
     * Enumeration to specify blend equation, i.e. glBlendEquation.
     */
    enum equation_t
      {
        /*!
         * Indicates to add the values.
         */
        ADD,

        /*!
         * Indicates to subtract the values.
         */
        SUBTRACT,

        /*!
         * Indicates to reverse-subtract the values.
         */
        REVERSE_SUBTRACT,

        /*!
         * Indicates to min the values.
         */
        MIN,

        /*!
         * Indicates to max the values.
         */
        MAX,

        NUMBER_OPS
      };

    /*!
     * \brief
     * Enumeration to specify the blend coefficient factor,
     * i.e. glBlendFunc.
     */
    enum func_t
      {
        /*!
         * Indicates the coefficient value of 0 in each channel.
         */
        ZERO,

        /*!
         * Indicates the coefficient value of 1 in each channel.
         */
        ONE,

        /*!
         * Indicates the coefficient where each channel value
         * is the output of the fragment shader.
         */
        SRC_COLOR,

        /*!
         * Indicates the coefficient where each channel value
         * is one minus the output of the fragment shader.
         */
        ONE_MINUS_SRC_COLOR,

        /*!
         * Indicates the coefficient where each channel value
         * is the value in the framebuffer.
         */
        DST_COLOR,

        /*!
         * Indicates the coefficient where each channel value
         * is one minus the value in the framebuffer.
         */
        ONE_MINUS_DST_COLOR,

        /*!
         * Indicates the coefficient where each channel value
         * is the alpha value of the output of the fragment shader.
         */
        SRC_ALPHA,

        /*!
         * Indicates the coefficient where each channel value
         * is one minus the alpha value of the output of the
         * fragment shader.
         */
        ONE_MINUS_SRC_ALPHA,

        /*!
         * Indicates the coefficient where each channel value
         * is the alpha value in the framebuffer.
         */
        DST_ALPHA,

        /*!
         * Indicates the coefficient where each channel value
         * is one minus the alpha value in the framebuffer.
         */
        ONE_MINUS_DST_ALPHA,

        /*!
         * Indicates the coefficient where each channel value
         * comes from the constant color value specified as part
         * of the 3D API state.
         */
        CONSTANT_COLOR,

        /*!
         * Indicates the coefficient where each channel value
         * comes from one minus the constant color value specified
         * as part of the 3D API state.
         */
        ONE_MINUS_CONSTANT_COLOR,

        /*!
         * Indicates the coefficient where each channel is
         * the alpha channel of the constant color value
         * specified as part of the 3D API state.
         */
        CONSTANT_ALPHA,

        /*!
         * Indicates the coefficient where each channel is
         * one minus the alpha channel of the constant color
         * value specified as part of the 3D API state.
         */
        ONE_MINUS_CONSTANT_ALPHA,

        /*!
         * Indicates the coefficient where each channel value
         * is the alpha value of the output of the fragment shader
         * clamped to [0, 1].
         */
        SRC_ALPHA_SATURATE,

        /*!
         * Indicates the coefficient where each channel value
         * is the secondary output of the fragment shader (the
         * secondary output as present in dual-src blending).
         */
        SRC1_COLOR,

        /*!
         * Indicates the coefficient where each channel value
         * is one minus the secondary output of the fragment
         * shader (the secondary output as present in dual-src
         * blending).
         */
        ONE_MINUS_SRC1_COLOR,

        /*!
         * Indicates the coefficient where each channel value
         * is the alpha channel of the secondary output of the
         * fragment shader (the secondary output as present in
         * dual-src blending).
         */
        SRC1_ALPHA,

        /*!
         * Indicates the coefficient  where each channel value
         * is one minus the alpha channel of the secondary output
         * of the fragment shader (the secondary output as present
         * in dual-src blending).
         */
        ONE_MINUS_SRC1_ALPHA,

        NUMBER_FUNCS,
      };

    /*!
     * Ctor. Initializes as valid with blending on,
     * with blend equation as add in all channels,
     * with src func in all channels as \ref ONE
     * and dest func in all channels as \ref ZERO.
     */
    BlendMode(void)
    {
      m_value = pack_bits(blending_on_bit, 1u, 1u)
        | pack_bits(equation_rgb_bit0, equation_num_bits, ADD)
        | pack_bits(equation_alpha_bit0, equation_num_bits, ADD)
        | pack_bits(src_func_rgb_bit0, func_num_bits, ONE)
        | pack_bits(src_func_alpha_bit0, func_num_bits, ONE)
        | pack_bits(dst_func_rgb_bit0, func_num_bits, ZERO)
        | pack_bits(dst_func_alpha_bit0, func_num_bits, ZERO);
    }

    /*!
     * Equality comparison operator.
     */
    bool
    operator==(BlendMode rhs) const
    {
      return m_value == rhs.m_value;
    }

    /*!
     * Inequality comparison operator.
     */
    bool
    operator!=(BlendMode rhs) const
    {
      return m_value != rhs.m_value;
    }

    /*!
     * Set the BlendMode to a value to mark it as invalid.
     */
    BlendMode&
    set_as_invalid(void)
    {
      m_value |= (1u << invalid_bit);
      return *this;
    }

    /*!
     * Set the BlendMode to a value to mark it as valid.
     */
    BlendMode&
    set_as_valid(void)
    {
      m_value &= ~(1u << invalid_bit);
      return *this;
    }

    /*!
     * Returns true if the BlendMode has been marked as invalid,
     * see set_as_invalid() and set_as_valid().
     */
    bool
    is_valid(void) const
    {
      return !(m_value & (1u << invalid_bit));
    }

    /*!
     * Set that 3D API blending is on or off.
     * Default value is true.
     */
    BlendMode&
    blending_on(bool v)
    {
      m_value &= ~FASTUIDRAW_MASK(blending_on_bit, 1u);
      m_value |= pack_bits(blending_on_bit, 1u, uint32_t(v));
      return *this;
    }

    /*!
     * Return the value as set by blending_on(bool).
     */
    bool
    blending_on(void) const
    {
      return unpack_bits(blending_on_bit, 1u, m_value) != 0u;
    }

    /*!
     * Set the blend equation for the RGB channels.
     * Default value is ADD.
     */
    BlendMode&
    equation_rgb(enum equation_t v)
    {
      m_value &= ~FASTUIDRAW_MASK(equation_rgb_bit0, equation_num_bits);
      m_value |= pack_bits(equation_rgb_bit0, equation_num_bits, v);
      return *this;
    }

    /*!
     * Return the value as set by equation_rgb(enum equation_t).
     */
    enum equation_t
    equation_rgb(void) const
    {
      return static_cast<enum equation_t>(unpack_bits(equation_rgb_bit0, equation_num_bits, m_value));
    }

    /*!
     * Set the blend equation for the Alpha channel.
     * Default value is ADD.
     */
    BlendMode&
    equation_alpha(enum equation_t v)
    {
      m_value &= ~FASTUIDRAW_MASK(equation_alpha_bit0, equation_num_bits);
      m_value |= pack_bits(equation_alpha_bit0, equation_num_bits, v);
      return *this;
    }

    /*!
     * Return the value as set by equation_alpha(enum equation_t).
     */
    enum equation_t
    equation_alpha(void) const
    {
      return static_cast<enum equation_t>(unpack_bits(equation_alpha_bit0, equation_num_bits, m_value));
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * equation_rgb(v);
     * equation_alpha(v);
     * \endcode
     */
    BlendMode&
    equation(enum equation_t v)
    {
      equation_rgb(v);
      return equation_alpha(v);
    }

    /*!
     * Set the source coefficient for the RGB channels.
     * Default value is ONE.
     */
    BlendMode&
    func_src_rgb(enum func_t v)
    {
      m_value &= ~FASTUIDRAW_MASK(src_func_rgb_bit0, func_num_bits);
      m_value |= pack_bits(src_func_rgb_bit0, func_num_bits, v);
      return *this;
    }

    /*!
     * Return the value as set by func_src_rgb(enum t).
     */
    enum func_t
    func_src_rgb(void) const
    {
      return static_cast<enum func_t>(unpack_bits(src_func_rgb_bit0, func_num_bits, m_value));
    }

    /*!
     * Set the source coefficient for the Alpha channel.
     * Default value is ONE.
     */
    BlendMode&
    func_src_alpha(enum func_t v)
    {
      m_value &= ~FASTUIDRAW_MASK(src_func_alpha_bit0, func_num_bits);
      m_value |= pack_bits(src_func_alpha_bit0, func_num_bits, v);
      return *this;
    }

    /*!
     * Return the value as set by func_src_alpha(enum t).
     */
    enum func_t
    func_src_alpha(void) const
    {
      return static_cast<enum func_t>(unpack_bits(src_func_alpha_bit0, func_num_bits, m_value));
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * func_src_rgb(v);
     * func_src_alpha(v);
     * \endcode
     */
    BlendMode&
    func_src(enum func_t v)
    {
      func_src_rgb(v);
      return func_src_alpha(v);
    }

    /*!
     * Set the destication coefficient for the RGB channels.
     * Default value is ZERO.
     */
    BlendMode&
    func_dst_rgb(enum func_t v)
    {
      m_value &= ~FASTUIDRAW_MASK(dst_func_rgb_bit0, func_num_bits);
      m_value |= pack_bits(dst_func_rgb_bit0, func_num_bits, v);
      return *this;
    }

    /*!
     * Return the value as set by func_dst_rgb(enum t).
     */
    enum func_t
    func_dst_rgb(void) const
    {
      return static_cast<enum func_t>(unpack_bits(dst_func_rgb_bit0, func_num_bits, m_value));
    }

    /*!
     * Set the destication coefficient for the Alpha channel.
     * Default value is ZERO.
     */
    BlendMode&
    func_dst_alpha(enum func_t v)
    {
      m_value &= ~FASTUIDRAW_MASK(dst_func_alpha_bit0, func_num_bits);
      m_value |= pack_bits(dst_func_alpha_bit0, func_num_bits, v);
      return *this;
    }

    /*!
     * Return the value as set by func_dst_alpha(enum t).
     */
    enum func_t
    func_dst_alpha(void) const
    {
      return static_cast<enum func_t>(unpack_bits(dst_func_alpha_bit0, func_num_bits, m_value));
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * func_dst_rgb(v);
     * func_dst_alpha(v);
     * \endcode
     */
    BlendMode&
    func_dst(enum func_t v)
    {
      func_dst_rgb(v);
      return func_dst_alpha(v);
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * func_src(src);
     * func_dst(dst);
     * \endcode
     */
    BlendMode&
    func(enum func_t src, enum func_t dst)
    {
      func_src(src);
      return func_dst(dst);
    }

  private:
    enum
      {
        equation_num_bits = 3,
        func_num_bits = 5,
      };

    enum
      {
        blending_on_bit = 0,

        equation_rgb_bit0 = 1,
        equation_alpha_bit0 = equation_rgb_bit0 + equation_num_bits,

        src_func_rgb_bit0 = equation_alpha_bit0 + equation_num_bits,
        src_func_alpha_bit0 = src_func_rgb_bit0 + func_num_bits,

        dst_func_rgb_bit0 = src_func_alpha_bit0 + func_num_bits,
        dst_func_alpha_bit0 = dst_func_rgb_bit0 + func_num_bits,

        invalid_bit = dst_func_alpha_bit0 + func_num_bits,
      };

    uint32_t m_value;
  };
/*! @} */
}
