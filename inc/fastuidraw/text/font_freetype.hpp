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
   */
  class FontFreeType:public FontBase
  {
  public:
    /*!
     * Ctor. Guess the FontProperties from the FreeTypeFace::GeneratorBase
     * \param pface_generator object used to generate the FreeTypeFace object(s)
     *                        used by the FontFreeType object.
     * \param plib the FreeTypeLib of the FreeTypeFace created by the FontFreeType,
     *             a null values indicates to use a private FreeTypeLib object
     * \param num_faces number of underlying faces for the FontFreeType to possess,
     *                  this is the number of simumtaneous requests the FontFreeType
     *                  can handle
     */
    FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
                 const reference_counted_ptr<FreeTypeLib> &plib = reference_counted_ptr<FreeTypeLib>(),
                 unsigned int num_faces = 8);

    /*!
     * Ctor.
     * \param pface_generator object used to generate the FreeTypeFace object(s)
     *                        used by the FontFreeType object.
     * \param props FontProperties with which to endow the created FontFreeType object
     * \param plib the FreeTypeLib of the FreeTypeFace created by the FontFreeType,
     *             a null values indicates to use a private FreeTypeLib object
     * \param num_faces number of underlying faces for the FontFreeType to possess,
     *                  this is the number of simumtaneous requests the FontFreeType
     *                  can handle
     */
    FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
                 const FontProperties &props,
                 const reference_counted_ptr<FreeTypeLib> &plib = reference_counted_ptr<FreeTypeLib>(),
                 unsigned int num_faces = 8);

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

    /*!
     * Fill the field of a FontMetrics from the values of an FT_Face.
     * Beware that the foundary name is not assigned!
     */
    static
    void
    compute_font_metrics_from_face(FT_Face in_face, FontMetrics &out_metrics);

    /*!
     * Return a FontMetrics from the values of an FT_Face.
     * Beware that the foundary name is not assigned!
     */
    static
    FontMetrics
    compute_font_metrics_from_face(FT_Face in_face);

    /*!
     * Return a FontMetrics from the values of an FT_Face.
     * Beware that the foundary name is not assigned!
     */
    static
    FontMetrics
    compute_font_metrics_from_face(const reference_counted_ptr<FreeTypeFace> &in_face);

    virtual
    void
    glyph_codes(enum CharacterEncoding::encoding_value_t encoding,
                c_array<const uint32_t> in_character_codes,
                c_array<uint32_t> out_glyph_codes) const override final;

    virtual
    unsigned int
    number_glyphs(void) const override final;

    virtual
    bool
    can_create_rendering_data(enum glyph_type tp) const override final;

    virtual
    void
    compute_metrics(uint32_t glyph_code, GlyphMetricsValue &metrics) const override final;

    virtual
    GlyphRenderData*
    compute_rendering_data(GlyphRenderer render, GlyphMetrics glyph_metrics,
			   Path &path, vec2 &render_size) const override final;

  private:
    void *m_d;
  };
/*! @} */
}
