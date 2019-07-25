/*!
 * \file painter_backend_gl_config.hpp
 * \brief file painter_backend_gl_config.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef FASTUIDRAW_PAINTER_BACKEND_GL_CONFIG_HPP
#define FASTUIDRAW_PAINTER_BACKEND_GL_CONFIG_HPP

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>

namespace fastuidraw { namespace gl { namespace detail {

enum interlock_type_t
  {
    intel_fragment_shader_ordering,
    nv_fragment_shader_interlock,
    arb_fragment_shader_interlock,
    no_interlock
  };

bool
shader_storage_buffers_supported(const ContextProperties &ctx);

enum interlock_type_t
compute_interlock_type(const ContextProperties &ctx);

enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t
compute_fbf_blending_type(enum interlock_type_t interlock_value,
                          enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t in_value,
                          const ContextProperties &ctx);

enum PainterBlendShader::shader_type
compute_preferred_blending_type(enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t fbf,
                                enum PainterBlendShader::shader_type in_value,
                                bool &out_have_dual_src_blending,
                                const ContextProperties &ctx);

enum glsl::PainterShaderRegistrarGLSL::clipping_type_t
compute_clipping_type(enum glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_blending_type,
                      enum glsl::PainterShaderRegistrarGLSL::clipping_type_t in_value,
                      const ContextProperties &ctx,
                      bool allow_gl_clip_distance = true);

}}}

#endif
