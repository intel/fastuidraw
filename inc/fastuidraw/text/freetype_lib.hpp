/*!
 * \file freetype_lib.hpp
 * \brief file freetype_lib.hpp
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


#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/

  /*!
    \brief
    A FreetypeLib wraps an FT_Library object of libFreeType
    together with a mutex in a reference counted object.
   */
  class FreetypeLib:public reference_counted<FreetypeLib>::default_base
  {
  public:
    /*!
      Ctor.
     */
    FreetypeLib(void);

    ~FreetypeLib();

    /*!
      Returns the FT_Library object about which
      this object wraps.
     */
    FT_Library
    lib(void);

    /*!
      Aquire the lock of the mutex used to access/use the FT_Library lib()
      safely across multiple threads.
     */
    void
    lock_lib(void);

    /*!
      Release the lock of the mutex used to access/use the FT_Library lib()
      safely across multiple threads.
     */
    void
    unlock_lib(void);

    /*!
      Try to aquire the lock of the mutex. Return true on success.
     */
    bool
    try_lock(void);

  private:
    void *m_d;
  };
/*! @} */
};
