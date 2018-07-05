/*!
 * \file painter_backend_gl.cpp
 * \brief file painter_backend_gl.cpp
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


#include <list>
#include <map>
#include <sstream>
#include <vector>
#include <iostream>

#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/gluniform.hpp>

#include "../private/util_private.hpp"
#include "private/tex_buffer.hpp"
#include "private/texture_gl.hpp"

#ifdef FASTUIDRAW_GL_USE_GLES
#define GL_SRC1_COLOR GL_SRC1_COLOR_EXT
#define GL_SRC1_ALPHA GL_SRC1_ALPHA_EXT
#define GL_ONE_MINUS_SRC1_COLOR GL_ONE_MINUS_SRC1_COLOR_EXT
#define GL_ONE_MINUS_SRC1_ALPHA GL_ONE_MINUS_SRC1_ALPHA_EXT
#endif

namespace
{
  enum
    {
      shader_group_discard_bit = 31u,
      shader_group_discard_mask = (1u << 31u)
    };

  enum interlock_type_t
    {
      intel_fragment_shader_ordering,
      nv_fragment_shader_interlock,
      arb_fragment_shader_interlock,

      no_interlock
    };

  enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t
  compute_provide_auxiliary_buffer(enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t in_value,
                                   const fastuidraw::gl::ContextProperties &ctx)
  {
    using namespace fastuidraw;
    using namespace fastuidraw::glsl;

    if (in_value == PainterBackendGLSL::no_auxiliary_buffer)
      {
        return in_value;
      }

    /* If asking for auxiliary_buffer_framebuffer_fetch and have
     * the extension, immediately give the return value, otherwise
     * fall back to interlock. Note that we do NOT fallback
     * from interlock to framebuffer fetch because framebuffer-fetch
     * makes MSAA render targets become shaded per-sample.
     */
    if (in_value == PainterBackendGLSL::auxiliary_buffer_framebuffer_fetch)
      {
        if (ctx.has_extension("GL_EXT_shader_framebuffer_fetch"))
          {
            return in_value;
          }
        else
          {
            in_value = PainterBackendGLSL::auxiliary_buffer_interlock;
          }
      }

    #ifdef FASTUIDRAW_GL_USE_GLES
      {
        if (ctx.version() <= ivec2(3, 0))
          {
            return PainterBackendGLSL::no_auxiliary_buffer;
          }

        if (in_value == PainterBackendGLSL::auxiliary_buffer_atomic)
          {
            return in_value;
          }

        if (ctx.has_extension("GL_NV_fragment_shader_interlock"))
          {
            return PainterBackendGLSL::auxiliary_buffer_interlock_main_only;
          }
        else
          {
            return PainterBackendGLSL::auxiliary_buffer_atomic;
          }
      }
    #else
      {
        bool have_interlock, have_interlock_main;

        if (ctx.version() <= ivec2(4, 1) && !ctx.has_extension("GL_ARB_shader_image_load_store"))
          {
            return PainterBackendGLSL::no_auxiliary_buffer;
          }

        if (in_value == PainterBackendGLSL::auxiliary_buffer_atomic)
          {
            return in_value;
          }

        have_interlock = ctx.has_extension("GL_INTEL_fragment_shader_ordering");
        have_interlock_main = ctx.has_extension("GL_ARB_fragment_shader_interlock")
          || ctx.has_extension("GL_NV_fragment_shader_interlock");

        if (!have_interlock && !have_interlock_main)
          {
            return PainterBackendGLSL::auxiliary_buffer_atomic;
          }

        switch(in_value)
          {
          case PainterBackendGLSL::auxiliary_buffer_interlock_main_only:
            return (have_interlock_main) ?
              PainterBackendGLSL::auxiliary_buffer_interlock_main_only:
              PainterBackendGLSL::auxiliary_buffer_interlock;

          case PainterBackendGLSL::auxiliary_buffer_interlock:
            return (have_interlock) ?
              PainterBackendGLSL::auxiliary_buffer_interlock:
              PainterBackendGLSL::auxiliary_buffer_interlock_main_only;

          default:
            return PainterBackendGLSL::auxiliary_buffer_atomic;
          }
      }
    #endif
  }

  enum interlock_type_t
  compute_interlock_type(const fastuidraw::gl::ContextProperties &ctx)
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

  fastuidraw::PainterBlendShader::shader_type
  compute_blend_type(enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t aux_value,
                     enum fastuidraw::PainterBlendShader::shader_type in_value,
                     const fastuidraw::gl::ContextProperties &ctx)
  {
    if (aux_value == fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_framebuffer_fetch)
      {
        /* auxiliary framebuffer fetch may only be used with framebuffer fetch blending */
        return fastuidraw::PainterBlendShader::framebuffer_fetch;
      }

    bool have_dual_src_blending, have_framebuffer_fetch;
    if (ctx.is_es())
      {
        have_dual_src_blending = ctx.has_extension("GL_EXT_blend_func_extended");
        have_framebuffer_fetch = ctx.has_extension("GL_EXT_shader_framebuffer_fetch");
      }
    else
      {
        have_dual_src_blending = true;
        have_framebuffer_fetch = ctx.has_extension("GL_EXT_shader_framebuffer_fetch");
      }

    if (in_value == fastuidraw::PainterBlendShader::framebuffer_fetch
        && !have_framebuffer_fetch)
      {
        in_value = fastuidraw::PainterBlendShader::dual_src;
      }

    if (in_value == fastuidraw::PainterBlendShader::dual_src
        && !have_dual_src_blending)
      {
        in_value = fastuidraw::PainterBlendShader::single_src;
      }

    return in_value;
  }

  enum fastuidraw::gl::PainterBackendGL::clipping_type_t
  compute_clipping_type(enum fastuidraw::gl::PainterBackendGL::clipping_type_t in_value,
                        const fastuidraw::gl::ContextProperties &ctx)
  {
    using namespace fastuidraw;
    using namespace gl;

    #ifdef FASTUIDRAW_GL_USE_GLES
      {
        switch(in_value)
          {
          case PainterBackendGL::clipping_via_clip_distance:
            if (ctx.has_extension("GL_APPLE_clip_distance")
                || ctx.has_extension("GL_EXT_clip_cull_distance"))
              {
                return in_value;
              }
            //fall through
          default:
            return PainterBackendGL::clipping_via_discard;
          }
      }
    #else
      {
        FASTUIDRAWunused(ctx);
        return in_value;
      }
    #endif
  }

  class painter_vao
  {
  public:
    painter_vao(void):
      m_vao(0),
      m_attribute_bo(0),
      m_header_bo(0),
      m_index_bo(0),
      m_data_bo(0),
      m_data_tbo(0)
    {}

    GLuint m_vao;
    GLuint m_attribute_bo, m_header_bo, m_index_bo, m_data_bo;
    GLuint m_data_tbo;
    enum fastuidraw::gl::PainterBackendGL::data_store_backing_t m_data_store_backing;
    unsigned int m_data_store_binding_point;
  };

  class painter_vao_pool:fastuidraw::noncopyable
  {
  public:
    explicit
    painter_vao_pool(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
                     const fastuidraw::PainterBackend::ConfigurationBase &params_base,
                     enum fastuidraw::gl::detail::tex_buffer_support_t tex_buffer_support,
                     const fastuidraw::glsl::PainterBackendGLSL::BindingPoints &binding_points);

    ~painter_vao_pool();

    unsigned int
    attribute_buffer_size(void) const
    {
      return m_attribute_buffer_size;
    }

    unsigned int
    header_buffer_size(void) const
    {
      return m_header_buffer_size;
    }

    unsigned int
    index_buffer_size(void) const
    {
      return m_index_buffer_size;
    }

    unsigned int
    data_buffer_size(void) const
    {
      return m_data_buffer_size;
    }

    painter_vao
    request_vao(void);

    void
    next_pool(void);

    GLuint //objects are recycled; make sure size never increases!
    request_uniform_ubo(unsigned int ubo_size, GLenum target);

  private:
    void
    generate_tbos(painter_vao &vao);

    GLuint
    generate_tbo(GLuint src_buffer, GLenum fmt, unsigned int unit);

    GLuint
    generate_bo(GLenum bind_target, GLsizei psize);

    unsigned int m_attribute_buffer_size, m_header_buffer_size;
    unsigned int m_index_buffer_size;
    int m_alignment, m_blocks_per_data_buffer;
    unsigned int m_data_buffer_size;
    enum fastuidraw::gl::PainterBackendGL::data_store_backing_t m_data_store_backing;
    enum fastuidraw::gl::detail::tex_buffer_support_t m_tex_buffer_support;
    fastuidraw::glsl::PainterBackendGLSL::BindingPoints m_binding_points;

    unsigned int m_current, m_pool;
    std::vector<std::vector<painter_vao> > m_vaos;
    std::vector<GLuint> m_ubos;
  };

  bool
  use_shader_helper(enum fastuidraw::gl::PainterBackendGL::program_type_t tp,
                    bool uses_discard)
  {
    return tp == fastuidraw::gl::PainterBackendGL::program_all
      || (tp == fastuidraw::gl::PainterBackendGL::program_without_discard && !uses_discard)
      || (tp == fastuidraw::gl::PainterBackendGL::program_with_discard && uses_discard);
  }

  class DiscardItemShaderFilter:public fastuidraw::glsl::PainterBackendGLSL::ItemShaderFilter
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
      bool discard_active;

      discard_active =  m_cp == PainterBackendGL::clipping_via_discard
        || shader->uses_discard();
      return use_shader_helper(m_tp, discard_active);
    }

  private:
    enum fastuidraw::gl::PainterBackendGL::program_type_t m_tp;
    enum fastuidraw::gl::PainterBackendGL::clipping_type_t m_cp;
  };

  class ImageBarrier:public fastuidraw::PainterDraw::Action
  {
  public:
    virtual
    fastuidraw::gpu_dirty_state
    execute(fastuidraw::PainterDraw::APIBase*) const
    {
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      return fastuidraw::gpu_dirty_state();
    }
  };

  class ImageBarrierByRegion:public fastuidraw::PainterDraw::Action
  {
  public:
    virtual
    fastuidraw::gpu_dirty_state
    execute(fastuidraw::PainterDraw::APIBase*) const
    {
      glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      return fastuidraw::gpu_dirty_state();
    }
  };

  class SurfaceGLPrivate;
  class PainterBackendGLPrivate
  {
  public:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::gl::Program> program_ref;
    enum { program_count = fastuidraw::gl::PainterBackendGL::number_program_types };
    typedef fastuidraw::vecN<program_ref, program_count + 1> program_set;

    PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &P,
                            fastuidraw::gl::PainterBackendGL *p);

    ~PainterBackendGLPrivate();

    static
    fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL
    compute_glsl_config(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &P);

    static
    fastuidraw::PainterBackend::ConfigurationBase
    compute_base_config(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &P,
                        const fastuidraw::PainterBackend::ConfigurationBase &config_base);

    const program_set&
    programs(bool rebuild);

    void
    configure_backend(void);

    void
    configure_source_front_matter(void);

    void
    build_programs(void);

    program_ref
    build_program(enum fastuidraw::gl::PainterBackendGL::program_type_t tp);

    void
    set_gl_state(fastuidraw::gpu_dirty_state v,
                 bool clear_depth, bool clear_color);

    enum interlock_type_t m_interlock_type;
    fastuidraw::gl::PainterBackendGL::ConfigurationGL m_params;
    fastuidraw::glsl::PainterBackendGLSL::UberShaderParams m_uber_shader_builder_params;

    fastuidraw::gl::ContextProperties m_ctx_properties;
    bool m_backend_configured;
    enum fastuidraw::gl::detail::tex_buffer_support_t m_tex_buffer_support;
    int m_number_clip_planes;
    GLenum m_clip_plane0;
    std::string m_gles_clip_plane_extension;
    bool m_has_multi_draw_elements;

    GLuint m_nearest_filter_sampler;
    fastuidraw::gl::PreLinkActionArray m_attribute_binder;
    fastuidraw::gl::ProgramInitializerArray m_initializer;
    fastuidraw::glsl::ShaderSource m_front_matter_vert;
    fastuidraw::glsl::ShaderSource m_front_matter_frag;
    program_set m_programs;
    std::vector<fastuidraw::generic_data> m_uniform_values;
    fastuidraw::c_array<fastuidraw::generic_data> m_uniform_values_ptr;
    painter_vao_pool *m_pool;
    SurfaceGLPrivate *m_surface_gl;

    fastuidraw::gl::PainterBackendGL *m_p;
  };

  class DrawState
  {
  public:
    explicit
    DrawState(fastuidraw::gl::Program *pr):
      m_current_program(pr),
      m_current_blend_mode(nullptr)
    {}

    void
    restore_gl_state(const painter_vao &vao,
                     PainterBackendGLPrivate *pr,
                     fastuidraw::gpu_dirty_state flags) const;

    fastuidraw::gl::Program *m_current_program;
    const fastuidraw::BlendMode *m_current_blend_mode;

  private:
    static
    GLenum
    convert_blend_op(enum fastuidraw::BlendMode::op_t v);

    static
    GLenum
    convert_blend_func(enum fastuidraw::BlendMode::func_t v);
  };

  class DrawEntry
  {
  public:
    DrawEntry(const fastuidraw::BlendMode &mode,
              PainterBackendGLPrivate *pr,
              unsigned int pz);

    DrawEntry(const fastuidraw::BlendMode &mode);

    DrawEntry(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action);

    void
    add_entry(GLsizei count, const void *offset);

    void
    draw(PainterBackendGLPrivate *pr, const painter_vao &vao,
         DrawState &st) const;

  private:
    bool m_set_blend;
    fastuidraw::BlendMode m_blend_mode;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_action;

    std::vector<GLsizei> m_counts;
    std::vector<const GLvoid*> m_indices;
    fastuidraw::gl::Program *m_new_program;
  };

  class DrawCommand:public fastuidraw::PainterDraw
  {
  public:
    explicit
    DrawCommand(painter_vao_pool *hnd,
                const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
                PainterBackendGLPrivate *pr);

    virtual
    ~DrawCommand()
    {}

    virtual
    void
    draw_break(const fastuidraw::PainterShaderGroup &old_shaders,
               const fastuidraw::PainterShaderGroup &new_shaders,
               unsigned int indices_written) const;

    virtual
    void
    draw_break(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action,
               unsigned int indices_written) const;

    virtual
    void
    draw(void) const;

  protected:

    virtual
    void
    unmap_implement(unsigned int attributes_written,
                    unsigned int indices_written,
                    unsigned int data_store_written) const;

  private:

    void
    add_entry(unsigned int indices_written) const;

    PainterBackendGLPrivate *m_pr;
    painter_vao m_vao;
    mutable unsigned int m_attributes_written, m_indices_written;
    mutable std::list<DrawEntry> m_draws;
  };

  class SurfacePropertiesPrivate
  {
  public:
    SurfacePropertiesPrivate(void):
      m_dimensions(1, 1),
      m_msaa(0)
    {}

    fastuidraw::ivec2 m_dimensions;
    unsigned int m_msaa;
  };

  class SurfaceGLPrivate:fastuidraw::noncopyable
  {
  public:
    enum fbo_t
      {
        fbo_just_color,
        fbo_with_auxiliary_buffer,

        number_fbo_t
      };

    enum auxiliary_buffer_t
      {
        auxiliary_buffer_u8,
        auxiliary_buffer_u32,

        number_auxiliary_buffer_t
      };

    explicit
    SurfaceGLPrivate(GLuint texture,
                     const fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties &props);

    ~SurfaceGLPrivate();

    static
    fastuidraw::gl::PainterBackendGL::SurfaceGL*
    surface_gl(const fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface> &surface);

    GLuint
    auxiliary_buffer(enum auxiliary_buffer_t tp);

    static
    GLenum
    auxiliaryBufferInternalFmt(enum auxiliary_buffer_t tp)
    {
      return tp == auxiliary_buffer_u8 ?
        GL_R8 :
        GL_R32UI;
    }

    GLuint
    color_buffer(void)
    {
      return buffer(buffer_color);
    }

    GLuint
    fbo(enum fbo_t tp);

    static
    enum fbo_t
    fbo_tp(enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t tp)
    {
      return (tp == fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_framebuffer_fetch) ?
        fbo_with_auxiliary_buffer :
        fbo_just_color;
    }

    fastuidraw::PainterBackend::Surface::Viewport m_viewport;
    fastuidraw::vec4 m_clear_color;
    fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties m_properties;

  private:
    enum buffer_t
      {
        buffer_color,
        buffer_depth,

        number_buffer_t
      };

    GLuint
    buffer(enum buffer_t);

    fastuidraw::vecN<GLuint, number_auxiliary_buffer_t> m_auxiliary_buffer;
    fastuidraw::vecN<GLuint, number_buffer_t> m_buffers;
    fastuidraw::vecN<GLuint, number_fbo_t> m_fbo;
    bool m_own_texture;
  };

  class ConfigurationGLPrivate
  {
  public:
    ConfigurationGLPrivate(void):
      m_attributes_per_buffer(512 * 512),
      m_indices_per_buffer((m_attributes_per_buffer * 6) / 4),
      m_data_blocks_per_store_buffer(1024 * 64),
      m_data_store_backing(fastuidraw::gl::PainterBackendGL::data_store_tbo),
      m_number_pools(3),
      m_break_on_shader_change(false),
      m_clipping_type(fastuidraw::gl::PainterBackendGL::clipping_via_clip_distance),
      /* on Mesa/i965 using switch statement gives much slower
       * performance than using if/else chain.
       */
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_blend_shader_use_switch(false),
      m_unpack_header_and_brush_in_frag_shader(false),
      m_assign_layout_to_vertex_shader_inputs(true),
      m_assign_layout_to_varyings(false),
      m_assign_binding_points(true),
      m_separate_program_for_discard(true),
      m_default_stroke_shader_aa_type(fastuidraw::PainterStrokeShader::draws_solid_then_fuzz),
      m_blend_type(fastuidraw::PainterBlendShader::dual_src),
      m_provide_auxiliary_image_buffer(fastuidraw::glsl::PainterBackendGLSL::no_auxiliary_buffer)
    {}

    unsigned int m_attributes_per_buffer;
    unsigned int m_indices_per_buffer;
    unsigned int m_data_blocks_per_store_buffer;
    enum fastuidraw::gl::PainterBackendGL::data_store_backing_t m_data_store_backing;
    unsigned int m_number_pools;
    bool m_break_on_shader_change;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL> m_image_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::ColorStopAtlasGL> m_colorstop_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL> m_glyph_atlas;
    enum fastuidraw::gl::PainterBackendGL::clipping_type_t m_clipping_type;
    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_blend_shader_use_switch;
    bool m_unpack_header_and_brush_in_frag_shader;
    bool m_assign_layout_to_vertex_shader_inputs;
    bool m_assign_layout_to_varyings;
    bool m_assign_binding_points;
    bool m_separate_program_for_discard;
    enum fastuidraw::PainterStrokeShader::type_t m_default_stroke_shader_aa_type;
    enum fastuidraw::PainterBlendShader::shader_type m_blend_type;
    enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t m_provide_auxiliary_image_buffer;
  };

}

///////////////////////////////////////////
// painter_vao_pool methods
painter_vao_pool::
painter_vao_pool(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
                 const fastuidraw::PainterBackend::ConfigurationBase &params_base,
                 enum fastuidraw::gl::detail::tex_buffer_support_t tex_buffer_support,
                 const fastuidraw::glsl::PainterBackendGLSL::BindingPoints &binding_points):
  m_attribute_buffer_size(params.attributes_per_buffer() * sizeof(fastuidraw::PainterAttribute)),
  m_header_buffer_size(params.attributes_per_buffer() * sizeof(uint32_t)),
  m_index_buffer_size(params.indices_per_buffer() * sizeof(fastuidraw::PainterIndex)),
  m_alignment(params_base.alignment()),
  m_blocks_per_data_buffer(params.data_blocks_per_store_buffer()),
  m_data_buffer_size(m_blocks_per_data_buffer * m_alignment * sizeof(fastuidraw::generic_data)),
  m_data_store_backing(params.data_store_backing()),
  m_tex_buffer_support(tex_buffer_support),
  m_binding_points(binding_points),
  m_current(0),
  m_pool(0),
  m_vaos(params.number_pools()),
  m_ubos(params.number_pools(), 0)
{}

painter_vao_pool::
~painter_vao_pool()
{
  FASTUIDRAWassert(m_ubos.size() == m_vaos.size());
  for(unsigned int p = 0, endp = m_vaos.size(); p < endp; ++p)
    {
      for(const painter_vao &vao : m_vaos[p])
        {
          if (vao.m_data_tbo != 0)
            {
              glDeleteTextures(1, &vao.m_data_tbo);
            }
          glDeleteBuffers(1, &vao.m_attribute_bo);
          glDeleteBuffers(1, &vao.m_header_bo);
          glDeleteBuffers(1, &vao.m_index_bo);
          glDeleteBuffers(1, &vao.m_data_bo);
          glDeleteVertexArrays(1, &vao.m_vao);
        }

      if (m_ubos[p] != 0)
        {
          glDeleteBuffers(1, &m_ubos[p]);
        }
    }
}

GLuint
painter_vao_pool::
request_uniform_ubo(unsigned int sz, GLenum target)
{
  if (m_ubos[m_pool] == 0)
    {
      m_ubos[m_pool] = generate_bo(target, sz);
    }
  else
    {
      glBindBuffer(target, m_ubos[m_pool]);
    }

  #ifndef NDEBUG
    {
      GLint psize(0);
      glGetBufferParameteriv(target, GL_BUFFER_SIZE, &psize);
      FASTUIDRAWassert(psize >= static_cast<int>(sz));
    }
  #endif

  return m_ubos[m_pool];
}

painter_vao
painter_vao_pool::
request_vao(void)
{
  painter_vao return_value;

  if (m_current == m_vaos[m_pool].size())
    {
      fastuidraw::gl::opengl_trait_value v;

      m_vaos[m_pool].resize(m_current + 1);
      glGenVertexArrays(1, &m_vaos[m_pool][m_current].m_vao);

      FASTUIDRAWassert(m_vaos[m_pool][m_current].m_vao != 0);
      glBindVertexArray(m_vaos[m_pool][m_current].m_vao);

      m_vaos[m_pool][m_current].m_data_store_backing = m_data_store_backing;

      switch(m_data_store_backing)
        {
        case fastuidraw::gl::PainterBackendGL::data_store_tbo:
          {
            m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_ARRAY_BUFFER, m_data_buffer_size);
            m_vaos[m_pool][m_current].m_data_store_binding_point = m_binding_points.data_store_buffer_tbo();
            generate_tbos(m_vaos[m_pool][m_current]);
          }
          break;

        case fastuidraw::gl::PainterBackendGL::data_store_ubo:
          {
            m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_ARRAY_BUFFER, m_data_buffer_size);
            m_vaos[m_pool][m_current].m_data_store_binding_point = m_binding_points.data_store_buffer_ubo();
          }
          break;
        }

      /* generate_bo leaves the returned buffer object bound to
       * the passed binding target.
       */
      m_vaos[m_pool][m_current].m_attribute_bo = generate_bo(GL_ARRAY_BUFFER, m_attribute_buffer_size);
      m_vaos[m_pool][m_current].m_index_bo = generate_bo(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_size);

      glEnableVertexAttribArray(fastuidraw::glsl::PainterBackendGLSL::primary_attrib_slot);
      v = fastuidraw::gl::opengl_trait_values<fastuidraw::uvec4>(sizeof(fastuidraw::PainterAttribute),
                                                                 offsetof(fastuidraw::PainterAttribute, m_attrib0));
      fastuidraw::gl::VertexAttribIPointer(fastuidraw::glsl::PainterBackendGLSL::primary_attrib_slot, v);

      glEnableVertexAttribArray(fastuidraw::glsl::PainterBackendGLSL::secondary_attrib_slot);
      v = fastuidraw::gl::opengl_trait_values<fastuidraw::uvec4>(sizeof(fastuidraw::PainterAttribute),
                                                                 offsetof(fastuidraw::PainterAttribute, m_attrib1));
      fastuidraw::gl::VertexAttribIPointer(fastuidraw::glsl::PainterBackendGLSL::secondary_attrib_slot, v);

      glEnableVertexAttribArray(fastuidraw::glsl::PainterBackendGLSL::uint_attrib_slot);
      v = fastuidraw::gl::opengl_trait_values<fastuidraw::uvec4>(sizeof(fastuidraw::PainterAttribute),
                                                                 offsetof(fastuidraw::PainterAttribute, m_attrib2));
      fastuidraw::gl::VertexAttribIPointer(fastuidraw::glsl::PainterBackendGLSL::uint_attrib_slot, v);

      m_vaos[m_pool][m_current].m_header_bo = generate_bo(GL_ARRAY_BUFFER, m_header_buffer_size);
      glEnableVertexAttribArray(fastuidraw::glsl::PainterBackendGLSL::header_attrib_slot);
      v = fastuidraw::gl::opengl_trait_values<uint32_t>();
      fastuidraw::gl::VertexAttribIPointer(fastuidraw::glsl::PainterBackendGLSL::header_attrib_slot, v);

      glBindVertexArray(0);
    }

  return_value = m_vaos[m_pool][m_current];
  ++m_current;

  return return_value;
}

void
painter_vao_pool::
next_pool(void)
{
  ++m_pool;
  if (m_pool == m_vaos.size())
    {
      m_pool = 0;
    }

  m_current = 0;
}


void
painter_vao_pool::
generate_tbos(painter_vao &vao)
{
  const GLenum uint_fmts[4] =
    {
      GL_R32UI,
      GL_RG32UI,
      GL_RGB32UI,
      GL_RGBA32UI,
    };
  vao.m_data_tbo = generate_tbo(vao.m_data_bo, uint_fmts[m_alignment - 1],
                                m_binding_points.data_store_buffer_tbo());
}

GLuint
painter_vao_pool::
generate_tbo(GLuint src_buffer, GLenum fmt, unsigned int unit)
{
  GLuint return_value(0);

  glGenTextures(1, &return_value);
  FASTUIDRAWassert(return_value != 0);

  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_BUFFER, return_value);
  fastuidraw::gl::detail::tex_buffer(m_tex_buffer_support, GL_TEXTURE_BUFFER, fmt, src_buffer);

  return return_value;
}

GLuint
painter_vao_pool::
generate_bo(GLenum bind_target, GLsizei psize)
{
  GLuint return_value(0);
  glGenBuffers(1, &return_value);
  FASTUIDRAWassert(return_value != 0);
  glBindBuffer(bind_target, return_value);
  glBufferData(bind_target, psize, nullptr, GL_STREAM_DRAW);
  return return_value;
}

////////////////////////////////////////////
// DrawState methods
void
DrawState::
restore_gl_state(const painter_vao &vao,
                 PainterBackendGLPrivate *pr,
                 fastuidraw::gpu_dirty_state flags) const
{
  using namespace fastuidraw;

  if (flags & gpu_dirty_state::shader)
    {
      FASTUIDRAWassert(m_current_program);
      m_current_program->use_program();
    }

  pr->set_gl_state(flags, false, false);

  /* If necessary, restore the UBO or TBO assoicated to the data
   * store binding point.
   */
  switch(vao.m_data_store_backing)
    {
    case fastuidraw::gl::PainterBackendGL::data_store_tbo:
      if (flags & gpu_dirty_state::textures)
        {
          glActiveTexture(GL_TEXTURE0 + vao.m_data_store_binding_point);
          glBindTexture(GL_TEXTURE_BUFFER, vao.m_data_tbo);
        }
      break;

    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      if (flags & gpu_dirty_state::constant_buffers)
        {
          glBindBufferBase(GL_UNIFORM_BUFFER, vao.m_data_store_binding_point, vao.m_data_bo);
        }
      break;

    default:
      FASTUIDRAWassert(!"Bad value for vao.m_data_store_backing");
    }

  if (flags & gpu_dirty_state::blend_mode)
    {
      FASTUIDRAWassert(m_current_blend_mode);
      if (m_current_blend_mode->blending_on())
        {
          glEnable(GL_BLEND);
          glBlendEquationSeparate(convert_blend_op(m_current_blend_mode->equation_rgb()),
                                  convert_blend_op(m_current_blend_mode->equation_alpha()));
          glBlendFuncSeparate(convert_blend_func(m_current_blend_mode->func_src_rgb()),
                              convert_blend_func(m_current_blend_mode->func_dst_rgb()),
                              convert_blend_func(m_current_blend_mode->func_src_alpha()),
                              convert_blend_func(m_current_blend_mode->func_dst_alpha()));
        }
      else
        {
          glDisable(GL_BLEND);
        }
    }
}

GLenum
DrawState::
convert_blend_op(enum fastuidraw::BlendMode::op_t v)
{
#define C(X) case fastuidraw::BlendMode::X: return GL_FUNC_##X
#define D(X) case fastuidraw::BlendMode::X: return GL_##X

  switch(v)
    {
      C(ADD);
      C(SUBTRACT);
      C(REVERSE_SUBTRACT);
      D(MIN);
      D(MAX);

    case fastuidraw::BlendMode::NUMBER_OPS:
    default:
      FASTUIDRAWassert(!"Bad fastuidraw::BlendMode::op_t v");
    }
#undef C
#undef D

  FASTUIDRAWassert("Invalid blend_op_t");
  return GL_INVALID_ENUM;
}

GLenum
DrawState::
convert_blend_func(enum fastuidraw::BlendMode::func_t v)
{
#define C(X) case fastuidraw::BlendMode::X: return GL_##X
  switch(v)
    {
      C(ZERO);
      C(ONE);
      C(SRC_COLOR);
      C(ONE_MINUS_SRC_COLOR);
      C(SRC_ALPHA);
      C(ONE_MINUS_SRC_ALPHA);
      C(DST_COLOR);
      C(ONE_MINUS_DST_COLOR);
      C(DST_ALPHA);
      C(ONE_MINUS_DST_ALPHA);
      C(CONSTANT_COLOR);
      C(ONE_MINUS_CONSTANT_COLOR);
      C(CONSTANT_ALPHA);
      C(ONE_MINUS_CONSTANT_ALPHA);
      C(SRC_ALPHA_SATURATE);
      C(SRC1_COLOR);
      C(ONE_MINUS_SRC1_COLOR);
      C(SRC1_ALPHA);
      C(ONE_MINUS_SRC1_ALPHA);

    case fastuidraw::BlendMode::NUMBER_FUNCS:
    default:
      FASTUIDRAWassert(!"Bad fastuidraw::BlendMode::func_t v");
    }
#undef C
  FASTUIDRAWassert("Invalid blend_t");
  return GL_INVALID_ENUM;
}

///////////////////////////////////////////////
// DrawEntry methods
DrawEntry::
DrawEntry(const fastuidraw::BlendMode &mode,
          PainterBackendGLPrivate *pr,
          unsigned int pz):
  m_set_blend(true),
  m_blend_mode(mode),
  m_new_program(pr->m_programs[pz].get())
{}

DrawEntry::
DrawEntry(const fastuidraw::BlendMode &mode):
  m_set_blend(true),
  m_blend_mode(mode),
  m_new_program(nullptr)
{}

DrawEntry::
DrawEntry(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action):
  m_set_blend(false),
  m_action(action),
  m_new_program(nullptr)
{}

void
DrawEntry::
add_entry(GLsizei count, const void *offset)
{
  m_counts.push_back(count);
  m_indices.push_back(offset);
}

void
DrawEntry::
draw(PainterBackendGLPrivate *pr, const painter_vao &vao,
     DrawState &st) const
{
  using namespace fastuidraw;

  uint32_t flags(0);

  if (m_action)
    {
      /* Rather than having something delicate to restore
       * the currently bound VAO, instead we unbind it
       * and rebind it after the action.
       */
      glBindVertexArray(0);
      flags |= m_action->execute(nullptr);
      glBindVertexArray(vao.m_vao);
    }

  if (m_set_blend)
    {
      st.m_current_blend_mode = &m_blend_mode;
      flags |= gpu_dirty_state::blend_mode;
    }

  if (m_new_program && st.m_current_program != m_new_program)
    {
      st.m_current_program = m_new_program;
      flags |= gpu_dirty_state::shader;
    }

  st.restore_gl_state(vao, pr, flags);

  if (m_counts.empty())
    {
      return;
    }

  FASTUIDRAWassert(m_counts.size() == m_indices.size());

  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      glMultiDrawElements(GL_TRIANGLES, &m_counts[0],
                          fastuidraw::gl::opengl_trait<fastuidraw::PainterIndex>::type,
                          &m_indices[0], m_counts.size());
    }
  #else
    {
      if (pr->m_has_multi_draw_elements)
        {
          glMultiDrawElementsEXT(GL_TRIANGLES, &m_counts[0],
                                 fastuidraw::gl::opengl_trait<fastuidraw::PainterIndex>::type,
                                 &m_indices[0], m_counts.size());
        }
      else
        {
          for(unsigned int i = 0, endi = m_counts.size(); i < endi; ++i)
            {
              glDrawElements(GL_TRIANGLES, m_counts[i],
                             fastuidraw::gl::opengl_trait<fastuidraw::PainterIndex>::type,
                             m_indices[i]);
            }
        }
    }
  #endif
}

////////////////////////////////////
// DrawCommand methods
DrawCommand::
DrawCommand(painter_vao_pool *hnd,
            const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
            PainterBackendGLPrivate *pr):
  m_pr(pr),
  m_vao(hnd->request_vao()),
  m_attributes_written(0),
  m_indices_written(0)
{
  /* map the buffers and set to the c_array<> fields of
   * fastuidraw::PainterDraw to the mapping location.
   */
  void *attr_bo, *index_bo, *data_bo, *header_bo;
  uint32_t flags;

  flags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_attribute_bo);
  attr_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->attribute_buffer_size(), flags);
  FASTUIDRAWassert(attr_bo != nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_header_bo);
  header_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->header_buffer_size(), flags);
  FASTUIDRAWassert(header_bo != nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vao.m_index_bo);
  index_bo = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, hnd->index_buffer_size(), flags);
  FASTUIDRAWassert(index_bo != nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_data_bo);
  data_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->data_buffer_size(), flags);
  FASTUIDRAWassert(data_bo != nullptr);

  m_attributes = fastuidraw::c_array<fastuidraw::PainterAttribute>(static_cast<fastuidraw::PainterAttribute*>(attr_bo),
                                                                 params.attributes_per_buffer());
  m_indices = fastuidraw::c_array<fastuidraw::PainterIndex>(static_cast<fastuidraw::PainterIndex*>(index_bo),
                                                          params.indices_per_buffer());
  m_store = fastuidraw::c_array<fastuidraw::generic_data>(static_cast<fastuidraw::generic_data*>(data_bo),
                                                          hnd->data_buffer_size() / sizeof(fastuidraw::generic_data));

  m_header_attributes = fastuidraw::c_array<uint32_t>(static_cast<uint32_t*>(header_bo),
                                                     params.attributes_per_buffer());

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void
DrawCommand::
draw_break(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action,
           unsigned int indices_written) const
{
  if (action)
    {
      if (!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(action);
    }
}

void
DrawCommand::
draw_break(const fastuidraw::PainterShaderGroup &old_shaders,
           const fastuidraw::PainterShaderGroup &new_shaders,
           unsigned int indices_written) const
{
  /* if the blend mode changes, then we need to start a new DrawEntry */
  fastuidraw::BlendMode::packed_value old_mode, new_mode;
  uint32_t new_disc, old_disc;

  old_mode = old_shaders.packed_blend_mode();
  new_mode = new_shaders.packed_blend_mode();

  old_disc = old_shaders.item_group() & shader_group_discard_mask;
  new_disc = new_shaders.item_group() & shader_group_discard_mask;

  if (old_disc != new_disc)
    {
      unsigned int pz;
      pz = (new_disc != 0u) ?
        fastuidraw::gl::PainterBackendGL::program_with_discard :
        fastuidraw::gl::PainterBackendGL::program_without_discard;

      if (!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(DrawEntry(fastuidraw::BlendMode(new_mode), m_pr, pz));
    }
  else if (old_mode != new_mode)
    {
      if (!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(fastuidraw::BlendMode(new_mode));
    }
  else
    {
      /* any other state changes means that we just need to add an
       * entry to the current draw entry.
       */
      add_entry(indices_written);
    }
}

void
DrawCommand::
draw(void) const
{
  using namespace fastuidraw::gl;

  glBindVertexArray(m_vao.m_vao);
  switch(m_vao.m_data_store_backing)
    {
    case PainterBackendGL::data_store_tbo:
      {
        glActiveTexture(GL_TEXTURE0 + m_vao.m_data_store_binding_point);
        glBindTexture(GL_TEXTURE_BUFFER, m_vao.m_data_tbo);
      }
      break;

    case PainterBackendGL::data_store_ubo:
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, m_vao.m_data_store_binding_point, m_vao.m_data_bo);
      }
      break;

    default:
      FASTUIDRAWassert(!"Bad value for m_vao.m_data_store_backing");
    }

  unsigned int choice;
  choice = m_pr->m_params.separate_program_for_discard() ?
    PainterBackendGL::program_without_discard :
    PainterBackendGL::program_all;

  DrawState st(m_pr->m_programs[choice].get());
  st.m_current_program->use_program();

  for(const DrawEntry &entry : m_draws)
    {
      entry.draw(m_pr, m_vao, st);
    }
  glBindVertexArray(0);
}

void
DrawCommand::
unmap_implement(unsigned int attributes_written,
                unsigned int indices_written,
                unsigned int data_store_written) const
{
  m_attributes_written = attributes_written;
  add_entry(indices_written);
  FASTUIDRAWassert(m_indices_written == indices_written);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_attribute_bo);
  glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, attributes_written * sizeof(fastuidraw::PainterAttribute));
  glUnmapBuffer(GL_ARRAY_BUFFER);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_header_bo);
  glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, attributes_written * sizeof(uint32_t));
  glUnmapBuffer(GL_ARRAY_BUFFER);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vao.m_index_bo);
  glFlushMappedBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, indices_written * sizeof(fastuidraw::PainterIndex));
  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_data_bo);
  glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, data_store_written * sizeof(fastuidraw::generic_data));
  glUnmapBuffer(GL_ARRAY_BUFFER);
}

void
DrawCommand::
add_entry(unsigned int indices_written) const
{
  unsigned int count;
  const fastuidraw::PainterIndex *offset(nullptr);

  if (m_draws.empty())
    {
      m_draws.push_back(fastuidraw::BlendMode());
    }
  FASTUIDRAWassert(indices_written >= m_indices_written);
  count = indices_written - m_indices_written;
  offset += m_indices_written;
  m_draws.back().add_entry(count, offset);
  m_indices_written = indices_written;
}

/////////////////////////////
//SurfaceGLPrivate methods
SurfaceGLPrivate::
SurfaceGLPrivate(GLuint texture,
                 const fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties &props):
  m_clear_color(0.0f, 0.0f, 0.0f, 0.0f),
  m_properties(props),
  m_auxiliary_buffer(0),
  m_buffers(0),
  m_fbo(0),
  m_own_texture(texture != 0)
{
  m_buffers[buffer_color] = texture;
  m_viewport.m_dimensions = props.dimensions();
  m_viewport.m_origin = fastuidraw::ivec2(0, 0);
}

SurfaceGLPrivate::
~SurfaceGLPrivate()
{
  if (!m_own_texture)
    {
      m_buffers[buffer_color] = 0;
    }
  glDeleteTextures(m_auxiliary_buffer.size(), m_auxiliary_buffer.c_ptr());
  glDeleteFramebuffers(m_fbo.size(), m_fbo.c_ptr());
  glDeleteTextures(m_buffers.size(), m_buffers.c_ptr());
}

fastuidraw::gl::PainterBackendGL::SurfaceGL*
SurfaceGLPrivate::
surface_gl(const fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend::Surface> &surface)
{
  fastuidraw::gl::PainterBackendGL::SurfaceGL *q;

  FASTUIDRAWassert(dynamic_cast<fastuidraw::gl::PainterBackendGL::SurfaceGL*>(surface.get()));
  q = static_cast<fastuidraw::gl::PainterBackendGL::SurfaceGL*>(surface.get());
  return q;
}

GLuint
SurfaceGLPrivate::
auxiliary_buffer(enum auxiliary_buffer_t tp)
{
  using namespace fastuidraw;
  using namespace gl;

  if (!m_auxiliary_buffer[tp])
    {
      GLenum internalFormat;
      detail::ClearImageSubData clearer;

      internalFormat = auxiliaryBufferInternalFmt(tp);
      glGenTextures(1, &m_auxiliary_buffer[tp]);
      FASTUIDRAWassert(m_auxiliary_buffer[tp] != 0u);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, m_auxiliary_buffer[tp]);
      detail::tex_storage<GL_TEXTURE_2D>(true,
                                         internalFormat,
                                         m_properties.dimensions());

      clearer.clear<GL_TEXTURE_2D>(m_auxiliary_buffer[tp], 0,
                                   0, 0, 0, //origin
                                   m_properties.dimensions().x(), m_properties.dimensions().y(), 1, //dimensions
                                   detail::format_from_internal_format(internalFormat),
                                   detail::type_from_internal_format(internalFormat));
    }

  return m_auxiliary_buffer[tp];
}

GLuint
SurfaceGLPrivate::
buffer(enum buffer_t tp)
{
  using namespace fastuidraw;
  using namespace gl;

  if (m_buffers[tp] == 0)
    {
      GLenum tex_target, tex_target_binding;
      GLenum internalFormat;
      GLint old_tex;

      tex_target = (m_properties.msaa() <= 1) ?
        GL_TEXTURE_2D :
        GL_TEXTURE_2D_MULTISAMPLE;

      tex_target_binding = (tex_target == GL_TEXTURE_2D) ?
        GL_TEXTURE_BINDING_2D :
        GL_TEXTURE_BINDING_2D_MULTISAMPLE;

      internalFormat = (tp == buffer_color) ?
        GL_RGBA8 :
        GL_DEPTH24_STENCIL8;

      glGetIntegerv(tex_target_binding, &old_tex);
      glGenTextures(1, &m_buffers[tp]);
      FASTUIDRAWassert(m_buffers[tp] != 0);
      glBindTexture(tex_target, m_buffers[tp]);

      if (tex_target == GL_TEXTURE_2D)
        {
          detail::tex_storage<GL_TEXTURE_2D>(true, internalFormat,
                                             m_properties.dimensions());
          glTexParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
      else
        {
          glTexStorage2DMultisample(tex_target, m_properties.msaa(),
                                    internalFormat,
                                    m_properties.dimensions().x(),
                                    m_properties.dimensions().y(),
                                    GL_TRUE);
        }
      glBindTexture(tex_target, old_tex);
    }

  return m_buffers[tp];
}

GLuint
SurfaceGLPrivate::
fbo(enum fbo_t tp)
{
  if (m_fbo[tp] == 0)
    {
      GLint old_fbo;
      GLenum tex_target;

      tex_target = (m_properties.msaa() <= 1) ?
        GL_TEXTURE_2D :
        GL_TEXTURE_2D_MULTISAMPLE;

      glGenFramebuffers(1, &m_fbo[tp]);
      FASTUIDRAWassert(m_fbo[tp] != 0);

      glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo[tp]);

      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                             tex_target, buffer(buffer_depth), 0);
      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             tex_target, buffer(buffer_color), 0);

      if (tp == fbo_with_auxiliary_buffer)
        {
          FASTUIDRAWassert(m_properties.msaa() <= 1);
          glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                 tex_target, auxiliary_buffer(auxiliary_buffer_u8), 0);
        }
      glBindFramebuffer(GL_READ_FRAMEBUFFER, old_fbo);
    }
  return m_fbo[tp];
}

///////////////////////////////////
// PainterBackendGLPrivate methods
PainterBackendGLPrivate::
PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &P,
                        fastuidraw::gl::PainterBackendGL *p):
  m_params(P),
  m_backend_configured(false),
  m_tex_buffer_support(fastuidraw::gl::detail::tex_buffer_not_computed),
  m_number_clip_planes(0),
  m_clip_plane0(GL_INVALID_ENUM),
  m_nearest_filter_sampler(0),
  m_pool(nullptr),
  m_surface_gl(nullptr),
  m_p(p)
{
  configure_backend();
}

PainterBackendGLPrivate::
~PainterBackendGLPrivate()
{
  if (m_nearest_filter_sampler != 0)
    {
      glDeleteSamplers(1, &m_nearest_filter_sampler);
    }

  if (m_pool)
    {
      FASTUIDRAWdelete(m_pool);
    }
}

fastuidraw::PainterBackend::ConfigurationBase
PainterBackendGLPrivate::
compute_base_config(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
                    const fastuidraw::PainterBackend::ConfigurationBase &config_base)
{
  using namespace fastuidraw;
  PainterBackend::ConfigurationBase return_value(config_base);

  if (params.data_store_backing() == gl::PainterBackendGL::data_store_ubo
     || gl::detail::compute_tex_buffer_support() == gl::detail::tex_buffer_not_supported)
    {
      //using UBO's requires that the data store alignment is 4.
      return_value.alignment(4);
    }

  /* bleck: framebuffer fetch on auxiliary buffer is
   * not compatible with single_src and dual_src
   * blending, need to override blend_type() for
   * that case.
   */
  fastuidraw::gl::ContextProperties ctx;
  return_value
    .supports_bindless_texturing(ctx.has_extension("GL_ARB_bindless_texture") || ctx.has_extension("GL_NV_bindless_texture"))
    .blend_type(compute_blend_type(compute_provide_auxiliary_buffer(params.provide_auxiliary_image_buffer(), ctx),
                                   params.blend_type(),
                                   ctx));
    
  return return_value;
}

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL
PainterBackendGLPrivate::
compute_glsl_config(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params)
{
  using namespace fastuidraw;
  enum glsl::PainterBackendGLSL::auxiliary_buffer_t aux_type;
  glsl::PainterBackendGLSL::ConfigurationGLSL return_value;
  gl::ContextProperties ctx;

  aux_type = params.provide_auxiliary_image_buffer();
  aux_type = compute_provide_auxiliary_buffer(aux_type, ctx);
  if (aux_type != glsl::PainterBackendGLSL::no_auxiliary_buffer)
    {
      return_value
        .default_stroke_shader_aa_type(params.default_stroke_shader_aa_type());
    }
  else
    {
      return_value
        .default_stroke_shader_aa_type(PainterStrokeShader::draws_solid_then_fuzz);
    }

  return_value.clipping_type(compute_clipping_type(params.clipping_type(), ctx));

  if (return_value.default_stroke_shader_aa_type() == PainterStrokeShader::cover_then_draw
      && aux_type == glsl::PainterBackendGLSL::auxiliary_buffer_atomic)
    {
      bool use_by_region;
      fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> q;

      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          use_by_region = true;
        }
      #else
        {
          use_by_region = ctx.version() >= ivec2(4, 5)
            || ctx.has_extension("GL_ARB_ES3_1_compatibility");
        }
      #endif

      if (use_by_region)
        {
          q = FASTUIDRAWnew ImageBarrierByRegion();
        }
      else
        {
          q = FASTUIDRAWnew ImageBarrier();
        }
      return_value
        .default_stroke_shader_aa_pass1_action(q)
        .default_stroke_shader_aa_pass2_action(q);
    }

  return return_value;
}

void
PainterBackendGLPrivate::
configure_backend(void)
{
  using namespace fastuidraw;
  using namespace gl;

  FASTUIDRAWassert(!m_backend_configured);

  m_backend_configured = true;
  m_tex_buffer_support = detail::compute_tex_buffer_support();

  if (m_params.data_store_backing() == PainterBackendGL::data_store_tbo
     && m_tex_buffer_support == detail::tex_buffer_not_supported)
    {
      // TBO's not supported, fall back to using UBO's.
      m_params.data_store_backing(PainterBackendGL::data_store_ubo);
    }

  /* Query GL what is good size for data store buffer. Size is dependent
   * how the data store is backed.
   */
  switch(m_params.data_store_backing())
    {
    case fastuidraw::gl::PainterBackendGL::data_store_tbo:
      {
        unsigned int max_texture_buffer_size(0);
        max_texture_buffer_size = context_get<GLint>(GL_MAX_TEXTURE_BUFFER_SIZE);
        m_params.data_blocks_per_store_buffer(t_min(max_texture_buffer_size,
                                                    m_params.data_blocks_per_store_buffer()));
      }
      break;

    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      {
        unsigned int max_ubo_size_bytes, max_num_blocks, block_size_bytes;
        block_size_bytes = m_p->configuration_base().alignment() * sizeof(fastuidraw::generic_data);
        max_ubo_size_bytes = context_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE);
        max_num_blocks = max_ubo_size_bytes / block_size_bytes;
        m_params.data_blocks_per_store_buffer(t_min(max_num_blocks,
                                                    m_params.data_blocks_per_store_buffer()));
      }
    }

  m_params.clipping_type(m_p->configuration_glsl().clipping_type());
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (m_ctx_properties.has_extension("GL_EXT_clip_cull_distance"))
        {
          m_number_clip_planes = context_get<GLint>(GL_MAX_CLIP_DISTANCES_EXT );
          m_clip_plane0 = GL_CLIP_DISTANCE0_EXT;
          m_gles_clip_plane_extension = "GL_EXT_clip_cull_distance";
        }
      else if (m_ctx_properties.has_extension("GL_APPLE_clip_distance"))
        {
          m_number_clip_planes = context_get<GLint>(GL_MAX_CLIP_DISTANCES_APPLE);
          m_clip_plane0 = GL_CLIP_DISTANCE0_APPLE;
          m_gles_clip_plane_extension = "GL_APPLE_clip_distance";
        }
      else
        {
          m_number_clip_planes = 0;
          m_clip_plane0 = GL_INVALID_ENUM;
        }
    }
  #else
    {
      m_number_clip_planes = context_get<GLint>(GL_MAX_CLIP_DISTANCES);
      m_clip_plane0 = GL_CLIP_DISTANCE0;
    }
  #endif

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      m_has_multi_draw_elements = m_ctx_properties.has_extension("GL_EXT_multi_draw_arrays");
    }
  #else
    {
      m_has_multi_draw_elements = true;
    }
  #endif

  /* if have to use discard for clipping, then there is zero point to
   * separate the discarding and non-discarding item shaders.
   */
  m_params.separate_program_for_discard(m_params.separate_program_for_discard()
                                        && m_params.clipping_type() == PainterBackendGL::clipping_via_discard);

  fastuidraw::gl::ColorStopAtlasGL *color;
  FASTUIDRAWassert(dynamic_cast<ColorStopAtlasGL*>(m_params.colorstop_atlas().get()));
  color = static_cast<ColorStopAtlasGL*>(m_params.colorstop_atlas().get());
  enum PainterBackendGL::colorstop_backing_t colorstop_tp;
  if (color->texture_bind_target() == GL_TEXTURE_2D_ARRAY)
    {
      colorstop_tp = PainterBackendGL::colorstop_texture_2d_array;
    }
  else
    {
      colorstop_tp = PainterBackendGL::colorstop_texture_1d_array;
    }

  /* Some shader features require new version of GL or
   * specific extensions.
   */
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (m_ctx_properties.version() < ivec2(3, 2))
        {
          m_params.assign_layout_to_varyings(m_params.assign_layout_to_varyings()
                                             && m_ctx_properties.has_extension("GL_EXT_separate_shader_objects"));
        }

      if (m_ctx_properties.version() <= ivec2(3, 0))
        {
          /* GL ES 3.0 does not support layout(binding=) and
           * does not support image-load-store either
           */
          m_params.assign_binding_points(false);
        }
    }
  #else
    {
      if (m_ctx_properties.version() < ivec2(4, 2))
        {
          m_params.assign_layout_to_varyings(m_params.assign_layout_to_varyings()
                                             && m_ctx_properties.has_extension("GL_ARB_separate_shader_objects"));
          m_params.assign_binding_points(m_params.assign_binding_points()
                                         && m_ctx_properties.has_extension("GL_ARB_shading_language_420pack"));
        }
    }
  #endif

  m_interlock_type = compute_interlock_type(m_ctx_properties);
  m_params.provide_auxiliary_image_buffer(compute_provide_auxiliary_buffer(m_params.provide_auxiliary_image_buffer(),
                                                                           m_ctx_properties));
  m_params.blend_type(compute_blend_type(m_params.provide_auxiliary_image_buffer(),
                                         m_params.blend_type(),
                                         m_ctx_properties));

  if (m_params.provide_auxiliary_image_buffer() == PainterBackendGL::no_auxiliary_buffer)
    {
      m_params.default_stroke_shader_aa_type(PainterStrokeShader::draws_solid_then_fuzz);
    }

  m_uber_shader_builder_params
    .assign_layout_to_vertex_shader_inputs(m_params.assign_layout_to_vertex_shader_inputs())
    .assign_layout_to_varyings(m_params.assign_layout_to_varyings())
    .assign_binding_points(m_params.assign_binding_points())
    .use_ubo_for_uniforms(true)
    .z_coordinate_convention(PainterBackendGL::z_minus_1_to_1)
    .negate_normalized_y_coordinate(false)
    .vert_shader_use_switch(m_params.vert_shader_use_switch())
    .frag_shader_use_switch(m_params.frag_shader_use_switch())
    .blend_shader_use_switch(m_params.blend_shader_use_switch())
    .unpack_header_and_brush_in_frag_shader(m_params.unpack_header_and_brush_in_frag_shader())
    .data_store_backing(m_params.data_store_backing())
    .data_blocks_per_store_buffer(m_params.data_blocks_per_store_buffer())
    .glyph_geometry_backing(m_params.glyph_atlas()->param_values().glyph_geometry_backing_store_type())
    .glyph_geometry_backing_log2_dims(m_params.glyph_atlas()->param_values().texture_2d_array_geometry_store_log2_dims())
    .have_float_glyph_texture_atlas(m_params.glyph_atlas()->texel_texture(false) != 0)
    .colorstop_atlas_backing(colorstop_tp)
    .provide_auxiliary_image_buffer(m_params.provide_auxiliary_image_buffer())
    .use_uvec2_for_bindless_handle(m_ctx_properties.has_extension("GL_ARB_bindless_texture"));

  /* now allocate m_pool after adjusting m_params */
  m_pool = FASTUIDRAWnew painter_vao_pool(m_params, m_p->configuration_base(),
                                          m_tex_buffer_support,
                                          m_uber_shader_builder_params.binding_points());

  configure_source_front_matter();
}

void
PainterBackendGLPrivate::
configure_source_front_matter(void)
{
  using namespace fastuidraw::glsl;

  if (!m_uber_shader_builder_params.assign_binding_points())
    {
      const PainterBackendGLSL::BindingPoints &binding_points(m_uber_shader_builder_params.binding_points());
      m_initializer
        .add_sampler_initializer("fastuidraw_imageAtlasLinear", binding_points.image_atlas_color_tiles_linear())
        .add_sampler_initializer("fastuidraw_imageAtlasNearest", binding_points.image_atlas_color_tiles_nearest())
        .add_sampler_initializer("fastuidraw_imageIndexAtlas", binding_points.image_atlas_index_tiles())
        .add_sampler_initializer("fastuidraw_glyphTexelStoreUINT", binding_points.glyph_atlas_texel_store_uint())
        .add_sampler_initializer("fastuidraw_glyphGeometryDataStore", binding_points.glyph_atlas_geometry_store())
        .add_sampler_initializer("fastuidraw_colorStopAtlas", binding_points.colorstop_atlas())
        .add_uniform_block_binding("fastuidraw_uniform_block", binding_points.uniforms_ubo());

      if (m_uber_shader_builder_params.have_float_glyph_texture_atlas())
        {
          m_initializer.add_sampler_initializer("fastuidraw_glyphTexelStoreFLOAT", binding_points.glyph_atlas_texel_store_float());
        }

      switch(m_uber_shader_builder_params.data_store_backing())
        {
        case PainterBackendGLSL::data_store_tbo:
          {
            m_initializer.add_sampler_initializer("fastuidraw_painterStore_tbo", binding_points.data_store_buffer_tbo());
          }
          break;

        case PainterBackendGLSL::data_store_ubo:
          {
            m_initializer.add_uniform_block_binding("fastuidraw_painterStore_ubo", binding_points.data_store_buffer_ubo());
          }
          break;
        }
    }

  if (!m_uber_shader_builder_params.assign_layout_to_vertex_shader_inputs())
    {
      m_attribute_binder
        .add_binding("fastuidraw_primary_attribute", PainterBackendGLSL::primary_attrib_slot)
        .add_binding("fastuidraw_secondary_attribute", PainterBackendGLSL::secondary_attrib_slot)
        .add_binding("fastuidraw_uint_attribute", PainterBackendGLSL::uint_attrib_slot)
        .add_binding("fastuidraw_header_attribute", PainterBackendGLSL::header_attrib_slot);
    }

  if (m_uber_shader_builder_params.provide_auxiliary_image_buffer())
    {
      switch(m_interlock_type)
        {
        case no_interlock:
          m_front_matter_frag
            .add_macro("fastuidraw_begin_interlock", "fastuidraw_do_nothing")
            .add_macro("fastuidraw_end_interlock", "fastuidraw_do_nothing");
          break;

        case intel_fragment_shader_ordering:
          m_front_matter_frag
            .add_macro("fastuidraw_begin_interlock", "beginFragmentShaderOrderingINTEL")
            .add_macro("fastuidraw_end_interlock", "fastuidraw_do_nothing");
          break;

        case arb_fragment_shader_interlock:
          m_front_matter_frag
            .add_macro("fastuidraw_begin_interlock", "beginInvocationInterlockARB")
            .add_macro("fastuidraw_end_interlock", "endInvocationInterlockARB");
          break;

        case nv_fragment_shader_interlock:
          m_front_matter_frag
            .add_macro("fastuidraw_begin_interlock", "beginInvocationInterlockNV")
            .add_macro("fastuidraw_end_interlock", "endInvocationInterlockNV");
          break;
        }

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

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (m_p->configuration_glsl().use_hw_clip_planes())
        {
          m_front_matter_vert.specify_extension(m_gles_clip_plane_extension.c_str(), ShaderSource::require_extension);
        }

      if (m_ctx_properties.version() >= fastuidraw::ivec2(3, 2))
        {
          m_front_matter_vert
            .specify_version("320 es");
          m_front_matter_frag
            .specify_version("320 es")
            .specify_extension("GL_EXT_shader_framebuffer_fetch", ShaderSource::enable_extension)
            .specify_extension("GL_EXT_blend_func_extended", ShaderSource::enable_extension);
        }
      else
        {
          std::string version;
          if (m_ctx_properties.version() >= fastuidraw::ivec2(3, 1))
            {
              version = "310 es";
            }
          else
            {
              version = "300 es";
            }

          if (m_uber_shader_builder_params.assign_layout_to_varyings())
            {
              m_front_matter_vert.specify_extension("GL_EXT_separate_shader_objects", ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_EXT_separate_shader_objects", ShaderSource::require_extension);
            }

          m_front_matter_vert
            .specify_version(version.c_str())
            .specify_extension("GL_EXT_texture_buffer", ShaderSource::enable_extension)
            .specify_extension("GL_OES_texture_buffer", ShaderSource::enable_extension);

          m_front_matter_frag
            .specify_version(version.c_str())
            .specify_extension("GL_EXT_shader_framebuffer_fetch", ShaderSource::enable_extension)
            .specify_extension("GL_EXT_blend_func_extended", ShaderSource::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", ShaderSource::enable_extension)
            .specify_extension("GL_OES_texture_buffer", ShaderSource::enable_extension);
        }
      m_front_matter_vert.add_source("fastuidraw_painter_gles_precision.glsl.resource_string", ShaderSource::from_resource);
      m_front_matter_frag.add_source("fastuidraw_painter_gles_precision.glsl.resource_string", ShaderSource::from_resource);
    }
  #else
    {
      bool using_glsl42;

      using_glsl42 = m_ctx_properties.version() >= fastuidraw::ivec2(4, 2)
        && (m_uber_shader_builder_params.assign_layout_to_varyings()
            || m_uber_shader_builder_params.assign_binding_points()
            || m_uber_shader_builder_params.provide_auxiliary_image_buffer() != PainterBackendGLSL::no_auxiliary_buffer);

      m_front_matter_frag
        .specify_extension("GL_EXT_shader_framebuffer_fetch", ShaderSource::enable_extension);

      if (using_glsl42)
        {
          m_front_matter_vert.specify_version("420");
          m_front_matter_frag.specify_version("420");
        }
      else
        {
          m_front_matter_vert.specify_version("330");
          m_front_matter_frag.specify_version("330");

          if (m_uber_shader_builder_params.assign_layout_to_varyings())
            {
              m_front_matter_vert.specify_extension("GL_ARB_separate_shader_objects", ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_ARB_separate_shader_objects", ShaderSource::require_extension);
            }

          if (m_uber_shader_builder_params.assign_binding_points())
            {
              m_front_matter_vert.specify_extension("GL_ARB_shading_language_420pack", ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_ARB_shading_language_420pack", ShaderSource::require_extension);
            }

          if (m_uber_shader_builder_params.provide_auxiliary_image_buffer() != PainterBackendGLSL::no_auxiliary_buffer)
            {
              m_front_matter_frag
                .specify_extension("GL_ARB_shader_image_load_store", ShaderSource::require_extension);
            }
        }
    }
  #endif

  switch(m_interlock_type)
    {
    case intel_fragment_shader_ordering:
      m_front_matter_frag
        .specify_extension("GL_INTEL_fragment_shader_ordering", ShaderSource::require_extension);
      break;

    case nv_fragment_shader_interlock:
      m_front_matter_frag
        .specify_extension("GL_NV_fragment_shader_interlock", ShaderSource::require_extension);
      break;

    case arb_fragment_shader_interlock:
      m_front_matter_frag
        .specify_extension("GL_ARB_fragment_shader_interlock", ShaderSource::require_extension);
      break;

    default:
      break;
    }

  if (m_p->configuration_base().supports_bindless_texturing())
    {
      if (m_uber_shader_builder_params.use_uvec2_for_bindless_handle())
        {
          m_front_matter_frag
            .specify_extension("GL_ARB_bindless_texture", ShaderSource::enable_extension);

          m_front_matter_vert
            .specify_extension("GL_ARB_bindless_texture", ShaderSource::enable_extension);
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

const PainterBackendGLPrivate::program_set&
PainterBackendGLPrivate::
programs(bool rebuild)
{
  if (rebuild)
    {
      build_programs();
    }
  return m_programs;
}

void
PainterBackendGLPrivate::
build_programs(void)
{
  for(unsigned int i = 0; i < fastuidraw::gl::PainterBackendGL::number_program_types; ++i)
    {
      enum fastuidraw::gl::PainterBackendGL::program_type_t tp;
      tp = static_cast<enum fastuidraw::gl::PainterBackendGL::program_type_t>(i);
      m_programs[tp] = build_program(tp);
      FASTUIDRAWassert(m_programs[tp]->link_success());
    }

  m_uniform_values.resize(m_p->ubo_size());
  m_uniform_values_ptr = fastuidraw::c_array<fastuidraw::generic_data>(&m_uniform_values[0],
                                                                       m_uniform_values.size());
}

PainterBackendGLPrivate::program_ref
PainterBackendGLPrivate::
build_program(enum fastuidraw::gl::PainterBackendGL::program_type_t tp)
{
  fastuidraw::glsl::ShaderSource vert, frag;
  program_ref return_value;
  DiscardItemShaderFilter item_filter(tp, m_params.clipping_type());
  fastuidraw::c_string discard_macro;

  if (tp == fastuidraw::gl::PainterBackendGL::program_without_discard)
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

  enum fastuidraw::return_code R;
  R = m_p->construct_shader(vert, frag,
                            m_uber_shader_builder_params,
                            &item_filter, discard_macro);
  FASTUIDRAWassert(R == fastuidraw::routine_success);
  FASTUIDRAWunused(R);

  return_value = FASTUIDRAWnew fastuidraw::gl::Program(vert, frag,
                                                       m_attribute_binder,
                                                       m_initializer);
  return return_value;
}

void
PainterBackendGLPrivate::
set_gl_state(fastuidraw::gpu_dirty_state v, bool clear_depth, bool clear_color_buffer)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;

  const PainterBackendGL::UberShaderParams &uber_params(m_uber_shader_builder_params);
  const PainterBackendGL::BindingPoints &binding_points(uber_params.binding_points());
  enum PainterBackendGL::auxiliary_buffer_t aux_type;
  const PainterBackend::Surface::Viewport &vwp(m_surface_gl->m_viewport);
  ivec2 dimensions(m_surface_gl->m_properties.dimensions());

  aux_type = uber_params.provide_auxiliary_image_buffer();
  if (v & gpu_dirty_state::images
      && aux_type != PainterBackendGL::no_auxiliary_buffer
      && aux_type != PainterBackendGL::auxiliary_buffer_framebuffer_fetch)
    {
      SurfaceGLPrivate::auxiliary_buffer_t tp;

      tp = (aux_type == PainterBackendGL::auxiliary_buffer_atomic) ?
        SurfaceGLPrivate::auxiliary_buffer_u32 :
        SurfaceGLPrivate::auxiliary_buffer_u8;

      glBindImageTexture(binding_points.auxiliary_image_buffer(),
                         m_surface_gl->auxiliary_buffer(tp), //texture
                         0, //level
                         GL_FALSE, //layered
                         0, //layer
                         GL_READ_WRITE, //access
                         SurfaceGLPrivate::auxiliaryBufferInternalFmt(tp));
    }

  if (v & gpu_dirty_state::render_target)
    {
      enum SurfaceGLPrivate::fbo_t fbo_tp(SurfaceGLPrivate::fbo_tp(aux_type));
      GLuint fbo(m_surface_gl->fbo(fbo_tp));
      vecN<GLenum, 2> draw_buffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1);

      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
      if (clear_depth)
        {
          GLbitfield mask(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
          if (clear_color_buffer)
            {
              glClearColor(m_surface_gl->m_clear_color.x(),
                           m_surface_gl->m_clear_color.y(),
                           m_surface_gl->m_clear_color.z(),
                           m_surface_gl->m_clear_color.w());
              mask |= GL_COLOR_BUFFER_BIT;
            }
          glDrawBuffers(1, draw_buffers.c_ptr());
          glClear(mask);
        }
      else
        {
          FASTUIDRAWassert(!clear_color_buffer);
        }

      if (fbo_tp == SurfaceGLPrivate::fbo_with_auxiliary_buffer)
        {
          glDrawBuffers(2, draw_buffers.c_ptr());
        }
      else
        {
          glDrawBuffers(1, draw_buffers.c_ptr());
        }

      v |= gpu_dirty_state::viewport_scissor;
    }
  else
    {
      FASTUIDRAWassert(!clear_color_buffer);
      FASTUIDRAWassert(!clear_depth);
    }

  if (v & gpu_dirty_state::depth_stencil)
    {
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_GEQUAL);
      glDisable(GL_STENCIL_TEST);

      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          glClearDepthf(0.0f);
        }
      #else
        {
          glClearDepth(0.0f);
        }
      #endif
    }

  if (v & gpu_dirty_state::buffer_masks)
    {
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDepthMask(GL_TRUE);
    }

  if (v & gpu_dirty_state::viewport_scissor)
    {
      if (dimensions.x() > vwp.m_dimensions.x()
          || dimensions.y() > vwp.m_dimensions.y()
          || vwp.m_origin.x() != 0
          || vwp.m_origin.y() != 0)
        {
          glEnable(GL_SCISSOR_TEST);
          glScissor(vwp.m_origin.x(), vwp.m_origin.y(),
                    vwp.m_dimensions.x(), vwp.m_dimensions.y());
        }
      else
        {
          glDisable(GL_SCISSOR_TEST);
        }

      glViewport(vwp.m_origin.x(), vwp.m_origin.y(),
                 vwp.m_dimensions.x(), vwp.m_dimensions.y());
    }

  if ((v & gpu_dirty_state::hw_clip) && m_number_clip_planes > 0)
    {
      if (m_params.clipping_type() == PainterBackendGL::clipping_via_clip_distance)
        {
          for (int i = 0; i < 4; ++i)
            {
              glEnable(m_clip_plane0 + i);
            }
        }
      else
        {
          for (int i = 0; i < 4; ++i)
            {
              glDisable(m_clip_plane0 + i);
            }
        }

      for(int i = 4; i < m_number_clip_planes; ++i)
        {
          glDisable(m_clip_plane0 + i);
        }
    }

  if (v & gpu_dirty_state::textures)
    {
      GlyphAtlasGL *glyphs;
      FASTUIDRAWassert(dynamic_cast<GlyphAtlasGL*>(m_p->glyph_atlas().get()));
      glyphs = static_cast<GlyphAtlasGL*>(m_p->glyph_atlas().get());

      ImageAtlasGL *image;
      FASTUIDRAWassert(dynamic_cast<ImageAtlasGL*>(m_p->image_atlas().get()));
      image = static_cast<ImageAtlasGL*>(m_p->image_atlas().get());

      ColorStopAtlasGL *color;
      FASTUIDRAWassert(dynamic_cast<ColorStopAtlasGL*>(m_p->colorstop_atlas().get()));
      color = static_cast<ColorStopAtlasGL*>(m_p->colorstop_atlas().get());

      glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_nearest());
      glBindSampler(binding_points.image_atlas_color_tiles_nearest(), m_nearest_filter_sampler);
      glBindTexture(GL_TEXTURE_2D_ARRAY, image->color_texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_linear());
      glBindSampler(binding_points.image_atlas_color_tiles_linear(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, image->color_texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_index_tiles());
      glBindSampler(binding_points.image_atlas_index_tiles(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, image->index_texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_uint());
      glBindSampler(binding_points.glyph_atlas_texel_store_uint(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, glyphs->texel_texture(true));

      glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_float());
      glBindSampler(binding_points.glyph_atlas_texel_store_float(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, glyphs->texel_texture(false));

      glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_geometry_store());
      glBindSampler(binding_points.glyph_atlas_geometry_store(), 0);
      glBindTexture(glyphs->geometry_texture_binding_point(), glyphs->geometry_texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.colorstop_atlas());
      glBindSampler(binding_points.colorstop_atlas(), 0);
      glBindTexture(ColorStopAtlasGL::texture_bind_target(), color->texture());
    }

  if (v & gpu_dirty_state::constant_buffers)
    {
      GLuint ubo;
      unsigned int size_generics(m_p->ubo_size());
      unsigned int size_bytes(sizeof(generic_data) * size_generics);
      void *ubo_mapped;

      /* Grabs and binds the buffer */
      ubo = m_pool->request_uniform_ubo(size_bytes, GL_UNIFORM_BUFFER);
      FASTUIDRAWassert(ubo != 0);
      ubo_mapped = glMapBufferRange(GL_UNIFORM_BUFFER, 0, size_bytes,
                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

      m_p->fill_uniform_buffer(c_array<generic_data>(static_cast<generic_data*>(ubo_mapped), size_generics));
      glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, size_bytes);
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBufferBase(GL_UNIFORM_BUFFER, m_uber_shader_builder_params.binding_points().uniforms_ubo(), ubo);
    }
}

/////////////////////////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties methods
fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties::
Properties(void)
{
  m_d = FASTUIDRAWnew SurfacePropertiesPrivate();
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties::
Properties(const Properties &obj)
{
  SurfacePropertiesPrivate *d;
  d = static_cast<SurfacePropertiesPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew SurfacePropertiesPrivate(*d);
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties::
~Properties()
{
  SurfacePropertiesPrivate *d;
  d = static_cast<SurfacePropertiesPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

assign_swap_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties)

setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties,
                 SurfacePropertiesPrivate,
                 fastuidraw::ivec2, dimensions);

setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties,
                 SurfacePropertiesPrivate,
                 unsigned int, msaa);

///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL::SurfaceGL methods
fastuidraw::gl::PainterBackendGL::SurfaceGL::
SurfaceGL(const Properties &props)
{
  m_d = FASTUIDRAWnew SurfaceGLPrivate(0u, props);
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::
SurfaceGL(const Properties &prop, GLuint color_buffer_texture)
{
  m_d = FASTUIDRAWnew SurfaceGLPrivate(color_buffer_texture, prop);
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::
~SurfaceGL()
{
  SurfaceGLPrivate *d;
  d = static_cast<SurfaceGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

GLuint
fastuidraw::gl::PainterBackendGL::SurfaceGL::
texture(void) const
{
  SurfaceGLPrivate *d;
  d = static_cast<SurfaceGLPrivate*>(m_d);
  return d->color_buffer();
}

void
fastuidraw::gl::PainterBackendGL::SurfaceGL::
blit_surface(const Viewport &src,
             const Viewport &dst,
             GLenum filter) const
{
  SurfaceGLPrivate *d;
  d = static_cast<SurfaceGLPrivate*>(m_d);
  GLint old_fbo;

  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo(SurfaceGLPrivate::fbo_just_color));
  glBlitFramebuffer(src.m_origin.x(),
                    src.m_origin.y(),
                    src.m_origin.x() + src.m_dimensions.x(),
                    src.m_origin.y() + src.m_dimensions.y(),
                    dst.m_origin.x(),
                    dst.m_origin.y(),
                    dst.m_origin.x() + dst.m_dimensions.x(),
                    dst.m_origin.y() + dst.m_dimensions.y(),
                    GL_COLOR_BUFFER_BIT, filter);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, old_fbo);
}

void
fastuidraw::gl::PainterBackendGL::SurfaceGL::
blit_surface(GLenum filter) const
{
  ivec2 dims(dimensions());
  Viewport vwp(0, 0, dims.x(), dims.y());
  blit_surface(vwp, vwp, filter);
}

fastuidraw::ivec2
fastuidraw::gl::PainterBackendGL::SurfaceGL::
dimensions(void) const
{
  return properties().dimensions();
}

get_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL, SurfaceGLPrivate,
              const fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties&,
              properties)
setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL, SurfaceGLPrivate,
                 fastuidraw::PainterBackend::Surface::Viewport, viewport);
setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL, SurfaceGLPrivate,
                 const fastuidraw::vec4&, clear_color)

///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL::ConfigurationGL methods
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
ConfigurationGL(void)
{
  m_d = FASTUIDRAWnew ConfigurationGLPrivate();
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL::
ConfigurationGL(const ConfigurationGL &obj)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationGLPrivate(*d);
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL::
~ConfigurationGL()
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL)

setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, attributes_per_buffer)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, indices_per_buffer)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, data_blocks_per_store_buffer)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, number_pools)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, break_on_shader_change)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL>&, image_atlas)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::gl::ColorStopAtlasGL>&, colorstop_atlas)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL>&, glyph_atlas)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterBackendGL::clipping_type_t, clipping_type)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, vert_shader_use_switch)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, frag_shader_use_switch)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, blend_shader_use_switch)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, unpack_header_and_brush_in_frag_shader)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterBackendGL::data_store_backing_t, data_store_backing)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_layout_to_vertex_shader_inputs)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_layout_to_varyings)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_binding_points)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, separate_program_for_discard)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::PainterStrokeShader::type_t, default_stroke_shader_aa_type)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::PainterBlendShader::shader_type, blend_type)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t, provide_auxiliary_image_buffer)

///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL methods
fastuidraw::gl::PainterBackendGL::
PainterBackendGL(const ConfigurationGL &config_gl,
                 const ConfigurationBase &config_base):
  PainterBackendGLSL(config_gl.glyph_atlas(),
                     config_gl.image_atlas(),
                     config_gl.colorstop_atlas(),
                     PainterBackendGLPrivate::compute_glsl_config(config_gl),
                     PainterBackendGLPrivate::compute_base_config(config_gl, config_base))
{
  m_d = FASTUIDRAWnew PainterBackendGLPrivate(config_gl, this);
}

fastuidraw::gl::PainterBackendGL::
~PainterBackendGL()
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::Program>
fastuidraw::gl::PainterBackendGL::
program(enum program_type_t tp)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return d->programs(shader_code_added())[tp];
}

const fastuidraw::gl::PainterBackendGL::ConfigurationGL&
fastuidraw::gl::PainterBackendGL::
configuration_gl(void) const
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return d->m_params;
}

uint32_t
fastuidraw::gl::PainterBackendGL::
compute_item_shader_group(PainterShader::Tag tag,
                          const reference_counted_ptr<PainterItemShader> &shader)
{
  bool b;
  uint32_t return_value;

  b = configuration_gl().break_on_shader_change();
  return_value = (b) ? tag.m_ID : 0u;
  return_value |= (shader_group_discard_mask & tag.m_group);

  if (configuration_gl().separate_program_for_discard())
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
fastuidraw::gl::PainterBackendGL::
compute_blend_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterBlendShader> &shader)
{
  bool b;
  uint32_t return_value;

  FASTUIDRAWunused(shader);
  b = configuration_gl().break_on_shader_change();
  return_value = (b) ? tag.m_ID : 0u;
  return return_value;
}

unsigned int
fastuidraw::gl::PainterBackendGL::
attribs_per_mapping(void) const
{
  return configuration_gl().attributes_per_buffer();
}

unsigned int
fastuidraw::gl::PainterBackendGL::
indices_per_mapping(void) const
{
  return configuration_gl().indices_per_buffer();
}

void
fastuidraw::gl::PainterBackendGL::
on_pre_draw(const reference_counted_ptr<Surface> &surface,
            bool clear_color_buffer)
{
  PainterBackendGLPrivate *d;

  d = static_cast<PainterBackendGLPrivate*>(m_d);
  d->m_surface_gl = static_cast<SurfaceGLPrivate*>(SurfaceGLPrivate::surface_gl(surface)->m_d);

  if (d->m_nearest_filter_sampler == 0)
    {
      glGenSamplers(1, &d->m_nearest_filter_sampler);
      FASTUIDRAWassert(d->m_nearest_filter_sampler != 0);
      glSamplerParameteri(d->m_nearest_filter_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glSamplerParameteri(d->m_nearest_filter_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    }

  PainterBackendGLSL::viewport(d->m_surface_gl->m_viewport);
  d->set_gl_state(gpu_dirty_state::all, true, clear_color_buffer);

  //makes sure that the GLSL programs are built.
  d->programs(shader_code_added());
  FASTUIDRAWassert(!shader_code_added());
}

void
fastuidraw::gl::PainterBackendGL::
on_post_draw(void)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);

  /* this is somewhat paranoid to make sure that
   * the GL objects do not leak...
   */
  glUseProgram(0);
  glBindVertexArray(0);

  const glsl::PainterBackendGLSL::UberShaderParams &uber_params(d->m_uber_shader_builder_params);
  const glsl::PainterBackendGLSL::BindingPoints &binding_points(uber_params.binding_points());

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_nearest());
  glBindSampler(binding_points.image_atlas_color_tiles_nearest(), 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_linear());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_index_tiles());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_uint());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_float());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  fastuidraw::gl::GlyphAtlasGL *glyphs;
  FASTUIDRAWassert(dynamic_cast<fastuidraw::gl::GlyphAtlasGL*>(glyph_atlas().get()));
  glyphs = static_cast<fastuidraw::gl::GlyphAtlasGL*>(glyph_atlas().get());

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_geometry_store());
  glBindTexture(glyphs->geometry_texture_binding_point(), 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.colorstop_atlas());
  glBindTexture(ColorStopAtlasGL::texture_bind_target(), 0);

  enum glsl::PainterBackendGLSL::auxiliary_buffer_t aux_type;
  aux_type = d->m_uber_shader_builder_params.provide_auxiliary_image_buffer();
  if (aux_type != fastuidraw::glsl::PainterBackendGLSL::no_auxiliary_buffer
      && aux_type != fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_framebuffer_fetch)
    {
      glBindImageTexture(binding_points.auxiliary_image_buffer(), 0,
			 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);
    }

  switch(d->m_params.data_store_backing())
    {
    case fastuidraw::gl::PainterBackendGL::data_store_tbo:
      {
        glActiveTexture(GL_TEXTURE0 + binding_points.data_store_buffer_tbo());
        glBindTexture(GL_TEXTURE_BUFFER, 0);
      }
      break;

    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding_points.data_store_buffer_ubo(), 0);
      }
      break;

    default:
      FASTUIDRAWassert(!"Bad value for m_params.data_store_backing()");
    }
  glBindBufferBase(GL_UNIFORM_BUFFER, binding_points.uniforms_ubo(), 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  d->m_pool->next_pool();
}

fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw>
fastuidraw::gl::PainterBackendGL::
map_draw(void)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);

  return FASTUIDRAWnew DrawCommand(d->m_pool, d->m_params, d);
}

const fastuidraw::gl::PainterBackendGL::BindingPoints&
fastuidraw::gl::PainterBackendGL::
binding_points(void) const
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return d->m_uber_shader_builder_params.binding_points();
}
