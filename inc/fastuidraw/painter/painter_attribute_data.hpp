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
#include <fastuidraw/painter/painter_attribute.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * PainterAttributeData represents the attribute and index
   * data ready to be consumed by a Painter. Data is organized
   * into individual chuncks that can be drawn seperately.
   */
  class PainterAttributeData:noncopyable
  {
  public:
    /*!
     * Ctor.
     */
    PainterAttributeData(void);

    ~PainterAttributeData();

    /*!
     * Set the index, attribute, z-increment and chunk
     * data of this PainterAttributeData using a
     * PainterAttributeDataFiller.
     * \param filler object that fills the data.
     */
    void
    set_data(const PainterAttributeDataFiller &filler);

    /*!
     * Returns the attribute data chunks. Usually, for each
     * attribute data chunk, there is a matching index data
     * chunk. Usually, one uses index_data_chunks()[i]
     * to draw the contents of attribute_data_chunks()[i].
     */
    c_array<const c_array<const PainterAttribute> >
    attribute_data_chunks(void) const;

    /*!
     * Provided as an API conveniance to fetch
     * the named chunk of attribute_data_chunks()
     * or an empty chunk if the index is larger then
     * attribute_data_chunks().size().
     * \param i index of attribute_data_chunks() to fetch
     */
    c_array<const PainterAttribute>
    attribute_data_chunk(unsigned int i) const;

    /*!
     * Returns the size of the largest attribute chunk.
     */
    unsigned int
    largest_attribute_chunk(void) const;

    /*!
     * Returns the index data chunks. Usually, for each
     * attribute data chunk, there is a matching index data
     * chunk. Usually, one uses index_data_chunks()[i]
     * to draw the contents of attribute_data_chunks()[i].
     */
    c_array<const c_array<const PainterIndex> >
    index_data_chunks(void) const;

    /*!
     * Returns the size of the largest index chunk.
     */
    unsigned int
    largest_index_chunk(void) const;

    /*!
     * Returns the index adjust value for all chunks.
     * The index adjust value is by how much to adjust
     * the indices of an index chunk.
     */
    c_array<const int>
    index_adjust_chunks(void) const;

    /*!
     * Provided as an API conveniance to fetch
     * the named chunk of index_data_chunks()
     * or an empty chunk if the index is larger then
     * index_data_chunks().size().
     * \param i index of index_data_chunks() to fetch
     */
    c_array<const PainterIndex>
    index_data_chunk(unsigned int i) const;

    /*!
     * Provided as an API conveniance to fetch
     * the index adjust for the named chunk.
     * \param i index of index_adjust_chunks() to fetch
     */
    int
    index_adjust_chunk(unsigned int i) const;

    /*!
     * Returns an array that holds those value i
     * for which index_data_chunk(i) is non-empty.
     */
    c_array<const unsigned int>
    non_empty_index_data_chunks(void) const;

    /*!
     * Returns the z-range of the data in an attribute/index
     * pair. After drawing a chunk, Painter will call on
     * itself Painter::increment_z(range_type::differnce())
     * and will set the header set to PainterPacker so
     * that while drawing so that range_type::m_begin maps
     * to Painter::current_z().
     */
    c_array<const range_type<int> >
    z_ranges(void) const;

    /*!
     * Provided as an API conveniance to fetch
     * the named z-range value of z_ranges() or
     * range_type(0, 0) if the index is
     * larger then z_ranges().size().
     * \param i index into z_ranges() to fetch
     */
    range_type<int>
    z_range(unsigned int i) const;

  private:
    void *m_d;
  };
/*! @} */
}
