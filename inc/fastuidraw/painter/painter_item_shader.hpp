/*!
 * \file painter_item_shader.hpp
 * \brief file painter_item_shader.hpp
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
#include <fastuidraw/painter/painter_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
  @{
 */

  /*!
    \brief
    A PainterItemShader represents a shader to
    draw an item (typically a vertex and fragment
    shader pair).
   */
  class PainterItemShader:public PainterShader
  {
  public:
    /*!
      Ctor for a PainterItemShader with no sub-shaders.
     */
    PainterItemShader(void):
      PainterShader()
    {}

    /*!
      Ctor for creating a PainterItemShader which has multiple
      sub-shaders. The purpose of sub-shaders is for the
      case where multiple shaders almost same code and those
      code differences can be realized by examining a sub-shader
      ID.
      \param num_sub_shaders number of sub-shaders
     */
    explicit
    PainterItemShader(unsigned int num_sub_shaders):
      PainterShader(num_sub_shaders)
    {}

    /*!
      Ctor to create a PainterItemShader realized as a sub-shader
      of an existing PainterItemShader
      \param sub_shader which sub-shader of the parent PainterItemShader
      \param parent parent PainterItemShader that has sub-shaders
     */
    PainterItemShader(unsigned int sub_shader,
                      reference_counted_ptr<PainterItemShader> parent):
      PainterShader(sub_shader, parent)
    {}
  };

/*! @} */
}
