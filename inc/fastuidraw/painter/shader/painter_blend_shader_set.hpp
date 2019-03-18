/*!
 * \file painter_blend_shader_set.hpp
 * \brief file painter_blend_shader_set.hpp
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
#include <fastuidraw/painter/shader/painter_blend_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterBlendShaderSet represents a set of shaders for
   * the blending modes enumerated by PainterEnums::blend_mode_t
   */
  class PainterBlendShaderSet
  {
  public:
    /*!
     * Ctor, inits as all return value from shader(enum glyph_type)
     * return a PainterItemShader with no shaders
     */
    PainterBlendShaderSet(void);

    /*!
     * Copy ctor.
     */
    PainterBlendShaderSet(const PainterBlendShaderSet &obj);

    ~PainterBlendShaderSet();

    /*!
     * Assignment operator.
     */
    PainterBlendShaderSet&
    operator=(const PainterBlendShaderSet &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterBlendShaderSet &obj);

    /*!
     * Return the PainterShader for a given
     * PainterEnums::blend_mode_t.
     * \param tp blend mode
     */
    const reference_counted_ptr<PainterBlendShader>&
    shader(enum PainterEnums::blend_w3c_mode_t tp) const;

    /*!
     * Set the PainterShader for a given
     * PainterEnums::blend_mode_t.
     * \param tp blend shader being specified
     * \param sh PainterShader to use for the blend mode
     */
    PainterBlendShaderSet&
    shader(enum PainterEnums::blend_w3c_mode_t tp,
           const reference_counted_ptr<PainterBlendShader> &sh);

    /*!
     * Returns the one plus the largest value for which
     * shader(enum PainterEnums::blend_w3c_mode_t, const reference_counted_ptr<PainterShader>&)
     * was called.
     */
    unsigned int
    shader_count(void) const;

  private:
    void *m_d;
  };

/*! @} */
}
