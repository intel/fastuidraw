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

fastuidraw::FreetypeLib::
FreetypeLib(void)
{
  int error_code;

  error_code = FT_Init_FreeType(&m_lib);
  if(error_code != 0)
    {
      m_lib = NULL;
    }
}

fastuidraw::FreetypeLib::
~FreetypeLib()
{
  if(m_lib != NULL)
    {
      FT_Done_FreeType(m_lib);
    }
}
