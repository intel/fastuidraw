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
     * \brief
     * A RenderParams specifies the parameters
     * for generating scalable glyph rendering data
     */
    class RenderParams
    {
    public:
      /*!
       * Ctor, initializes values to defaults.
       */
      RenderParams(void);

      /*!
       * Copy ctor.
       * \param obj value from which to copy
       */
      RenderParams(const RenderParams &obj);

      ~RenderParams();

      /*!
       * Assignment operator.
       * \param rhs value from which to copy
       */
      RenderParams&
      operator=(const RenderParams &rhs);

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(RenderParams &obj);

      /*!
       * Pixel size at which to render distance field scalable glyphs.
       */
      unsigned int
      distance_field_pixel_size(void) const;

      /*!
       * Set the value returned by distance_field_pixel_size(void) const,
       * initial value is 48.
       * \param v value
       */
      RenderParams&
      distance_field_pixel_size(unsigned int v);

      /*!
       * When creating distance field data, the distances are normalized
       * and clamped to [0, 1]. This value provides the normalization
       * which effectivly gives the maximum distance recorded in the
       * distance field texture. Recall that the values stored in texels
       * are uint8_t's so larger values will have lower accuracy. The
       * units are in 1/64'th of a pixel. Default value is 96.0.
       */
      float
      distance_field_max_distance(void) const;

      /*!
       * Set the value returned by distance_field_max_distance(void) const,
       * initial value is 96.0, i.e. 1.5 pixels
       * \param v value
       */
      RenderParams&
      distance_field_max_distance(float v);

      /*!
       * Pixel size at which to render curve pair scalable glyphs.
       */
      unsigned int
      curve_pair_pixel_size(void) const;

      /*!
       * Set the value returned by curve_pair_pixel_size(void) const,
       * initial value is 32
       * \param v value
       */
      RenderParams&
      curve_pair_pixel_size(unsigned int v);

    private:
      void *m_d;
    };

    /*!
     * Ctor. Guess the FontProperties from the FT_Face
     * \param pface_generator object used to generate the FreeTypeFace object(s)
     *                        used by the FontFreeType object.
     * \param render_params specifies how to generate data for scalable glyph data
     * \param plib the FreeTypeLib of the FreeTypeFace created by the FontFreeType,
     *             a null values indicates to use a private FreeTypeLib object
     */
    FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
                 const RenderParams &render_params = RenderParams(),
                 const reference_counted_ptr<FreeTypeLib> &plib = reference_counted_ptr<FreeTypeLib>());

    /*!
     * Ctor.
     * \param pface_generator object used to generate the FreeTypeFace object(s)
     *                        used by the FontFreeType object.
     * \param props FontProperties with which to endow the created FontFreeType object
     * \param render_params specifies how to generate data for scalable glyph data
     * \param plib the FreeTypeLib of the FreeTypeFace created by the FontFreeType,
     *             a null values indicates to use a private FreeTypeLib object
     */
    FontFreeType(const reference_counted_ptr<FreeTypeFace::GeneratorBase> &pface_generator,
                 const FontProperties &props,
                 const RenderParams &render_params = RenderParams(),
                 const reference_counted_ptr<FreeTypeLib> &plib = reference_counted_ptr<FreeTypeLib>());

    virtual
    ~FontFreeType();

    /*!
     * Returns the rendering parameters of this font.
     */
    const RenderParams&
    render_params(void) const;

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
    bool
    can_create_rendering_data(enum glyph_type tp) const;

    virtual
    GlyphRenderData*
    compute_rendering_data(GlyphRender render, uint32_t glyph_code, Path &path) const;

    virtual
    void
    compute_layout_data(uint32_t glyph_code, GlyphLayoutData &layout) const;

  private:
    void *m_d;
  };
/*! @} */
}
