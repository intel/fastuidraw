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
 * @{
 */

  /*!
   * \brief
   * A PainterItemCoverageShader represents a shader to draw an item to a
   * coverage buffer (see PainterSurface::deferred_coverage_buffer_type).
   * Typically such a shader represents both a vertex and fragment shader.
   */
  class PainterItemCoverageShader:public PainterShader
  {
  public:
    /*!
     * Ctor for a PainterItemCoverageShader with no sub-shaders.
     */
    PainterItemCoverageShader(void):
      PainterShader()
    {}

    /*!
     * Ctor for creating a PainterItemCoverageShader which has multiple
     * sub-shaders. The purpose of sub-shaders is for the
     * case where multiple shaders almost same code and those
     * code differences can be realized by examining a sub-shader
     * ID.
     * \param num_sub_shaders number of sub-shaders
     */
    explicit
    PainterItemCoverageShader(unsigned int num_sub_shaders):
      PainterShader(num_sub_shaders)
    {}

    /*!
     * Ctor to create a PainterItemCoverageShader realized as a sub-shader
     * of an existing PainterItemCoverageShader
     * \param parent parent PainterItemCoverageShader that has sub-shaders
     * \param sub_shader which sub-shader of the parent PainterItemCoverageShader
     */
    PainterItemCoverageShader(reference_counted_ptr<PainterItemCoverageShader> parent,
                              unsigned int sub_shader):
      PainterShader(parent, sub_shader)
    {}
  };

/*! @} */
}
