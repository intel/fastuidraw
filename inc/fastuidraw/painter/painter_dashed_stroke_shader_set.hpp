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
    \brief
    A DashEvaluatorBase is the interface used by Painter
    to realize the data to send to a PainterPacker for
    the purpose of dashed stroking.
   */
  class DashEvaluatorBase:
    public reference_counted<DashEvaluatorBase>::default_base
  {
  public:
    /*!
      To be implemented by a derived class to return the number
      of joins.
      \param data source PainterAttributeData
      \param edge_closed if true, include in the return value the
                         number of joins including those joins
                         from the closing edges of each contour.
     */
    virtual
    unsigned int
    number_joins(const PainterAttributeData &data, bool edge_closed) const = 0;

    /*!
      To be implemented by a derived class to return
      the chunk index, i.e. the value to feed
      \ref PainterAttributeData::attribute_data_chunk()
      and \ref PainterAttributeData::index_data_chunk(),
      for the named join.
      \param J (global) join index
    */
    virtual
    unsigned int
    named_join_chunk(unsigned int J) const = 0;

    /*!
      To be implemented by a derived class to return true if and
      only if a point from a join emobodied by a PainterAttribute
      is covered by a dash pattern.
      \param data PainterItemShaderData::DataBase object holding the data to
                  be sent to the shader
      \param attrib PainterAttribute from which to extract the distance to
                    use to compute the return value
     */
    virtual
    bool
    covered_by_dash_pattern(const PainterShaderData::DataBase *data,
                            const PainterAttribute &attrib) const = 0;
  };

  /*!
    \brief
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
      Initial value is nullptr.
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
