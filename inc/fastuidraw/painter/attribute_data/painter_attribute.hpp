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
 * @{
 */

  /*!
   * \brief
   * The attribute data generated/filled by \ref Painter.
   * Attribute data is sent to 3D API as raw bits with
   * the expectation that shaders will cast the bits
   * to the appropiate types for themselves.
   */
  class PainterAttribute
  {
  public:
    /*!
     * conveniance typedef to declare point to an element
     * of PainterAttribute.
     */
    typedef uvec4 PainterAttribute::*pointer_to_field;

    /*!
     * Generic attribute data
     */
    uvec4 m_attrib0;

    /*!
     * Generic attribute data
     */
    uvec4 m_attrib1;

    /*!
     * Generic attribute data
     */
    uvec4 m_attrib2;
  };

  /*!
   * \brief
   * Typedef for the index type used by \ref Painter
   */
  typedef uint32_t PainterIndex;

/*! @} */
}
