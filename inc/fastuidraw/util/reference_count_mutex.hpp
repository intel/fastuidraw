/*!
 * \file reference_count_mutex.hpp
 * \brief file reference_count_mutex.hpp
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

namespace fastuidraw
{
/*!\addtogroup Utility
  @{
 */

  /*!
    Reference counting that is thread safe by being
    having operations behind a mutex lock. Usually
    slower (and much slower) than
    reference_count_atomic.
   */
  class reference_count_mutex:noncopyable
  {
  public:
    /*!
      Initializes the counter as zero.
     */
    reference_count_mutex(void);

    ~reference_count_mutex();

    /*!
      Increment reference counter by 1.
     */
    void
    add_reference(void);

    /*!
      Decrements the counter by 1 and returns status of if the counter
      is 0 after the decrement operation.
     */
    bool
    remove_reference(void);

  private:
    void *m_d;
  };
/*! @} */
}
