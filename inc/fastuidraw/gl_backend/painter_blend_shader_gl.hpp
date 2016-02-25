/*!
 * \file painter_blend_shader_gl.hpp
 * \brief file painter_blend_shader_gl.hpp
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
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace fastuidraw
{
  namespace gl
  {
/*!\addtogroup GLBackend
  @{
 */

    /*!
      Class to hold the blend mode as exposed by the GL API
      points.
     */
    class BlendMode
    {
    public:
      BlendMode(void):
        m_data(GL_ONE)
      {
        m_data[Kequation_rgb] = m_data[Kequation_alpha] = GL_FUNC_ADD;
      }

      /*!
        Set the argument to feed for the RGB for
        glBlendEquationSeparate, should be one of
        GL_FUNC_ADD, GL_FUNC_SUBTRACT,
        GL_FUNC_REVERSE_SUBTRACT, GL_MIN or GL_MAX
       */
      BlendMode&
      equation_rgb(GLenum v) { m_data[Kequation_rgb] = v; return *this; }

      /*!
        The value for the argument to feed for the RGB
        for glBlendEquationSeparate, as set by
        equation_rgb(GLenum).
       */
      GLenum
      equation_rgb(void) const { return m_data[Kequation_rgb]; }

      /*!
        Set the argument to feed for the Alpha for
        glBlendEquationSeparate, should be one of
        GL_FUNC_ADD, GL_FUNC_SUBTRACT,
        GL_FUNC_REVERSE_SUBTRACT, GL_MIN or GL_MAX
       */
      BlendMode&
      equation_alpha(GLenum v) { m_data[Kequation_alpha] = v; return *this; }

      /*!
        The value for the argument to feed for the Alpha
        for glBlendEquationSeparate, as set by
        equation_alpha(GLenum).
       */
      GLenum
      equation_alpha(void) const { return m_data[Kequation_alpha]; }

      /*!
        Provided as a conveniance, equivalent to
        \code
        equation_rgb(v);
        equation_alpha(v);
        \endcode
       */
      BlendMode&
      equation(GLenum v)
      {
        m_data[Kequation_rgb] = v;
        m_data[Kequation_alpha] = v;
        return *this;
      }

      /*!
        Set the argument to feed for the source coefficient RGB
        value to feed glBlendFuncSeparate, should be one of
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
        GL_SRC_ALPHA_SATURATE, GL_SRC1_COLOR, GL_SRC1_ALPHA,
        GL_ONE_MINUS_SRC1_COLOR or GL_ONE_MINUS_SRC1_ALPHA.
       */
      BlendMode&
      func_src_rgb(GLenum v) { m_data[Kfunc_src_rgb] = v; return *this; }

      /*!
        The value for the argument to feed for the source coefficient
        RGB to feed to glBlendFuncSeparate, as set by
        func_src_rgb(GLenum).
       */
      GLenum
      func_src_rgb(void) const { return m_data[Kfunc_src_rgb]; }

      /*!
        Set the argument to feed for the source coefficient Alpha
        value to feed glBlendFuncSeparate, should be one of
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
        GL_SRC_ALPHA_SATURATE, GL_SRC1_COLOR, GL_SRC1_ALPHA,
        GL_ONE_MINUS_SRC1_COLOR or GL_ONE_MINUS_SRC1_ALPHA.
       */
      BlendMode&
      func_src_alpha(GLenum v) { m_data[Kfunc_src_alpha] = v; return *this; }

      /*!
        The value for the argument to feed for the source coefficient
        Alpha to feed to glBlendFuncSeparate, as set by
        func_src_alpha(GLenum).
       */
      GLenum
      func_src_alpha(void) const { return m_data[Kfunc_src_alpha]; }

      /*!
        Provided as a conveniance, equivalent to
        \code
        func_src_rgb(v);
        func_src_alpha(v);
        \endcode
       */
      BlendMode&
      func_src(GLenum v)
      {
        m_data[Kfunc_src_rgb] = v;
        m_data[Kfunc_src_alpha] = v;
        return *this;
      }

      /*!
        Set the argument to feed for the destination coefficient RGB
        value to feed glBlendFuncSeparate, should be one of
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
        GL_SRC_ALPHA_SATURATE, GL_SRC1_COLOR, GL_SRC1_ALPHA,
        GL_ONE_MINUS_SRC1_COLOR or GL_ONE_MINUS_SRC1_ALPHA.
       */
      BlendMode&
      func_dst_rgb(GLenum v) { m_data[Kfunc_dst_rgb] = v; return *this; }

      /*!
        The value for the argument to feed for the destination coefficient
        RGB to feed to glBlendFuncSeparate, as set by
        func_dst_rgb(GLenum).
       */
      GLenum
      func_dst_rgb(void) const { return m_data[Kfunc_dst_rgb]; }

      /*!
        Set the argument to feed for the destination coefficient Alpha
        value to feed glBlendFuncSeparate, should be one of
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
        GL_SRC_ALPHA_SATURATE, GL_SRC1_COLOR, GL_SRC1_ALPHA,
        GL_ONE_MINUS_SRC1_COLOR or GL_ONE_MINUS_SRC1_ALPHA.
       */
      BlendMode&
      func_dst_alpha(GLenum v) { m_data[Kfunc_dst_alpha] = v; return *this; }

      /*!
        The value for the argument to feed for the destination coefficient
        Alpha to feed to glBlendFuncSeparate, as set by
        func_dst_alpha(GLenum).
       */
      GLenum
      func_dst_alpha(void) const { return m_data[Kfunc_dst_alpha]; }

      /*!
        Provided as a conveniance, equivalent to
        \code
        func_dst_rgb(v);
        func_dst_alpha(v);
        \endcode
       */
      BlendMode&
      func_dst(GLenum v)
      {
        m_data[Kfunc_dst_rgb] = v;
        m_data[Kfunc_dst_alpha] = v;
        return *this;
      }

      /*!
        Comparison operator for sorting of blend modes
        \param rhs value to which to compare
       */
      bool
      operator<(const BlendMode &rhs) const
      {
        return m_data < rhs.m_data;
      }

      /*!
        Comparison operator to check for equality of blend modes
        \param rhs value to which to compare
       */
      bool
      operator==(const BlendMode &rhs) const
      {
        return m_data == rhs.m_data;
      }

    private:
      enum
        {
          Kequation_rgb,
          Kequation_alpha,
          Kfunc_src_rgb,
          Kfunc_src_alpha,
          Kfunc_dst_rgb,
          Kfunc_dst_alpha,
          Knumber_args
        };

      vecN<GLenum, Knumber_args> m_data;
    };

    /*!
      A PainterBlendShaderGL is a GL blend mode
      together with GLSL source code -fragment- to perform
      blending.

      The shader must implement the function
      \code
        void
        fastuidraw_gl_compute_blend_factors(in vec4 in_src, out vec4 out_src0, out vec4 out_src1)
      \endcode

      where in_src is the pre-multiplied alpha value of the incoming fragment,
      out_src0 is the vec4 color value fed to GL for blending corresponding to GL_SRC_COLOR
      and out_src1 is the vec4 color value fed to GL for blending corresponding to GL_SRC1_COLOR.
     */
    class PainterBlendShaderGL:public PainterShader
    {
    public:
      /*!
        Overload typedef for handle
      */
      typedef reference_counted_ptr<PainterBlendShaderGL> handle;

      /*!
        Overload typedef for const_handle
      */
      typedef reference_counted_ptr<const PainterBlendShaderGL> const_handle;

      /*!
        Ctor.
        \param src GLSL source for blending source code fragment
        \param blend_mode BlendMode for shader
       */
      PainterBlendShaderGL(const Shader::shader_source &src,
                           const BlendMode &blend_mode);

      ~PainterBlendShaderGL();

      /*!
        Return the GLSL source of the shader
       */
      const Shader::shader_source&
      src(void) const;

      /*!
        Return the blend mode of shader.
       */
      const BlendMode&
      blend_mode(void) const;

    private:
      void *m_d;
    };
  }
/*! @} */
}
