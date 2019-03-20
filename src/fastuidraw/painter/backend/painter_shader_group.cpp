/*!
 * \file painter_shader_group.cpp
 * \brief file painter_shader_group.cpp
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

#include <private/painter_backend/painter_packer.hpp>

uint32_t
fastuidraw::PainterShaderGroup::
composite_group(void) const
{
  return PainterPacker::composite_group(this);
}

uint32_t
fastuidraw::PainterShaderGroup::
item_group(void) const
{
  return PainterPacker::item_group(this);
}

uint32_t
fastuidraw::PainterShaderGroup::
brush(void) const
{
  return PainterPacker::brush(this);
}

fastuidraw::BlendMode
fastuidraw::PainterShaderGroup::
composite_mode(void) const
{
  return PainterPacker::composite_mode(this);
}
