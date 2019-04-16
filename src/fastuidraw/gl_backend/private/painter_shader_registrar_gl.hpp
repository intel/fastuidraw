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

#include <vector>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>

#include "tex_buffer.hpp"
#include "painter_backend_gl.hpp"
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
      program_count = PainterBackendFactoryGL::number_program_types
    };

  typedef reference_counted_ptr<Program> program_ref;
  typedef vecN<program_ref, PainterBackendFactoryGL::number_program_types> programs_per_blend;

  class program_set
  {
  public:
    const programs_per_blend&
    programs(enum PainterBlendShader::shader_type blend_type) const
    {
      return m_item_programs[blend_type];
    }

    const program_ref&
    program(enum PainterBackendFactoryGL::program_type_t tp,
            enum PainterBlendShader::shader_type blend_type) const
    {
      return programs(blend_type)[tp];
    }

    const program_ref&
    program(enum PainterBlendShader::shader_type blend_type,
            enum PainterBackendFactoryGL::program_type_t tp) const
    {
      return programs(blend_type)[tp];
    }

    vecN<programs_per_blend, PainterBlendShader::number_types> m_item_programs;
    program_ref m_deferred_coverage_program;
  };

  class CachedItemPrograms:
    public reference_counted<CachedItemPrograms>::non_concurrent
  {
  public:
    explicit
    CachedItemPrograms(const reference_counted_ptr<PainterShaderRegistrarGL> &reg):
      m_reg(reg),
      m_blend_shader_counts(0)
    {}

    void
    reset(void);

    const program_ref&
    program_of_item_shader(enum PainterSurface::render_type_t render_type,
                           unsigned int shader_group,
                           enum PainterBlendShader::shader_type blend_type);

  private:
    reference_counted_ptr<PainterShaderRegistrarGL> m_reg;
    vecN<unsigned int, PainterBlendShader::number_types> m_blend_shader_counts;
    vecN<std::vector<PainterShaderRegistrarGL::program_ref>, PainterBlendShader::number_types + 1> m_item_programs;
  };

  explicit
  PainterShaderRegistrarGL(const PainterBackendFactoryGL::ConfigurationGL &P,
                           const UberShaderParams &uber_params);

  const program_set&
  programs(void);

  program_ref
  program_of_item_shader(enum PainterSurface::render_type_t render_type,
                         unsigned int shader_group,
                         enum PainterBlendShader::shader_type blend_type);

  const PainterBackendFactoryGL::ConfigurationGL&
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
  set_hints(PainterBackendFactory::PerformanceHints &hints)
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
  bool
  blend_type_supported(enum PainterBlendShader::shader_type) const override;

  uint32_t
  compute_blend_shader_group(PainterShader::Tag tag,
                             const reference_counted_ptr<PainterBlendShader> &shader) override;

  uint32_t
  compute_item_shader_group(PainterShader::Tag tag,
                            const reference_counted_ptr<PainterItemShader> &shader) override;

  uint32_t
  compute_item_coverage_shader_group(PainterShader::Tag tag,
                                     const reference_counted_ptr<PainterItemCoverageShader> &shader) override;

private:
  void
  configure_backend(void);

  void
  configure_source_front_matter(void);

  void
  build_programs(void);

  program_ref
  build_program(enum PainterBackendFactoryGL::program_type_t tp,
                enum PainterBlendShader::shader_type blend_type);

  program_ref
  build_deferred_coverage_program(void);

  program_ref
  build_program_of_item_shader(unsigned int shader, bool allow_discard,
                               enum PainterBlendShader::shader_type blend_type);

  static
  program_ref*
  resize_item_shader_vector_as_needed(enum PainterSurface::render_type_t prender_type,
                                      unsigned int shader_group,
                                      enum PainterBlendShader::shader_type blend_type,
                                      vecN<std::vector<program_ref>, PainterBlendShader::number_types + 1> &elements);

  program_ref
  build_program_of_coverage_item_shader(unsigned int shader);

  PainterBackendFactoryGL::ConfigurationGL m_params;
  UberShaderParams m_uber_shader_builder_params;
  enum interlock_type_t m_interlock_type;
  BackendConstants m_backend_constants;

  std::string m_gles_clip_plane_extension;
  PreLinkActionArray m_attribute_binder;
  ProgramInitializerArray m_initializer;
  glsl::ShaderSource m_front_matter_vert;
  glsl::ShaderSource m_front_matter_frag;
  unsigned int m_number_shaders_in_program;
  vecN<unsigned int, 3> m_number_blend_shaders_in_item_programs;
  program_set m_programs;
  vecN<std::vector<program_ref>, PainterBlendShader::number_types + 1> m_item_programs;

  ContextProperties m_ctx_properties;
  enum tex_buffer_support_t m_tex_buffer_support;
  int m_number_clip_planes;
  bool m_has_multi_draw_elements;
};

}}}
