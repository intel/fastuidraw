/*!
 * \file painter_dashed_stroke_shader_set.hpp
 * \brief file painter_dashed_stroke_shader_set.hpp
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

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/painter/painter_attribute.hpp>
#include <fastuidraw/painter/painter_stroke_shader.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterDashedStrokeShaderSet holds a collection of
   * PainterStrokeShaderSet objects for the purpose of
   * dashed stroking. The shaders within a
   * PainterDashedStrokeShaderSet are expected to draw
   * any caps of dashed stroking from using just the edge
   * data. In particular, attributes/indices for caps induced
   * by stroking are NOT given to a shader within a
   * PainterDashedStrokeShaderSet.
   */
  class PainterDashedStrokeShaderSet
  {
  public:
    /*!
     * Ctor
     */
    PainterDashedStrokeShaderSet(void);

    /*!
     * Copy ctor.
     */
    PainterDashedStrokeShaderSet(const PainterDashedStrokeShaderSet &obj);

    ~PainterDashedStrokeShaderSet();

    /*!
     * Assignment operator.
     */
    PainterDashedStrokeShaderSet&
    operator=(const PainterDashedStrokeShaderSet &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterDashedStrokeShaderSet &obj);

    /*!
     * Shader set for dashed stroking of paths where the stroking
     * width is given in same units as the original path.
     * The stroking parameters are given by PainterDashedStrokeParams.
     * \param st cap style
     */
    const PainterStrokeShader&
    shader(enum PainterEnums::cap_style st) const;

    /*!
     * Set the value returned by dashed_stroke_shader(enum PainterEnums::cap_style) const.
     * \param st cap style
     * \param sh value to use
     */
    PainterDashedStrokeShaderSet&
    shader(enum PainterEnums::cap_style st, const PainterStrokeShader &sh);

  private:
    void *m_d;
  };

/*! @} */
}
