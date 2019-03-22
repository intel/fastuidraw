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
#include <fastuidraw/painter/shader/painter_blend_shader.hpp>
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
     * The code to implement is dependent on the PainterBlendShader::type()
     * of the created PainterBlendShaderGLSL.
     * - PainterBlendShader::type() == PainterBlendShader::single_src
     *   The shader code fragment must provide the function
     *   \code
     *   void
     *   fastuidraw_gl_compute_blend_value(in uint sub_shader, in uint blend_shader_data_location,
     *                                     in vec4 in_src, out vec4 out_src)
     *   \endcode
     *   where in_src is the output of the item fragment shader modulated by the
     *   current brush with alpha applied to rgb and out_src is the value for the
     *   fragment shader to emit.
     *
     * - PainterBlendShader::type() == PainterBlendShader::dual_src
     *   The shader code fragment must provide the function
     *   \code
     *   void
     *   fastuidraw_gl_compute_blend_factors(in uint sub_shader, in uint blend_shader_data_location,
     *                                       in vec4 in_src, out vec4 out_src0, out vec4 out_src1)
     *   \endcode
     *   where in_src is the output of the item fragment shader modulated by the
     *   current brush with alpha applied to rgb, out_src0 is the value for the
     *   fragment shader to emit for GL_SRC_COLOR and out_src1 is the value for
     *   the fragment shader to emit value for GL_SRC1_COLOR.
     *
     * - PainterBlendShader::type() == PainterBlendShader::framebuffer_fetch
     *   The shader code fragment must provide the function
     *   \code
     *   void
     *   fastuidraw_gl_compute_post_blended_value(in uint sub_shader, in uint blend_shader_data_location,
     *                                            in vec4 in_src, in vec4 in_fb, out vec4 out_src)
     *   \endcode
     *   where in_src is the output of the item fragment shader modulated by the
     *   current brush with alpha applied to rgb, in_fb is the value of the
     *   framebuffer at the location  and out_src is the value for the fragment
     *   shader to emit.
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
       * \param tp blend shader type
       * \param src GLSL code fragment for blend shading
       * \param num_sub_shaders the number of sub-shaders it supports
       */
      PainterBlendShaderGLSL(enum shader_type tp, const ShaderSource &src,
                             unsigned int num_sub_shaders = 1);

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
