/*!
 * \file painter_data_writer.hpp
 * \brief file painter_data_writer.hpp
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
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/painter/painter_attribute.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A provides an interface to write attribute and index data when
   * a simple copy of data as exposed by Painter::draw_generic()
   * methods is not sufficient (for example to modify or filter data
   * based off of some dynamic state).
   */
  class PainterDataWriter:fastuidraw::noncopyable
  {
  public:
    virtual
    ~PainterDataWriter()
    {}

    /*!
     * To be implemented by a derived class to return
     * the number of attribute chunks of the PainterDataWriter.
     */
    virtual
    unsigned int
    number_attribute_chunks(void) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the number of attribute of an attribute chunk
     * of the PainterDataWriter.
     * \param attribute_chunk which chunk of attributes
     */
    virtual
    unsigned int
    number_attributes(unsigned int attribute_chunk) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the number of index chunks of the PainterDataWriter.
     */
    virtual
    unsigned int
    number_index_chunks(void) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the number of indices of an index chunk
     * of the PainterDataWriter.
     * \param index_chunk which chunk of attributes
     */
    virtual
    unsigned int
    number_indices(unsigned int index_chunk) const = 0;

    /*!
     * To be implemented by a derived class to return
     * what attribute chunk to use for a given index
     * chunk.
     * \param index_chunk index chunk with 0 <= index_chunk <= number_index_chunks()
     */
    virtual
    unsigned int
    attribute_chunk_selection(unsigned int index_chunk) const = 0;

    /*!
     * To be implemented by a derived class to write indices.
     * \param dst location to which to write indices
     * \param index_offset_value value by which to increment the index
     *                           values written
     * \param index_chunk which chunk of indices to write
     */
    virtual
    void
    write_indices(c_array<PainterIndex> dst,
                  unsigned int index_offset_value,
                  unsigned int index_chunk) const = 0;

    /*!
     * To be implemented by a derived class to write attributes.
     * \param dst location to which to write indices
     * \param attribute_chunk which chunk of attributes to write
     */
    virtual
    void
    write_attributes(c_array<PainterAttribute> dst,
                     unsigned int attribute_chunk) const = 0;
  };
/*! @} */
}
