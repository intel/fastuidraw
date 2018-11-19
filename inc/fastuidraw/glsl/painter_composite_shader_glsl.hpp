/*!
 * \file painter_composite_shader_glsl.hpp
 * \brief file painter_composite_shader_glsl.hpp
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
#include <fastuidraw/painter/painter_composite_shader.hpp>
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
     * A PainterCompositeShaderGLSL is a PainterCompositeShader whose
     * shader code fragment is via GLSL.
     *
     * The code to implement is dependent on the PainterCompositeShader::type()
     * of the created PainterCompositeShaderGLSL.
     * - PainterCompositeShader::type() == PainterCompositeShader::single_src
     *   The shader code fragment must provide the function
     *   \code
     *   void
     *   fastuidraw_gl_compute_composite_value(in uint sub_shader, in uint composite_shader_data_location,
     *                                         in vec4 in_src, out vec4 out_src)
     *   \endcode
     *   where in_src is the pre-multiplied by alpha color value for the
     *   fragment and out_src is the value for the fragment shader to emit.
     *
     * - PainterCompositeShader::type() == PainterCompositeShader::dual_src
     *   The shader code fragment must provide the function
     *   \code
     *   void
     *   fastuidraw_gl_compute_composite_factors(in uint sub_shader, in uint composite_shader_data_location,
     *                                           in vec4 in_src, out vec4 out_src0, out vec4 out_src1)
     *   \endcode
     *   where in_src is the pre-multiplied by alpha color value for the
     *   fragment, out_src0 is the value for the fragment shader to emit
     *   for GL_SRC_COLOR and out_src1 is the value for the fragment shader
     *   to emit value for GL_SRC1_COLOR.
     *
     * - PainterCompositeShader::type() == PainterCompositeShader::framebuffer_fetch
     *   The shader code fragment must provide the function
     *   \code
     *   void
     *   fastuidraw_gl_compute_post_compositeed_value(in uint sub_shader, in uint composite_shader_data_location,
     *                                                in vec4 in_src, in vec4 in_fb, out vec4 out_src)
     *   \endcode
     *   where in_src is the pre-multiplied by alpha color value for the
     *   fragment, in_fb is the value of the framebuffer at the location
     *   and out_src is the value for the fragment shader to emit.
     *
     * For each of the composite shader type:
     * - sub_shader corresponds to PainterCompositeShader::sub_shader(),
     * - the  same globals available to a fragment shader in \ref
     *   PainterItemShaderGLSL are also avalailable to the composite shader and
     * - composite_shader_data_location is the block from which to fetch the
     *   data packed into the data store by PainterCompositeShaderData::pack_data();
     *   use the macro fastuidraw_fetch_data() (see the description of PainterItemShaderGLSL)
     *   to fetch the data.
     */
    class PainterCompositeShaderGLSL:public PainterCompositeShader
    {
    public:
      /*!
       * Ctor.
       * \param tp composite shader type
       * \param src GLSL code fragment for composite shading
       * \param num_sub_shaders the number of sub-shaders it supports
       */
      PainterCompositeShaderGLSL(enum shader_type tp, const ShaderSource &src,
                                 unsigned int num_sub_shaders = 1);

      ~PainterCompositeShaderGLSL(void);

      /*!
       * Return the GLSL source of the composite shader
       */
      const glsl::ShaderSource&
      composite_src(void) const;

    private:
      void *m_d;
    };
/*! @} */

  }
}
