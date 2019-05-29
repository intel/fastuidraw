/*!
 * \file painter_shader_group.cpp
 * \brief file painter_shader_group.cpp
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

#include <private/painter_backend/painter_packer.hpp>

uint32_t
fastuidraw::PainterShaderGroup::
blend_group(void) const
{
  return PainterPacker::blend_group(this);
}

uint32_t
fastuidraw::PainterShaderGroup::
item_group(void) const
{
  return PainterPacker::item_group(this);
}

uint32_t
fastuidraw::PainterShaderGroup::
brush_group(void) const
{
  return PainterPacker::brush_group(this);
}

fastuidraw::BlendMode
fastuidraw::PainterShaderGroup::
blend_mode(void) const
{
  return PainterPacker::blend_mode(this);
}

enum fastuidraw::PainterBlendShader::shader_type
fastuidraw::PainterShaderGroup::
blend_shader_type(void) const
{
  return PainterPacker::blend_shader_type(this);
}
