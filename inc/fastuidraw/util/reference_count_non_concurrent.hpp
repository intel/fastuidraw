/*!
 * \file reference_count_non_concurrent.hpp
 * \brief file reference_count_non_concurrent.hpp
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

#include <assert.h>
#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{

/*!\addtogroup Utility
  @{
 */
  /*!
    \brief
    Reference counter that is NOT thread safe
   */
  class reference_count_non_concurrent:noncopyable
  {
  public:
    /*!
      Initializes the counter as zero.
     */
    reference_count_non_concurrent(void):
      m_reference_count(0)
    {}

    ~reference_count_non_concurrent()
    {
      assert(m_reference_count == 0);
    }

    /*!
      Increment reference counter by 1.
     */
    void
    add_reference(void)
    {
      ++m_reference_count;
    }

    /*!
      Decrements the counter by 1 and returns status of if the counter
      is 0 after the decrement operation.
     */
    bool
    remove_reference(void)
    {
      --m_reference_count;
      return m_reference_count == 0;
    }

  private:
    int m_reference_count;
  };
/*! @} */
}
