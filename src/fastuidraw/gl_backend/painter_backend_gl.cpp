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

#include <fastuidraw/glsl/shader_code.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>

#include <fastuidraw/stroked_path.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>
#include <fastuidraw/painter/painter_stroke_value.hpp>

#include "private/tex_buffer.hpp"

#ifdef FASTUIDRAW_GL_USE_GLES
#define GL_SRC1_COLOR GL_SRC1_COLOR_EXT
#define GL_SRC1_ALPHA GL_SRC1_ALPHA_EXT
#define GL_ONE_MINUS_SRC1_COLOR GL_ONE_MINUS_SRC1_COLOR_EXT
#define GL_ONE_MINUS_SRC1_ALPHA GL_ONE_MINUS_SRC1_ALPHA_EXT
#endif

namespace
{
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
    painter_vao_pool(const fastuidraw::gl::PainterBackendGL::params &params,
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
  };

  class PainterBackendGLPrivate
  {
  public:
    PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::params &P,
                            fastuidraw::gl::PainterBackendGL *p);

    ~PainterBackendGLPrivate();

    void
    configure_backend(void);

    void
    build_program(void);

    void
    build_vao_tbos(void);

    static
    fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL
    compute_glsl_config(const fastuidraw::gl::PainterBackendGL::params &P);

    fastuidraw::gl::PainterBackendGL::params m_params;
    fastuidraw::glsl::PainterBackendGLSL::UberShaderParams m_uber_shader_builder_params;

    fastuidraw::gl::ContextProperties m_ctx_properties;
    bool m_backend_configured;
    enum fastuidraw::gl::detail::tex_buffer_support_t m_tex_buffer_support;
    int m_number_clip_planes;
    GLenum m_clip_plane0;

    GLuint m_linear_filter_sampler;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::Program> m_program;
    GLint m_target_resolution_loc, m_target_resolution_recip_loc;
    GLint m_target_resolution_recip_magnitude_loc;
    fastuidraw::vec2 m_target_resolution;
    fastuidraw::vec2 m_target_resolution_recip;
    float m_target_resolution_recip_magnitude;
    painter_vao_pool *m_pool;

    fastuidraw::gl::PainterBackendGL *m_p;
  };

  class DrawEntry
  {
  public:
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
  };

  class DrawCommand:public fastuidraw::PainterDrawCommand
  {
  public:
    explicit
    DrawCommand(painter_vao_pool *hnd,
                const fastuidraw::gl::PainterBackendGL::params &params);

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

    painter_vao m_vao;
    mutable unsigned int m_attributes_written, m_indices_written;
    mutable std::list<DrawEntry> m_draws;
  };

  class PainterBackendGLParamsPrivate
  {
  public:
    PainterBackendGLParamsPrivate(void):
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
      m_unpack_header_and_brush_in_frag_shader(false)
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
  };

}

///////////////////////////////////////////
// painter_vao_pool methods
painter_vao_pool::
painter_vao_pool(const fastuidraw::gl::PainterBackendGL::params &params,
                 enum fastuidraw::gl::detail::tex_buffer_support_t tex_buffer_support,
                 const fastuidraw::glsl::PainterBackendGLSL::BindingPoints &binding_points):
  m_attribute_buffer_size(params.attributes_per_buffer() * sizeof(fastuidraw::PainterAttribute)),
  m_header_buffer_size(params.attributes_per_buffer() * sizeof(uint32_t)),
  m_index_buffer_size(params.indices_per_buffer() * sizeof(fastuidraw::PainterIndex)),
  m_alignment(params.m_config.alignment()),
  m_blocks_per_data_buffer(params.data_blocks_per_store_buffer()),
  m_data_buffer_size(m_blocks_per_data_buffer * m_alignment * sizeof(fastuidraw::generic_data)),
  m_data_store_backing(params.data_store_backing()),
  m_tex_buffer_support(tex_buffer_support),
  m_binding_points(binding_points),
  m_current(0),
  m_pool(0),
  m_vaos(params.number_pools())
{}

painter_vao_pool::
~painter_vao_pool()
{
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
    }
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

      glEnableVertexAttribArray(0);
      v = fastuidraw::gl::opengl_trait_values<fastuidraw::vec4>(sizeof(fastuidraw::PainterAttribute),
                                                                offsetof(fastuidraw::PainterAttribute, m_primary_attrib));
      fastuidraw::gl::VertexAttribPointer(0, v);

      glEnableVertexAttribArray(1);
      v = fastuidraw::gl::opengl_trait_values<fastuidraw::vec4>(sizeof(fastuidraw::PainterAttribute),
                                                                offsetof(fastuidraw::PainterAttribute, m_secondary_attrib));
      fastuidraw::gl::VertexAttribPointer(1, v);

      glEnableVertexAttribArray(2);
      v = fastuidraw::gl::opengl_trait_values<fastuidraw::uvec4>(sizeof(fastuidraw::PainterAttribute),
                                                                 offsetof(fastuidraw::PainterAttribute, m_uint_attrib));
      fastuidraw::gl::VertexAttribIPointer(2, v);

      m_vaos[m_pool][m_current].m_header_bo = generate_bo(GL_ARRAY_BUFFER, m_header_buffer_size);
      glEnableVertexAttribArray(3);
      v = fastuidraw::gl::opengl_trait_values<uint32_t>();
      fastuidraw::gl::VertexAttribIPointer(3, v);

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
DrawEntry(const fastuidraw::BlendMode &mode):
  m_blend_mode(mode)
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
  //std::cout << m_counts.size() << " " ;
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
            const fastuidraw::gl::PainterBackendGL::params &params):
  m_vao(hnd->request_vao()),
  m_attributes_written(0),
  m_indices_written(0)
{
  /* map the buffers and set to the c_array<> fields of
     fastuidraw::PainterDrawCommand to the mapping location.
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

  m_attributes = fastuidraw::c_array<fastuidraw::PainterAttribute>(reinterpret_cast<fastuidraw::PainterAttribute*>(attr_bo),
                                                                 params.attributes_per_buffer());
  m_indices = fastuidraw::c_array<fastuidraw::PainterIndex>(reinterpret_cast<fastuidraw::PainterIndex*>(index_bo),
                                                          params.indices_per_buffer());
  m_store = fastuidraw::c_array<fastuidraw::generic_data>(reinterpret_cast<fastuidraw::generic_data*>(data_bo),
                                                          hnd->data_buffer_size() / sizeof(fastuidraw::generic_data));

  m_header_attributes = fastuidraw::c_array<uint32_t>(reinterpret_cast<uint32_t*>(header_bo),
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
      
  old_mode = old_shaders.packed_blend_mode();
  new_mode = new_shaders.packed_blend_mode();
  if(old_mode != new_mode)
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

  for(std::list<DrawEntry>::const_iterator iter = m_draws.begin(),
        end = m_draws.end(); iter != end; ++iter)
    {
      iter->draw();
    }
  //std::cout << "\n";
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
PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::params &P,
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

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL
PainterBackendGLPrivate::
compute_glsl_config(const fastuidraw::gl::PainterBackendGL::params &params)
{
  fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL return_value;
  fastuidraw::gl::ContextProperties ctx;

  return_value.m_config = params.m_config;
  return_value.unique_group_per_item_shader(params.break_on_shader_change());
  return_value.unique_group_per_blend_shader(params.break_on_shader_change());

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      return_value
        .use_hw_clip_planes(params.use_hw_clip_planes() && ctx.has_extension("GL_APPLE_clip_distance"));
    }
  #else
    {
      return_value.use_hw_clip_planes(params.use_hw_clip_planes());
    }
  #endif

  bool have_dual_src_blending, have_framebuffer_fetch;

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      have_dual_src_blending = ctx.has_extension("GL_EXT_blend_func_extended");
      have_framebuffer_fetch = ctx.has_extension("GL_EXT_shader_framebuffer_fetch");
    }
  #else
    {
      have_dual_src_blending = true;
      have_framebuffer_fetch = ctx.has_extension("GL_EXT_shader_framebuffer_fetch");
    }
  #endif

  if(have_framebuffer_fetch && false)
    {
      return_value
        .default_blend_shader_type(fastuidraw::PainterBlendShader::framebuffer_fetch);
    }
  else if(have_dual_src_blending)
    {
      return_value
        .default_blend_shader_type(fastuidraw::PainterBlendShader::dual_src);
    }
  else
    {
      return_value
        .default_blend_shader_type(fastuidraw::PainterBlendShader::single_src);
    }

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
        block_size_bytes = m_params.m_config.alignment() * sizeof(fastuidraw::generic_data);
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
          if(m_ctx_properties.has_extension("GL_APPLE_clip_distance"))
            {
              m_number_clip_planes = fastuidraw::gl::context_get<GLint>(GL_MAX_CLIP_DISTANCES_APPLE);
              m_clip_plane0 = GL_CLIP_DISTANCE0_APPLE;
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

  /*
    configure m_uber_shader_builder_params now that m_params has been
    sanitized. NOTES:

    assign_layout_to_varyings()
       - for GL requires 4.2 or GL_ARB_seperate_shader_objects
       - for GLES requires 3.2 or GL_EXT_seperate_shader_objects

    assign_binding_points()
       - for GL requires 4.2 or GL_ARB_shading_language_420pack
       - for GLES requires 3.2
   */
  m_uber_shader_builder_params
    .z_coordinate_convention(fastuidraw::glsl::PainterBackendGLSL::z_minus_1_to_1)
    .assign_layout_to_varyings(false)
    .assign_binding_points(false)
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
  m_pool = FASTUIDRAWnew painter_vao_pool(m_params, m_tex_buffer_support,
                                          m_uber_shader_builder_params.binding_points());
}

void
PainterBackendGLPrivate::
build_program(void)
{
  fastuidraw::glsl::ShaderSource vert, frag;

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if(m_p->configuration_glsl().use_hw_clip_planes())
        {
          vert.specify_extension("GL_APPLE_clip_distance", fastuidraw::glsl::ShaderSource::require_extension);
          frag.specify_extension("GL_APPLE_clip_distance", fastuidraw::glsl::ShaderSource::require_extension);
        }

      if(m_ctx_properties.version() >= fastuidraw::ivec2(3, 2))
        {
          vert
            .specify_version("320 es");
          frag
            .specify_version("320 es")
            .specify_extension("GL_EXT_blend_func_extended", fastuidraw::glsl::ShaderSource::enable_extension);
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
              vert.specify_extension("GL_EXT_separate_shader_objects", fastuidraw::glsl::ShaderSource::require_extension);
              frag.specify_extension("GL_EXT_separate_shader_objects", fastuidraw::glsl::ShaderSource::require_extension);
            }

          vert
            .specify_version(version.c_str())
            .specify_extension("GL_EXT_texture_buffer", fastuidraw::glsl::ShaderSource::enable_extension)
            .specify_extension("GL_OES_texture_buffer", fastuidraw::glsl::ShaderSource::enable_extension);

          frag
            .specify_version(version.c_str())
            .specify_extension("GL_EXT_blend_func_extended", fastuidraw::glsl::ShaderSource::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", fastuidraw::glsl::ShaderSource::enable_extension)
            .specify_extension("GL_OES_texture_buffer", fastuidraw::glsl::ShaderSource::enable_extension);
        }
    }
  #else
    {
      /* NOTE: layout for vertex out / fragment in
         is part of GLSL version 420 (or higher);
         if we have high enough GL, we can use
         that version number instead of enabling
         the extension GL_ARB_separate_shader_objects
         or the extension GL_ARB_shading_language_420pack
       */
      vert.specify_version("330");
      frag.specify_version("330");

      if(m_uber_shader_builder_params.assign_layout_to_varyings())
        {
          vert.specify_extension("GL_ARB_separate_shader_objects", fastuidraw::glsl::ShaderSource::require_extension);
          frag.specify_extension("GL_ARB_separate_shader_objects", fastuidraw::glsl::ShaderSource::require_extension);
        }

      if(m_uber_shader_builder_params.assign_binding_points())
        {
          vert.specify_extension("GL_ARB_shading_language_420pack", fastuidraw::glsl::ShaderSource::require_extension);
          frag.specify_extension("GL_ARB_shading_language_420pack", fastuidraw::glsl::ShaderSource::require_extension);
        }
    }
  #endif

  m_p->construct_shader(vert, frag, m_uber_shader_builder_params);

  fastuidraw::gl::ProgramInitializerArray initializer;
  if(!m_uber_shader_builder_params.assign_binding_points())
    {
      const fastuidraw::glsl::PainterBackendGLSL::BindingPoints &binding_points(m_uber_shader_builder_params.binding_points());
      initializer
        .add_sampler_initializer("fastuidraw_imageAtlas", binding_points.image_atlas_color_tiles_unfiltered())
        .add_sampler_initializer("fastuidraw_imageAtlasFiltered", binding_points.image_atlas_color_tiles_filtered())
        .add_sampler_initializer("fastuidraw_imageIndexAtlas", binding_points.image_atlas_index_tiles())
        .add_sampler_initializer("fastuidraw_glyphTexelStoreUINT", binding_points.glyph_atlas_texel_store_uint())
        .add_sampler_initializer("fastuidraw_glyphTexelStoreFLOAT", binding_points.glyph_atlas_texel_store_float())
        .add_sampler_initializer("fastuidraw_glyphGeometryDataStore", binding_points.glyph_atlas_geometry_store())
        .add_sampler_initializer("fastuidraw_colorStopAtlas", binding_points.colorstop_atlas())
        .add_sampler_initializer("fastuidraw_painterStore_tbo", binding_points.data_store_buffer_tbo())
        .add_uniform_block_binding("fastuidraw_painterStore_ubo", binding_points.data_store_buffer_ubo());
    }

  m_program = FASTUIDRAWnew fastuidraw::gl::Program(vert, frag,
                                                    fastuidraw::gl::PreLinkActionArray()
                                                    .add_binding("fastuidraw_primary_attribute", 0)
                                                    .add_binding("fastuidraw_secondary_attribute", 1)
                                                    .add_binding("fastuidraw_uint_attribute", 2)
                                                    .add_binding("fastuidraw_header_attribute", 3),
                                                    initializer);

  m_target_resolution_loc = m_program->uniform_location("fastuidraw_viewport_pixels");
  m_target_resolution_recip_loc = m_program->uniform_location("fastuidraw_viewport_recip_pixels");
  m_target_resolution_recip_magnitude_loc = m_program->uniform_location("fastuidraw_viewport_recip_pixels_magnitude");
}

///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL::params methods
fastuidraw::gl::PainterBackendGL::params::
params(void)
{
  m_d = FASTUIDRAWnew PainterBackendGLParamsPrivate();
}

fastuidraw::gl::PainterBackendGL::params::
params(const params &obj):
  m_config(obj.m_config)
{
  PainterBackendGLParamsPrivate *d;
  d = reinterpret_cast<PainterBackendGLParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterBackendGLParamsPrivate(*d);
}

fastuidraw::gl::PainterBackendGL::params::
~params()
{
  PainterBackendGLParamsPrivate *d;
  d = reinterpret_cast<PainterBackendGLParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::gl::PainterBackendGL::params&
fastuidraw::gl::PainterBackendGL::params::
operator=(const params &rhs)
{
  if(this != &rhs)
    {
      PainterBackendGLParamsPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterBackendGLParamsPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterBackendGLParamsPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define paramsSetGet(type, name)                                     \
  fastuidraw::gl::PainterBackendGL::params&                          \
  fastuidraw::gl::PainterBackendGL::params::                         \
  name(type v)                                                       \
  {                                                                  \
    PainterBackendGLParamsPrivate *d;                                \
    d = reinterpret_cast<PainterBackendGLParamsPrivate*>(m_d);       \
    d->m_##name = v;                                                 \
    return *this;                                                    \
  }                                                                  \
                                                                     \
  type                                                               \
  fastuidraw::gl::PainterBackendGL::params::                         \
  name(void) const                                                   \
  {                                                                  \
    PainterBackendGLParamsPrivate *d;                                \
    d = reinterpret_cast<PainterBackendGLParamsPrivate*>(m_d);       \
    return d->m_##name;                                              \
  }

paramsSetGet(unsigned int, attributes_per_buffer)
paramsSetGet(unsigned int, indices_per_buffer)
paramsSetGet(unsigned int, data_blocks_per_store_buffer)
paramsSetGet(unsigned int, number_pools)
paramsSetGet(bool, break_on_shader_change)
paramsSetGet(const fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL>&, image_atlas)
paramsSetGet(const fastuidraw::reference_counted_ptr<fastuidraw::gl::ColorStopAtlasGL>&, colorstop_atlas)
paramsSetGet(const fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL>&, glyph_atlas)
paramsSetGet(bool, use_hw_clip_planes)
paramsSetGet(bool, vert_shader_use_switch)
paramsSetGet(bool, frag_shader_use_switch)
paramsSetGet(bool, blend_shader_use_switch)
paramsSetGet(bool, unpack_header_and_brush_in_frag_shader)
paramsSetGet(enum fastuidraw::gl::PainterBackendGL::data_store_backing_t, data_store_backing)

#undef paramsSetGet

///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL methods
fastuidraw::gl::PainterBackendGL::
PainterBackendGL(const params &P):
  PainterBackendGLSL(P.glyph_atlas(),
                     P.image_atlas(),
                     P.colorstop_atlas(),
                     PainterBackendGLPrivate::compute_glsl_config(P))
{
  m_d = FASTUIDRAWnew PainterBackendGLPrivate(P, this);
}

fastuidraw::gl::PainterBackendGL::
~PainterBackendGL()
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::gl::PainterBackendGL::
target_resolution(int w, int h)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  w = std::max(1, w);
  h = std::max(1, h);

  d->m_target_resolution = vec2(w, h);
  d->m_target_resolution_recip = vec2(1.0f, 1.0f) / vec2(d->m_target_resolution);
  d->m_target_resolution_recip_magnitude = d->m_target_resolution_recip.magnitude();
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::Program>
fastuidraw::gl::PainterBackendGL::
program(void)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  assert(d->m_backend_configured);
  if(shader_code_added())
    {
      d->build_program();
      //std::cout << "Took " << d->m_program->program_build_time() << " seconds to build uber-shader\n";
    }
  assert(!shader_code_added());
  return d->m_program;
}

void
fastuidraw::gl::PainterBackendGL::
on_begin(void)
{
  //nothing to do.
}

void
fastuidraw::gl::PainterBackendGL::
on_pre_draw(void)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  /* we delay setting up GL state until on_pre_draw() for several reasons:
       1. the atlases may have been resized, if so the underlying textures
          change
       2. we gaurantee GL state regardless of what the calling application
          is doing because PainterPacker() calls on_pre_draw() within
          PainterPacker::end() and the on_pre_draw() call is immediately
          followed by PainterDrawCommand::draw() calls.
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
  
  // std::cout << "number_clip_planes = " << m_number_clip_planes << "\n";
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

  program()->use_program();
  if(d->m_target_resolution_loc != -1)
    {
      glUniform2fv(d->m_target_resolution_loc, 1, d->m_target_resolution.c_ptr());
    }
  if(d->m_target_resolution_recip_loc != -1)
    {
      glUniform2fv(d->m_target_resolution_recip_loc, 1, d->m_target_resolution_recip.c_ptr());
    }
  if(d->m_target_resolution_recip_magnitude_loc != -1)
    {
      glUniform1f(d->m_target_resolution_recip_magnitude_loc, d->m_target_resolution_recip_magnitude);
    }
}

void
fastuidraw::gl::PainterBackendGL::
on_end(void)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

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
    }
  d->m_pool->next_pool();
}

fastuidraw::reference_counted_ptr<const fastuidraw::PainterDrawCommand>
fastuidraw::gl::PainterBackendGL::
map_draw_command(void)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  return FASTUIDRAWnew DrawCommand(d->m_pool, d->m_params);
}
