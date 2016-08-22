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
#include <fastuidraw/glsl/shader_source.hpp>

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
      /*!
        Enumeration to specify blend equation, i.e. glBlendEquation.
       */
      enum op_t
        {
          ADD,
          SUBTRACT,
          REVERSE_SUBTRACT,
          MIN,
          MAX,
        };

      /*!
        Enumeration to specify the blend coefficient factor,
        i.e. glBlendFunc.
       */
      enum func_t
        {
          ZERO,
          ONE,
          SRC_COLOR,
          ONE_MINUS_SRC_COLOR,
          DST_COLOR,
          ONE_MINUS_DST_COLOR,
          SRC_ALPHA,
          ONE_MINUS_SRC_ALPHA,
          DST_ALPHA,
          ONE_MINUS_DST_ALPHA,
          CONSTANT_COLOR,
          ONE_MINUS_CONSTANT_COLOR,
          CONSTANT_ALPHA,
          ONE_MINUS_CONSTANT_ALPHA,
          SRC_ALPHA_SATURATE,
          SRC1_COLOR,
          ONE_MINUS_SRC1_COLOR,
          SRC1_ALPHA,
          ONE_MINUS_SRC1_ALPHA,
        };

      /*!
        Ctor.
       */
      BlendMode(void)
      {
        m_blend_equation[Kequation_rgb] = m_blend_equation[Kequation_alpha] = ADD;
        m_blend_func[Kfunc_src_rgb] = m_blend_func[Kfunc_src_alpha] = ONE;
        m_blend_func[Kfunc_dst_rgb] = m_blend_func[Kfunc_dst_alpha] = ZERO;
      }

      /*!
        Set the blend equation for the RGB channels.
        Default value is ADD.
       */
      BlendMode&
      equation_rgb(enum op_t v) { m_blend_equation[Kequation_rgb] = v; return *this; }

      /*!
        Return the value as set by equation_rgb(enum op_t).
       */
      enum op_t
      equation_rgb(void) const { return m_blend_equation[Kequation_rgb]; }

      /*!
        Set the blend equation for the Alpha channel.
        Default value is ADD.
       */
      BlendMode&
      equation_alpha(enum op_t v) { m_blend_equation[Kequation_alpha] = v; return *this; }

      /*!
        Return the value as set by equation_alpha(enum op_t).
       */
      enum op_t
      equation_alpha(void) const { return m_blend_equation[Kequation_alpha]; }

      /*!
        Provided as a conveniance, equivalent to
        \code
        equation_rgb(v);
        equation_alpha(v);
        \endcode
       */
      BlendMode&
      equation(enum op_t v)
      {
        m_blend_equation[Kequation_rgb] = v;
        m_blend_equation[Kequation_alpha] = v;
        return *this;
      }

      /*!
        Set the source coefficient for the RGB channels.
        Default value is ONE.
       */
      BlendMode&
      func_src_rgb(enum func_t v) { m_blend_func[Kfunc_src_rgb] = v; return *this; }

      /*!
        Return the value as set by func_src_rgb(enum t).
       */
      enum func_t
      func_src_rgb(void) const { return m_blend_func[Kfunc_src_rgb]; }

      /*!
        Set the source coefficient for the Alpha channel.
        Default value is ONE.
       */
      BlendMode&
      func_src_alpha(enum func_t v) { m_blend_func[Kfunc_src_alpha] = v; return *this; }

      /*!
        Return the value as set by func_src_alpha(enum t).
       */
      enum func_t
      func_src_alpha(void) const { return m_blend_func[Kfunc_src_alpha]; }

      /*!
        Provided as a conveniance, equivalent to
        \code
        func_src_rgb(v);
        func_src_alpha(v);
        \endcode
       */
      BlendMode&
      func_src(enum func_t v)
      {
        m_blend_func[Kfunc_src_rgb] = m_blend_func[Kfunc_src_alpha] = v;
        return *this;
      }

      /*!
        Set the destication coefficient for the RGB channels.
        Default value is ZERO.
       */
      BlendMode&
      func_dst_rgb(enum func_t v) { m_blend_func[Kfunc_dst_rgb] = v; return *this; }

      /*!
        Return the value as set by func_dst_rgb(enum t).
       */
      enum func_t
      func_dst_rgb(void) const { return m_blend_func[Kfunc_dst_rgb]; }

      /*!
        Set the destication coefficient for the Alpha channel.
        Default value is ZERO.
       */
      BlendMode&
      func_dst_alpha(enum func_t v) { m_blend_func[Kfunc_dst_alpha] = v; return *this; }

      /*!
        Return the value as set by func_dst_alpha(enum t).
       */
      enum func_t
      func_dst_alpha(void) const { return m_blend_func[Kfunc_dst_alpha]; }

      /*!
        Provided as a conveniance, equivalent to
        \code
        func_dst_rgb(v);
        func_dst_alpha(v);
        \endcode
       */
      BlendMode&
      func_dst(enum func_t v)
      {
        m_blend_func[Kfunc_dst_rgb] = m_blend_func[Kfunc_dst_alpha] = v;
        return *this;
      }

      /*!
        Provided as a conveniance, equivalent to
        \code
        func_src(src);
        func_dst(dst);
        \endcode
       */
      BlendMode&
      func(enum func_t src, enum func_t dst)
      {
        m_blend_func[Kfunc_src_rgb] = m_blend_func[Kfunc_src_alpha] = src;
        m_blend_func[Kfunc_dst_rgb] = m_blend_func[Kfunc_dst_alpha] = dst;
        return *this;
      }

      /*!
        Comparison operator for sorting of blend modes
        \param rhs value to which to compare
       */
      bool
      operator<(const BlendMode &rhs) const
      {
        return m_blend_equation < rhs.m_blend_equation
          || (m_blend_equation == rhs.m_blend_equation && m_blend_func < rhs.m_blend_func);
      }

      /*!
        Comparison operator to check for equality of blend modes
        \param rhs value to which to compare
       */
      bool
      operator==(const BlendMode &rhs) const
      {
        return m_blend_equation == rhs.m_blend_equation
          && m_blend_func == rhs.m_blend_func;
      }

    private:
      enum
        {
          Kequation_rgb,
          Kequation_alpha,
          Knumber_blend_equation_args
        };

      enum
        {
          Kfunc_src_rgb,
          Kfunc_src_alpha,
          Kfunc_dst_rgb,
          Kfunc_dst_alpha,
          Knumber_blend_args
        };

      vecN<enum op_t, Knumber_blend_equation_args> m_blend_equation;
      vecN<enum func_t, Knumber_blend_args> m_blend_func;
    };

    /*!
      A BlendShaderSourceCode represents a shareable code to be used
      by multiple \ref PainterBlendShaderGL objects. A fixed
      BlendShaderSourceCode object can be used in multiple
      PainterBlendShaderGL even if the associated BlendMode is
      different. As an example, if one is using single source
      blending (see \ref SingleSourceBlenderShader) then the
      same GLSL shader code is used for all Porter-Duff modes
      because the GL blend mode handles all of the actual blending.
     */
    class BlendShaderSourceCode:
      public reference_counted<BlendShaderSourceCode>::default_base
    {
    public:
      /*!
        Ctor.
        \param psrc GLSL source code fragment
        \param num_sub_shaders number of sub-shaders the code supports
       */
      explicit
      BlendShaderSourceCode(const glsl::ShaderSource &psrc,
                            unsigned int num_sub_shaders = 1);

      ~BlendShaderSourceCode();

      /*!
        Returns the shader source code
        of the BlendShaderSourceCode.
       */
      const glsl::ShaderSource&
      shader_src(void) const;

      /*!
        Returns the number of sub-shaders the
        BlendShaderSourceCode supports.
       */
      unsigned int
      number_sub_shaders(void) const;

      /*!
        The GLSL shader ID for the BlendShaderSourceCode. This value
        is not assigned until the BlendShaderSourceCode is registered
        to a PainterBackendGL.
       */
      uint32_t
      ID(void) const;

      /*!
        The PainterBackendGL to which the BlendShaderSourceCode is
        registered. A BlendShaderSourceCode is registered as necessary
        by PainterBackendGL when a PainterBlendShaderGL that uses
        the BlendShaderSourceCode is registered to a PainterBackendGL.
       */
      const PainterBackend*
      registered_to(void) const;

      /*!
        Register the BlendShaderSourceCode to a PainterBackend.
        This function is called automatically by PainterBackendGL
        the first time a PainterBlendShaderGL needs to use the
        BlendShaderSourceCode. A BlendShaderSourceCode may only
        be registered once.
        \param pshader_id value to be returned by ID(void) const
        \param backend value to be returned by registered_to(void) const
       */
      void
      register_shader_code(uint32_t pshader_id,
                           const PainterBackend *backend);
    private:
      void *m_d;
    };

    /*!
      A SingleSourceBlenderShader gives GLSL code and GL API
      blend mode for blend shading.
     */
    class SingleSourceBlenderShader
    {
    public:
      /*!
        Ctor. Intializes \ref m_src with a BlendShaderSourceCode
        constructed directly from a \ref glsl::ShaderSource
        and sets \ref m_sub_shader to 0.
        \param md value with which to initialize \ref m_blend_mode
        \param src source code with which construct the BlendShaderSourceCode object
                   to be held by \ref m_src
       */
      SingleSourceBlenderShader(const BlendMode &md,
                                const glsl::ShaderSource &src):
        m_blend_mode(md), m_sub_shader(0)
      {
        m_src = FASTUIDRAWnew BlendShaderSourceCode(src, 1);
      }

      /*!
        Ctor to initialize members.
        \param md value with which to initialize \ref m_blend_mode
        \param src value with which to initialize \ref m_src
        \param sub_shader value with which to initialize \ref m_sub_shader
       */
      SingleSourceBlenderShader(const BlendMode &md,
                                const reference_counted_ptr<BlendShaderSourceCode> &src,
                                unsigned int sub_shader = 0):
        m_blend_mode(md), m_src(src), m_sub_shader(sub_shader)
      {}
      
      /*!
        Provides the BlendMode to be used with the shader.
       */
      BlendMode m_blend_mode;

      /*!
        Provides the GLSL code fragment for a SingleSourceBlenderShader.
        Must provide the function
        \code
        void
        fastuidraw_gl_compute_blend_value(in uint sub_shader, in uint blend_shader_data_location,
                                          in vec4 in_src, out vec4 out_src)
        \endcode
        where in_src is the pre-multiplied by alpha color value for the
        fragment and out_src is the value for the fragment shader to emit.
        The same globals available to a fragment shader in PainterItemShaderGL
        are also avalailable to the blend shader.
       */
      reference_counted_ptr<BlendShaderSourceCode> m_src;

      /*!
        The sub-shader that the SingleSourceBlenderShader will use
        from the BlendShaderSourceCode \ref m_src to shade.
       */
      unsigned int m_sub_shader;
    };

    /*!
      A DualSourceBlenderShader gives GLSL code and GL API
      blend mode for blend shading when GL implementation
      support dual source blending. That functionality is
      available as follows:
      - GL available in GL starting at version 3.2 and also
        via the extension GL_ARB_blend_func_extended
      - GLES available in the extension GL_EXT_blend_func_extended
     */
    class DualSourceBlenderShader
    {
    public:

      /*!
        Ctor. Intializes \ref m_src with a BlendShaderSourceCode
        constructed directly from a \ref glsl::ShaderSource
        and sets \ref m_sub_shader to 0.
        \param md value with which to initialize \ref m_blend_mode
        \param src source code with which construct the BlendShaderSourceCode object
                   to be held by \ref m_src
       */
      DualSourceBlenderShader(const BlendMode &md,
                              const glsl::ShaderSource &src):
        m_blend_mode(md), m_sub_shader(0)
      {
        m_src = FASTUIDRAWnew BlendShaderSourceCode(src, 1);
      }

      /*!
        Ctor to initialize \ref m_src and \ref m_blend_mode.
        \param md value with which to initialize \ref m_blend_mode
        \param src value with which to initialize \ref m_src
        \param sub_shader value with which to initialize \ref m_sub_shader
       */
      DualSourceBlenderShader(const BlendMode &md,
                              const reference_counted_ptr<BlendShaderSourceCode> &src,
                              unsigned int sub_shader = 0):
        m_blend_mode(md), m_src(src), m_sub_shader(sub_shader)
      {}
      
      /*!
        Provides the BlendMode to be used with the shader.
       */
      BlendMode m_blend_mode;

      /*!
        Provides the GLSL code fragment for a DualSourceBlenderShader.
        Must provide the function
        \code
        void
        fastuidraw_gl_compute_blend_factors(in uint sub_shader, in uint blend_shader_data_location,
                                            in vec4 in_src, out vec4 out_src0, out vec4 out_src1)
        \endcode
        where in_src is the pre-multiplied by alpha color value for the
        fragment, out_src0 is the value for the fragment shader to emit
        for GL_SRC_COLOR and out_src1 is the value for the fragment shader
        to emit value for GL_SRC1_COLOR. The same globals available to a
        fragment shader in PainterItemShaderGL are also avalailable to the
        blend shader.
       */
      reference_counted_ptr<BlendShaderSourceCode> m_src;

      /*!
        The sub-shader that the DualSourceBlenderShader will use
        from the BlendShaderSourceCode \ref m_src to shade.
       */
      unsigned int m_sub_shader;
    };

    /*!
      A FramebufferFetchBlendShader gives GLSL code for blend shading
      that uses the framebuffer fetching to perform shader blending.
     */
    class FramebufferFetchBlendShader
    {
    public:

      /*!
        Ctor. Intializes \ref m_src with a BlendShaderSourceCode
        constructed directly from a \ref glsl::ShaderSource
        and sets \ref m_sub_shader to 0.
        \param md value with which to initialize \ref m_blend_mode
        \param src source code with which construct the BlendShaderSourceCode object
                   to be held by \ref m_src
       */
      explicit
      FramebufferFetchBlendShader(const glsl::ShaderSource &src):
        m_sub_shader(0)
      {
        m_src = FASTUIDRAWnew BlendShaderSourceCode(src, 1);
      }

      /*!
        Ctor to initialize \ref m_src and \ref m_blend_mode.
        \param md value with which to initialize \ref m_blend_mode
        \param src value with which to initialize \ref m_src
        \param sub_shader value with which to initialize \ref m_sub_shader
       */
      explicit
      FramebufferFetchBlendShader(const reference_counted_ptr<BlendShaderSourceCode> &src,
                                  unsigned int sub_shader = 0):
        m_src(src), m_sub_shader(sub_shader)
      {}

      /*!
        Provides the GLSL code fragment for a FramebufferFetchBlendShader.
        Must provide the function
        \code
        void
        fastuidraw_gl_compute_post_blended_value(in uint sub_shader, in uint blend_shader_data_location,
                                                 in vec4 in_src, in vec4 in_fb, out vec4 out_src)
        \endcode
        where in_src is the pre-multiplied by alpha color value for the
        fragment, in_fb is the value of the framebuffer at the location
        and out_src is the value for the fragment shader to emit. The
        same globals available to a fragment shader in PainterItemShaderGL
        are also avalailable to the blend shader.
       */
      reference_counted_ptr<BlendShaderSourceCode> m_src;

      /*!
        The sub-shader that the FramebufferFetchBlendShader will use
        from the BlendShaderSourceCode \ref m_src to shade.
       */
      unsigned int m_sub_shader;
    };

    
    /*!
      A PainterBlendShaderGL is the GL backend's implementation of
      PainterShader for blending. Internally, it is composed of
      a SingleSourceBlenderShader, DualSourceBlenderShader and
      FramebufferFetchBlendShader.
     */
    class PainterBlendShaderGL:public PainterBlendShader
    {
    public:
      /*!
        Ctor.
        \param psingle_src_blender value copied that single_src_blender() returns
        \param pdual_src_blender value copied that dual_src_blender() returns
        \param pfetch_blender value copied that fetch_blender() returns
       */
      PainterBlendShaderGL(const SingleSourceBlenderShader &psingle_src_blender,
                           const DualSourceBlenderShader &pdual_src_blender,
                           const FramebufferFetchBlendShader &pfetch_blender);

      ~PainterBlendShaderGL();

      /*!
        Returns the shader code and blend mode to use when
        performing blending via single source blending.
       */
      const SingleSourceBlenderShader&
      single_src_blender(void) const;

      /*!
        Returns the shader code and blend mode to use when
        performing blending via dual source blending.
       */
      const DualSourceBlenderShader&
      dual_src_blender(void) const;
      
      /*!
        Returns the shader code and blend mode to use when
        performing blending via framebuffer fetch.
       */
      const FramebufferFetchBlendShader&
      fetch_blender(void) const;

    private:
      void *m_d;
    };
/*! @} */
  }
}
