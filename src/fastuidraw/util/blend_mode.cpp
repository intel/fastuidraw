/*!
 * \file blend_mode.cpp
 * \brief file blend_mode.cpp
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

#include <assert.h>

#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/util/util.hpp>

namespace
{
  enum
    {
      op_num_bits = 3,
      func_num_bits = 5,
    };

  enum
    {
      blending_on_bit0 = 0,
      blending_on_num_bits = 1,

      op_rgb_bit0 = blending_on_bit0 + blending_on_num_bits,
      op_rgb_num_bits = op_num_bits,

      op_alpha_bit0 = op_rgb_bit0 + op_rgb_num_bits,
      op_alpha_num_bits = op_num_bits,

      func_src_rgb_bit0 = op_alpha_bit0 + op_alpha_num_bits,
      func_src_rgb_num_bits = func_num_bits,

      fun_src_alpha_bit0 = func_src_rgb_bit0 + func_src_rgb_num_bits,
      fun_src_alpha_num_bits = func_num_bits,

      func_dst_rgb_bit0 = fun_src_alpha_bit0 + fun_src_alpha_num_bits,
      func_dst_rgb_num_bits = func_num_bits,

      func_dst_alpha_bit0 = func_dst_rgb_bit0 + func_dst_rgb_num_bits,
      func_dst_alpha_num_bits = func_num_bits,

      total_blending_bits = func_dst_alpha_bit0 + func_dst_alpha_num_bits
    };

  template<typename T>
  void
  set_value(fastuidraw::BlendMode::packed_value bit0,
            fastuidraw::BlendMode::packed_value num_bits,
            fastuidraw::BlendMode::packed_value value,
            T &dest)
  {
    dest = static_cast<T>(fastuidraw::unpack_bits(bit0, num_bits, value));
  }

  template<typename T>
  fastuidraw::BlendMode::packed_value
  pack_value(fastuidraw::BlendMode::packed_value bit0,
             fastuidraw::BlendMode::packed_value num_bits,
             T value)
  {
    return fastuidraw::pack_bits(bit0, num_bits, fastuidraw::BlendMode::packed_value(value));
  }
}

////////////////////////////////////////////
// fastuidraw::BlendMode methods
fastuidraw::BlendMode::
BlendMode(packed_value v)
{
  assert(total_blending_bits <= 8 * sizeof(packed_value));
  set_value(blending_on_bit0, blending_on_num_bits, v, m_blending_on);
  set_value(op_rgb_bit0, op_rgb_num_bits, v, m_blend_equation[Kequation_rgb]);
  set_value(op_alpha_bit0, op_alpha_num_bits, v, m_blend_equation[Kequation_alpha]);
  set_value(func_src_rgb_bit0, func_src_rgb_num_bits, v, m_blend_func[Kfunc_src_rgb]);
  set_value(fun_src_alpha_bit0, fun_src_alpha_num_bits, v, m_blend_func[Kfunc_src_alpha]);
  set_value(func_dst_rgb_bit0, func_dst_rgb_num_bits, v, m_blend_func[Kfunc_dst_rgb]);
  set_value(func_dst_alpha_bit0, func_dst_alpha_num_bits, v, m_blend_func[Kfunc_dst_alpha]);
}

fastuidraw::BlendMode::packed_value
fastuidraw::BlendMode::
packed(void) const
{
  return pack_value(blending_on_bit0, blending_on_num_bits, m_blending_on)
    | pack_value(op_rgb_bit0, op_rgb_num_bits, m_blend_equation[Kequation_rgb])
    | pack_value(op_alpha_bit0, op_alpha_num_bits, m_blend_equation[Kequation_alpha])
    | pack_value(func_src_rgb_bit0, func_src_rgb_num_bits, m_blend_func[Kfunc_src_rgb])
    | pack_value(fun_src_alpha_bit0, fun_src_alpha_num_bits, m_blend_func[Kfunc_src_alpha])
    | pack_value(func_dst_rgb_bit0, func_dst_rgb_num_bits, m_blend_func[Kfunc_dst_rgb])
    | pack_value(func_dst_alpha_bit0, func_dst_alpha_num_bits, m_blend_func[Kfunc_dst_alpha]);
}
