/*!
 * \file painter_brush_shader_set.hpp
 * \brief file painter_brush_shader_set.hpp
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

#pragma once

#include <fastuidraw/painter/shader/painter_brush_shader.hpp>
#include <fastuidraw/painter/shader/painter_image_brush_shader.hpp>
#include <fastuidraw/painter/shader/painter_gradient_brush_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A \ref PainterBrushShaderSet holds the \ref PainterBrushShader
   * objects to be used with the default brushes.
   */
  class PainterBrushShaderSet
  {
  public:
    /*!
     * Ctor
     */
    PainterBrushShaderSet(void);

    /*!
     * Copy ctor.
     */
    PainterBrushShaderSet(const PainterBrushShaderSet &obj);

    ~PainterBrushShaderSet();

    /*!
     * Assignment operator.
     */
    PainterBrushShaderSet&
    operator=(const PainterBrushShaderSet &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterBrushShaderSet &obj);

    /*!
     * Returns the \ref PainterBrushShader that uses
     * data packed by \ref PainterBrush to apply a
     * brush as specified by the \ref PainterBrush that
     * packed the data.
     */
    const reference_counted_ptr<PainterBrushShader>&
    standard_brush(void) const;

    /*!
     * Set the value returned by standard_brush(void) const.
     * \param sh value to use
     */
    PainterBrushShaderSet&
    standard_brush(const reference_counted_ptr<PainterBrushShader> &sh);

    /*!
     * Returns the \ref PainterImageBrushShader.
     */
    const reference_counted_ptr<const PainterImageBrushShader>&
    image_brush(void) const;

    /*!
     * Set the value returned by image_brush(void) const.
     * \param sh value to use
     */
    PainterBrushShaderSet&
    image_brush(const reference_counted_ptr<const PainterImageBrushShader> &sh);

    /*!
     * Returns the \ref PainterGradientBrushShader.
     */
    const reference_counted_ptr<const PainterGradientBrushShader>&
    gradient_brush(void) const;

    /*!
     * Set the value returned by gradient_brush(void) const.
     * \param sh value to use
     */
    PainterBrushShaderSet&
    gradient_brush(const reference_counted_ptr<const PainterGradientBrushShader> &sh);
  private:
    void *m_d;
  };

/*! @} */
}
