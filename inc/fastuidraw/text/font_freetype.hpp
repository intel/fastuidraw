/*!
 * \file font_freetype.hpp
 * \brief file font_freetype.hpp
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

#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/freetype_lib.hpp>
#include <fastuidraw/text/freetype_face.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * A FontFreeType implements the interface of FontBase
   * from a scalable font loaded by libfreetype.
   *
   * The conversion from character codes to glyph codes
   * for FontFreeType, i.e. glyph_code(uint32_t) const,
   * is performed by libfreetype's FT_Get_Char_Index().
   */
  class FontFreeType:public FontBase
  {
  public:
    /*!
     * Ctor. Guess the FontProperties from the FT_Face
     * \param pface_generator object used to generate the FreeTypeFace object(s)
     *                        used by the FontFreeType object.
     * \param plib the FreeTypeLib of the FreeTypeFace created by the FontFreeType,
     *             a null values indicates to use a private FreeTypeLib object
     */
    FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
                 const reference_counted_ptr<FreeTypeLib> &plib = reference_counted_ptr<FreeTypeLib>());

    /*!
     * Ctor.
     * \param pface_generator object used to generate the FreeTypeFace object(s)
     *                        used by the FontFreeType object.
     * \param props FontProperties with which to endow the created FontFreeType object
     * \param plib the FreeTypeLib of the FreeTypeFace created by the FontFreeType,
     *             a null values indicates to use a private FreeTypeLib object
     */
    FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
                 const FontProperties &props,
                 const reference_counted_ptr<FreeTypeLib> &plib = reference_counted_ptr<FreeTypeLib>());

    virtual
    ~FontFreeType();

    /*!
     * Returns the FreeTypeFace::GeneratorBase object used
     * to generate the faces to generate the glyph data.
     */
    const reference_counted_ptr<FreeTypeFace::GeneratorBase>&
    face_generator(void) const;

    /*!
     * Returns the FreeTypeLib used by this FontFreeType
     */
    const reference_counted_ptr<FreeTypeLib>&
    lib(void) const;

    /*!
     * Fill the field of a FontProperties from the values of an FT_Face.
     * Beware that the foundary name is not assigned!
     */
    static
    void
    compute_font_properties_from_face(FT_Face in_face, FontProperties &out_properties);

    /*!
     * Return a FontProperties from the values of an FT_Face.
     * Beware that the foundary name is not assigned!
     */
    static
    FontProperties
    compute_font_properties_from_face(FT_Face in_face);

    /*!
     * Return a FontProperties from the values of an FT_Face.
     * Beware that the foundary name is not assigned!
     */
    static
    FontProperties
    compute_font_properties_from_face(const reference_counted_ptr<FreeTypeFace> &in_face);

    virtual
    uint32_t
    glyph_code(uint32_t pcharacter_code) const;

    virtual
    unsigned int
    number_glyphs(void) const;

  private:
    virtual
    bool
    can_create_rendering_data(enum glyph_type tp) const;

    virtual
    void
    compute_metrics(uint32_t glyph_code, GlyphMetricsValue &metrics) const;

    virtual
    GlyphRenderData*
    compute_rendering_data(GlyphRender render,
                           GlyphMetrics glyph_metrics, Path &path) const;

    void *m_d;
  };
/*! @} */
}
