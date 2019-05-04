/*!
 * \file painter_shader_registrar_gl.cpp
 * \brief file painter_shader_registrar_gl.cpp
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

#include <sstream>
#include <private/gl_backend/painter_shader_registrar_gl.hpp>
#include <private/gl_backend/glyph_atlas_gl.hpp>

namespace
{
  bool
  use_shader_helper(enum fastuidraw::gl::PainterEngineGL::program_type_t tp,
                    bool uses_discard)
  {
    return tp == fastuidraw::gl::PainterEngineGL::program_all
      || (tp == fastuidraw::gl::PainterEngineGL::program_without_discard && !uses_discard)
      || (tp == fastuidraw::gl::PainterEngineGL::program_with_discard && uses_discard);
  }

  class DiscardItemShaderFilter:public fastuidraw::gl::PainterEngineGL::ShaderFilter<fastuidraw::glsl::PainterItemShaderGLSL>
  {
  public:
    explicit
    DiscardItemShaderFilter(enum fastuidraw::gl::PainterEngineGL::program_type_t tp,
                            enum fastuidraw::gl::PainterEngineGL::clipping_type_t cp):
      m_tp(tp),
      m_cp(cp)
    {}

    bool
    use_shader(const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &shader) const
    {
      using namespace fastuidraw::gl;

      bool uses_discard;
      uses_discard = (m_cp == PainterEngineGL::clipping_via_discard)
        || shader->uses_discard();
      return use_shader_helper(m_tp, uses_discard);
    }

  private:
    enum fastuidraw::gl::PainterEngineGL::program_type_t m_tp;
    enum fastuidraw::gl::PainterEngineGL::clipping_type_t m_cp;
  };
}

//////////////////////////////////////////////////////////////
// fastuidraw::gl::detail::PainterShaderRegistrarGL::CachedItemPrograms methods
void
fastuidraw::gl::detail::PainterShaderRegistrarGL::CachedItemPrograms::
reset(void)
{
  Mutex::Guard m(m_reg->mutex());
  for (unsigned int i = 0; i < PainterBlendShader::number_types; ++i)
    {
      enum PainterBlendShader::shader_type e;

      e = static_cast<enum PainterBlendShader::shader_type>(i);
      unsigned int cnt(m_reg->registered_blend_shader_count(e));
      if (cnt != m_blend_shader_counts[e])
        {
          m_blend_shader_counts[e] = cnt;
          m_item_programs[e].clear();
        }
    }
}

const fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref&
fastuidraw::gl::detail::PainterShaderRegistrarGL::CachedItemPrograms::
program_of_item_shader(enum PainterSurface::render_type_t render_type,
                       unsigned int shader_group,
                       enum PainterBlendShader::shader_type blend_type)
{
  program_ref &dst(*PainterShaderRegistrarGL::resize_item_shader_vector_as_needed(render_type, shader_group,
                                                                                  blend_type, m_item_programs));
  if (!dst)
    {
      dst = m_reg->program_of_item_shader(render_type, shader_group, blend_type);
    }
  return dst;
}

//////////////////////////////////////
// fastuidraw::gl::detail::PainterShaderRegistrarGL methods
fastuidraw::gl::detail::PainterShaderRegistrarGL::
PainterShaderRegistrarGL(const PainterEngineGL::ConfigurationGL &P,
                         const PainterEngineGL::UberShaderParams &uber_params):
  fastuidraw::glsl::PainterShaderRegistrarGLSL(),
  m_params(P),
  m_uber_shader_builder_params(uber_params),
  m_number_shaders_in_program(0),
  m_number_blend_shaders_in_item_programs(0)
{
  configure_backend();
  m_backend_constants
    .set_from_atlas(*m_params.colorstop_atlas())
    .set_from_atlas(*m_params.image_atlas());
  m_scratch_renderer = FASTUIDRAWnew ScratchRenderer();
}

bool
fastuidraw::gl::detail::PainterShaderRegistrarGL::
blend_type_supported(enum PainterBlendShader::shader_type tp) const
{
  switch(tp)
    {
    case PainterBlendShader::single_src:
      return true;

    case PainterBlendShader::dual_src:
      return m_params.support_dual_src_blend_shaders();

    case PainterBlendShader::framebuffer_fetch:
      return m_params.fbf_blending_type() != fbf_blending_not_supported;

    default:
      FASTUIDRAWassert(!"Bad blending_type_t in UberShaderParams");
      return false;
    }
}

uint32_t
fastuidraw::gl::detail::PainterShaderRegistrarGL::
compute_blend_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterBlendShader> &shader)
{
  FASTUIDRAWunused(shader);
  return m_params.break_on_shader_change() ?
    tag.m_ID:
    0u;
}

uint32_t
fastuidraw::gl::detail::PainterShaderRegistrarGL::
compute_item_shader_group(PainterShader::Tag tag,
                          const reference_counted_ptr<PainterItemShader> &shader)
{
  uint32_t return_value;
  bool group_id_is_shader_id;

  group_id_is_shader_id = (!m_params.use_uber_item_shader() || m_params.break_on_shader_change());
  return_value = (group_id_is_shader_id) ? tag.m_ID : 0u;
  return_value |= (shader_group_discard_mask & tag.m_group);

  if (m_params.separate_program_for_discard())
    {
      const glsl::PainterItemShaderGLSL *sh;
      sh = dynamic_cast<const glsl::PainterItemShaderGLSL*>(shader.get());
      if (sh && sh->uses_discard())
        {
          return_value |= shader_group_discard_mask;
        }
    }
  return return_value;
}

uint32_t
fastuidraw::gl::detail::PainterShaderRegistrarGL::
compute_item_coverage_shader_group(PainterShader::Tag tag,
                                   const reference_counted_ptr<PainterItemCoverageShader> &shader)
{
  uint32_t return_value;
  bool group_id_is_shader_id;

  FASTUIDRAWunused(shader);
  group_id_is_shader_id = (!m_params.use_uber_item_shader() || m_params.break_on_shader_change());
  return_value = (group_id_is_shader_id) ? tag.m_ID : 0u;

  return return_value;
}

void
fastuidraw::gl::detail::PainterShaderRegistrarGL::
configure_backend(void)
{
  using namespace fastuidraw::glsl;

  m_tex_buffer_support = compute_tex_buffer_support(m_ctx_properties);
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (m_ctx_properties.has_extension("GL_EXT_clip_cull_distance")
          || m_ctx_properties.has_extension("GL_APPLE_clip_distance"))
        {
          m_number_clip_planes = context_get<GLint>(GL_MAX_CLIP_DISTANCES_EXT);
          m_gles_clip_plane_extension = "GL_EXT_clip_cull_distance";
        }
      else
        {
          m_number_clip_planes = 0;
        }
    }
  #else
    {
      m_number_clip_planes = context_get<GLint>(GL_MAX_CLIP_DISTANCES);
    }
  #endif

  FASTUIDRAWassert(m_number_clip_planes >= 4
                   || m_params.clipping_type() != clipping_via_gl_clip_distance);

 #ifdef FASTUIDRAW_GL_USE_GLES
    {
      m_has_multi_draw_elements = m_ctx_properties.has_extension("GL_EXT_multi_draw_arrays");
    }
  #else
    {
      m_has_multi_draw_elements = true;
    }
  #endif

  m_interlock_type = compute_interlock_type(m_ctx_properties);
  configure_source_front_matter();
}

void
fastuidraw::gl::detail::PainterShaderRegistrarGL::
configure_source_front_matter(void)
{
  using namespace fastuidraw::glsl;
  if (!m_uber_shader_builder_params.assign_binding_points())
    {
      /* TODO: the names of the GLSL uniforms are not publicly documented;
       * we should have an interface in UberShaderParams that gives the
       * GLSL name for each of these uniforms.
       */
      m_initializer
        .add_sampler_initializer("fastuidraw_imageAtlasLinear",
                                 m_uber_shader_builder_params.image_atlas_color_tiles_linear_binding())
        .add_sampler_initializer("fastuidraw_imageAtlasNearest",
                                 m_uber_shader_builder_params.image_atlas_color_tiles_nearest_binding())
        .add_sampler_initializer("fastuidraw_imageIndexAtlas",
                                 m_uber_shader_builder_params.image_atlas_index_tiles_binding())
        .add_sampler_initializer("fastuidraw_colorStopAtlas",
                                 m_uber_shader_builder_params.colorstop_atlas_binding())
        .add_sampler_initializer("fastuidraw_context_texture",
                                 m_uber_shader_builder_params.context_texture_binding())
        .add_sampler_initializer("fastuidraw_deferred_coverage_buffer",
                                 m_uber_shader_builder_params.coverage_buffer_texture_binding())
        .add_uniform_block_binding("fastuidraw_uniform_block",
                                   m_uber_shader_builder_params.uniforms_ubo_binding());

      switch(m_uber_shader_builder_params.data_store_backing())
        {
        case data_store_tbo:
          {
            m_initializer
              .add_sampler_initializer("fastuidraw_painterStore_tbo",
                                       m_uber_shader_builder_params.data_store_buffer_binding());
          }
          break;

        case data_store_ubo:
          {
            m_initializer
              .add_uniform_block_binding("fastuidraw_painterStore_ubo",
                                         m_uber_shader_builder_params.data_store_buffer_binding());
          }
          break;

        case data_store_ssbo:
          {
            #ifndef FASTUIDRAW_GL_USE_GLES
              {
                m_initializer
                  .add(FASTUIDRAWnew ShaderStorageBlockInitializer("fastuidraw_painterStore_ssbo",
                                                                   m_uber_shader_builder_params.data_store_buffer_binding()));
              }
            #endif
          }
          break;
        }

      switch(m_uber_shader_builder_params.glyph_data_backing())
        {
        case glyph_data_tbo:
        case glyph_data_texture_array:
          {
            m_initializer
              .add_sampler_initializer("fastuidraw_glyphDataStore",
                                       m_uber_shader_builder_params.glyph_atlas_store_binding());
          }
          break;
        case glyph_data_ssbo:
          {
            #ifndef FASTUIDRAW_GL_USE_GLES
              {
                m_initializer
                  .add(FASTUIDRAWnew ShaderStorageBlockInitializer("fastuidraw_glyphDataStore",
                                                                   m_uber_shader_builder_params.glyph_atlas_store_binding()));
              }
            #endif
          }
          break;
        }
    }

  if (!m_uber_shader_builder_params.assign_layout_to_vertex_shader_inputs())
    {
      m_attribute_binder
        .add_binding("fastuidraw_attribute0", attribute0_slot)
        .add_binding("fastuidraw_attribute1", attribute1_slot)
        .add_binding("fastuidraw_attribute2", attribute2_slot)
        .add_binding("fastuidraw_header_attribute", header_attrib_slot);
    }

  c_string begin_interlock_fcn(nullptr), end_interlock_fcn(nullptr);
  switch(m_interlock_type)
    {
    case no_interlock:
      begin_interlock_fcn = "fastuidraw_do_nothing";
      end_interlock_fcn = "fastuidraw_do_nothing";
      break;

    case intel_fragment_shader_ordering:
      begin_interlock_fcn = "beginFragmentShaderOrderingINTEL";
      end_interlock_fcn = "fastuidraw_do_nothing";
      break;

    case arb_fragment_shader_interlock:
      begin_interlock_fcn = "beginInvocationInterlockARB";
      end_interlock_fcn = "endInvocationInterlockARB";
      break;

    case nv_fragment_shader_interlock:
      begin_interlock_fcn = "beginInvocationInterlockNV";
      end_interlock_fcn = "endInvocationInterlockNV";
      break;
    }
  FASTUIDRAWassert(begin_interlock_fcn);
  FASTUIDRAWassert(end_interlock_fcn);

  m_front_matter_frag
    .add_macro("fastuidraw_begin_interlock", begin_interlock_fcn)
    .add_macro("fastuidraw_end_interlock", end_interlock_fcn);

  if (m_params.fbf_blending_type() == fbf_blending_interlock)
    {
      /* Only have this front matter present if FASTUIDRAW_DISCARD is empty defined;
       * The issue is that when early_fragment_tests are enabled, then the depth
       * write happens even if the fragment shader hits discard.
       */
      std::ostringstream early_fragment_tests;
      early_fragment_tests << "#ifdef FASTUIDRAW_ALLOW_EARLY_FRAGMENT_TESTS\n"
                           << "layout(early_fragment_tests) in;\n"
                           << "#endif\n";
      m_front_matter_frag.add_source(early_fragment_tests.str().c_str(),
                                     ShaderSource::from_string);
    }

  std::string glsl_version;
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (m_params.clipping_type() == clipping_via_gl_clip_distance)
        {
          m_front_matter_vert.specify_extension(m_gles_clip_plane_extension.c_str(),
                                                ShaderSource::require_extension);
        }

      if (m_ctx_properties.version() >= ivec2(3, 2))
        {
          glsl_version = "320 es";
          m_front_matter_frag
            .specify_extension("GL_EXT_shader_framebuffer_fetch", ShaderSource::enable_extension)
            .specify_extension("GL_EXT_blend_func_extended", ShaderSource::enable_extension)
            .specify_extension("GL_NV_image_formats", ShaderSource::enable_extension);
        }
      else
        {
          if (m_ctx_properties.version() >= ivec2(3, 1))
            {
              glsl_version = "310 es";
            }
          else
            {
              glsl_version = "300 es";
            }

          if (m_uber_shader_builder_params.assign_layout_to_varyings())
            {
              m_front_matter_vert.specify_extension("GL_EXT_separate_shader_objects",
                                                    ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_EXT_separate_shader_objects",
                                                    ShaderSource::require_extension);
            }

          m_front_matter_vert
            .specify_extension("GL_EXT_texture_buffer", ShaderSource::enable_extension)
            .specify_extension("GL_OES_texture_buffer", ShaderSource::enable_extension)
            .specify_extension("GL_NV_image_formats", ShaderSource::enable_extension);

          m_front_matter_frag
            .specify_extension("GL_EXT_shader_framebuffer_fetch", ShaderSource::enable_extension)
            .specify_extension("GL_EXT_blend_func_extended", ShaderSource::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", ShaderSource::enable_extension)
            .specify_extension("GL_OES_texture_buffer", ShaderSource::enable_extension);
        }
      m_front_matter_vert.add_source("fastuidraw_painter_gles_precision.glsl.resource_string",
                                     ShaderSource::from_resource);
      m_front_matter_frag.add_source("fastuidraw_painter_gles_precision.glsl.resource_string",
                                     ShaderSource::from_resource);
    }
  #else
    {
      bool using_glsl42, using_glsl43;
      bool require_ssbo, require_image_load_store;

      require_ssbo = (m_uber_shader_builder_params.data_store_backing() == data_store_ssbo)
        || (m_params.glyph_atlas_params().glyph_data_backing_store_type() == glyph_data_ssbo);

      require_image_load_store = (m_params.fbf_blending_type() == fbf_blending_interlock)
        || require_ssbo;

      using_glsl42 = m_ctx_properties.version() >= ivec2(4, 2)
        && (m_uber_shader_builder_params.assign_layout_to_varyings()
            || m_uber_shader_builder_params.assign_binding_points()
            || require_image_load_store);

      using_glsl43 = using_glsl42
        && m_ctx_properties.version() >= ivec2(4, 3)
        && require_ssbo;

      m_front_matter_frag
        .specify_extension("GL_EXT_shader_framebuffer_fetch", ShaderSource::enable_extension);

      if (using_glsl43)
        {
          glsl_version = "430";
        }
      else if (using_glsl42)
        {
          glsl_version = "420";
        }
      else
        {
          glsl_version = "330";

          /* We need this extension for unpackHalf2x16() */
          m_front_matter_vert.specify_extension("GL_ARB_shading_language_packing",
                                                ShaderSource::require_extension);
          m_front_matter_frag.specify_extension("GL_ARB_shading_language_packing",
                                                ShaderSource::require_extension);
          if (m_uber_shader_builder_params.assign_layout_to_varyings())
            {
              m_front_matter_vert.specify_extension("GL_ARB_separate_shader_objects",
                                                    ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_ARB_separate_shader_objects",
                                                    ShaderSource::require_extension);
            }

          if (m_uber_shader_builder_params.assign_binding_points())
            {
              m_front_matter_vert.specify_extension("GL_ARB_shading_language_420pack",
                                                    ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_ARB_shading_language_420pack",
                                                    ShaderSource::require_extension);
            }
        }

      if (require_image_load_store && !using_glsl42)
        {
          m_front_matter_frag
            .specify_extension("GL_ARB_shader_image_load_store", ShaderSource::require_extension);
        }

      if (require_ssbo && !using_glsl43)
        {
          m_front_matter_vert.specify_extension("GL_ARB_shader_storage_buffer_object",
                                                ShaderSource::require_extension);
          m_front_matter_frag.specify_extension("GL_ARB_shader_storage_buffer_object",
                                                ShaderSource::require_extension);
        }
    }
  #endif

  std::string tmp(m_params.glsl_version_override());
  if (!tmp.empty())
    {
      glsl_version = t_max(tmp, glsl_version);
    }

  m_front_matter_vert.specify_version(glsl_version.c_str());
  m_front_matter_frag.specify_version(glsl_version.c_str());

  switch(m_interlock_type)
    {
    case intel_fragment_shader_ordering:
      m_front_matter_frag
        .specify_extension("GL_INTEL_fragment_shader_ordering",
                           ShaderSource::require_extension);
      break;

    case nv_fragment_shader_interlock:
      m_front_matter_frag
        .specify_extension("GL_NV_fragment_shader_interlock",
                           ShaderSource::require_extension);
      break;

    case arb_fragment_shader_interlock:
      m_front_matter_frag
        .specify_extension("GL_ARB_fragment_shader_interlock",
                           ShaderSource::require_extension);
      break;

    default:
      break;
    }

  if (m_uber_shader_builder_params.supports_bindless_texturing())
    {
      if (m_uber_shader_builder_params.use_uvec2_for_bindless_handle())
        {
          m_front_matter_frag
            .specify_extension("GL_ARB_bindless_texture",
                               ShaderSource::enable_extension);

          m_front_matter_vert
            .specify_extension("GL_ARB_bindless_texture",
                               ShaderSource::enable_extension);
        }
      else
        {
          m_front_matter_frag
            .specify_extension("GL_NV_gpu_shader5", ShaderSource::enable_extension)
            .specify_extension("GL_NV_bindless_texture", ShaderSource::enable_extension);

          m_front_matter_vert
            .specify_extension("GL_NV_gpu_shader5", ShaderSource::enable_extension)
            .specify_extension("GL_NV_bindless_texture", ShaderSource::enable_extension);
        }
    }
}

const fastuidraw::gl::detail::PainterShaderRegistrarGL::program_set&
fastuidraw::gl::detail::PainterShaderRegistrarGL::
programs(void)
{
  Mutex::Guard m(mutex());

  unsigned int number_shaders(registered_shader_count());
  if (number_shaders != m_number_shaders_in_program)
    {
      build_programs();
      m_number_shaders_in_program = number_shaders;
    }
  return m_programs;
}

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref*
fastuidraw::gl::detail::PainterShaderRegistrarGL::
resize_item_shader_vector_as_needed(enum PainterSurface::render_type_t prender_type,
                                    unsigned int shader_group,
                                    enum PainterBlendShader::shader_type blend_type,
                                    vecN<std::vector<program_ref>, PainterBlendShader::number_types + 1> &elements)
{
  unsigned int shader, idx;

  idx = (prender_type == PainterSurface::color_buffer_type) ?
    blend_type :
    PainterBlendShader::number_types;

  shader = shader_group & ~PainterShaderRegistrarGL::shader_group_discard_mask;
  if (shader >= elements[idx].size())
    {
      elements[idx].resize(shader + 1);
    }
  return &elements[idx][shader];
}

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref
fastuidraw::gl::detail::PainterShaderRegistrarGL::
program_of_item_shader(enum PainterSurface::render_type_t prender_type,
                       unsigned int shader_group,
                       enum PainterBlendShader::shader_type blend_type)
{
  Mutex::Guard m(mutex());
  unsigned int shader;

  if (prender_type == PainterSurface::color_buffer_type)
    {
      unsigned int blend_shader_count(registered_blend_shader_count(blend_type));
      if (blend_shader_count != m_number_blend_shaders_in_item_programs[blend_type])
        {
          m_item_programs[blend_type].clear();
          m_number_blend_shaders_in_item_programs[blend_type] = blend_shader_count;
        }
    }
  program_ref &dst(*resize_item_shader_vector_as_needed(prender_type, shader_group,
                                                        blend_type, m_item_programs));

  shader = shader_group & ~PainterShaderRegistrarGL::shader_group_discard_mask;
  if (!dst)
    {
      if (prender_type == PainterSurface::color_buffer_type)
        {
          dst = build_program_of_item_shader(shader,
                                             shader_group & PainterShaderRegistrarGL::shader_group_discard_mask,
                                             blend_type);
        }
      else
        {
          dst = build_program_of_coverage_item_shader(shader);
        }
    }

  return dst;
}

void
fastuidraw::gl::detail::PainterShaderRegistrarGL::
build_programs(void)
{
  using namespace fastuidraw::glsl;
  for (unsigned int blend_tp = 0; blend_tp < PainterBlendShader::number_types; ++blend_tp)
    {
      for (unsigned int discard_tp = 0; discard_tp < PainterEngineGL::number_program_types; ++discard_tp)
        {
          m_programs.m_item_programs[blend_tp][discard_tp] =
            build_program(static_cast<enum PainterEngineGL::program_type_t>(discard_tp),
                          static_cast<enum PainterBlendShader::shader_type>(blend_tp));
        }
    }
  m_programs.m_deferred_coverage_program = build_deferred_coverage_program();
}

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref
fastuidraw::gl::detail::PainterShaderRegistrarGL::
build_program_of_item_shader(unsigned int shader, bool shader_uses_discard,
                             enum PainterBlendShader::shader_type blend_type)
{
  using namespace fastuidraw::glsl;

  ShaderSource vert, frag;
  program_ref return_value;
  c_string discard_macro;
  bool glsl_discard_active(shader_uses_discard);

  if (!blend_type_supported(blend_type))
    {
      return nullptr;
    }

  if (m_params.clipping_type() == clipping_via_discard
      || (m_params.clipping_type() == clipping_via_skip_color_write
          && blend_type != PainterBlendShader::framebuffer_fetch))
    {
      glsl_discard_active = true;
    }

  if (!glsl_discard_active)
    {
      discard_macro = "fastuidraw_do_nothing()";
      frag.add_macro("FASTUIDRAW_ALLOW_EARLY_FRAGMENT_TESTS");
    }
  else
    {
      discard_macro = "discard";
    }

  vert
    .specify_version(m_front_matter_vert.version())
    .specify_extensions(m_front_matter_vert)
    .add_source(m_front_matter_vert);

  frag
    .specify_version(m_front_matter_frag.version())
    .specify_extensions(m_front_matter_frag)
    .add_source(m_front_matter_frag);

  construct_item_shader(blend_type,
                        m_backend_constants, vert, frag,
                        m_uber_shader_builder_params,
                        shader, discard_macro);

  return_value = FASTUIDRAWnew Program(vert, frag,
                                       m_attribute_binder,
                                       m_initializer);
  return return_value;
}

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref
fastuidraw::gl::detail::PainterShaderRegistrarGL::
build_program_of_coverage_item_shader(unsigned int shader)
{
  using namespace fastuidraw::glsl;

  ShaderSource vert, frag;
  program_ref return_value;

  vert
    .specify_version(m_front_matter_vert.version())
    .specify_extensions(m_front_matter_vert)
    .add_source(m_front_matter_vert);

  frag
    .specify_version(m_front_matter_frag.version())
    .specify_extensions(m_front_matter_frag)
    .add_source(m_front_matter_frag);

  construct_item_coverage_shader(m_backend_constants, vert, frag,
                                 m_uber_shader_builder_params,
                                 shader);

  return_value = FASTUIDRAWnew Program(vert, frag,
                                       m_attribute_binder,
                                       m_initializer);
  return return_value;
}

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref
fastuidraw::gl::detail::PainterShaderRegistrarGL::
build_program(enum PainterEngineGL::program_type_t tp,
              enum PainterBlendShader::shader_type blend_type)
{
  using namespace fastuidraw::glsl;

  ShaderSource vert, frag;
  program_ref return_value;
  c_string discard_macro;
  bool glsl_discard_active(tp != PainterEngineGL::program_without_discard);

  if (!blend_type_supported(blend_type))
    {
      return nullptr;
    }

  if (m_params.clipping_type() == clipping_via_discard
      || (m_params.clipping_type() == clipping_via_skip_color_write
          && blend_type != PainterBlendShader::framebuffer_fetch))
    {
      glsl_discard_active = true;
    }

  if (!glsl_discard_active)
    {
      discard_macro = "fastuidraw_do_nothing()";
      frag.add_macro("FASTUIDRAW_ALLOW_EARLY_FRAGMENT_TESTS");
    }
  else
    {
      discard_macro = "discard";
    }

  vert
    .specify_version(m_front_matter_vert.version())
    .specify_extensions(m_front_matter_vert)
    .add_source(m_front_matter_vert);

  frag
    .specify_version(m_front_matter_frag.version())
    .specify_extensions(m_front_matter_frag)
    .add_source(m_front_matter_frag);

  DiscardItemShaderFilter item_filter(tp, m_params.clipping_type());
  construct_item_uber_shader(blend_type, m_backend_constants, vert, frag,
                             m_uber_shader_builder_params,
                             &item_filter, discard_macro);

  return_value = FASTUIDRAWnew Program(vert, frag,
                                       m_attribute_binder,
                                       m_initializer);
  return return_value;
}

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref
fastuidraw::gl::detail::PainterShaderRegistrarGL::
build_deferred_coverage_program(void)
{
  using namespace fastuidraw::glsl;

  ShaderSource vert, frag;
  program_ref return_value;

  vert
    .specify_version(m_front_matter_vert.version())
    .specify_extensions(m_front_matter_vert)
    .add_source(m_front_matter_vert);

  frag
    .specify_version(m_front_matter_frag.version())
    .specify_extensions(m_front_matter_frag)
    .add_macro("FASTUIDRAW_ALLOW_EARLY_FRAGMENT_TESTS")
    .add_source(m_front_matter_frag);


  construct_item_uber_coverage_shader(m_backend_constants, vert, frag,
                                      m_uber_shader_builder_params,
                                      nullptr);

  return_value = FASTUIDRAWnew Program(vert, frag,
                                       m_attribute_binder,
                                       m_initializer);
  return return_value;
}
