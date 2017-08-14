/*!
 * \file freetype_lib.cpp
 * \brief file freetype_lib.cpp
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
#include <fastuidraw/text/freetype_lib.hpp>
#include <mutex>

namespace
{
  class FreetypeLibPrivate
  {
  public:
    FreetypeLibPrivate(void);
    ~FreetypeLibPrivate();

    std::mutex m_mutex;
    FT_Library m_lib;
  };
}

////////////////////////////
// FreetypeLibPrivate methods
FreetypeLibPrivate::
FreetypeLibPrivate(void):
  m_lib(nullptr)
{
  int error_code;

  error_code = FT_Init_FreeType(&m_lib);
  FASTUIDRAWassert(error_code == 0);
}

FreetypeLibPrivate::
~FreetypeLibPrivate()
{
  if(m_lib != nullptr)
    {
      FT_Done_FreeType(m_lib);
    }
}

/////////////////////////////
// fastuidraw::FreetypeLib methods
fastuidraw::FreetypeLib::
FreetypeLib(void)
{
  m_d = FASTUIDRAWnew FreetypeLibPrivate();
}

fastuidraw::FreetypeLib::
~FreetypeLib()
{
  FreetypeLibPrivate *d;
  d = static_cast<FreetypeLibPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

FT_Library
fastuidraw::FreetypeLib::
lib(void)
{
  FreetypeLibPrivate *d;
  d = static_cast<FreetypeLibPrivate*>(m_d);
  return d->m_lib;
}

void
fastuidraw::FreetypeLib::
lock(void)
{
  FreetypeLibPrivate *d;
  d = static_cast<FreetypeLibPrivate*>(m_d);
  d->m_mutex.lock();
}

void
fastuidraw::FreetypeLib::
unlock(void)
{
  FreetypeLibPrivate *d;
  d = static_cast<FreetypeLibPrivate*>(m_d);
  d->m_mutex.unlock();
}

bool
fastuidraw::FreetypeLib::
try_lock(void)
{
  FreetypeLibPrivate *d;
  d = static_cast<FreetypeLibPrivate*>(m_d);
  return d->m_mutex.try_lock();
}
