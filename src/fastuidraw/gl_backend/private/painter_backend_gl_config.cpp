/*!
 * \file painter_backend_gl_config.cpp
 * \brief file painter_backend_gl_config.cpp
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

#include "painter_backend_gl_config.hpp"

namespace fastuidraw { namespace gl { namespace detail {

bool
shader_storage_buffers_supported(const ContextProperties &ctx)
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      return ctx.version() >= ivec2(3, 1);
    }
  #else
    {
      return ctx.version() >= ivec2(4, 3)
        || ctx.has_extension("GL_ARB_shader_storage_buffer_object");
    }
  #endif
}

enum glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_t
compute_provide_immediate_coverage_buffer(enum glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_t in_value,
                                          const ContextProperties &ctx)
{
  if (in_value == glsl::PainterShaderRegistrarGLSL::no_immediate_coverage_buffer)
    {
      return in_value;
    }

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (ctx.version() <= ivec2(3, 0))
        {
          return glsl::PainterShaderRegistrarGLSL::no_immediate_coverage_buffer;
        }

      if (in_value == glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_atomic)
        {
          return in_value;
        }

      if (ctx.has_extension("GL_NV_fragment_shader_interlock")
          && ctx.has_extension("GL_NV_image_formats"))
        {
          return glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_interlock_main_only;
        }
      else
        {
          return glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_atomic;
        }
    }
  #else
    {
      bool have_interlock, have_interlock_main;

      if (ctx.version() <= ivec2(4, 1) && !ctx.has_extension("GL_ARB_shader_image_load_store"))
        {
          return glsl::PainterShaderRegistrarGLSL::no_immediate_coverage_buffer;
        }

      if (in_value == glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_atomic)
        {
          return in_value;
        }

      have_interlock = ctx.has_extension("GL_INTEL_fragment_shader_ordering");
      have_interlock_main = ctx.has_extension("GL_ARB_fragment_shader_interlock")
        || ctx.has_extension("GL_NV_fragment_shader_interlock");

      if (have_interlock_main && !have_interlock)
        {
          std::string vendor, device;

          /* Intel on Mesa treats beginInvocationInterlockARB
           * the same as beginFragmentShaderOrderingINTEL;
           * the upshot being that we can get avoid the stall
           * (potentially) that is hit if the shader does
           * not use the auxiliary buffer.
           *
           * WARNING: this is a hack that may stop working
           * if Mesa's GLSL front-end enforces the rules
           * or the ARB-extension. The better thing would be
           * if Mesa did not revert the patch providing
           * support for GL_INTEL_fragment_shader_ordering
           */
          vendor = reinterpret_cast<const char*>(fastuidraw_glGetString(GL_VENDOR));
          device = reinterpret_cast<const char*>(fastuidraw_glGetString(GL_RENDERER));
          have_interlock = vendor.find("Intel") != std::string::npos
            && device.find("Mesa") != std::string::npos;
        }

      if (!have_interlock && !have_interlock_main)
        {
          return glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_atomic;
        }

      switch(in_value)
        {
        case glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_interlock_main_only:
          return (have_interlock_main) ?
            glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_interlock_main_only:
            glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_interlock;

        case glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_interlock:
          return (have_interlock) ?
            glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_interlock:
            glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_interlock_main_only;

        default:
          return glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_atomic;
        }
    }
  #endif
}

enum gl::detail::interlock_type_t
compute_interlock_type(const ContextProperties &ctx)
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (ctx.has_extension("GL_NV_fragment_shader_interlock"))
        {
          return nv_fragment_shader_interlock;
        }
      else
        {
          return no_interlock;
        }
    }
  #else
    {
      if (ctx.has_extension("GL_INTEL_fragment_shader_ordering"))
        {
          return intel_fragment_shader_ordering;
        }
      else if (ctx.has_extension("GL_ARB_fragment_shader_interlock"))
        {
          return arb_fragment_shader_interlock;
        }
      else if (ctx.has_extension("GL_NV_fragment_shader_interlock"))
        {
          return nv_fragment_shader_interlock;
        }
      else
        {
          return no_interlock;
        }
    }
  #endif
}

enum PainterBlendShader::shader_type
compute_preferred_blending_type(enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_type,
                                enum PainterBlendShader::shader_type in_value,
                                bool &have_dual_src_blending,
                                const ContextProperties &ctx)
{
  bool have_framebuffer_fetch;

  have_framebuffer_fetch = (fbf_type != glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported);
  if (have_framebuffer_fetch && in_value == PainterBlendShader::framebuffer_fetch)
    {
      return in_value;
    }
  else
    {
      in_value = PainterBlendShader::dual_src;
    }

  if (ctx.is_es())
    {
      have_dual_src_blending = ctx.has_extension("GL_EXT_blend_func_extended");
    }
  else
    {
      have_dual_src_blending = true;
    }

  if (have_dual_src_blending && in_value == PainterBlendShader::dual_src)
    {
      return in_value;
    }

  return PainterBlendShader::single_src;
}

enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t
compute_fbf_blending_type(enum interlock_type_t interlock_value,
                          enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t in_value,
                          const ContextProperties &ctx)
{
  bool have_framebuffer_fetch;
  have_framebuffer_fetch = ctx.has_extension("GL_EXT_shader_framebuffer_fetch");

  if (interlock_value == no_interlock
      && in_value == glsl::PainterShaderRegistrarGLSL::fbf_blending_interlock)
    {
      if (have_framebuffer_fetch)
        {
          in_value = glsl::PainterShaderRegistrarGLSL::fbf_blending_framebuffer_fetch;
        }
      else
        {
          in_value = glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported;
        }
    }

  if (in_value == glsl::PainterShaderRegistrarGLSL::fbf_blending_framebuffer_fetch
      && !have_framebuffer_fetch)
    {
      if (interlock_value != no_interlock)
        {
          in_value = glsl::PainterShaderRegistrarGLSL::fbf_blending_interlock;
        }
      else
        {
          in_value = glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported;
        }
    }

  return in_value;
}

enum glsl::PainterShaderRegistrarGLSL::clipping_type_t
compute_clipping_type(enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_blending_type,
                      enum glsl::PainterShaderRegistrarGLSL::clipping_type_t in_value,
                      const ContextProperties &ctx,
                      bool allow_gl_clip_distance)
{
  bool clip_distance_supported, skip_color_write_supported;
  skip_color_write_supported =
    fbf_blending_type != glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported;

  if (in_value == glsl::PainterShaderRegistrarGLSL::clipping_via_discard)
    {
      return in_value;
    }

  if (in_value == glsl::PainterShaderRegistrarGLSL::clipping_via_skip_color_write)
    {
      if (skip_color_write_supported)
        {
          return in_value;
        }
      else
        {
          in_value = glsl::PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance;
        }
    }

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (allow_gl_clip_distance)
        {
          clip_distance_supported = ctx.has_extension("GL_EXT_clip_cull_distance")
            || ctx.has_extension("GL_APPLE_clip_distance");
        }
      else
        {
          clip_distance_supported = false;
        }
    }
  #else
    {
      FASTUIDRAWunused(ctx);
      clip_distance_supported = allow_gl_clip_distance;
    }
  #endif

  if (in_value == glsl::PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
    {
      if (clip_distance_supported)
        {
          return in_value;
        }
      else if (skip_color_write_supported)
        {
          in_value = glsl::PainterShaderRegistrarGLSL::clipping_via_skip_color_write;
        }
      else
        {
          in_value = glsl::PainterShaderRegistrarGLSL::clipping_via_discard;
        }
    }

  return in_value;
}

}}}
