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
#include <fastuidraw/painter/painter_value.hpp>
#include <fastuidraw/painter/painter_stroke_shader.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    A DashEvaluator is used by Painter to realize the
    data to send to a PainterPacker for the purpose
    of dashed stroking.
   */
  class DashEvaluator:
    public reference_counted<DashEvaluator>::default_base
  {
  public:
    /*!
      To be implemented by a derived class to compute the interval
      a point along dashing belongs. If the interval is part of the
      solid part of drawing, then returns true, otherwise returns
      false.
      \param data PainterItemShaderData::DataBase object holding the data to
                  be sent to the shader
      \param distance the distance to use to compute the return value
      \param[out] out_interval interval to which input point belongs.
     */
    virtual
    bool
    compute_dash_interval(const PainterShaderData::DataBase *data,
                          float distance,
                          range_type<float> &out_interval) const;
  };

  /*!
    A PainterDashedStrokeShaderSet holds a collection of
    PainterStrokeShaderSet objects for the purpose of
    dashed stroking. The shaders within a
    PainterDashedStrokeShaderSet are expected to draw
    any caps of dashed stroking from using just the edge
    data. In particular, attributes/indices for caps are
    NEVER given to a shader within a PainterDashedStrokeShaderSet.
   */
  class PainterDashedStrokeShaderSet
  {
  public:
    /*!
      Ctor
     */
    PainterDashedStrokeShaderSet(void);

    /*!
      Copy ctor.
     */
    PainterDashedStrokeShaderSet(const PainterDashedStrokeShaderSet &obj);

    ~PainterDashedStrokeShaderSet();

    /*!
      Assignment operator.
     */
    PainterDashedStrokeShaderSet&
    operator=(const PainterDashedStrokeShaderSet &rhs);

    /*!
      Returns the DashEvaluator object to be used with
      the expected PainterItemShaderData passed to the
      PainterStrokeShader objects of this
      PainterDashedStrokeShaderSet.
     */
    const reference_counted_ptr<const DashEvaluator>&
    dash_evaluator(void) const;

    /*!
      Set the value returned by dash_evaluator(void) const.
      Initial value is NULL.
     */
    PainterDashedStrokeShaderSet&
    dash_evaluator(const reference_counted_ptr<const DashEvaluator>&);

    /*!
      Shader set for dashed stroking of paths where the stroking
      width is given in same units as the original path.
      The stroking parameters are given by PainterDashedStrokeParams.
      \param st cap style
     */
    const PainterStrokeShader&
    shader(enum PainterEnums::cap_style st) const;

    /*!
      Set the value returned by dashed_stroke_shader(enum PainterEnums::cap_style) const.
      \param st cap style
      \param sh value to use
     */
    PainterDashedStrokeShaderSet&
    shader(enum PainterEnums::cap_style st, const PainterStrokeShader &sh);

  private:
    void *m_d;
  };

/*! @} */
}
