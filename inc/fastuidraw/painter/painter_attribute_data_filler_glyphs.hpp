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
 * @{
 */

  /*!
   * \brief
   * A PainterAttributeDataFillerGlyphs is for filling the data of a
   * PainterAttributeDataFiller for the purpose of drawing glyphs.
   *
   * The enumeration glyph_type provide the indices into
   * PainterAttributeData::attribute_data_chunks() and
   * PainterAttributeData::index_data_chunks() for the different
   * glyph types. If a glyph is not uploaded to its GlyphCache and
   * failed to be uploaded to its GlyphCache, then filling will
   * only fill the PainterAttrributeData to the last glyph that
   * successfully uploaded to its GlyphCache. That value can be
   * queried by number_glyphs(). If all glyphs are uploaded or
   * successfully loaded, then number_glyphs() returns the number
   * glyph in the glyph run. Data for glyphs is packed as follows:
   *   - PainterAttribute::m_attrib0 .xy -> position in item coordinates (float)
   *   - PainterAttribute::m_attrib0 .z  -> glyph offset (uint)
   *   - PainterAttribute::m_attrib0 .w  -> xyz-texel location of Glyph::atlas_locations()[0] packed (uint)
   *   - PainterAttribute::m_attrib1 .y  -> xyz-texel location of Glyph::atlas_locations()[1] packed (uint)
   *   - PainterAttribute::m_attrib1 .y  -> xyz-texel location of Glyph::atlas_locations()[2] packed (uint)
   *   - PainterAttribute::m_attrib1 .z  -> xyz-texel location of Glyph::atlas_locations()[3] packed (uint)
   *   - PainterAttribute::m_attrib1. w  -> xyz-texel location of Glyph::atlas_locations()[4] packed (uint)
   *   - PainterAttribute::m_attrib2 .x  -> xyz-texel location of Glyph::atlas_locations()[5] packed (uint)
   *   - PainterAttribute::m_attrib2 .y  -> xyz-texel location of Glyph::atlas_locations()[6] packed (uint)
   *   - PainterAttribute::m_attrib2 .z  -> xyz-texel location of Glyph::atlas_locations()[7] packed (uint)
   *   - PainterAttribute::m_attrib2 .w  -> xyz-texel location of Glyph::atlas_locations()[8] packed (uint)
   *
   * Where we pack a location on the glyph as according to \ref packed_glyph_layout.
   */
  class PainterAttributeDataFillerGlyphs:public PainterAttributeDataFiller
  {
  public:
    /*!
     * Enumeration to describe how a texel location within a \ref GlyphAtlas
     * is packed.
     */
    enum packed_glyph_layout
      {
        /*!
         * Number of bits used to describe the unnormalized x, y or z-coordinate
         */
        num_texel_coord_bits = GlyphAtlasTexelBackingStoreBase::log2_max_size,

        /*!
         * First bit used to describe the x-texel coordinate
         */
        bit0_x_texel = 0,

        /*!
         * First bit used to describe the y-texel coordinate
         */
        bit0_y_texel = bit0_x_texel + num_texel_coord_bits,

        /*!
         * First bit used to describe the z-texel coordinate
         */
        bit0_z_texel = bit0_y_texel + num_texel_coord_bits,

        /*!
         * If this bit is up, indicates that there is no texel location
         * encoded (i.e. Glyph::atlas_locations()[K] for the packed
         * location gives a \ref GlyphLocation for which
         * GlyphLocation::valid() returns false
         */
        invalid_bit = bit0_z_texel + num_texel_coord_bits,

        /*!
         * Mask generated from \ref invalid_bit
         */
        invalid_mask = 1u << invalid_bit
      };

    /*!
     * Ctor. The values behind the arrays passed are NOT copied. As such
     * the memory behind the arrays need to stay in scope for the duration
     * of the call to PainterAttributeData::set_data() when passed this.
     * \param glyph_positions position of the bottom left corner of each glyph
     * \param glyphs glyphs to draw, array must be same size as glyph_positions
     * \param scale_factors scale factors to apply to each glyph, must be either
     *                      empty (indicating no scaling factors) or the exact
     *                      same length as glyph_positions
     * \param orientation orientation of drawing
     */
    PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                     c_array<const Glyph> glyphs,
                                     c_array<const float> scale_factors,
                                     enum PainterEnums::glyph_orientation orientation
                                     = PainterEnums::y_increases_downwards);

    /*!
     * Ctor. The values behind the arrays passed are NOT copied. As such
     * the memory behind the arrays need to stay in scope for the duration
     * of the call to PainterAttributeData::set_data() when passed this.
     * \param glyph_positions position of the bottom left corner of each glyph
     * \param glyphs glyphs to draw, array must be same size as glyph_positions
     * \param render_pixel_size pixel size to which to scale the glyphs
     * \param orientation orientation of drawing
     */
    PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                     c_array<const Glyph> glyphs,
                                     float render_pixel_size,
                                     enum PainterEnums::glyph_orientation orientation
                                     = PainterEnums::y_increases_downwards);

    /*!
     * Ctor. The values behind the arrays passed are NOT copied. As such
     * the memory behind the arrays need to stay in scope for the duration
     * of the call to PainterAttributeData::set_data() when passed this.
     * When filling the data, the glyphs are not scaled.
     * \param glyph_positions position of the bottom left corner of each glyph
     * \param glyphs glyphs to draw, array must be same size as glyph_positions
     * \param orientation orientation of drawing
     */
    PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                     c_array<const Glyph> glyphs,
                                     enum PainterEnums::glyph_orientation orientation
                                     = PainterEnums::y_increases_downwards);

    ~PainterAttributeDataFillerGlyphs();

    /*!
     * After calling PainterAttributeData::set_data() with this object,
     * returns the number of glyphs of the glyph runs set at the ctor
     * that are in the filled PainterAttributeData. The filling by
     * a PainterAttributeDataFillerGlyphs stops as soon as a glyph
     * is not and cannot be uploaded to its GlyphCache. If all glyphs
     * are or can be uploaded to their GlyphCache, returns the
     * length of the glyph array passed to the ctor, otherwise returns
     * the index into the glyph array of the first glyph that cannot
     * be uploaded to its GlyphCache.
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
              c_array<c_array<const PainterAttribute> > attrib_chunks,
              c_array<c_array<const PainterIndex> > index_chunks,
              c_array<range_type<int> > zranges,
              c_array<int> index_adjusts) const;

  private:
    void *m_d;
  };
/*! @} */
}
