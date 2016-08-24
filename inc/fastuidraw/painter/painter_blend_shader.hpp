/*!
 * \file painter_blend_shader.hpp
 * \brief file painter_blend_shader.hpp
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
    A PainterBlendShader represents a shader
    for performing blending operations.
   */
  class PainterBlendShader:public PainterShader
  {
  public:
    /*!
      Enumeration to specify how blend shader operates
     */
    enum shader_type
      {
        /*!
          Indicates blending is via fixed function blending
          with single source blending.
         */
        single_src,

        /*!
          Indicates blending is via fixed function blending
          with dual source blending.
         */
        dual_src,

        /*!
          Indicates blending is via framebuffer fetch.
         */
        framebuffer_fetch,

        number_types,
      };

    /*!
      Ctor for creating a PainterBlendShader which has multiple
      sub-shaders. The purpose of sub-shaders is for the case
      where multiple shaders have almost same code and those
      code differences can be realized by examining a sub-shader
      ID.
      \param tp the "how" the blend shader operates
      \param num_sub_shaders number of sub-shaders
     */
    explicit
    PainterBlendShader(enum shader_type tp,
                       unsigned int num_sub_shaders = 1):
      PainterShader(num_sub_shaders),
      m_type(tp)
    {}

    /*!
      Ctor to create a PainterBlendShader realized as a sub-shader
      of an existing PainterBlendShader
      \param sub_shader which sub-shader of the parent PainterBlendShader
      \param parent parent PainterBlendShader that has sub-shaders
     */
    PainterBlendShader(unsigned int sub_shader,
                       reference_counted_ptr<PainterBlendShader> parent):
      PainterShader(sub_shader, parent),
      m_type(parent->type())
    {}

    /*!
      Returns how the PainterBlendShader operates.
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
