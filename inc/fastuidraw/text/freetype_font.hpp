/*!
 * \file freetype_font.hpp
 * \brief file freetype_font.hpp
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


#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/freetype_lib.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/

  /*!
    A FontFreeType implements the interface of FontBase
    from a scalable font loaded by libfreetype. All glyph texel
    data, for the sake of texturing, are padded on the bottom
    and right by empty texels if the glyph has non-zero size.
   */
  class FontFreeType:public FontBase
  {
  public:
    /*!
      Overload typedef to handle
     */
    typedef reference_counted_ptr<FontFreeType> handle;
    /*!
      Overload typedef to const_handle
     */
    typedef reference_counted_ptr<const FontFreeType> const_handle;

    /*!
      A RenderParams specifies the parameters
      for generating scalable glyph rendering data
     */
    class RenderParams
    {
    public:
      /*!
        Ctor, initializes values to defaults.
       */
      RenderParams(void);

      /*!
        Copy ctor.
        \param obj value from which to copy
       */
      RenderParams(const RenderParams &obj);

      ~RenderParams();

      /*!
        Assignment operator.
        \param rhs value from which to copy
       */
      RenderParams&
      operator=(const RenderParams &rhs);

      /*!
        Pixel size at which to render distance field scalable glyphs.
       */
      unsigned int
      distance_field_pixel_size(void) const;

      /*!
        Set the value returned by distance_field_pixel_size(void) const,
        initial value is 48.
        \param v value
       */
      RenderParams&
      distance_field_pixel_size(unsigned int v);

      /*!
        When creating distance field data, the distances are normalized
        and clamped to [0, 1]. This value provides the normalization
        which effectivly gives the maximum distance recorded in the
        distance field texture. Recall that the values stored in texels
        are uint8_t's so larger values will have lower accuracy. The
        units are in 1/64'th of a pixel. Default value is 96.0.
       */
      float
      distance_field_max_distance(void) const;

      /*!
        Set the value returned by distance_field_max_distance(void) const,
        initial value is 96.0, i.e. 1.5 pixels
        \param v value
       */
      RenderParams&
      distance_field_max_distance(float v);

      /*!
        Pixel size at which to render curve pair scalable glyphs.
       */
      unsigned int
      curve_pair_pixel_size(void) const;

      /*!
        Set the value returned by curve_pair_pixel_size(void) const,
        initial value is 32
        \param v value
       */
      RenderParams&
      curve_pair_pixel_size(unsigned int v);

    private:
      void *m_d;
    };

    /*!
      Ctor.
      \param pface FT_Face for constructed FontFreeType. Onwership is NOT
                   passed to the created FontFreeType object
      \param props font properties describing the font
      \param render_params specifies how to generate data for scalable glyph data
     */
    FontFreeType(FT_Face pface, const FontProperties &props,
                 const RenderParams &render_params = RenderParams());

    /*!
      Ctor. Guess the FontProperties from the FT_Face
      \param pface FT_Face for constructed FontFreeType. Onwership is NOT
                   passed to the created FontFreeType object
      \param render_params specifies how to generate data for scalable glyph data
     */
    FontFreeType(FT_Face pface,
                 const RenderParams &render_params = RenderParams());

    /*!
      Ctor.
      \param pface FT_Face for constructed FontFreeType. Onwership is
                   passed to the created FontFreeType object
      \param lib FreetypeLib that was used to create pface
      \param props font properties describing the font
      \param render_params specifies how to generate data for scalable glyph data
     */
    FontFreeType(FT_Face pface, FreetypeLib::handle lib,
                 const FontProperties &props,
                 const RenderParams &render_params = RenderParams());

    /*!
      Ctor. Guess the FontProperties from the FT_Face
      \param pface FT_Face for constructed FontFreeType. Onwership is
                   passed to the created FontFreeType object
      \param lib FreetypeLib that was used to create pface
      \param render_params specifies how to generate data for scalable glyph data
     */
    FontFreeType(FT_Face pface, FreetypeLib::handle lib,
                 const RenderParams &render_params = RenderParams());

    /*!
      Ctor. Create a font from file and guess the FontProperties from the FT_Face.
      \param filename from which to load the font
      \param lib FreetypeLib used to create FreeTypeFont object
      \param render_params specifies how to generate data for scalable glyph data
      \param face_index face index for face into font file to load
     */
    static
    handle
    create(const char *filename, FreetypeLib::handle lib,
           const RenderParams &render_params = RenderParams(),
           int face_index = 0);

    /*!
      Ctor. Create a font from file and guess the FontProperties from the FT_Face.
      The font created will use its own private \ref FreetypeLib object.
      \param filename from which to load the font
      \param render_params specifies how to generate data for scalable glyph data
      \param face_index face index for face into font file to load
     */
    static
    handle
    create(const char *filename,
           const RenderParams &render_params = RenderParams(),
           int face_index = 0);

    /*!
      Create fonts from all faces of a font file.
      Returns the number of faces that are in font file.
      \param fonts location to which to place handles to new fonts
      \param filename from which to load the fonts
      \param lib FreetypeLib used to create FreeTypeFont objects
      \param render_params specifies how to generate data for scalable glyph data
     */
    static
    int
    create(c_array<handle> fonts, const char *filename,
           FreetypeLib::handle lib, const RenderParams &render_params = RenderParams());

    virtual
    ~FontFreeType();

    /*!
      Returns the rendering parameters of this font.
     */
    const RenderParams&
    render_params(void) const;

    /*!
      Return the FT_Face of this FontFreeType object.
     */
    FT_Face
    face(void) const;

    /*!
      Fill the field of a FontProperties from the values of an FT_Face.
      Beware that the foundary name is not assigned!
     */
    static
    void
    compute_font_propertes_from_face(FT_Face in_face, FontProperties &out_properties);

    /*!
      Return a FontProperties from the values of an FT_Face.
      Beware that the foundary name is not assigned!
     */
    static
    FontProperties
    compute_font_propertes_from_face(FT_Face in_face);

    virtual
    uint32_t
    glyph_code(uint32_t pcharacter_code) const;

    virtual
    bool
    can_create_rendering_data(enum glyph_type tp) const;

    virtual
    GlyphRenderData*
    compute_rendering_data(GlyphRender render, uint32_t glyph_code, GlyphLayoutData &layout) const;

  private:
    void *m_d;
  };
/*! @} */
}
