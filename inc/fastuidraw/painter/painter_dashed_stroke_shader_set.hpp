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
  @{
 */

  /*!
    A DashEvaluatorBase is the interface used by Painter
    to realize the data to send to a PainterPacker for
    the purpose of dashed stroking.
   */
  class DashEvaluatorBase:
    public reference_counted<DashEvaluatorBase>::default_base
  {
  public:
    /*!
      To be implemented by a derived class to compute the interval
      a point along dashing belongs. If the interval is part of the
      solid part of drawing, then returns true, otherwise returns
      false.
      \param data PainterItemShaderData::DataBase object holding the data to
                  be sent to the shader
      \param attrib PainterAttribute from which to extract the distance to
                    use to compute the return value
      \param[out] out_interval interval to which input point belongs
      \param[out] distance distance value within out_interval
     */
    virtual
    bool
    compute_dash_interval(const PainterShaderData::DataBase *data,
                          const PainterAttribute &attrib,
                          range_type<float> &out_interval,
                          float &distance) const = 0;

    /*!
      To be implemented by derived class to adjust cap-join
      attribute values at join whose distance value is in a
      skip interval.
      \param data PainterItemShaderData::DataBase object holding the data to
                  be sent to the shader
      \param [inout] attribs attributes to adjust
      \param out_interval out_interval value as returned by compute_dash_interval()
      \param distance distance as returned by compute_dash_interval()
      \param item_matrix transformation from local item coordinates
                         to 3D API clip coordinates (i.e. the value of
                         PainterItemMatrix::m_item_matrix of
                         Painter::transformation(void) const.
     */
    virtual
    void
    adjust_cap_joins(const PainterShaderData::DataBase *data,
                     c_array<PainterAttribute> attribs,
                     range_type<float> out_interval,
                     float distance,
                     const float3x3 &item_matrix) const = 0;
    /*!
      To be implemented by derived class to adjust attributes
      coming from adjustable caps of starts and endings of
      contours.
      \param data PainterItemShaderData::DataBase object holding the data to
                  be sent to the shader
      \param [inout] attribs attributes to adjust
      \param item_matrix transformation from local item coordinates
                         to 3D API clip coordinates (i.e. the value of
                         PainterItemMatrix::m_item_matrix of
                         Painter::transformation(void) const.
     */
    virtual
    void
    adjust_caps(const PainterShaderData::DataBase *data,
                c_array<PainterAttribute> attribs,
                const float3x3 &item_matrix) const = 0;
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
    const reference_counted_ptr<const DashEvaluatorBase>&
    dash_evaluator(void) const;

    /*!
      Set the value returned by dash_evaluator(void) const.
      Initial value is NULL.
     */
    PainterDashedStrokeShaderSet&
    dash_evaluator(const reference_counted_ptr<const DashEvaluatorBase>&);

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
