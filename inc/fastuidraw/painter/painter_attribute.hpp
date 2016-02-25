/*!
 * \file painter_attribute.hpp
 * \brief file painter_attribute.hpp
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


#include <fastuidraw/util/vecN.hpp>
namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    The attribute data generated/filled by PainterPacker.
   */
  class PainterAttribute
  {
  public:
    /*!
      Generic float attribute data
     */
    vec4 m_primary_attrib;

    /*!
      Generic float attribute data
     */
    vec4 m_secondary_attrib;

    /*!
       Generic attribute uint data
     */
    uvec4 m_uint_attrib;
  };

  /*!
    Typedef for the index type used by PainterPacker
   */
  typedef uint32_t PainterIndex;

/*! @} */
}
