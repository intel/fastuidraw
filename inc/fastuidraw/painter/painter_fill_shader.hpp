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


#include <fastuidraw/painter/painter_item_shader.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/packing/painter_draw.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterFillShader holds the shaders for drawing filled paths.
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
     * Returns if high quality two pass anti-alias shading
     * is supported. In the two pass shader, the first pass
     * writes a coverage to an auxiliary buffer and the
     * second pass returns the value as the coverage and
     * clears the auxilary buffer. The non-high quality
     * shader is a single pass solution that relies on the
     * depth buffer to avoid overdraw at the possible expense
     * than some fragments (typically where the path crosses
     * itself or when the path is drawn very minified) will
     * have lower coverage than they should.
     */
    bool
    supports_hq_aa_shading(void) const;

    /*!
     * Set the value returned by supports_hq_aa_shading(void) const.
     * \param sh value to use
     */
    PainterFillShader&
    supports_hq_aa_shading(bool sh);

    /*!
     * Returns the PainterItemShader to use to draw
     * the anti-alias fuzz around the boundary
     * of a filled path. The expected format of the
     * attributes is as found in the \ref
     * PainterAttributeData returned by \ref
     * FilledPath::Subset::aa_fuzz_painter_data().
     */
    const reference_counted_ptr<PainterItemShader>&
    aa_fuzz_shader(void) const;

    /*!
     * Set the value returned by aa_fuzz_shader(void) const.
     * \param sh value to use
     */
    PainterFillShader&
    aa_fuzz_shader(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
     * Returns the PainterItemShader to use to draw
     * the 1st pass for high quality anti-alias fuzz
     * around the boundary of a filled path. The
     * expected format of the attributes is as found
     * in the \ref PainterAttributeData returned by
     * \ref FilledPath::Subset::aa_fuzz_painter_data().
     */
    const reference_counted_ptr<PainterItemShader>&
    aa_fuzz_hq_shader_pass1(void) const;

    /*!
     * Set the value returned by aa_fuzz_hq_shader_pass1(void) const.
     * \param sh value to use
     */
    PainterFillShader&
    aa_fuzz_hq_shader_pass1(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
     * Returns the PainterItemShader to use to draw
     * the 2nd pass for high quality anti-alias fuzz
     * around the boundary of a filled path. The
     * expected format of the attributes is as found
     * in the \ref PainterAttributeData returned by
     * \ref FilledPath::Subset::aa_fuzz_painter_data().
     */
    const reference_counted_ptr<PainterItemShader>&
    aa_fuzz_hq_shader_pass2(void) const;

    /*!
     * Set the value returned by aa_fuzz_hq_shader_pass2(void) const.
     * \param sh value to use
     */
    PainterFillShader&
    aa_fuzz_hq_shader_pass2(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
     * Returns the action to be called before the 1st pass
     * of high quality anti-alias shading, a return value
     * of nullptr indicates to not have an action (and thus
     * no draw-call break).
     */
    const reference_counted_ptr<const PainterDraw::Action>&
    aa_hq_action_pass1(void) const;

    /*!
     * Set the value returned by aa_hq_action_pass1(void) const.
     * Initial value is nullptr.
     * \param a value to use
     */
    PainterFillShader&
    aa_hq_action_pass1(const reference_counted_ptr<const PainterDraw::Action> &a);

    /*!
     * Returns the action to be called before the 2nd pass
     * of high quality anti-alias shading, a return value
     * of nullptr indicates to not have an action (and thus
     * no draw-call break).
     */
    const reference_counted_ptr<const PainterDraw::Action>&
    aa_hq_action_pass2(void) const;

    /*!
     * Set the value returned by aa_hq_action_pass2(void) const.
     * Initial value is nullptr.
     * \param a value to use
     */
    PainterFillShader&
    aa_hq_action_pass2(const reference_counted_ptr<const PainterDraw::Action> &a);

  private:
    void *m_d;
  };

/*! @} */
}
