/*!
 * \file painter_composite_shader.hpp
 * \brief file painter_composite_shader.hpp
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
   * A PainterCompositeShader represents a shader
   * for performing compositing operations.
   */
  class PainterCompositeShader:public PainterShader
  {
  public:
    /*!
     * \brief
     * Enumeration to specify how composite shader operates
     */
    enum shader_type
      {
        /*!
         * Indicates compositing is via fixed function compositing
         * with single source compositing.
         */
        single_src,

        /*!
         * Indicates compositing is via fixed function compositing
         * with dual source compositing.
         */
        dual_src,

        /*!
         * Indicates compositing is via framebuffer fetch.
         */
        framebuffer_fetch,

        number_types,
      };

    /*!
     * Ctor for creating a PainterCompositeShader which has multiple
     * sub-shaders. The purpose of sub-shaders is for the case
     * where multiple shaders have almost same code and those
     * code differences can be realized by examining a sub-shader
     * ID.
     * \param tp the "how" the composite shader operates
     * \param num_sub_shaders number of sub-shaders
     */
    explicit
    PainterCompositeShader(enum shader_type tp,
                           unsigned int num_sub_shaders = 1):
      PainterShader(num_sub_shaders),
      m_type(tp)
    {}

    /*!
     * Ctor to create a PainterCompositeShader realized as a sub-shader
     * of an existing PainterCompositeShader
     * \param sub_shader which sub-shader of the parent PainterCompositeShader
     * \param parent parent PainterCompositeShader that has sub-shaders
     */
    PainterCompositeShader(unsigned int sub_shader,
                           reference_counted_ptr<PainterCompositeShader> parent):
      PainterShader(sub_shader, parent),
      m_type(parent->type())
    {}

    /*!
     * Returns how the PainterCompositeShader operates.
     */
    enum shader_type
    type(void) const
    {
      return m_type;
    }

  private:
    enum shader_type m_type;
  };

/*! @} */
}
