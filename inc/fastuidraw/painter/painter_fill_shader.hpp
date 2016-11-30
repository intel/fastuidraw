/*!
 * \file painter_fill_shader.hpp
 * \brief file painter_fill_shader.hpp
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


#include <fastuidraw/painter/painter_item_shader.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    A PainterFillShader holds the shader for
    drawing filled paths.
   */
  class PainterFillShader
  {
  public:
    /*!
      Ctor
     */
    PainterFillShader(void);

    /*!
      Copy ctor.
     */
    PainterFillShader(const PainterFillShader &obj);

    ~PainterFillShader();

    /*!
      Assignment operator.
     */
    PainterFillShader&
    operator=(const PainterFillShader &rhs);

    /*!
      Returns the PainterItemShader to use.
     */
    const reference_counted_ptr<PainterItemShader>&
    item_shader(void) const;

    /*!
      Set the value returned by item_shader(void) const.
      \param sh value to use
     */
    PainterFillShader&
    item_shader(const reference_counted_ptr<PainterItemShader> &sh);

  private:
    void *m_d;
  };

/*! @} */
}
