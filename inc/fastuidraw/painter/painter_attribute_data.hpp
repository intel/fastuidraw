/*!
 * \file painter_attribute_data.hpp
 * \brief file painter_attribute_data.hpp
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/stroked_path.hpp>
#include <fastuidraw/filled_path.hpp>
#include <fastuidraw/painter/painter_attribute.hpp>
#include <fastuidraw/painter/painter_enums.hpp>
#include <fastuidraw/text/glyph.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    PainterAttributeData represents the attribute and index
    data ready to be consumed by a Painter. Data is organized
    into individual chuncks that can be drawn seperately.
   */
  class PainterAttributeData:noncopyable
  {
  public:
    /*!
      Enumation values are indexes into attribute_data_chunks()
      and index_data_chunks() for different portions of
      data needed for stroking a path when the data of this
      PainterAttributeData has been set with
      set_data(const reference_counted_ptr<const StrokedPath> &).
     */
    enum stroking_data_t
      {
        rounded_joins_closing_edge, /*!< index for rounded join data with closing edge */
        bevel_joins_closing_edge, /*!< index for bevel join data with closing edge */
        miter_joins_closing_edge, /*!< index for miter join data with closing edge */
        edge_closing_edge, /*!< index for edge data including closing edge */

        number_with_closing_edge, /*!< number of types with closing edge */

        rounded_joins_no_closing_edge = number_with_closing_edge, /*!< index for rounded join data without closing edge */
        bevel_joins_no_closing_edge, /*!< index for bevel join data without closing edge */
        miter_joins_no_closing_edge, /*!< index for miter join data without closing edge */
        edge_no_closing_edge, /*!< index for edge data not including closing edge */

        rounded_cap, /*!< index for rounded cap data */
        square_cap,  /*!< index for square cap data */

        stroking_data_count /*!< count of enums */
      };

    /*!
      Given an enumeration of stroking_data_t, returns
      the matching enumeration for drawing without the
      closing edge.
     */
    static
    enum stroking_data_t
    without_closing_edge(enum stroking_data_t v)
    {
      return (v < number_with_closing_edge) ?
        static_cast<enum stroking_data_t>(v + number_with_closing_edge) :
        v;
    }

    /*!
      Ctor.
     */
    PainterAttributeData(void);

    ~PainterAttributeData();

    /*!
      Set the attribute and index data for stroking a path.
      The enumerations of \ref stroking_data_t provide
      the indices into attribute_data_chunks() and
      index_data_chunks() for the data to draw the
      path stroked. The indices into attribute_data_chunks(V)
      for a join style V match the indices for the join
      style coming from the generating StrokedPath.
      Data for stroking is packed as follows:
      - PainterAttribute::m_attrib0 .xy -> StrokedPath::point::m_position (float)
      - PainterAttribute::m_attrib0 .zw -> StrokedPath::point::m_pre_offset (float)
      - PainterAttribute::m_attrib1 .x -> StrokedPath::point::m_distance_from_edge_start (float)
      - PainterAttribute::m_attrib1 .y -> StrokedPath::point::m_distance_from_contour_start (float)
      - PainterAttribute::m_attrib1 .zw -> StrokedPath::point::m_auxilary_offset (float)
      - PainterAttribute::m_attrib2 .x -> StrokedPath::point::m_packed_data (uint)
      - PainterAttribute::m_attrib2 .y -> StrokedPath::point::m_edge_length (float)
      - PainterAttribute::m_attrib2 .z -> StrokedPath::point::m_open_contour_length (float)
      - PainterAttribute::m_attrib2 .w -> StrokedPath::point::m_closed_contour_length (float)
     */
    void
    set_data(const reference_counted_ptr<const StrokedPath> &path);

    /*!
      Set the attribute and index data for filling a path.
      The enumeration values of PainterEnums::fill_rule_t provide
      the indices into attribute_data_chunks() for the fill rules.
      To get the index data for the component of a filled
      path with a given winding number, use the function
      index_chunk_from_winding_number(int). The attribute
      data, regardless of winding number or fill rule is
      the same value, the 0'th chunk. Data for filling is packed
      as follows:
      - PainterAttribute::m_attrib0 .xy    -> coordinate of point (float)
      - PainterAttribute::m_attrib0 .zw    -> 0 (free)
      - PainterAttribute::m_attrib1 .xyz -> 0 (free)
      - PainterAttribute::m_attrib1 .w   -> 0 (free)
      - PainterAttribute::m_attrib2 .x -> 0 (free)
      - PainterAttribute::m_attrib2 .y -> 0 (free)
      - PainterAttribute::m_attrib2 .z -> 0 (free)
      - PainterAttribute::m_attrib2 .w -> 0 (free)
     */
    void
    set_data(const reference_counted_ptr<const FilledPath> &path);

    /*!
      Set the data for drawing glyphs. The enumeration glyph_type
      provide the indices into attribute_data_chunks() and
      index_data_chunks() for the different glyph types.
      If a glyph is not uploaded to the GlyphCache and failed to
      be uploaded to its GlyphCache, then set_data() will create
      index and attribute data up to that glyph and returns the
      index into glyphs of the glyph that failed to be uploaded.
      If all glyphs can be in the cache, then returns the
      size of the array. Data for glyphs is packed as follows:
      - PainterAttribute::m_attrib0 .xy   -> xy-texel location in primary atlas (float)
      - PainterAttribute::m_attrib0 .zw   -> xy-texel location in secondary atlas (float)
      - PainterAttribute::m_attrib1 .xy -> position in item coordinates (float)
      - PainterAttribute::m_attrib1 .z  -> 0 (free)
      - PainterAttribute::m_attrib1 .w  -> 0 (free)
      - PainterAttribute::m_attrib2 .x -> 0 (free)
      - PainterAttribute::m_attrib2 .y -> glyph offset (uint)
      - PainterAttribute::m_attrib2 .z -> layer in primary atlas (uint)
      - PainterAttribute::m_attrib2 .w -> layer in secondary atlas (uint)

      \param glyph_positions position of the bottom left corner of each glyph
      \param glyphs glyphs to draw, array must be same size as glyph_positions
      \param scale_factors scale factors to apply to each glyph, must be either
                           empty (indicating no scaling factors) or the exact
                           same length as glyph_positions
      \param orientation orientation of drawing
     */
    unsigned int
    set_data(const_c_array<vec2> glyph_positions,
             const_c_array<Glyph> glyphs,
             const_c_array<float> scale_factors,
             enum PainterEnums::glyph_orientation orientation = PainterEnums::y_increases_downwards);

    /*!
      Set the data for drawing glyphs. The enumeration glyph_type
      provide the indices into attribute_data_chunks() and
      index_data_chunks() for the different glyph types.
      If a glyph is not uploaded to the GlyphCache and failed to
      be uploaded to its GlyphCache, then set_data() will create
      index and attribute data up to that glyph and returns the
      index into glyphs of the glyph that failed to be uploaded.
      If all glyphs can be in the cache, then returns the
      size of the array. Data for glyphs is packed as follows:
      - PainterAttribute::m_attrib0 .xy   -> xy-texel location in primary atlas (float)
      - PainterAttribute::m_attrib0 .zw   -> xy-texel location in secondary atlas (float)
      - PainterAttribute::m_attrib1 .xy -> position in item coordinates (float)
      - PainterAttribute::m_attrib1 .z  -> 0 (free)
      - PainterAttribute::m_attrib1 .w  -> 0 (free)
      - PainterAttribute::m_attrib2 .x -> 0 (free)
      - PainterAttribute::m_attrib2 .y -> glyph offset (uint)
      - PainterAttribute::m_attrib2 .z -> layer in primary atlas (uint)
      - PainterAttribute::m_attrib2 .w -> layer in secondary atlas (uint)

      \param glyph_positions position of the bottom left corner of each glyph
      \param glyphs glyphs to draw, array must be same size as glyph_positions
      \param render_pixel_size pixel size to which to scale the glyphs
      \param orientation orientation of drawing
     */
    unsigned int
    set_data(const_c_array<vec2> glyph_positions,
             const_c_array<Glyph> glyphs,
             float render_pixel_size,
             enum PainterEnums::glyph_orientation orientation = PainterEnums::y_increases_downwards);

    /*!
      Set the data for drawing glyphs. The enumeration glyph_type
      provide the indices into attribute_data_chunks() and
      index_data_chunks() for the different glyph types.
      If a glyph is not uploaded to the GlyphCache and failed to
      be uploaded to its GlyphCache, then set_data() will create
      index and attribute data up to that glyph and returns the
      index into glyphs of the glyph that failed to be uploaded.
      If all glyphs can be in the cache, then returns the
      size of the array. Data for glyphs is packed as follows:
      - PainterAttribute::m_attrib0 .xy   -> xy-texel location in primary atlas (float)
      - PainterAttribute::m_attrib0 .zw   -> xy-texel location in secondary atlas (float)
      - PainterAttribute::m_attrib1 .xy -> position in item coordinates (float)
      - PainterAttribute::m_attrib1 .z  -> 0 (free)
      - PainterAttribute::m_attrib1 .w  -> 0 (free)
      - PainterAttribute::m_attrib2 .x -> 0 (free)
      - PainterAttribute::m_attrib2 .y -> glyph offset (uint)
      - PainterAttribute::m_attrib2 .z -> layer in primary atlas (uint)
      - PainterAttribute::m_attrib2 .w -> layer in secondary atlas (uint)

      \param glyph_positions position of the bottom left corner of each glyph
      \param glyphs glyphs to draw, array must be same size as glyph_positions
      \param orientation orientation of drawing
     */
    unsigned int
    set_data(const_c_array<vec2> glyph_positions,
             const_c_array<Glyph> glyphs,
             enum PainterEnums::glyph_orientation orientation = PainterEnums::y_increases_downwards)
    {
      c_array<float> empty;
      return set_data(glyph_positions, glyphs, empty, orientation);
    }

    /*!
      Returns the attribute data chunks. For all but those
      objects set by set_data(const reference_counted_ptr<const FilledPath> &),
      for each attribute data chunk, there is a matching index
      data chunk. A chunk is an attribute and index data chunk pair.
      Specifically one uses index_data_chunks()[i] to draw the contents
      of attribute_data_chunks()[i].
     */
    const_c_array<const_c_array<PainterAttribute> >
    attribute_data_chunks(void) const;

    /*!
      Provided as an API conveniance to fetch
      the named chunk of attribute_data_chunks()
      or an empty chunk if the index is larger then
      attribute_data_chunks().size().
      \param i index of attribute_data_chunks() to fetch
     */
    const_c_array<PainterAttribute>
    attribute_data_chunk(unsigned int i) const;

    /*!
      Returns the index data chunks. For all but those
      objects set by set_data(const reference_counted_ptr<const FilledPath> &),
      for each attribute data chunk, there is a matching index
      data chunk. A chunk is an attribute and index data chunk pair.
      Specifically one uses index_data_chunks()[i] to draw the contents
      of attribute_data_chunks()[i].
    */
    const_c_array<const_c_array<PainterIndex> >
    index_data_chunks(void) const;

    /*!
      Provided as an API conveniance to fetch
      the named chunk of index_data_chunks()
      or an empty chunk if the index is larger then
      index_data_chunks().size().
      \param i index of index_data_chunks() to fetch
     */
    const_c_array<PainterIndex>
    index_data_chunk(unsigned int i) const;

    /*!
      Returns an array that holds those value i
      for which index_data_chunk(i) is non-empty.
     */
    const_c_array<unsigned int>
    non_empty_index_data_chunks(void) const;

    /*!
      Returns by how much to increment a z-value
      (see Painter::increment_z()) when using
      an attribute/index pair.
     */
    const_c_array<unsigned int>
    increment_z_values(void) const;

    /*!
      Provided as an API conveniance to fetch
      the named increment-z value of
      increment_z_values() or 0 if the index is
      larger then increment_z_values().size().
      \param i index of increment_z_values() to fetch
     */
    unsigned int
    increment_z_value(unsigned int i) const;

    /*!
      Returns the value to feed to index_data_chunk()
      to get the index data for the fill of a path
      (see set_data(const reference_counted_ptr<const FilledPath>&))
      with a specified winding number.
      \param winding_number winding number of fill data to fetch
     */
    static
    unsigned int
    index_chunk_from_winding_number(int winding_number);

    /*!
      Is the inverse of index_chunk_from_winding_number(), i.e.
      returns the winding number that lives on a given index
      chunk. It is required that the index fed is not one of
      PainterEnums::odd_even_fill_rule, PainterEnums::nonzero_fill_rule
      and PainterEnums::complement_odd_even_fill_rule.
      \param idx index into index_data_chunk()
     */
    static
    int
    winding_number_from_index_chunk(unsigned int idx);

  private:
    void *m_d;
  };
/*! @} */
}
