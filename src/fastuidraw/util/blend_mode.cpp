/*!
 * \file blend_mode.cpp
 * \brief file blend_mode.cpp
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

#include <fastuidraw/util/blend_mode.hpp>

fastuidraw::c_string
fastuidraw::BlendMode::
label(enum equation_t v)
{
  static const c_string labels[NUMBER_OPS] =
    {
      [ADD] = "ADD",
      [SUBTRACT] = "SUBTRACT",
      [REVERSE_SUBTRACT] = "REVERSE_SUBTRACT",
      [MIN] = "MIN",
      [MAX] = "MAX",
    };
  return (v < NUMBER_OPS) ? labels[v] : "InvalidEnum";
}

fastuidraw::c_string
fastuidraw::BlendMode::
label(enum func_t v)
{
  static const c_string labels[NUMBER_FUNCS] =
    {
      [ZERO] = "ZERO",
      [ONE] = "ONE",
      [SRC_COLOR] = "SRC_COLOR",
      [ONE_MINUS_SRC_COLOR] = "ONE_MINUS_SRC_COLOR",
      [DST_COLOR] = "DST_COLOR",
      [ONE_MINUS_DST_COLOR] = "ONE_MINUS_DST_COLOR",
      [SRC_ALPHA] = "SRC_ALPHA",
      [ONE_MINUS_SRC_ALPHA] = "ONE_MINUS_SRC_ALPHA",
      [DST_ALPHA] = "DST_ALPHA",
      [ONE_MINUS_DST_ALPHA] = "ONE_MINUS_DST_ALPHA",
      [CONSTANT_COLOR] = "CONSTANT_COLOR",
      [ONE_MINUS_CONSTANT_COLOR] = "ONE_MINUS_CONSTANT_COLOR",
      [CONSTANT_ALPHA] = "CONSTANT_ALPHA",
      [ONE_MINUS_CONSTANT_ALPHA] = "ONE_MINUS_CONSTANT_ALPHA",
      [SRC_ALPHA_SATURATE] = "SRC_ALPHA_SATURATE",
      [SRC1_COLOR] = "SRC1_COLOR",
      [ONE_MINUS_SRC1_COLOR] = "ONE_MINUS_SRC1_COLOR",
      [SRC1_ALPHA] = "SRC1_ALPHA",
      [ONE_MINUS_SRC1_ALPHA] = "ONE_MINUS_SRC1_ALPHA",
    };
  return (v < NUMBER_FUNCS) ? labels[v] : "InvalidEnum";
}
