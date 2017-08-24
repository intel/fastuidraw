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

#include <stdint.h>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup Utility
  @{
 */

  /*!
    \brief
   */
  class DataBuffer:public reference_counted<DataBuffer>::default_base
  {
  public:
    /*!
      Ctor. Initialize the DataBuffer to be backed by uninitalized
      memory. Use the pointer returned by data() to set the data.
     */
    explicit
    DataBuffer(unsigned int num_bytes);

    /*!
      Ctor. Initialize the DataBuffer to be backed by memory
      whose value is copied from a file.
     */
    explicit
    DataBuffer(const char *filename);

    ~DataBuffer();

    /*!
      Return the data that backs the Source
    */
    c_array<uint8_t>
    data(void);

    /*!
      Return the data that backs the Source
    */
    const_c_array<uint8_t>
    data(void) const;

  private:
    void *m_d;
  };

/*! @} */
} //namespace fastuidraw
