/*!
 * \file painter_blend_shader_set.hpp
 * \brief file painter_blend_shader_set.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef FASTUIDRAW_PAINTER_BLEND_SHADER_SET_HPP
#define FASTUIDRAW_PAINTER_BLEND_SHADER_SET_HPP

#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/shader/painter_blend_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A PainterBlendShaderSet represents a set of shaders
   * for the blend modes enumerated by PainterEnums::blend_mode_t
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
    shader(enum PainterEnums::blend_mode_t tp) const;

    /*!
     * Returns the BlendMode for a given PainterEnums::blend_mode_t.
     * \param tp blend mode
     */
    BlendMode
    blend_mode(enum PainterEnums::blend_mode_t tp) const;

    /*!
     * Set the PainterShader for a given
     * PainterEnums::blend_mode_t.
     * \param tp blend shader being specified
     * \param mode 3D API BlendMode to use
     * \param sh PainterShader to use for the blend mode
     */
    PainterBlendShaderSet&
    shader(enum PainterEnums::blend_mode_t tp,
           const BlendMode &mode,
           const reference_counted_ptr<PainterBlendShader> &sh);

    /*!
     * Returns the one plus the largest value for which
     * shader(enum PainterEnums::blend_mode_t, const reference_counted_ptr<PainterShader>&)
     * was called.
     */
    unsigned int
    shader_count(void) const;

  private:
    void *m_d;
  };

/*! @} */
}

#endif
