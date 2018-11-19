/*!
 * \file painter_blend_shader_glsl.hpp
 * \brief file painter_blend_shader_glsl.hpp
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
#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/painter/painter_blend_shader.hpp>
#include <fastuidraw/glsl/shader_source.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSL
 * @{
 */
    /*!
     * \brief
     * A PainterBlendShaderGLSL is a PainterBlendShader whose
     * shader code fragment is via GLSL.
     *
     * The GLSL code needs to implement the function:
     * \code
     * void
     * fastuidraw_gl_compute_post_blended_value(in uint sub_shader, in uint blend_shader_data_location,
     *                                          in vec3 in_src, in vec3 in_fb, out vec3 out_src)
     * \endcode
     * where in_src is the color value for the fragment coming from the tiem, in_fb is
     * the value of the framebuffer at the location and out_src is the value for the
     * fragment shader to submit to the composite operation.
     *
     * For each of the blend shader type:
     * - sub_shader corresponds to PainterBlendShader::sub_shader(),
     * - the  same globals available to a fragment shader in \ref
     *   PainterItemShaderGLSL are also avalailable to the blend shader and
     * - blend_shader_data_location is the block from which to fetch the
     *   data packed into the data store by PainterBlendShaderData::pack_data();
     *   use the macro fastuidraw_fetch_data() (see the description of PainterItemShaderGLSL)
     *   to fetch the data.
     */
    class PainterBlendShaderGLSL:public PainterBlendShader
    {
    public:
      /*!
       * Ctor.
       * \param src GLSL code fragment for blend shading
       * \param num_sub_shaders the number of sub-shaders it supports
       */
      PainterBlendShaderGLSL(const ShaderSource &src, unsigned int num_sub_shaders = 1);

      ~PainterBlendShaderGLSL(void);

      /*!
       * Return the GLSL source of the blend shader
       */
      const glsl::ShaderSource&
      blend_src(void) const;

    private:
      void *m_d;
    };
/*! @} */

  }
}
