/*!
 * \file painter_attribute_writer.hpp
 * \brief file painter_attribute_writer.hpp
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
#include <fastuidraw/painter/attribute_data/painter_attribute.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterAttribute
 * @{
 */

  /*!
   * \brief
   * Orovides an interface to write attribute and index data when
   * a simple copy of data from \ref c_array objects is not sufficient.
   *
   * A \ref PainterAttributeWriter is to be stateless. However, in
   * order to be able to successfully write multiple chunks of
   * attributes and indices, a \ref PainterAttributeWriter is given
   * a "cookie" (see \ref WriteState) which represents how far along
   * streaming it is.
   */
  class PainterAttributeWriter:fastuidraw::noncopyable
  {
  public:
    /*!
     * A \ref WriteState represents how far along a \ref
     * PainterAttributeWriter has written its attribute
     * and index data along with if there is more to write.
     */
    class WriteState
    {
    public:
      /*!
       * \ref m_state represents how far along a \ref
       * PainterAttributeWriter has written its attribute
       * and index data. The length of state will be
       * \ref PainterAttributeWriter::state_length().
       */
      c_array<unsigned int> m_state;

      /*!
       * Gives the minimum size of the next attribute
       * array passed to \ref PainterAttributeWriter::write()
       * must be in order to successfully write data.
       */
      unsigned int m_min_attributes_for_next;

      /*!
       * Gives the minimum size of the next index
       * array passed to \ref PainterAttributeWriter::write()
       * must be in order to successfully write data.
       */
      unsigned int m_min_indices_for_next;
    };

    virtual
    ~PainterAttributeWriter()
    {}

    /*!
     * To be implemented by a derived class to return
     * how large the array \ref WriteState::m_state
     * should be.
     */
    virtual
    unsigned int
    state_length(void) const = 0;

    /*!
     * To be implemented by a derived clas to initialize a
     * \ref WriteState to indicate the start of writing data
     * for a subsequence calls to \ref write_data(). Returns
     * true if there is attribute and index data to upload.
     * \parm state \ref WriteState to initialize
     */
    virtual
    bool
    initialize_state(WriteState *state) const = 0;

    /*!
     * To be implemented by a derived class. Called by the
     * caller of a \ref PainterAttributeWriter to indicate
     * that a new data store has been started.
     */
    virtual
    void
    on_new_store(WriteState *state) const = 0;

    /*!
     * To be implemented by a derived class to write attribute
     * and index data. Returns true if there is further attribute
     * and index data to upload.
     * \param dst_attribs location to which to write attributes.
     *                    The size of the array is guaranteed by
     *                    the caller to be atleast the value of
     *                    \ref WriteState::m_min_attributes_for_next.
     * \param dst_indices location to which to write indices.
     *                    The size of the array is guaranteed by
     *                    the caller to be atleast the value of
     *                    \ref WriteState::m_min_indices_for_next.
     * \param attrib_location index value of the attribute at
     *                        dst_attribs[0].
     * \param state the write state of the session which is to be
     *              updated for the next call to write_data().
     * \param num_attribs_written location to which to write
     *                            the number of attributes
     *                            written by the call to \ref
     *                            write_data().
     * \param num_indices_written location to which to write the
     *                            number of indices written by the
     *                            call to \ref write_data().
     */
    virtual
    bool
    write_data(c_array<PainterAttribute> dst_attribs,
               c_array<PainterIndex> dst_indices,
               unsigned int attrib_location,
               WriteState *state,
               unsigned int *num_attribs_written,
               unsigned int *num_indices_written) const = 0;
  };
/*! @} */
}
