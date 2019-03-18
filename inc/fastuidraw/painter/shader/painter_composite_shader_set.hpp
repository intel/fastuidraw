/*!
 * \file painter_composite_shader_set.hpp
 * \brief file painter_composite_shader_set.hpp
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

#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/painter/shader/painter_composite_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterCompositeShaderSet represents a set of shaders
   * for the composite modes enumerated by PainterEnums::composite_mode_t
   */
  class PainterCompositeShaderSet
  {
  public:
    /*!
     * Ctor, inits as all return value from shader(enum glyph_type)
     * return a PainterItemShader with no shaders
     */
    PainterCompositeShaderSet(void);

    /*!
     * Copy ctor.
     */
    PainterCompositeShaderSet(const PainterCompositeShaderSet &obj);

    ~PainterCompositeShaderSet();

    /*!
     * Assignment operator.
     */
    PainterCompositeShaderSet&
    operator=(const PainterCompositeShaderSet &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterCompositeShaderSet &obj);

    /*!
     * Return the PainterShader for a given
     * PainterEnums::composite_mode_t.
     * \param tp composite mode
     */
    const reference_counted_ptr<PainterCompositeShader>&
    shader(enum PainterEnums::composite_mode_t tp) const;

    /*!
     * Returns the BlendMode for a given PainterEnums::composite_mode_t.
     * \param tp composite mode
     */
    BlendMode
    composite_mode(enum PainterEnums::composite_mode_t tp) const;

    /*!
     * Set the PainterShader for a given
     * PainterEnums::composite_mode_t.
     * \param tp composite shader being specified
     * \param mode 3D API BlendMode to use
     * \param sh PainterShader to use for the composite mode
     */
    PainterCompositeShaderSet&
    shader(enum PainterEnums::composite_mode_t tp,
           const BlendMode &mode,
           const reference_counted_ptr<PainterCompositeShader> &sh);

    /*!
     * Returns the one plus the largest value for which
     * shader(enum PainterEnums::composite_mode_t, const reference_counted_ptr<PainterShader>&)
     * was called.
     */
    unsigned int
    shader_count(void) const;

  private:
    void *m_d;
  };

/*! @} */
}
