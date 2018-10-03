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
   * glyph types. It is an error for any of the valid glyphs passed
   * to not be uploaded to the GlyphAtlas.
   * Data for glyphs is packed as follows:
   *   - PainterAttribute::m_attrib0 .xy -> position in item coordinates (float)
   *   - PainterAttribute::m_attrib0 .zw -> the difference in item coordinates
   *                                        between the bottom-left vertex position
   *                                        and the top-right vertex position.
   *   - PainterAttribute::m_attrib1 .x  -> Glyph::attribute()[0]
   *   - PainterAttribute::m_attrib1 .y  -> Glyph::attribute()[1]
   *   - PainterAttribute::m_attrib1. z  -> Glyph::attribute()[2]
   *   - PainterAttribute::m_attrib1 .w  -> Glyph::attribute()[3]
   *   - PainterAttribute::m_attrib2 .x  -> Glyph::attribute()[4]
   *   - PainterAttribute::m_attrib2 .y  -> Glyph::attribute()[5]
   *   - PainterAttribute::m_attrib2 .z  -> Glyph::attribute()[6]
   *   - PainterAttribute::m_attrib2 .w  -> Glyph::attribute()[7]
   */
  class PainterAttributeDataFillerGlyphs:public PainterAttributeDataFiller
  {
  public:
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
     * \param layout if glyph positions are for horizontal or vertical layout
     */
    PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                     c_array<const Glyph> glyphs,
                                     c_array<const float> scale_factors,
                                     enum PainterEnums::screen_orientation orientation,
                                     enum PainterEnums::glyph_layout_type layout
                                     = PainterEnums::glyph_layout_horizontal);

    /*!
     * Ctor. The values behind the arrays passed are NOT copied. As such
     * the memory behind the arrays need to stay in scope for the duration
     * of the call to PainterAttributeData::set_data() when passed this.
     * \param glyph_positions position of the bottom left corner of each glyph
     * \param glyphs glyphs to draw, array must be same size as glyph_positions
     * \param render_pixel_size pixel size to which to scale the glyphs
     * \param orientation orientation of drawing
     * \param layout if glyph positions are for horizontal or vertical layout
     */
    PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                     c_array<const Glyph> glyphs,
                                     float render_pixel_size,
                                     enum PainterEnums::screen_orientation orientation,
                                     enum PainterEnums::glyph_layout_type layout
                                     = PainterEnums::glyph_layout_horizontal);

    /*!
     * Ctor. The values behind the arrays passed are NOT copied. As such
     * the memory behind the arrays need to stay in scope for the duration
     * of the call to PainterAttributeData::set_data() when passed this.
     * When filling the data, the glyphs are not scaled.
     * \param glyph_positions position of the bottom left corner of each glyph
     * \param glyphs glyphs to draw, array must be same size as glyph_positions
     * \param orientation orientation of drawing
     * \param layout if glyph positions are for horizontal or vertical layout
     */
    PainterAttributeDataFillerGlyphs(c_array<const vec2> glyph_positions,
                                     c_array<const Glyph> glyphs,
                                     enum PainterEnums::screen_orientation orientation,
                                     enum PainterEnums::glyph_layout_type layout
                                     = PainterEnums::glyph_layout_horizontal);

    ~PainterAttributeDataFillerGlyphs();

    /*!
     * Utility function to return the number of indices and attributes
     * that are needed to realize a sequence of Glyphs with the
     * requirement that each valid \ref Glyph has the same value
     * for Glyph::type().
     * \param glyphs sequence of glyps to query
     * \param[out] number of indices needed
     * \param[out] number of attributes needed
     * \returns routine_success if all value \ref Glyph values are
     *          the same renderer type and \ref routine_fail if they
     *          are not. For returning \ref routine_fail both output
     *          values will beset to zero.
     */
    static
    enum return_code
    compute_number_attributes_indices_needed(c_array<const Glyph> glyphs,
					     unsigned int *out_number_attributes,
					     unsigned int *out_number_indices);

    /*!
     * Utility function to pack a sequence of \ref Glyph values with
     * each valid glyph having the same value for Glyph::type().
     * Will return \ref routine_fail if either of the arrays to
     * write to is not large enough or there are two (or more)
     * valid \ref Glyph values for which Glyph::type() is different.
     * \param glyph_positions position of the bottom left corner of each glyph
     * \param glyphs glyphs to draw, array must be same size as glyph_positions
     * \param render_pixel_size pixel size to which to scale the glyphs
     * \param orientation orientation of drawing
     * \param layout if glyph positions are for horizontal or vertical layout
     * \param[out] dst_attribs location to which to write attribute data
     * \param[out] dst_indices location to which to write index data
     */
    static
    enum return_code
    pack_attributes_indices(c_array<const vec2> glyph_positions,
			    c_array<const Glyph> glyphs,
			    float render_pixel_size,
			    enum PainterEnums::screen_orientation orientation,
			    enum PainterEnums::glyph_layout_type layout,
			    c_array<PainterAttribute> dst_attribs,
			    c_array<PainterIndex> dst_indices);

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
