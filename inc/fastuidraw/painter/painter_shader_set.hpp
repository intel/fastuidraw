/*!
 * \file painter_shader_set.hpp
 * \brief file painter_shader_set.hpp
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

#include <fastuidraw/painter/painter_fill_shader.hpp>
#include <fastuidraw/painter/painter_stroke_shader.hpp>
#include <fastuidraw/painter/painter_glyph_shader.hpp>
#include <fastuidraw/painter/painter_composite_shader_set.hpp>
#include <fastuidraw/painter/painter_blend_shader_set.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_shader_set.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterShaderSet provides shaders for compositing
   * and drawing each of the item types glyphs, stroking
   * paths and filling paths.
   */
  class PainterShaderSet
  {
  public:
    /*!
     * Ctor, inits all as empty
     */
    PainterShaderSet(void);

    /*!
     * Copy ctor.
     * \param obj value from which to copy
     */
    PainterShaderSet(const PainterShaderSet &obj);

    ~PainterShaderSet();

    /*!
     * Assignment operator.
     * \param rhs value from which to copy
     */
    PainterShaderSet&
    operator=(const PainterShaderSet &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterShaderSet &obj);

    /*!fn glyph_shader(void) const
     * Shader set for rendering of glyphs with isotropic
     * anti-aliasing.
     */
    const PainterGlyphShader&
    glyph_shader(void) const;

    /*!
     * Set the value returned by glyph_shader(void) const.
     * \param sh value to use
     */
    PainterShaderSet&
    glyph_shader(const PainterGlyphShader &sh);

    /*!
     * Shader set for stroking of paths; the stroking
     * parameters are given by a \ref PainterStrokeParams
     * value.
     */
    const PainterStrokeShader&
    stroke_shader(void) const;

    /*!
     * Set the value returned by stroke_shader(void) const.
     * \param sh value to use
     */
    PainterShaderSet&
    stroke_shader(const PainterStrokeShader &sh);

    /*!
     * Shader set for dashed stroking of paths;
     * the stroking parameters are given by a \ref
     * PainterDashedStrokeParams value.
     */
    const PainterDashedStrokeShaderSet&
    dashed_stroke_shader(void) const;

    /*!
     * Set the value returned by dashed_stroke_shader(void) const.
     * \param sh value to use
     */
    PainterShaderSet&
    dashed_stroke_shader(const PainterDashedStrokeShaderSet &sh);

    /*!
     * Shader for filling of paths via \ref FilledPath.
     */
    const PainterFillShader&
    fill_shader(void) const;

    /*!
     * Set the value returned by fill_shader(void) const.
     * \param sh value to use
     */
    PainterShaderSet&
    fill_shader(const PainterFillShader &sh);

    /*!
     * Composite shaders. If an element is a nullptr shader, then that
     * composite mode is not supported.
     */
    const PainterCompositeShaderSet&
    composite_shaders(void) const;

    /*!
     * Set the value returned by composite_shaders(void) const.
     * \param sh value to use
     */
    PainterShaderSet&
    composite_shaders(const PainterCompositeShaderSet &sh);

    /*!
     * Blend shaders. If an element is a nullptr shader, then that
     * blend mode is not supported.
     */
    const PainterBlendShaderSet&
    blend_shaders(void) const;

    /*!
     * Set the value returned by blend_shaders(void) const.
     * \param sh value to use
     */
    PainterShaderSet&
    blend_shaders(const PainterBlendShaderSet &sh);

  private:
    void *m_d;
  };

/*! @} */
}
