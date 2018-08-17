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
#include "painter_shader_registrar_gl.hpp"

namespace
{
  bool
  use_shader_helper(enum fastuidraw::gl::PainterBackendGL::program_type_t tp,
                    bool uses_discard)
  {
    return tp == fastuidraw::gl::PainterBackendGL::program_all
      || (tp == fastuidraw::gl::PainterBackendGL::program_without_discard && !uses_discard)
      || (tp == fastuidraw::gl::PainterBackendGL::program_with_discard && uses_discard);
  }

  class DiscardItemShaderFilter:public fastuidraw::gl::PainterBackendGL::ItemShaderFilter
  {
  public:
    explicit
    DiscardItemShaderFilter(enum fastuidraw::gl::PainterBackendGL::program_type_t tp,
                            enum fastuidraw::gl::PainterBackendGL::clipping_type_t cp):
      m_tp(tp),
      m_cp(cp)
    {}

    bool
    use_shader(const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &shader) const
    {
      using namespace fastuidraw::gl;

      bool uses_discard;
      uses_discard = (m_cp == PainterBackendGL::clipping_via_discard)
        || shader->uses_discard();
      return use_shader_helper(m_tp, uses_discard);
    }

  private:
    enum fastuidraw::gl::PainterBackendGL::program_type_t m_tp;
    enum fastuidraw::gl::PainterBackendGL::clipping_type_t m_cp;
  };
}

//////////////////////////////////////
// fastuidraw::gl::detail::PainterShaderRegistrarGL methods
fastuidraw::gl::detail::PainterShaderRegistrarGL::
PainterShaderRegistrarGL(const PainterBackendGL::ConfigurationGL &P,
                         const PainterBackendGL::UberShaderParams &uber_params):
  fastuidraw::glsl::PainterShaderRegistrarGLSL(),
  m_params(P),
  m_uber_shader_builder_params(uber_params),
  m_number_shaders_in_program(0)
{
  configure_backend();
  m_backend_constants
    .data_store_alignment(P.alignment())
    .set_from_atlas(m_params.colorstop_atlas().static_cast_ptr<ColorStopAtlas>())
    .set_from_atlas(m_params.glyph_atlas().static_cast_ptr<GlyphAtlas>())
    .set_from_atlas(m_params.image_atlas().static_cast_ptr<ImageAtlas>());
}

uint32_t
fastuidraw::gl::detail::PainterShaderRegistrarGL::
compute_composite_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterCompositeShader> &shader)
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

  return_value = m_params.break_on_shader_change() ? tag.m_ID : 0u;
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
      const BindingPoints &binding_points(m_uber_shader_builder_params.binding_points());
      m_initializer
        .add_sampler_initializer("fastuidraw_imageAtlasLinear", binding_points.image_atlas_color_tiles_linear())
        .add_sampler_initializer("fastuidraw_imageAtlasNearest", binding_points.image_atlas_color_tiles_nearest())
        .add_sampler_initializer("fastuidraw_imageIndexAtlas", binding_points.image_atlas_index_tiles())
        .add_sampler_initializer("fastuidraw_glyphTexelStoreUINT", binding_points.glyph_atlas_texel_store_uint())
        .add_sampler_initializer("fastuidraw_glyphGeometryDataStore",
                                 binding_points.glyph_atlas_geometry_store(m_uber_shader_builder_params.glyph_geometry_backing()))
        .add_sampler_initializer("fastuidraw_colorStopAtlas", binding_points.colorstop_atlas())
        .add_sampler_initializer("fastuidraw_external_texture", binding_points.external_texture())
        .add_uniform_block_binding("fastuidraw_uniform_block", binding_points.uniforms_ubo());

      if (m_uber_shader_builder_params.have_float_glyph_texture_atlas())
        {
          m_initializer
            .add_sampler_initializer("fastuidraw_glyphTexelStoreFLOAT",
                                     binding_points.glyph_atlas_texel_store_float());
        }

      switch(m_uber_shader_builder_params.data_store_backing())
        {
        case data_store_tbo:
          {
            m_initializer
              .add_sampler_initializer("fastuidraw_painterStore_tbo",
                                       binding_points.data_store_buffer_tbo());
          }
          break;

        case data_store_ubo:
          {
            m_initializer
              .add_uniform_block_binding("fastuidraw_painterStore_ubo",
                                         binding_points.data_store_buffer_ubo());
          }
          break;

        case data_store_ssbo:
          {
            m_initializer
              .add_uniform_block_binding("fastuidraw_painterStore_ssbo",
                                         binding_points.data_store_buffer_ssbo());
          }
          break;
        }
    }

  if (!m_uber_shader_builder_params.assign_layout_to_vertex_shader_inputs())
    {
      m_attribute_binder
        .add_binding("fastuidraw_primary_attribute", primary_attrib_slot)
        .add_binding("fastuidraw_secondary_attribute", secondary_attrib_slot)
        .add_binding("fastuidraw_uint_attribute", uint_attrib_slot)
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

  if (m_params.compositing_type() == compositing_interlock
      || m_uber_shader_builder_params.provide_auxiliary_image_buffer() != no_auxiliary_buffer)
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
            .specify_extension("GL_EXT_composite_func_extended", ShaderSource::enable_extension)
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
            .specify_extension("GL_EXT_composite_func_extended", ShaderSource::enable_extension)
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
      GlyphAtlasGL *glyphs(m_params.glyph_atlas().get());
      bool require_ssbo, require_image_load_store;

      require_ssbo = (m_uber_shader_builder_params.data_store_backing() == data_store_ssbo)
        || (glyphs->geometry_binding_point() == GL_SHADER_STORAGE_BUFFER);

      require_image_load_store = (m_params.compositing_type() == compositing_interlock)
        || (m_uber_shader_builder_params.provide_auxiliary_image_buffer() != no_auxiliary_buffer)
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

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_set
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

void
fastuidraw::gl::detail::PainterShaderRegistrarGL::
build_programs(void)
{
  using namespace fastuidraw::glsl;
  for(unsigned int i = 0; i < PainterBackendGL::number_program_types; ++i)
    {
      enum PainterBackendGL::program_type_t tp;
      tp = static_cast<enum PainterBackendGL::program_type_t>(i);
      m_programs[tp] = build_program(tp);
      FASTUIDRAWassert(m_programs[tp]->link_success());
    }
}

fastuidraw::gl::detail::PainterShaderRegistrarGL::program_ref
fastuidraw::gl::detail::PainterShaderRegistrarGL::
build_program(enum fastuidraw::gl::PainterBackendGL::program_type_t tp)
{
  using namespace fastuidraw::glsl;

  ShaderSource vert, frag;
  program_ref return_value;
  DiscardItemShaderFilter item_filter(tp, m_params.clipping_type());
  c_string discard_macro;

  if (tp == PainterBackendGL::program_without_discard)
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

  construct_shader(m_backend_constants, vert, frag,
                   m_uber_shader_builder_params,
                   &item_filter, discard_macro);

  return_value = FASTUIDRAWnew Program(vert, frag,
                                       m_attribute_binder,
                                       m_initializer);
  return return_value;
}
