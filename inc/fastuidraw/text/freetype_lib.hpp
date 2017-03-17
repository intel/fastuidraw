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
    A FreetypeLib wraps an FT_Library object of libFreeType
    in a reference counted object.
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
      this object wraps. Asserts if valid()
      return false.
     */
    FT_Library
    lib(void)
    {
      assert(valid());
      return m_lib;
    }

    /*!
      Returns true if this object wraps a valid FT_Library object.
     */
    bool
    valid(void)
    {
      return m_lib != nullptr;
    }

  private:
    FT_Library m_lib;
  };
/*! @} */
};
