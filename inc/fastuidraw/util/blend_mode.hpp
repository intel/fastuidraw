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
        ADD,
        SUBTRACT,
        REVERSE_SUBTRACT,
        MIN,
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
        ZERO,
        ONE,
        SRC_COLOR,
        ONE_MINUS_SRC_COLOR,
        DST_COLOR,
        ONE_MINUS_DST_COLOR,
        SRC_ALPHA,
        ONE_MINUS_SRC_ALPHA,
        DST_ALPHA,
        ONE_MINUS_DST_ALPHA,
        CONSTANT_COLOR,
        ONE_MINUS_CONSTANT_COLOR,
        CONSTANT_ALPHA,
        ONE_MINUS_CONSTANT_ALPHA,
        SRC_ALPHA_SATURATE,
        SRC1_COLOR,
        ONE_MINUS_SRC1_COLOR,
        SRC1_ALPHA,
        ONE_MINUS_SRC1_ALPHA,

        NUMBER_FUNCS,
      };

    /*!
     * Ctor.
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
        dst_func_alpha_bit0 = dst_func_rgb_bit0 + func_num_bits
      };

    uint32_t m_value;
  };
/*! @} */
}
