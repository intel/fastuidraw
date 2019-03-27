/*!
 * \file painter_fill_shader.hpp
 * \brief file painter_fill_shader.hpp
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

#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/shader/painter_item_shader.hpp>
#include <fastuidraw/painter/backend/painter_draw.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A PainterFillShader holds the shaders for drawing filled paths.
   * Anti-aliasing is accomplished by drawing 1-pixel thick rects
   * about the boundary of the filled path.
   */
  class PainterFillShader
  {
  public:
    /*!
     * Ctor
     */
    PainterFillShader(void);

    /*!
     * Copy ctor.
     */
    PainterFillShader(const PainterFillShader &obj);

    ~PainterFillShader();

    /*!
     * Assignment operator.
     */
    PainterFillShader&
    operator=(const PainterFillShader &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterFillShader &obj);

    /*!
     * Returns the PainterItemShader to use to draw
     * the filled path triangles. The expected format
     * of the attributes is as found in the \ref
     * PainterAttributeData returned by \ref
     * FilledPath::Subset::painter_data().
     */
    const reference_counted_ptr<PainterItemShader>&
    item_shader(void) const;

    /*!
     * Set the value returned by item_shader(void) const.
     * \param sh value to use
     */
    PainterFillShader&
    item_shader(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
     * Returns the PainterItemShader to use to draw the
     * anti-alias fuzz via the deferred coverage buffer.
     * This shader will draw the aa-fuzz in two passes:
     * the first pass draws the coverage as computed by
     * a fragment shader to the deferred coverage buffer
     * (via the PainterItemShader::coverage_shader())
     * and the 2nd pass reads from the deferred coverage
     * buffer to emit the alpha value.
     */
    const reference_counted_ptr<PainterItemShader>&
    aa_fuzz_shader(void) const;

    /*!
     * Set the value returned by aa_fuzz_shader(void) const.
     * \param sh value to use
     */
    PainterFillShader&
    aa_fuzz_shader(const reference_counted_ptr<PainterItemShader> &sh);

  private:
    void *m_d;
  };

/*! @} */
}
