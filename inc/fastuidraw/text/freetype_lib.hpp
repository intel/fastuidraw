/*!
 * \file freetype_lib.hpp
 * \brief file freetype_lib.hpp
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


#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

  /*!
   * \brief
   * A FreeTypeLib wraps an FT_Library object of the FreeType
   * library together with a mutex in a reference counted object.
   *
   * The threading model for the FreeType appears to be:
   * - Create an FT_Library object
   * - When creating or releasing FT_Face objects, lock a mutex
   *   around the FT_Library when doing so
   * - If an FT_Face is accessed from multiple threads, the FT_Face
   *   (but not the FT_Library) needs to be mutex locked
   */
  class FreeTypeLib:public reference_counted<FreeTypeLib>::concurrent
  {
  public:
    /*!
     * Ctor.
     */
    FreeTypeLib(void);

    ~FreeTypeLib();

    /*!
     * Returns the FT_Library object about which
     * this object wraps.
     */
    FT_Library
    lib(void);

    /*!
     * Aquire the lock of the mutex used to access/use the FT_Library
     * return by lib() safely across multiple threads.
     */
    void
    lock(void);

    /*!
     * Release the lock of the mutex used to access/use the FT_Library
     * return by lib() safely across multiple threads.
     */
    void
    unlock(void);

    /*!
     * Try to aquire the lock of the mutex. Return true on success.
     */
    bool
    try_lock(void);

  private:
    void *m_d;
  };
/*! @} */
};
