/*!
 * \file data_buffer_base.hpp
 * \brief file data_buffer_base.hpp
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

#pragma once

#include <stdint.h>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup Utility
 * @{
 */
  /*!
   * \brief
   * Base class for passing around buffers of data; derived
   * classes have the responsibility of maintaining storage
   * cleanup at destruction.
   */
  class DataBufferBase:public reference_counted<DataBufferBase>::concurrent
  {
  public:
    /*!
     * Ctor.
     * \param pdata_ro value which data_ro() will return
     * \param pdata_rw value which data_rw() will return
     */
    DataBufferBase(c_array<const uint8_t> pdata_ro,
                   c_array<uint8_t> pdata_rw):
      m_data_ro(pdata_ro),
      m_data_rw(pdata_rw)
    {}

    /*!
     * Return the memory as read-only
     */
    c_array<const uint8_t>
    data_ro(void) const
    {
      return m_data_ro;
    }

    /*!
     * Return the memory as read-write
     */
    c_array<uint8_t>
    data_rw(void)
    {
      return m_data_rw;
    }

  private:
    c_array<const uint8_t> m_data_ro;
    c_array<uint8_t> m_data_rw;
  };

/*! @} */
} //namespace fastuidraw
