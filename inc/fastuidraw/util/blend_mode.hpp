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
    enum op_t
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
     * \brief
     * Represents a BlendMode packed as a single
     * 32-bit integer value, see also packed().
     */
    typedef uint32_t packed_value;

    /*!
     * Ctor.
     */
    BlendMode(void)
    {
      m_blending_on = true;
      m_blend_equation[Kequation_rgb] = m_blend_equation[Kequation_alpha] = ADD;
      m_blend_func[Kfunc_src_rgb] = m_blend_func[Kfunc_src_alpha] = ONE;
      m_blend_func[Kfunc_dst_rgb] = m_blend_func[Kfunc_dst_alpha] = ZERO;
    }

    /*!
     * Construct a BlendMode from a value as packed by
     * packed().
     */
    explicit
    BlendMode(packed_value v);

    /*!
     * Set that 3D API blending is on or off.
     * Default value is true.
     */
    BlendMode&
    blending_on(bool v)
    {
      m_blending_on = v;
      return *this;
    }

    /*!
     * Return the value as set by equation_rgb(enum op_t).
     */
    bool
    blending_on(void) const
    {
      return m_blending_on;
    }

    /*!
     * Set the blend equation for the RGB channels.
     * Default value is ADD.
     */
    BlendMode&
    equation_rgb(enum op_t v) { m_blend_equation[Kequation_rgb] = v; return *this; }

    /*!
     * Return the value as set by equation_rgb(enum op_t).
     */
    enum op_t
    equation_rgb(void) const { return m_blend_equation[Kequation_rgb]; }

    /*!
     * Set the blend equation for the Alpha channel.
     * Default value is ADD.
     */
    BlendMode&
    equation_alpha(enum op_t v) { m_blend_equation[Kequation_alpha] = v; return *this; }

    /*!
     * Return the value as set by equation_alpha(enum op_t).
     */
    enum op_t
    equation_alpha(void) const { return m_blend_equation[Kequation_alpha]; }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * equation_rgb(v);
     * equation_alpha(v);
     * \endcode
     */
    BlendMode&
    equation(enum op_t v)
    {
      m_blend_equation[Kequation_rgb] = v;
      m_blend_equation[Kequation_alpha] = v;
      return *this;
    }

    /*!
     * Set the source coefficient for the RGB channels.
     * Default value is ONE.
     */
    BlendMode&
    func_src_rgb(enum func_t v) { m_blend_func[Kfunc_src_rgb] = v; return *this; }

    /*!
     * Return the value as set by func_src_rgb(enum t).
     */
    enum func_t
    func_src_rgb(void) const { return m_blend_func[Kfunc_src_rgb]; }

    /*!
     * Set the source coefficient for the Alpha channel.
     * Default value is ONE.
     */
    BlendMode&
    func_src_alpha(enum func_t v) { m_blend_func[Kfunc_src_alpha] = v; return *this; }

    /*!
     * Return the value as set by func_src_alpha(enum t).
     */
    enum func_t
    func_src_alpha(void) const { return m_blend_func[Kfunc_src_alpha]; }

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
      m_blend_func[Kfunc_src_rgb] = m_blend_func[Kfunc_src_alpha] = v;
      return *this;
    }

    /*!
     * Set the destication coefficient for the RGB channels.
     * Default value is ZERO.
     */
    BlendMode&
    func_dst_rgb(enum func_t v) { m_blend_func[Kfunc_dst_rgb] = v; return *this; }

    /*!
     * Return the value as set by func_dst_rgb(enum t).
     */
    enum func_t
    func_dst_rgb(void) const { return m_blend_func[Kfunc_dst_rgb]; }

    /*!
     * Set the destication coefficient for the Alpha channel.
     * Default value is ZERO.
     */
    BlendMode&
    func_dst_alpha(enum func_t v) { m_blend_func[Kfunc_dst_alpha] = v; return *this; }

    /*!
     * Return the value as set by func_dst_alpha(enum t).
     */
    enum func_t
    func_dst_alpha(void) const { return m_blend_func[Kfunc_dst_alpha]; }

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
      m_blend_func[Kfunc_dst_rgb] = m_blend_func[Kfunc_dst_alpha] = v;
      return *this;
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
      m_blend_func[Kfunc_src_rgb] = m_blend_func[Kfunc_src_alpha] = src;
      m_blend_func[Kfunc_dst_rgb] = m_blend_func[Kfunc_dst_alpha] = dst;
      return *this;
    }

    /*!
     * Return the blend mode as a single packed 64-bit
     * unsigned integer.
     */
    packed_value
    packed(void) const;

  private:
    enum
      {
        Kequation_rgb,
        Kequation_alpha,
        Knumber_blend_equation_args
      };

    enum
      {
        Kfunc_src_rgb,
        Kfunc_src_alpha,
        Kfunc_dst_rgb,
        Kfunc_dst_alpha,
        Knumber_blend_args
      };

    bool m_blending_on;
    vecN<enum op_t, Knumber_blend_equation_args> m_blend_equation;
    vecN<enum func_t, Knumber_blend_args> m_blend_func;
  };
/*! @} */
}
