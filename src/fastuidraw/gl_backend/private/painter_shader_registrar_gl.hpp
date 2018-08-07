/*!
 * \file painter_shader_registrar_gl.hpp
 * \brief file painter_shader_registrar_gl.hpp
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

#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>

#include "tex_buffer.hpp"
#include "painter_backend_gl_config.hpp"

namespace fastuidraw { namespace gl { namespace detail {

class PainterShaderRegistrarGL:public glsl::PainterShaderRegistrarGLSL
{
public:
  enum
    {
      shader_group_discard_bit = 31u,
      shader_group_discard_mask = (1u << 31u)
    };

  enum
    {
      program_count = PainterBackendGL::number_program_types
    };

  typedef reference_counted_ptr<Program> program_ref;
  typedef vecN<program_ref, program_count + 1> program_set;

  explicit
  PainterShaderRegistrarGL(const PainterBackendGL::ConfigurationGL &P,
                           const UberShaderParams &uber_params);

  program_set
  programs(void);

  const PainterBackendGL::ConfigurationGL&
  params(void) const
  {
    return m_params;
  }

  enum tex_buffer_support_t
  tex_buffer_support(void) const
  {
    return m_tex_buffer_support;
  }

  const UberShaderParams&
  uber_shader_builder_params(void) const
  {
    return m_uber_shader_builder_params;
  }

  unsigned int
  number_clip_planes(void) const
  {
    return m_number_clip_planes;
  }

  bool
  has_multi_draw_elements(void) const
  {
    return m_has_multi_draw_elements;
  }

  void
  set_hints(PainterBackend::PerformanceHints &hints)
  {
    /* should this instead be clipping_type() != clipping_via_discard ?
     * On one hand, letting the GPU do the virtual no-write incurs no CPU load,
     * but a per-pixel load that can be avoided by CPU-clipping. On the other
     * hand, making the CPU do as little as possble is one of FastUIDraw's
     * sub-goals.
     */
    hints.clipping_via_hw_clip_planes(m_params.clipping_type() == clipping_via_gl_clip_distance);
  }

protected:
  uint32_t
  compute_composite_shader_group(PainterShader::Tag tag,
                             const reference_counted_ptr<PainterCompositeShader> &shader);

  uint32_t
  compute_item_shader_group(PainterShader::Tag tag,
                            const reference_counted_ptr<PainterItemShader> &shader);

private:
  void
  configure_backend(void);

  void
  configure_source_front_matter(void);

  void
  build_programs(void);

  program_ref
  build_program(enum PainterBackendGL::program_type_t tp);

  PainterBackendGL::ConfigurationGL m_params;
  UberShaderParams m_uber_shader_builder_params;
  enum interlock_type_t m_interlock_type;
  BackendConstants m_backend_constants;

  std::string m_gles_clip_plane_extension;
  PreLinkActionArray m_attribute_binder;
  ProgramInitializerArray m_initializer;
  glsl::ShaderSource m_front_matter_vert;
  glsl::ShaderSource m_front_matter_frag;
  unsigned int m_number_shaders_in_program;
  program_set m_programs;

  ContextProperties m_ctx_properties;
  enum tex_buffer_support_t m_tex_buffer_support;
  int m_number_clip_planes;
  bool m_has_multi_draw_elements;
};

}}}
