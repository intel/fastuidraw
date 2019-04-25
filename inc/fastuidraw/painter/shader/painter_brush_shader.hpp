/*!
 * \file painter_brush_shader.hpp
 * \brief file painter_brush_shader.hpp
 *
 * Copyright 2019 by Intel.
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
#include <fastuidraw/painter/shader/painter_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A PainterBrushShader represents a shader
   * for performing a custom brush coloring.
   */
  class PainterBrushShader:public PainterShader
  {
  public:
    /*!
     * Ctor for creating a PainterBrushShader which has multiple
     * sub-shaders. The purpose of sub-shaders is for the case
     * where multiple shaders have almost same code and those
     * code differences can be realized by examining a sub-shader
     * ID.
     * \param num_sub_shaders number of sub-shaders
     */
    explicit
    PainterBrushShader(unsigned int num_sub_shaders = 1):
      PainterShader(num_sub_shaders)
    {}

    /*!
     * Ctor to create a PainterBrushShader realized as a sub-shader
     * of an existing PainterBrushShader
     * \param parent parent PainterBrushShader that has sub-shaders
     * \param sub_shader which sub-shader of the parent PainterBrushShader
     */
    PainterBrushShader(reference_counted_ptr<PainterBrushShader> parent,
                             unsigned int sub_shader):
      PainterShader(parent, sub_shader)
    {}
  };

/*! @} */
}
