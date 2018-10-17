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

enum glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_t
compute_provide_auxiliary_buffer(enum glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_t in_value,
                                 const ContextProperties &ctx)
{
  if (in_value == glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer)
    {
      return in_value;
    }

  /* If asking for auxiliary_buffer_framebuffer_fetch and have
   * the extension, immediately give the return value, otherwise
   * fall back to interlock. Note that we do NOT fallback
   * from interlock to framebuffer fetch because framebuffer-fetch
   * makes MSAA render targets become shaded per-sample.
   */
  if (in_value == glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_framebuffer_fetch)
    {
      if (ctx.has_extension("GL_EXT_shader_framebuffer_fetch"))
        {
          return in_value;
        }
      else
        {
          in_value = glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock;
        }
    }

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (ctx.version() <= ivec2(3, 0))
        {
          return glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer;
        }

      if (in_value == glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_atomic)
        {
          return in_value;
        }

      if (ctx.has_extension("GL_NV_fragment_shader_interlock")
          && ctx.has_extension("GL_NV_image_formats"))
        {
          return glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock_main_only;
        }
      else
        {
          return glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_atomic;
        }
    }
  #else
    {
      bool have_interlock, have_interlock_main;

      if (ctx.version() <= ivec2(4, 1) && !ctx.has_extension("GL_ARB_shader_image_load_store"))
        {
          return glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer;
        }

      if (in_value == glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_atomic)
        {
          return in_value;
        }

      have_interlock = ctx.has_extension("GL_INTEL_fragment_shader_ordering");
      have_interlock_main = ctx.has_extension("GL_ARB_fragment_shader_interlock")
        || ctx.has_extension("GL_NV_fragment_shader_interlock");

      if (!have_interlock && !have_interlock_main)
        {
          return glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_atomic;
        }

      switch(in_value)
        {
        case glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock_main_only:
          return (have_interlock_main) ?
            glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock_main_only:
            glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock;

        case glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock:
          return (have_interlock) ?
            glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock:
            glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock_main_only;

        default:
          return glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_atomic;
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

enum glsl::PainterShaderRegistrarGLSL::compositing_type_t
compute_compositing_type(enum glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_t aux_value,
                         enum interlock_type_t interlock_value,
                         enum glsl::PainterShaderRegistrarGLSL::compositing_type_t in_value,
                         const ContextProperties &ctx)
{
  /*
   * First fallback to compositing_framebuffer_fetch if interlock is
   * requested but not availabe.
   */
  if (interlock_value == no_interlock
      && in_value == glsl::PainterShaderRegistrarGLSL::compositing_interlock)
    {
      in_value = glsl::PainterShaderRegistrarGLSL::compositing_framebuffer_fetch;
    }

  if (aux_value == glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_framebuffer_fetch)
    {
      /*
       * auxiliary framebuffer fetch cannot be used with single and
       * dual source compositing; if it is single or dual source compositing
       * we will set it to framebuffer_fetch.
       */
      if (in_value == glsl::PainterShaderRegistrarGLSL::compositing_single_src
          || in_value == glsl::PainterShaderRegistrarGLSL::compositing_dual_src)
        {
          in_value = glsl::PainterShaderRegistrarGLSL::compositing_framebuffer_fetch;
        }
    }

  bool have_dual_src_compositing, have_framebuffer_fetch;
  if (ctx.is_es())
    {
      have_dual_src_compositing = ctx.has_extension("GL_EXT_composite_func_extended");
    }
  else
    {
      have_dual_src_compositing = true;
    }
  have_framebuffer_fetch = (aux_value == glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_framebuffer_fetch)
    || ctx.has_extension("GL_EXT_shader_framebuffer_fetch");

  if (in_value == glsl::PainterShaderRegistrarGLSL::compositing_framebuffer_fetch
      && !have_framebuffer_fetch)
    {
      in_value = glsl::PainterShaderRegistrarGLSL::compositing_interlock;
    }

  /* we do the test again against interlock because framebuffer
   * fetch code may have fallen back to interlock, but now
   * lacking interlock falls back to compositing_dual_src.
   */
  if (interlock_value == no_interlock
      && in_value == glsl::PainterShaderRegistrarGLSL::compositing_interlock)
    {
      in_value = glsl::PainterShaderRegistrarGLSL::compositing_dual_src;
    }

  if (in_value == glsl::PainterShaderRegistrarGLSL::compositing_dual_src
      && !have_dual_src_compositing)
    {
      in_value = glsl::PainterShaderRegistrarGLSL::compositing_single_src;
    }

  return in_value;
}

enum glsl::PainterShaderRegistrarGLSL::clipping_type_t
compute_clipping_type(enum glsl::PainterShaderRegistrarGLSL::compositing_type_t compositing_type,
                      enum glsl::PainterShaderRegistrarGLSL::clipping_type_t in_value,
                      const ContextProperties &ctx,
                      bool allow_gl_clip_distance)
{
  bool clip_distance_supported, skip_color_write_supported;
  skip_color_write_supported =
    compositing_type == glsl::PainterShaderRegistrarGLSL::compositing_framebuffer_fetch
    || compositing_type == glsl::PainterShaderRegistrarGLSL::compositing_interlock;

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
