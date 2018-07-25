/*!
 * \file mutex.cpp
 * \brief file mutex.cpp
 *
 * Copyright 2018 by Intel.
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

#include <fastuidraw/util/mutex.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <mutex>

//////////////////////////
// fastuidraw::Mutex methods
fastuidraw::Mutex::
Mutex(void)
{
  m_d = FASTUIDRAWnew std::mutex();
}

fastuidraw::Mutex::
~Mutex()
{
  std::mutex *d;
  d = static_cast<std::mutex*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::Mutex::
lock(void)
{
  std::mutex *d;
  d = static_cast<std::mutex*>(m_d);
  d->lock();
}

void
fastuidraw::Mutex::
unlock(void)
{
  std::mutex *d;
  d = static_cast<std::mutex*>(m_d);
  d->unlock();
}

bool
fastuidraw::Mutex::
try_lock(void)
{
  std::mutex *d;
  d = static_cast<std::mutex*>(m_d);
  return d->try_lock();
}
