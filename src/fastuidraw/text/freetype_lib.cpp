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
  class FreeTypeLibPrivate
  {
  public:
    FreeTypeLibPrivate(void);
    ~FreeTypeLibPrivate();

    std::mutex m_mutex;
    FT_Library m_lib;
  };
}

////////////////////////////
// FreeTypeLibPrivate methods
FreeTypeLibPrivate::
FreeTypeLibPrivate(void):
  m_lib(nullptr)
{
  int error_code;

  error_code = FT_Init_FreeType(&m_lib);
  FASTUIDRAWassert(error_code == 0);
  FASTUIDRAWunused(error_code);
}

FreeTypeLibPrivate::
~FreeTypeLibPrivate()
{
  if(m_lib != nullptr)
    {
      FT_Done_FreeType(m_lib);
    }
}

/////////////////////////////
// fastuidraw::FreeTypeLib methods
fastuidraw::FreeTypeLib::
FreeTypeLib(void)
{
  m_d = FASTUIDRAWnew FreeTypeLibPrivate();
}

fastuidraw::FreeTypeLib::
~FreeTypeLib()
{
  FreeTypeLibPrivate *d;
  d = static_cast<FreeTypeLibPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

FT_Library
fastuidraw::FreeTypeLib::
lib(void)
{
  FreeTypeLibPrivate *d;
  d = static_cast<FreeTypeLibPrivate*>(m_d);
  return d->m_lib;
}

void
fastuidraw::FreeTypeLib::
lock(void)
{
  FreeTypeLibPrivate *d;
  d = static_cast<FreeTypeLibPrivate*>(m_d);
  d->m_mutex.lock();
}

void
fastuidraw::FreeTypeLib::
unlock(void)
{
  FreeTypeLibPrivate *d;
  d = static_cast<FreeTypeLibPrivate*>(m_d);
  d->m_mutex.unlock();
}

bool
fastuidraw::FreeTypeLib::
try_lock(void)
{
  FreeTypeLibPrivate *d;
  d = static_cast<FreeTypeLibPrivate*>(m_d);
  return d->m_mutex.try_lock();
}
