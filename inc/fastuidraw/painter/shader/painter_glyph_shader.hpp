/*!
 * \file painter_glyph_shader.hpp
 * \brief file painter_glyph_shader.hpp
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

#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/painter/shader/painter_item_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterGlyphShader holds a shader for each \ref glyph_type.
   * The shaders are to handle attribute data as packed by
   * Glyph::pack_glyph().
   */
  class PainterGlyphShader
  {
  public:
    /*!
     * Ctor, inits as return value from shader(enum glyph_type)
     * as a nullptr for each \ref glyph_type value.
     */
    PainterGlyphShader(void);

    /*!
     * Copy ctor.
     */
    PainterGlyphShader(const PainterGlyphShader &obj);

    ~PainterGlyphShader();

    /*!
     * Assignment operator.
     */
    PainterGlyphShader&
    operator=(const PainterGlyphShader &rhs);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(PainterGlyphShader &obj);

    /*!
     * Return the PainterItemShader for a given \ref glyph_type.
     * \param tp glyph type to render
     */
    const reference_counted_ptr<PainterItemShader>&
    shader(enum glyph_type tp) const;

    /*!
     * Set the PainterItemShader for a given \ref glyph_type.
     * \param tp glyph type to render
     * \param sh PainterItemShader to use for the glyph type
     */
    PainterGlyphShader&
    shader(enum glyph_type tp, const reference_counted_ptr<PainterItemShader> &sh);

    /*!
     * Returns the one plus the largest value for which
     * shader(enum glyph_type, PainterItemShader)
     * was called.
     */
    unsigned int
    shader_count(void) const;

  private:
    void *m_d;
  };

/*! @} */
}
