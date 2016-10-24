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
      Ctor.
     */
    PainterAttributeData(void);

    ~PainterAttributeData();

    /*!
      Set the index, attribute, z-increment and chunk
      data of this PainterAttributeData using a
      PainterAttributeDataFiller.
      \param filler object that fills the data.
     */
    void
    set_data(const PainterAttributeDataFiller &filler);

    /*!
      Returns the attribute data chunks. Usually, for each
      attribute data chunk, there is a matching index data
      chunk. Usually, one uses index_data_chunks()[i]
      to draw the contents of attribute_data_chunks()[i].
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
      Returns the index data chunks. Usually, for each
      attribute data chunk, there is a matching index data
      chunk. Usually, one uses index_data_chunks()[i]
      to draw the contents of attribute_data_chunks()[i].
    */
    const_c_array<const_c_array<PainterIndex> >
    index_data_chunks(void) const;

    /*!
      Returns the index adjust value for all chunks.
      The index adjust value is by how much to adjust
      the indices of an index chunk.
     */
    const_c_array<int>
    index_adjust_chunks(void) const;

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
      Provided as an API conveniance to fetch
      the index adjust for the named chunk.
      \param i index of index_adjust_chunks() to fetch
     */
    int
    index_adjust_chunk(unsigned int i) const;

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

  private:
    void *m_d;
  };
/*! @} */
}
