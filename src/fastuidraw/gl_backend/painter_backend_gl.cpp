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

#include "private/tex_buffer.hpp"

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
    DiscardItemShaderFilter(enum fastuidraw::gl::PainterBackendGL::program_type_t tp):
      m_tp(tp)
    {}

    bool
    use_shader(const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &shader) const
    {
      return use_shader_helper(m_tp, shader->uses_discard());
    }

  private:
    enum fastuidraw::gl::PainterBackendGL::program_type_t m_tp;
  };

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
    build_vao_tbos(void);

    fastuidraw::gl::PainterBackendGL::ConfigurationGL m_params;
    fastuidraw::glsl::PainterBackendGLSL::UberShaderParams m_uber_shader_builder_params;

    fastuidraw::gl::ContextProperties m_ctx_properties;
    bool m_backend_configured;
    enum fastuidraw::gl::detail::tex_buffer_support_t m_tex_buffer_support;
    int m_number_clip_planes;
    GLenum m_clip_plane0;
    std::string m_gles_clip_plane_extension;

    GLuint m_linear_filter_sampler;
    fastuidraw::gl::PreLinkActionArray m_attribute_binder;
    fastuidraw::gl::ProgramInitializerArray m_initializer;
    fastuidraw::glsl::ShaderSource m_front_matter_vert;
    fastuidraw::glsl::ShaderSource m_front_matter_frag;
    program_set m_programs;
    fastuidraw::vecN<GLint, fastuidraw::gl::PainterBackendGL::number_program_types> m_shader_uniforms_loc;
    std::vector<fastuidraw::generic_data> m_uniform_values;
    fastuidraw::c_array<fastuidraw::generic_data> m_uniform_values_ptr;
    painter_vao_pool *m_pool;

    fastuidraw::gl::PainterBackendGL *m_p;
  };

  class DrawEntry
  {
  public:
    DrawEntry(const fastuidraw::BlendMode &mode,
              PainterBackendGLPrivate *pr,
              unsigned int pz);


    DrawEntry(const fastuidraw::BlendMode &mode);

    void
    add_entry(GLsizei count, const void *offset);

    void
    draw(void) const;

  private:

    static
    GLenum
    convert_blend_op(enum fastuidraw::BlendMode::op_t v);

    static
    GLenum
    convert_blend_func(enum fastuidraw::BlendMode::func_t v);

    fastuidraw::BlendMode m_blend_mode;
    std::vector<GLsizei> m_counts;
    std::vector<const GLvoid*> m_indices;
    PainterBackendGLPrivate *m_private;
    unsigned int m_choice;
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
               unsigned int attributes_written, unsigned int indices_written) const;

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
      m_use_hw_clip_planes(true),
      /* on Mesa/i965 using switch statement gives much slower
         performance than using if/else chain.
       */
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_blend_shader_use_switch(false),
      m_unpack_header_and_brush_in_frag_shader(false),
      m_assign_layout_to_vertex_shader_inputs(true),
      m_assign_layout_to_varyings(false),
      m_assign_binding_points(true),
      m_use_ubo_for_uniforms(false),
      m_separate_program_for_discard(true),
      m_non_dashed_stroke_shader_uses_discard(false),
      m_blend_type(fastuidraw::PainterBlendShader::dual_src)
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
    bool m_use_hw_clip_planes;
    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_blend_shader_use_switch;
    bool m_unpack_header_and_brush_in_frag_shader;
    bool m_assign_layout_to_vertex_shader_inputs;
    bool m_assign_layout_to_varyings;
    bool m_assign_binding_points;
    bool m_use_ubo_for_uniforms;
    bool m_separate_program_for_discard;
    bool m_non_dashed_stroke_shader_uses_discard;
    enum fastuidraw::PainterBlendShader::shader_type m_blend_type;
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
  assert(m_ubos.size() == m_vaos.size());
  for(unsigned int p = 0, endp = m_vaos.size(); p < endp; ++p)
    {
      for(unsigned int i = 0, endi = m_vaos[p].size(); i < endi; ++i)
        {
          if(m_vaos[p][i].m_data_tbo != 0)
            {
              glDeleteTextures(1, &m_vaos[p][i].m_data_tbo);
            }
          glDeleteBuffers(1, &m_vaos[p][i].m_attribute_bo);
          glDeleteBuffers(1, &m_vaos[p][i].m_header_bo);
          glDeleteBuffers(1, &m_vaos[p][i].m_index_bo);
          glDeleteBuffers(1, &m_vaos[p][i].m_data_bo);
          glDeleteVertexArrays(1, &m_vaos[p][i].m_vao);
        }

      if(m_ubos[p] != 0)
        {
          glDeleteBuffers(1, &m_ubos[p]);
        }
    }
}

GLuint
painter_vao_pool::
request_uniform_ubo(unsigned int sz, GLenum target)
{
  if(m_ubos[m_pool] == 0)
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
      assert(psize >= static_cast<int>(sz));
    }
  #endif

  return m_ubos[m_pool];
}

painter_vao
painter_vao_pool::
request_vao(void)
{
  painter_vao return_value;

  if(m_current == m_vaos[m_pool].size())
    {
      fastuidraw::gl::opengl_trait_value v;

      m_vaos[m_pool].resize(m_current + 1);
      glGenVertexArrays(1, &m_vaos[m_pool][m_current].m_vao);

      assert(m_vaos[m_pool][m_current].m_vao != 0);
      glBindVertexArray(m_vaos[m_pool][m_current].m_vao);

      m_vaos[m_pool][m_current].m_data_store_backing = m_data_store_backing;

      switch(m_data_store_backing)
        {
        case fastuidraw::gl::PainterBackendGL::data_store_tbo:
          {
            m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_TEXTURE_BUFFER, m_data_buffer_size);
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
         the passed binding target.
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
  if(m_pool == m_vaos.size())
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
  assert(return_value != 0);

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
  assert(return_value != 0);
  glBindBuffer(bind_target, return_value);
  glBufferData(bind_target, psize, NULL, GL_STREAM_DRAW);
  return return_value;
}

///////////////////////////////////////////////
// DrawEntry methods
DrawEntry::
DrawEntry(const fastuidraw::BlendMode &mode,
          PainterBackendGLPrivate *pr,
          unsigned int pz):
  m_blend_mode(mode),
  m_private(pr),
  m_choice(pz)
{}


DrawEntry::
DrawEntry(const fastuidraw::BlendMode &mode):
  m_blend_mode(mode),
  m_private(NULL),
  m_choice(fastuidraw::gl::PainterBackendGL::number_program_types)
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
draw(void) const
{
  if(m_private)
    {
      m_private->m_programs[m_choice]->use_program();
    }

  if(m_blend_mode.blending_on())
    {
      glEnable(GL_BLEND);
      glBlendEquationSeparate(convert_blend_op(m_blend_mode.equation_rgb()),
                              convert_blend_op(m_blend_mode.equation_alpha()));
      glBlendFuncSeparate(convert_blend_func(m_blend_mode.func_src_rgb()),
                          convert_blend_func(m_blend_mode.func_dst_rgb()),
                          convert_blend_func(m_blend_mode.func_src_alpha()),
                          convert_blend_func(m_blend_mode.func_dst_alpha()));
    }
  else
    {
      glDisable(GL_BLEND);
    }
  assert(!m_counts.empty());
  assert(m_counts.size() == m_indices.size());

  /* TODO:
     Get rid of this unholy mess of #ifdef's here and move
     it to an internal private function that also has a tag
     on what to call.
  */
  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      glMultiDrawElements(GL_TRIANGLES, &m_counts[0],
                          fastuidraw::gl::opengl_trait<fastuidraw::PainterIndex>::type,
                          &m_indices[0], m_counts.size());
    }
  #else
    {
      if(FASTUIDRAWglfunctionExists(glMultiDrawElementsEXT))
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

GLenum
DrawEntry::
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
      assert(!"Bad fastuidraw::BlendMode::op_t v");
    }
#undef C
#undef D

  assert("Invalid blend_op_t");
  return GL_INVALID_ENUM;
}

GLenum
DrawEntry::
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
      assert(!"Bad fastuidraw::BlendMode::func_t v");
    }
#undef C
  assert("Invalid blend_t");
  return GL_INVALID_ENUM;
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
     fastuidraw::PainterDraw to the mapping location.
  */
  void *attr_bo, *index_bo, *data_bo, *header_bo;
  uint32_t flags;

  flags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_attribute_bo);
  attr_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->attribute_buffer_size(), flags);
  assert(attr_bo != NULL);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_header_bo);
  header_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->header_buffer_size(), flags);
  assert(header_bo != NULL);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vao.m_index_bo);
  index_bo = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, hnd->index_buffer_size(), flags);
  assert(index_bo != NULL);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_data_bo);
  data_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->data_buffer_size(), flags);
  assert(data_bo != NULL);

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
draw_break(const fastuidraw::PainterShaderGroup &old_shaders,
           const fastuidraw::PainterShaderGroup &new_shaders,
           unsigned int attributes_written, unsigned int indices_written) const
{
  /* if the blend mode changes, then we need to start a new DrawEntry
   */
  fastuidraw::BlendMode::packed_value old_mode, new_mode;
  uint32_t new_disc, old_disc;

  old_mode = old_shaders.packed_blend_mode();
  new_mode = new_shaders.packed_blend_mode();

  old_disc = old_shaders.item_group() & shader_group_discard_mask;
  new_disc = new_shaders.item_group() & shader_group_discard_mask;

  if(old_disc != new_disc)
    {
      unsigned int pz;
      pz = (new_disc != 0u) ?
        fastuidraw::gl::PainterBackendGL::program_with_discard :
        fastuidraw::gl::PainterBackendGL::program_without_discard;

      if(!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(DrawEntry(fastuidraw::BlendMode(new_mode), m_pr, pz));
    }
  else if(old_mode != new_mode)
    {
      if(!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(fastuidraw::BlendMode(new_mode));
    }
  else
    {
      /* any other state changes means that we just need to add an
         entry to the current draw entry.
      */
      add_entry(indices_written);
    }

  FASTUIDRAWunused(attributes_written);
}

void
DrawCommand::
draw(void) const
{
  glBindVertexArray(m_vao.m_vao);
  switch(m_vao.m_data_store_backing)
    {
    case fastuidraw::gl::PainterBackendGL::data_store_tbo:
      {
        glActiveTexture(GL_TEXTURE0 + m_vao.m_data_store_binding_point);
        glBindTexture(GL_TEXTURE_BUFFER, m_vao.m_data_tbo);
      }
      break;

    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, m_vao.m_data_store_binding_point, m_vao.m_data_bo);
      }
      break;

    default:
      assert(!"Bad value for m_vao.m_data_store_backing");
    }

  if(m_pr->m_params.separate_program_for_discard())
    {
      m_pr->m_programs[fastuidraw::gl::PainterBackendGL::program_without_discard]->use_program();
    }

  for(std::list<DrawEntry>::const_iterator iter = m_draws.begin(),
        end = m_draws.end(); iter != end; ++iter)
    {
      iter->draw();
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
  assert(m_indices_written == indices_written);

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
  const fastuidraw::PainterIndex *offset(NULL);

  if(m_draws.empty())
    {
      m_draws.push_back(fastuidraw::BlendMode());
    }
  assert(indices_written >= m_indices_written);
  count = indices_written - m_indices_written;
  offset += m_indices_written;
  m_draws.back().add_entry(count, offset);
  m_indices_written = indices_written;
}

/////////////////////////////////////////
// PainterBackendGLPrivate methods
PainterBackendGLPrivate::
PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &P,
                        fastuidraw::gl::PainterBackendGL *p):
  m_params(P),
  m_backend_configured(false),
  m_tex_buffer_support(fastuidraw::gl::detail::tex_buffer_not_computed),
  m_number_clip_planes(0),
  m_clip_plane0(GL_INVALID_ENUM),
  m_linear_filter_sampler(0),
  m_pool(NULL),
  m_p(p)
{
  configure_backend();
}

PainterBackendGLPrivate::
~PainterBackendGLPrivate()
{
  if(m_linear_filter_sampler != 0)
    {
      glDeleteSamplers(1, &m_linear_filter_sampler);
    }

  if(m_pool != NULL)
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

  if(params.data_store_backing() == gl::PainterBackendGL::data_store_ubo
     || gl::detail::compute_tex_buffer_support() == gl::detail::tex_buffer_not_supported)
    {
      //using UBO's requires that the data store alignment is 4.
      return_value.alignment(4);
    }
  return return_value;
}

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL
PainterBackendGLPrivate::
compute_glsl_config(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params)
{
  using namespace fastuidraw;
  glsl::PainterBackendGLSL::ConfigurationGLSL return_value;
  gl::ContextProperties ctx;

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      bool use_hw_clip_planes;
      use_hw_clip_planes = params.use_hw_clip_planes()
        && (ctx.has_extension("GL_APPLE_clip_distance") || ctx.has_extension("GL_EXT_clip_cull_distance"));

      return_value.use_hw_clip_planes(use_hw_clip_planes);
    }
  #else
    {
      return_value.use_hw_clip_planes(params.use_hw_clip_planes());
    }
  #endif

  return_value
    .non_dashed_stroke_shader_uses_discard(params.non_dashed_stroke_shader_uses_discard())
    .default_blend_shader_type(params.blend_type());

  return return_value;
}

void
PainterBackendGLPrivate::
configure_backend(void)
{
  assert(!m_backend_configured);

  m_backend_configured = true;
  m_tex_buffer_support = fastuidraw::gl::detail::compute_tex_buffer_support();

  if(m_params.data_store_backing() == fastuidraw::gl::PainterBackendGL::data_store_tbo
     && m_tex_buffer_support == fastuidraw::gl::detail::tex_buffer_not_supported)
    {
      // TBO's not supported, fall back to using UBO's.
      m_params.data_store_backing(fastuidraw::gl::PainterBackendGL::data_store_ubo);
    }

  bool have_dual_src_blending, have_framebuffer_fetch;

  if(m_ctx_properties.is_es())
    {
      have_dual_src_blending = m_ctx_properties.has_extension("GL_EXT_blend_func_extended");
      have_framebuffer_fetch = m_ctx_properties.has_extension("GL_EXT_shader_framebuffer_fetch");
    }
  else
    {
      have_dual_src_blending = true;
      have_framebuffer_fetch = m_ctx_properties.has_extension("GL_EXT_shader_framebuffer_fetch")
	|| m_ctx_properties.has_extension("GL_MESA_shader_framebuffer_fetch");
    }

  if(m_params.blend_type() == fastuidraw::PainterBlendShader::framebuffer_fetch
     && !have_framebuffer_fetch)
    {
      m_params.blend_type(fastuidraw::PainterBlendShader::dual_src);
    }
  
  if(m_params.blend_type() == fastuidraw::PainterBlendShader::dual_src
     && !have_dual_src_blending)
    {
      m_params.blend_type(fastuidraw::PainterBlendShader::single_src);
    }

  /* Query GL what is good size for data store buffer. Size is dependent
     how the data store is backed.
   */
  switch(m_params.data_store_backing())
    {
    case fastuidraw::gl::PainterBackendGL::data_store_tbo:
      {
        unsigned int max_texture_buffer_size(0);
        max_texture_buffer_size = fastuidraw::gl::context_get<GLint>(GL_MAX_TEXTURE_BUFFER_SIZE);
        m_params.data_blocks_per_store_buffer(fastuidraw::t_min(max_texture_buffer_size,
                                                                m_params.data_blocks_per_store_buffer()));
      }
      break;

    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      {
        unsigned int max_ubo_size_bytes, max_num_blocks, block_size_bytes;
        block_size_bytes = m_p->configuration_base().alignment() * sizeof(fastuidraw::generic_data);
        max_ubo_size_bytes = fastuidraw::gl::context_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE);
        max_num_blocks = max_ubo_size_bytes / block_size_bytes;
        m_params.data_blocks_per_store_buffer(fastuidraw::t_min(max_num_blocks,
                                                                m_params.data_blocks_per_store_buffer()));
      }
    }

  if(!m_params.use_hw_clip_planes())
    {
      m_number_clip_planes = 0;
      m_clip_plane0 = GL_INVALID_ENUM;
    }
  else
    {
      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          if(m_ctx_properties.has_extension("GL_EXT_clip_cull_distance"))
            {
              m_number_clip_planes = fastuidraw::gl::context_get<GLint>(GL_MAX_CLIP_DISTANCES_EXT );
              m_clip_plane0 = GL_CLIP_DISTANCE0_EXT;
              m_gles_clip_plane_extension = "GL_EXT_clip_cull_distance";
            }
          else if(m_ctx_properties.has_extension("GL_APPLE_clip_distance"))
            {
              m_number_clip_planes = fastuidraw::gl::context_get<GLint>(GL_MAX_CLIP_DISTANCES_APPLE);
              m_clip_plane0 = GL_CLIP_DISTANCE0_APPLE;
              m_gles_clip_plane_extension = "GL_APPLE_clip_distance";
            }
          else
            {
              m_number_clip_planes = 0;
              m_clip_plane0 = GL_INVALID_ENUM;
              m_params.use_hw_clip_planes(false);
            }
        }
      #else
        {
          m_number_clip_planes = fastuidraw::gl::context_get<GLint>(GL_MAX_CLIP_DISTANCES);
          m_clip_plane0 = GL_CLIP_DISTANCE0;
        }
      #endif
    }
  assert(m_params.use_hw_clip_planes() == m_p->configuration_glsl().use_hw_clip_planes());

  /* if have to use discard for clipping, then there is zero point to
     separate the discarding and non-discarding item shaders.
  */
  m_params.separate_program_for_discard(m_params.separate_program_for_discard() && m_params.use_hw_clip_planes());

  fastuidraw::gl::ColorStopAtlasGL *color;
  assert(dynamic_cast<fastuidraw::gl::ColorStopAtlasGL*>(m_params.colorstop_atlas().get()));
  color = static_cast<fastuidraw::gl::ColorStopAtlasGL*>(m_params.colorstop_atlas().get());
  enum fastuidraw::glsl::PainterBackendGLSL::colorstop_backing_t colorstop_tp;
  if(color->texture_bind_target() == GL_TEXTURE_2D_ARRAY)
    {
      colorstop_tp = fastuidraw::glsl::PainterBackendGLSL::colorstop_texture_2d_array;
    }
  else
    {
      colorstop_tp = fastuidraw::glsl::PainterBackendGLSL::colorstop_texture_1d_array;
    }

  /* Some shader features require new version of GL or
     specific extensions.
   */
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if(m_ctx_properties.version() < fastuidraw::ivec2(3, 2))
        {
          m_params.assign_layout_to_varyings(m_params.assign_layout_to_varyings()
                                             && m_ctx_properties.has_extension("GL_EXT_separate_shader_objects"));
        }

      if(m_ctx_properties.version() <= fastuidraw::ivec2(3, 0))
        {
          //GL ES 3.0 does not support layout(binding=).
          m_params.assign_binding_points(false);
        }
    }
  #else
    {
      if(m_ctx_properties.version() < fastuidraw::ivec2(4, 2))
        {
          m_params.assign_layout_to_varyings(m_params.assign_layout_to_varyings()
                                             && m_ctx_properties.has_extension("GL_ARB_separate_shader_objects"));
          m_params.assign_binding_points(m_params.assign_binding_points()
                                         && m_ctx_properties.has_extension("GL_ARB_shading_language_420pack"));
        }
    }
  #endif

  m_uber_shader_builder_params
    .assign_layout_to_vertex_shader_inputs(m_params.assign_layout_to_vertex_shader_inputs())
    .assign_layout_to_varyings(m_params.assign_layout_to_varyings())
    .assign_binding_points(m_params.assign_binding_points())
    .use_ubo_for_uniforms(m_params.use_ubo_for_uniforms())
    .z_coordinate_convention(fastuidraw::glsl::PainterBackendGLSL::z_minus_1_to_1)
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
    .blend_type(m_p->configuration_glsl().default_blend_shader_type());

  /* now allocate m_pool after adjusting m_params
   */
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

  if(!m_uber_shader_builder_params.assign_binding_points())
    {
      const PainterBackendGLSL::BindingPoints &binding_points(m_uber_shader_builder_params.binding_points());
      m_initializer
        .add_sampler_initializer("fastuidraw_imageAtlas", binding_points.image_atlas_color_tiles_unfiltered())
        .add_sampler_initializer("fastuidraw_imageAtlasFiltered", binding_points.image_atlas_color_tiles_filtered())
        .add_sampler_initializer("fastuidraw_imageIndexAtlas", binding_points.image_atlas_index_tiles())
        .add_sampler_initializer("fastuidraw_glyphTexelStoreUINT", binding_points.glyph_atlas_texel_store_uint())
        .add_sampler_initializer("fastuidraw_glyphGeometryDataStore", binding_points.glyph_atlas_geometry_store())
        .add_sampler_initializer("fastuidraw_colorStopAtlas", binding_points.colorstop_atlas());

      if(m_uber_shader_builder_params.have_float_glyph_texture_atlas())
        {
          m_initializer.add_sampler_initializer("fastuidraw_glyphTexelStoreFLOAT", binding_points.glyph_atlas_texel_store_float());
        }

      if(m_uber_shader_builder_params.use_ubo_for_uniforms())
        {
          m_initializer.add_uniform_block_binding("fastuidraw_uniform_block", binding_points.uniforms_ubo());
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

  if(!m_uber_shader_builder_params.assign_layout_to_vertex_shader_inputs())
    {
      m_attribute_binder
        .add_binding("fastuidraw_primary_attribute", PainterBackendGLSL::primary_attrib_slot)
        .add_binding("fastuidraw_secondary_attribute", PainterBackendGLSL::secondary_attrib_slot)
        .add_binding("fastuidraw_uint_attribute", PainterBackendGLSL::uint_attrib_slot)
        .add_binding("fastuidraw_header_attribute", PainterBackendGLSL::header_attrib_slot);
    }

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if(m_p->configuration_glsl().use_hw_clip_planes())
        {
          m_front_matter_vert.specify_extension(m_gles_clip_plane_extension.c_str(), ShaderSource::require_extension);
        }

      if(m_ctx_properties.version() >= fastuidraw::ivec2(3, 2))
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
          if(m_ctx_properties.version() >= fastuidraw::ivec2(3, 1))
            {
              version = "310 es";
            }
          else
            {
              version = "300 es";
            }

          if(m_uber_shader_builder_params.assign_layout_to_varyings())
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
            || m_uber_shader_builder_params.assign_binding_points());

      m_front_matter_frag
	.specify_extension("GL_MESA_shader_framebuffer_fetch", ShaderSource::enable_extension)
	.specify_extension("GL_EXT_shader_framebuffer_fetch", ShaderSource::enable_extension);

      if(using_glsl42)
        {
          m_front_matter_vert.specify_version("420");
          m_front_matter_frag.specify_version("420");
        }
      else
        {
          m_front_matter_vert.specify_version("330");
          m_front_matter_frag.specify_version("330");

          if(m_uber_shader_builder_params.assign_layout_to_varyings())
            {
              m_front_matter_vert.specify_extension("GL_ARB_separate_shader_objects", ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_ARB_separate_shader_objects", ShaderSource::require_extension);
            }

          if(m_uber_shader_builder_params.assign_binding_points())
            {
              m_front_matter_vert.specify_extension("GL_ARB_shading_language_420pack", ShaderSource::require_extension);
              m_front_matter_frag.specify_extension("GL_ARB_shading_language_420pack", ShaderSource::require_extension);
            }
        }
    }
  #endif

}

const PainterBackendGLPrivate::program_set&
PainterBackendGLPrivate::
programs(bool rebuild)
{
  if(rebuild)
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
      m_shader_uniforms_loc[tp] = m_programs[tp]->uniform_location("fastuidraw_shader_uniforms");
      assert(m_shader_uniforms_loc[tp] != -1 || m_uber_shader_builder_params.use_ubo_for_uniforms());
    }

  if(!m_uber_shader_builder_params.use_ubo_for_uniforms())
    {
      m_uniform_values.resize(m_p->ubo_size());
      m_uniform_values_ptr = fastuidraw::c_array<fastuidraw::generic_data>(&m_uniform_values[0],
                                                                           m_uniform_values.size());
    }
}

PainterBackendGLPrivate::program_ref
PainterBackendGLPrivate::
build_program(enum fastuidraw::gl::PainterBackendGL::program_type_t tp)
{
  fastuidraw::glsl::ShaderSource vert, frag;
  program_ref return_value;
  DiscardItemShaderFilter item_filter(tp);
  const char *discard_macro;

  if(tp == fastuidraw::gl::PainterBackendGL::program_without_discard)
    {
      discard_macro = "fastuidraw_do_nothing()";
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

  m_p->construct_shader(vert, frag, m_uber_shader_builder_params, &item_filter, discard_macro);
  return_value = FASTUIDRAWnew fastuidraw::gl::Program(vert, frag,
                                                       m_attribute_binder,
                                                       m_initializer);
  return return_value;
}

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
  m_d = NULL;
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL&
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
operator=(const ConfigurationGL &rhs)
{
  if(this != &rhs)
    {
      ConfigurationGLPrivate *d, *rhs_d;
      d = static_cast<ConfigurationGLPrivate*>(m_d);
      rhs_d = static_cast<ConfigurationGLPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                                    \
  fastuidraw::gl::PainterBackendGL::ConfigurationGL&                    \
  fastuidraw::gl::PainterBackendGL::ConfigurationGL::                   \
  name(type v)                                                          \
  {                                                                     \
    ConfigurationGLPrivate *d;                                          \
    d = static_cast<ConfigurationGLPrivate*>(m_d);                 \
    d->m_##name = v;                                                    \
    return *this;                                                       \
  }                                                                     \
                                                                        \
  type                                                                  \
  fastuidraw::gl::PainterBackendGL::ConfigurationGL::                   \
  name(void) const                                                      \
  {                                                                     \
    ConfigurationGLPrivate *d;                                          \
    d = static_cast<ConfigurationGLPrivate*>(m_d);                 \
    return d->m_##name;                                                 \
  }

setget_implement(unsigned int, attributes_per_buffer)
setget_implement(unsigned int, indices_per_buffer)
setget_implement(unsigned int, data_blocks_per_store_buffer)
setget_implement(unsigned int, number_pools)
setget_implement(bool, break_on_shader_change)
setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL>&, image_atlas)
setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::gl::ColorStopAtlasGL>&, colorstop_atlas)
setget_implement(const fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL>&, glyph_atlas)
setget_implement(bool, use_hw_clip_planes)
setget_implement(bool, vert_shader_use_switch)
setget_implement(bool, frag_shader_use_switch)
setget_implement(bool, blend_shader_use_switch)
setget_implement(bool, unpack_header_and_brush_in_frag_shader)
setget_implement(enum fastuidraw::gl::PainterBackendGL::data_store_backing_t, data_store_backing)
setget_implement(bool, assign_layout_to_vertex_shader_inputs)
setget_implement(bool, assign_layout_to_varyings)
setget_implement(bool, assign_binding_points)
setget_implement(bool, use_ubo_for_uniforms)
setget_implement(bool, separate_program_for_discard)
setget_implement(bool, non_dashed_stroke_shader_uses_discard)
setget_implement(enum fastuidraw::PainterBlendShader::shader_type, blend_type)

#undef setget_implement

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
  m_d = NULL;
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

  if(configuration_gl().separate_program_for_discard())
    {
      const glsl::PainterItemShaderGLSL *sh;
      sh = dynamic_cast<const glsl::PainterItemShaderGLSL*>(shader.get());
      if(sh && sh->uses_discard())
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
on_pre_draw(void)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);

  /* we delay setting up GL state until on_pre_draw() for several reasons:
       1. the atlases may have been resized, if so the underlying textures
          change
       2. we gaurantee GL state regardless of what the calling application
          is doing because PainterPacker() calls on_pre_draw() within
          PainterPacker::end() and the on_pre_draw() call is immediately
          followed by PainterDraw::draw() calls.
   */

  if(d->m_linear_filter_sampler == 0)
    {
      glGenSamplers(1, &d->m_linear_filter_sampler);
      assert(d->m_linear_filter_sampler != 0);
      glSamplerParameteri(d->m_linear_filter_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glSamplerParameteri(d->m_linear_filter_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GEQUAL);
  glDisable(GL_STENCIL_TEST);

  if(d->m_number_clip_planes > 0)
    {
      glEnable(d->m_clip_plane0 + 0);
      glEnable(d->m_clip_plane0 + 1);
      glEnable(d->m_clip_plane0 + 2);
      glEnable(d->m_clip_plane0 + 3);
      for(int i = 4; i < d->m_number_clip_planes; ++i)
        {
          glDisable(d->m_clip_plane0 + i);
        }
    }

  /* all of our texture units, oh my.
   */
  GlyphAtlasGL *glyphs;
  assert(dynamic_cast<GlyphAtlasGL*>(glyph_atlas().get()));
  glyphs = static_cast<GlyphAtlasGL*>(glyph_atlas().get());

  ImageAtlasGL *image;
  assert(dynamic_cast<ImageAtlasGL*>(image_atlas().get()));
  image = static_cast<ImageAtlasGL*>(image_atlas().get());

  ColorStopAtlasGL *color;
  assert(dynamic_cast<ColorStopAtlasGL*>(colorstop_atlas().get()));
  color = static_cast<ColorStopAtlasGL*>(colorstop_atlas().get());

  const glsl::PainterBackendGLSL::UberShaderParams &uber_params(d->m_uber_shader_builder_params);
  const glsl::PainterBackendGLSL::BindingPoints &binding_points(uber_params.binding_points());

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_unfiltered());
  glBindSampler(binding_points.image_atlas_color_tiles_unfiltered(), 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, image->color_texture());

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_filtered());
  glBindSampler(binding_points.image_atlas_color_tiles_filtered(), d->m_linear_filter_sampler);
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

  //grabbing the programs via programs() makes sure they
  //are built.
  const PainterBackendGLPrivate::program_set &prs(d->programs(shader_code_added()));
  assert(!shader_code_added());

  if(!d->m_params.separate_program_for_discard())
    {
      prs[program_all]->use_program();
    }

  if(d->m_uber_shader_builder_params.use_ubo_for_uniforms())
    {
      /* Grab the buffer, map it, fill it and leave it bound.
       */
      GLuint ubo;
      unsigned int size_generics(ubo_size());
      unsigned int size_bytes(sizeof(generic_data) * size_generics);
      void *ubo_mapped;

      ubo = d->m_pool->request_uniform_ubo(size_bytes, GL_UNIFORM_BUFFER);
      assert(ubo != 0);
      // request_uniform_ubo also binds the buffer for us
      ubo_mapped = glMapBufferRange(GL_UNIFORM_BUFFER, 0, size_bytes,
                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

      fill_uniform_buffer(c_array<generic_data>(static_cast<generic_data*>(ubo_mapped), size_generics));
      glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, size_bytes);
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBufferBase(GL_UNIFORM_BUFFER, d->m_uber_shader_builder_params.binding_points().uniforms_ubo(), ubo);
    }
  else
    {
      /* the uniform is type float[]
       */
      fill_uniform_buffer(d->m_uniform_values_ptr);
      if(d->m_params.separate_program_for_discard())
        {
          prs[program_without_discard]->use_program();
          Uniform(d->m_shader_uniforms_loc[program_without_discard], ubo_size(), d->m_uniform_values_ptr.reinterpret_pointer<float>());

          prs[program_with_discard]->use_program();
          Uniform(d->m_shader_uniforms_loc[program_with_discard], ubo_size(), d->m_uniform_values_ptr.reinterpret_pointer<float>());
        }
      else
        {
          Uniform(d->m_shader_uniforms_loc[program_all], ubo_size(), d->m_uniform_values_ptr.reinterpret_pointer<float>());
        }
    }
}

void
fastuidraw::gl::PainterBackendGL::
on_post_draw(void)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);

  /* this is somewhat paranoid to make sure that
     the GL objects do not leak...
   */
  glUseProgram(0);
  glBindVertexArray(0);

  if(d->m_tex_buffer_support != fastuidraw::gl::detail::tex_buffer_not_supported)
    {
      glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

  const glsl::PainterBackendGLSL::UberShaderParams &uber_params(d->m_uber_shader_builder_params);
  const glsl::PainterBackendGLSL::BindingPoints &binding_points(uber_params.binding_points());

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_unfiltered());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_filtered());
  glBindSampler(binding_points.image_atlas_color_tiles_filtered(), 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_index_tiles());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_uint());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_float());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  fastuidraw::gl::GlyphAtlasGL *glyphs;
  assert(dynamic_cast<fastuidraw::gl::GlyphAtlasGL*>(glyph_atlas().get()));
  glyphs = static_cast<fastuidraw::gl::GlyphAtlasGL*>(glyph_atlas().get());

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_geometry_store());
  glBindTexture(glyphs->geometry_texture_binding_point(), 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.colorstop_atlas());
  glBindTexture(ColorStopAtlasGL::texture_bind_target(), 0);

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
      assert(!"Bad value for m_params.data_store_backing()");
    }
  glBindBufferBase(GL_UNIFORM_BUFFER, binding_points.uniforms_ubo(), 0);
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
