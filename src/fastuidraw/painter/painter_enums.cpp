/*!
 * \file painter_enums.cpp
 * \brief file painter_enums.cpp
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


#include <fastuidraw/painter/painter_enums.hpp>

enum fastuidraw::PainterEnums::fill_rule_t
fastuidraw::PainterEnums::
complement_fill_rule(enum fill_rule_t f)
{
  switch(f)
    {
    case odd_even_fill_rule:
      return complement_odd_even_fill_rule;

    case complement_odd_even_fill_rule:
      return odd_even_fill_rule;

    case nonzero_fill_rule:
      return complement_nonzero_fill_rule;

    case complement_nonzero_fill_rule:
      return nonzero_fill_rule;

    default:
      return f;
    }
}
