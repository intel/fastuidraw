/*!
 * \file painter_shader_group.hpp
 * \brief file painter_shader_group.hpp
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
#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/painter/shader/painter_composite_shader.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A PainterShaderGroup gives to what groups the active shaders
   * of a \ref Painter belong.
   */
  class PainterShaderGroup:noncopyable
  {
  public:
    /*!
     * The group (see PainterShader::group())
     * of the active composite shader.
     */
    uint32_t
    composite_group(void) const;

    /*!
     * The group (see PainterShader::group())
     * of the active item shader.
     */
    uint32_t
    item_group(void) const;

    /*!
     * The shading ID as returned by PainterBrush::shader()
     * of the active brush.
     */
    uint32_t
    brush(void) const;

    /*!
     * The \ref BlendMode value for the active composite shader.
     */
    BlendMode
    composite_mode(void) const;

    /*!
     * Returns the \ref PainterCompositeShader::shader_type
     * of the active \ref PainterCompositeShader. If there is
     * no active \ref PainterCompositeShader, returns
     * \ref PainterCompositeShader::number_types.
     */
    enum PainterCompositeShader::shader_type
    composite_shader_type(void) const;

  protected:
    /*!
     * Ctor, do NOT derive from PainterShaderGroup, doing
     * so is asking for heaps of trouble and pain.
     */
    PainterShaderGroup(void) {}
  };

/*! @} */
}
