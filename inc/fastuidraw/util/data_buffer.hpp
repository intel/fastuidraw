/*!
 * \file data_buffer.hpp
 * \brief file data_buffer.hpp
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

#include <fastuidraw/util/data_buffer_base.hpp>

namespace fastuidraw {
/*!\addtogroup Utility
  @{
 */
  /*!
    \brief
    Represents a buffer directly stored in memory.
   */
  class DataBufferBackingStore
  {
  public:
    /*!
      Ctor.
      Copies a file into memory.
      \param filename name of file to open
     */
    DataBufferBackingStore(const char *filename);

    /*!
      Ctor. Allocate memory and fill the buffer
      with a fixed value.
      \param num_bytes number of bytes to give the backing store
      \param init initial value to give each byte
     */
    DataBufferBackingStore(unsigned int num_bytes, uint8_t init = uint8_t(0));

    ~DataBufferBackingStore();

    /*!
      Return a pointer to the backing store of the memory.
     */
    c_array<uint8_t>
    data(void);

  private:
    void *m_d;
  };

  /*!
    \brief
    DataBuffer is an implementation of DataBufferBase where the
    data is directly backed by memory
   */
  class DataBuffer:
    private DataBufferBackingStore,
    public DataBufferBase
  {
  public:
    /*!
      Ctor. Initialize the DataBuffer to be backed by uninitalized
      memory. Use the pointer returned by data() to set the data.
     */
    explicit
    DataBuffer(unsigned int num_bytes):
      DataBufferBackingStore(num_bytes),
      DataBufferBase(data(), data())
    {}

    /*!
      Ctor. Initialize the DataBuffer to be backed by memory
      whose value is copied from a file.
     */
    explicit
    DataBuffer(const char *filename):
      DataBufferBackingStore(filename),
      DataBufferBase(data(), data())
    {}
  };

/*! @} */
} //namespace fastuidraw
