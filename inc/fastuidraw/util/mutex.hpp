/*!
 * \file mutex.hpp
 * \brief file mutex.hpp
 *
 * Copyright 2018 by Intel.
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

#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * Mutex implements a non-recursive mutex.
   */
  class Mutex:noncopyable
  {
  public:
    /*!
     * Guard locks a mutex on ctor and unlock
     * the mutex on dtor.
     */
    class Guard:noncopyable
    {
    public:
      /*!
       * Ctor.
       * \param m mutex is locked on ctor and unlocked on dtor.
       */
      explicit
      Guard(Mutex &m):
        m_mutex(m)
      {
        m_mutex.lock();
      }

      ~Guard()
      {
        m_mutex.unlock();
      }

    private:
      Mutex &m_mutex;
    };

    /*!
     * Ctor.
     */
    Mutex(void);

    ~Mutex(void);

    /*!
     * Aquire the lock of the mutex;
     * only return once the lock is aquired.
     */
    void
    lock(void);

    /*!
     * Release the lock of the mutex.
     */
    void
    unlock(void);

    /*!
     * Try to aquire the lock of the mutex;
     * if the mutex is already locked return
     * false; otherwise return true.
     */
    bool
    try_lock(void);

  private:
    void *m_d;
  };

/*! @} */
}
