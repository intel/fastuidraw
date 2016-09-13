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

#include <fastuidraw/painter/painter_item_shader.hpp>
#include <fastuidraw/text/glyph.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterGlyphShader holds a shader pair
    for each glyph_type. The shaders are to
    handle attribute data as packed by
    PainterAttributeDataFillerGlyphs.
   */
  class PainterGlyphShader
  {
  public:
    /*!
      Ctor, inits as all return value from shader(enum glyph_type)
      as a NULL PainterItemShader
     */
    PainterGlyphShader(void);

    /*!
      Copy ctor.
     */
    PainterGlyphShader(const PainterGlyphShader &obj);

    ~PainterGlyphShader();

    /*!
      Assignment operator.
     */
    PainterGlyphShader&
    operator=(const PainterGlyphShader &rhs);

    /*!
      Return the PainterItemShader for a given
      glyph_type.
      \param tp glyph type to render
     */
    const reference_counted_ptr<PainterItemShader>&
    shader(enum glyph_type tp) const;

    /*!
      Set the PainterItemShader for a given
      glyph_type.
      \param tp glyph type to render
      \param sh PainterItemShader to use for the glyph type
     */
    PainterGlyphShader&
    shader(enum glyph_type tp, const reference_counted_ptr<PainterItemShader> &sh);

    /*!
      Returns the one plus the largest value for which
      shader(enum glyph_type, PainterItemShader)
      was called.
     */
    unsigned int
    shader_count(void) const;

  private:
    void *m_d;
  };

/*! @} */
}
