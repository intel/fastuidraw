/*!
 * \file data_buffer.hpp
 * \brief file data_buffer.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef FASTUIDRAW_DATA_BUFFER_HPP
#define FASTUIDRAW_DATA_BUFFER_HPP

#include <fastuidraw/util/data_buffer_base.hpp>

namespace fastuidraw {
/*!\addtogroup Utility
 * @{
 */
  /*!
   * \brief
   * Represents a buffer directly stored in memory.
   */
  class DataBufferBackingStore
  {
  public:
    /*!
     * Ctor.
     * Copies a file into memory.
     * \param filename name of file to open
     */
    DataBufferBackingStore(c_string filename);

    /*!
     * Ctor. Allocate memory and fill the buffer
     * with a fixed value.
     * \param num_bytes number of bytes to give the backing store
     * \param init initial value to give each byte
     */
    explicit
    DataBufferBackingStore(unsigned int num_bytes, uint8_t init = uint8_t(0));

    /*!
     * Ctor. Allocates the memory and initializes it with data.
     */
    explicit
    DataBufferBackingStore(c_array<const uint8_t> init_data);

    ~DataBufferBackingStore();

    /*!
     * Return a pointer to the backing store of the memory.
     */
    c_array<uint8_t>
    data(void);

  private:
    void *m_d;
  };

  /*!
   * \brief
   * DataBuffer is an implementation of DataBufferBase where the
   * data is directly backed by memory
   */
  class DataBuffer:
    private DataBufferBackingStore,
    public DataBufferBase
  {
  public:
    /*!
     * Ctor. Initialize the DataBuffer to be backed by uninitalized
     * memory. Use the pointer returned by data() to set the data.
     * \param num_bytes number of bytes to give the backing store
     * \param init initial value to give each byte
     */
    explicit
    DataBuffer(unsigned int num_bytes, uint8_t init = uint8_t(0)):
      DataBufferBackingStore(num_bytes, init),
      DataBufferBase(data(), data())
    {}

    /*!
     * Ctor. Initialize the DataBuffer to be backed by memory
     * whose value is copied from a file.
     */
    explicit
    DataBuffer(c_string filename):
      DataBufferBackingStore(filename),
      DataBufferBase(data(), data())
    {}

    /*!
     * Ctor. Initialize the DataBuffer to be backed by memory
     * whose value is copied from a file.
     */
    explicit
    DataBuffer(c_array<const uint8_t> init_data):
      DataBufferBackingStore(init_data),
      DataBufferBase(data(), data())
    {}
  };

/*! @} */
} //namespace fastuidraw

#endif
