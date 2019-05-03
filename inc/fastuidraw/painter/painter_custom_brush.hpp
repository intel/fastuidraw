/*!
 * \file painter_data.hpp
 * \brief file painter_data.hpp
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

#include <fastuidraw/painter/painter_data_value.hpp>
#include <fastuidraw/painter/painter_brush_shader_data.hpp>
#include <fastuidraw/painter/shader/painter_brush_shader.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterCustomBrush is just a conveniance to wrap a
   * pointer to a \ref PainterBrushShader
   * together with a PainterDataValue<PainterBrushShaderData>.
   */
  class PainterCustomBrush
  {
  public:
    /*!
     * Ctor.
     * \param sh value with which to initialize \ref m_shader
     * \param d value with which to initialize \ref m_data
     */
    PainterCustomBrush(const PainterBrushShader *sh,
                       const PainterDataValue<PainterBrushShaderData> &d
                       = PainterDataValue<PainterBrushShaderData>()):
      m_shader(sh),
      m_data(d)
    {}

    /*!
     * Ctor.
     * \param sh value with which to initialize \ref m_shader
     * \param d value with which to initialize \ref m_data
     */
    PainterCustomBrush(const PainterDataValue<PainterBrushShaderData> &d,
                       const PainterBrushShader *sh):
      m_shader(sh),
      m_data(d)
    {}

    /*!
     * What \ref PainterBrushShader is used
     */
    const PainterBrushShader *m_shader;

    /*!
     * What, if any, data for \ref m_shader to use.
     */
    PainterDataValue<PainterBrushShaderData> m_data;
  };
/*! @} */
}
