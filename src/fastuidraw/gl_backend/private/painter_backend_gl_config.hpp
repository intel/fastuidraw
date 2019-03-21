/*!
 * \file painter_backend_gl_config.hpp
 * \brief file painter_backend_gl_config.hpp
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

enum glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_t
compute_provide_immediate_coverage_buffer(enum glsl::PainterShaderRegistrarGLSL::immediate_coverage_buffer_t in_value,
                                 const ContextProperties &ctx);

enum interlock_type_t
compute_interlock_type(const ContextProperties &ctx);

enum glsl::PainterShaderRegistrarGLSL::blending_type_t
compute_blending_type(enum interlock_type_t interlock_value,
                         enum glsl::PainterShaderRegistrarGLSL::blending_type_t in_value,
                         const ContextProperties &ctx);

enum glsl::PainterShaderRegistrarGLSL::clipping_type_t
compute_clipping_type(enum glsl::PainterShaderRegistrarGLSL::blending_type_t blending_type,
                      enum glsl::PainterShaderRegistrarGLSL::clipping_type_t in_value,
                      const ContextProperties &ctx,
                      bool allow_gl_clip_distance = true);

}}}
