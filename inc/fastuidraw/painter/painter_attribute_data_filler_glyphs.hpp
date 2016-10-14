/*!
 * \file painter_attribute_data_filler_glyphs.hpp
 * \brief file painter_attribute_data_filler_glyphs.hpp
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

#include <fastuidraw/painter/painter_attribute_data_filler.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/text/glyph.hpp>


namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterAttributeDataFillerGlyphs is for filling the data of a
    PainterAttributeDataFiller for the purpose of drawing glyphs.
    The enumeration glyph_type provide the indices into
    PainterAttributeData::attribute_data_chunks() and
    PainterAttributeData::index_data_chunks() for the different
    glyph types. If a glyph is not uploaded to its GlyphCache and
    failed to be uploaded to its GlyphCache, then filling will
    only fill the PainterAttrributeData to the last glyph that
    successfully uploaded to its GlyphCache. That value can be
    queried by number_glyphs(). If all glyphs are uploaded or
    successfully loaded, then number_glyphs() returns the number
    glyph in the glyph run. Data for glyphs is packed as follows:
      - PainterAttribute::m_attrib0 .xy   -> xy-texel location in primary atlas (float)
      - PainterAttribute::m_attrib0 .zw   -> xy-texel location in secondary atlas (float)
      - PainterAttribute::m_attrib1 .xy -> position in item coordinates (float)
      - PainterAttribute::m_attrib1 .z  -> 0 (free)
      - PainterAttribute::m_attrib1 .w  -> 0 (free)
      - PainterAttribute::m_attrib2 .x -> 0 (free)
      - PainterAttribute::m_attrib2 .y -> glyph offset (uint)
      - PainterAttribute::m_attrib2 .z -> layer in primary atlas (uint)
      - PainterAttribute::m_attrib2 .w -> layer in secondary atlas (uint)
   */
  class PainterAttributeDataFillerGlyphs:public PainterAttributeDataFiller
  {
  public:
    /*!
      Ctor. The values behind the arrays passed are NOT copied. As such
      the memory behind the arrays need to stay in scope for the duration
      of the call to PainterAttributeData::set_data() when passed this.
      \param glyph_positions position of the bottom left corner of each glyph
      \param glyphs glyphs to draw, array must be same size as glyph_positions
      \param scale_factors scale factors to apply to each glyph, must be either
                           empty (indicating no scaling factors) or the exact
                           same length as glyph_positions
      \param orientation orientation of drawing
     */
    PainterAttributeDataFillerGlyphs(const_c_array<vec2> glyph_positions,
                                     const_c_array<Glyph> glyphs,
                                     const_c_array<float> scale_factors,
                                     enum PainterEnums::glyph_orientation orientation
                                     = PainterEnums::y_increases_downwards);

    /*!
      Ctor. The values behind the arrays passed are NOT copied. As such
      the memory behind the arrays need to stay in scope for the duration
      of the call to PainterAttributeData::set_data() when passed this.
      \param glyph_positions position of the bottom left corner of each glyph
      \param glyphs glyphs to draw, array must be same size as glyph_positions
      \param render_pixel_size pixel size to which to scale the glyphs
      \param orientation orientation of drawing
     */
    PainterAttributeDataFillerGlyphs(const_c_array<vec2> glyph_positions,
                                     const_c_array<Glyph> glyphs,
                                     float render_pixel_size,
                                     enum PainterEnums::glyph_orientation orientation
                                     = PainterEnums::y_increases_downwards);

    /*!
      Ctor. The values behind the arrays passed are NOT copied. As such
      the memory behind the arrays need to stay in scope for the duration
      of the call to PainterAttributeData::set_data() when passed this.
      When filling the data, the glyphs are not scaled.
      \param glyph_positions position of the bottom left corner of each glyph
      \param glyphs glyphs to draw, array must be same size as glyph_positions
      \param orientation orientation of drawing
     */
    PainterAttributeDataFillerGlyphs(const_c_array<vec2> glyph_positions,
                                     const_c_array<Glyph> glyphs,
                                     enum PainterEnums::glyph_orientation orientation
                                     = PainterEnums::y_increases_downwards);

    ~PainterAttributeDataFillerGlyphs();

    /*!
      After calling PainterAttributeData::set_data() with this object,
      returns the number of glyphs of the glyph runs set at the ctor
      that are in the filled PainterAttributeData. The filling by
      a PainterAttributeDataFillerGlyphs stops as soon as a glyph
      is not and cannot be uploaded to its GlyphCache. If all glyphs
      are or can be uploaded to their GlyphCache, returns the
      length of the glyph array passed to the ctor, otherwise returns
      the index into the glyph array of the first glyph that cannot
      be uploaded to its GlyphCache.
     */
    unsigned int
    number_glyphs(void) const;

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_increments) const;
    virtual
    void
    fill_data(c_array<PainterAttribute> attributes,
              c_array<PainterIndex> indices,
              c_array<const_c_array<PainterAttribute> > attrib_chunks,
              c_array<const_c_array<PainterIndex> > index_chunks,
              c_array<unsigned int> zincrements,
              c_array<int> index_adjusts) const;

  private:
    void *m_d;
  };
/*! @} */
}
