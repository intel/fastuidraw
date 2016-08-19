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
#include "private/uber_shader_builder.hpp"
#include "private/backend_shaders.hpp"
#include "../private/util_private.hpp"

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
  };

  class painter_vao_pool:fastuidraw::noncopyable
  {
  public:
    explicit
    painter_vao_pool(const fastuidraw::gl::PainterBackendGL::params &params,
                     enum fastuidraw::gl::detail::tex_buffer_support_t tex_buffer_support);

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

    unsigned int m_current, m_pool;
    std::vector<std::vector<painter_vao> > m_vaos;
  };

  class BlendModeTracker
  {
  public:
    BlendModeTracker(void);

    unsigned int
    blend_index(const fastuidraw::gl::BlendMode &mode);

    const fastuidraw::gl::BlendMode&
    blend_mode(unsigned int idx) const
    {
      assert(idx < m_modes.size());
      return m_modes[idx];
    }

    fastuidraw::const_c_array<fastuidraw::gl::BlendMode>
    blend_modes(void) const
    {
      return fastuidraw::make_c_array<fastuidraw::gl::BlendMode>(m_modes);
    }
    
  private:
    std::map<fastuidraw::gl::BlendMode, uint32_t> m_map;
    std::vector<fastuidraw::gl::BlendMode> m_modes;
  };

  class PainterBackendGLPrivate
  {
  public:
    PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::params &P,
                            fastuidraw::gl::PainterBackendGL *p);

    ~PainterBackendGLPrivate();

    static
    fastuidraw::PainterShaderSet
    create_shader_set(void)
    {
      fastuidraw::gl::detail::backend_shaders::ShaderSetCreator cr;
      return cr.create_shader_set();
    }

    void
    build_program(void);

    void
    build_vao_tbos(void);

    void
    update_varying_size(const fastuidraw::gl::varying_list &plist);

    void
    configure_backend(void);

    fastuidraw::gl::PainterBackendGL::params m_params;
    unsigned int m_color_tile_size;

    GLuint m_linear_filter_sampler;

    bool m_rebuild_program;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterItemShaderGL> > m_item_shaders;
    unsigned int m_next_item_shader_ID;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::BlendShaderSourceCode> > m_blend_shaders;
    unsigned int m_next_blend_shader_ID;
    fastuidraw::gl::Shader::shader_source m_vert_shader_utils;
    fastuidraw::gl::Shader::shader_source m_frag_shader_utils;
    BlendModeTracker m_blend_mode_tracker;
    bool m_blend_enabled;
    std::string m_shader_blend_macro;
    enum fastuidraw::gl::detail::shader_builder::blending_code_type m_blend_type;
    
    fastuidraw::vecN<size_t, fastuidraw::gl::varying_list::interpolation_number_types> m_number_float_varyings;
    size_t m_number_uint_varyings;
    size_t m_number_int_varyings;

    bool m_backend_configured;
    int m_number_clip_planes; //0 indicates no hw clip planes.
    GLenum m_clip_plane0;
    bool m_have_dual_src_blending, m_have_framebuffer_fetch;
    fastuidraw::gl::ContextProperties m_ctx_properties;
    enum fastuidraw::gl::detail::tex_buffer_support_t m_tex_buffer_support;
    
    fastuidraw::reference_counted_ptr<fastuidraw::gl::Program> m_program;
    GLint m_target_resolution_loc, m_target_resolution_recip_loc;
    GLint m_target_resolution_recip_magnitude_loc;
    fastuidraw::vec2 m_target_resolution;
    fastuidraw::vec2 m_target_resolution_recip;
    float m_target_resolution_recip_magnitude;
    painter_vao_pool *m_pool;

    fastuidraw::gl::PainterBackendGL *m_p;
  };

  enum painter_backend_texture_bindings
    {
      bind_image_color_unfiltered = 0,
      bind_image_color_filtered,
      bind_image_index,
      bind_glyph_texel_usampler,
      bind_glyph_texel_sampler,
      bind_glyph_geomtry,
      bind_colorstop,
      bind_painter_data_store_tbo,
    };

  enum painter_backend_uniform_buffer_bindings
    {
      bind_painter_data_store_ubo,
    };

  class DrawEntry
  {
  public:
    DrawEntry(const fastuidraw::gl::BlendMode &mode);

    void
    add_entry(GLsizei count, const void *offset);

    void
    draw(void) const;

  private:
    fastuidraw::gl::BlendMode m_blend_mode;
    std::vector<GLsizei> m_counts;
    std::vector<const GLvoid*> m_indices;
  };

  class DrawCommand:public fastuidraw::PainterDrawCommand
  {
  public:
    explicit
    DrawCommand(painter_vao_pool *hnd,
                const fastuidraw::gl::PainterBackendGL::params &params,
                fastuidraw::const_c_array<fastuidraw::gl::BlendMode> blend_modes);

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
    fastuidraw::const_c_array<fastuidraw::gl::BlendMode> m_blend_modes;
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
                 enum fastuidraw::gl::detail::tex_buffer_support_t tex_buffer_support):
  m_attribute_buffer_size(params.attributes_per_buffer() * sizeof(fastuidraw::PainterAttribute)),
  m_header_buffer_size(params.attributes_per_buffer() * sizeof(uint32_t)),
  m_index_buffer_size(params.indices_per_buffer() * sizeof(fastuidraw::PainterIndex)),
  m_alignment(params.m_config.alignment()),
  m_blocks_per_data_buffer(params.data_blocks_per_store_buffer()),
  m_data_buffer_size(m_blocks_per_data_buffer * m_alignment * sizeof(fastuidraw::generic_data)),
  m_data_store_backing(params.data_store_backing()),
  m_tex_buffer_support(tex_buffer_support),
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
            generate_tbos(m_vaos[m_pool][m_current]);
          }
          break;

        case fastuidraw::gl::PainterBackendGL::data_store_ubo:
          {
            m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_ARRAY_BUFFER, m_data_buffer_size);
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
  vao.m_data_tbo = generate_tbo(vao.m_data_bo, uint_fmts[m_alignment - 1], bind_painter_data_store_tbo);
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
DrawEntry(const fastuidraw::gl::BlendMode &mode):
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
  glBlendEquationSeparate(m_blend_mode.equation_rgb(), m_blend_mode.equation_alpha());
  glBlendFuncSeparate(m_blend_mode.func_src_rgb(), m_blend_mode.func_dst_rgb(),
                      m_blend_mode.func_src_alpha(), m_blend_mode.func_dst_alpha());
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

////////////////////////////////////
// DrawCommand methods
DrawCommand::
DrawCommand(painter_vao_pool *hnd,
            const fastuidraw::gl::PainterBackendGL::params &params,
            fastuidraw::const_c_array<fastuidraw::gl::BlendMode> blend_modes):
  m_vao(hnd->request_vao()),
  m_blend_modes(blend_modes),
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
  uint32_t old_mode, new_mode;
      
  old_mode = old_shaders.blend_group();
  new_mode = new_shaders.blend_group();
  if(old_mode != new_mode)
    {
      if(!m_draws.empty())
        {
              add_entry(indices_written);
        }
      m_draws.push_back(m_blend_modes[new_mode]);
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
        glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, m_vao.m_data_tbo);
      }
      break;

    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, bind_painter_data_store_ubo, m_vao.m_data_bo);
      }
      break;
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
      if(!m_blend_modes.empty())
        {
          m_draws.push_back(m_blend_modes[0]);
        }
      else
        {
          m_draws.push_back(fastuidraw::gl::BlendMode());
        }
    }
  assert(indices_written >= m_indices_written);
  count = indices_written - m_indices_written;
  offset += m_indices_written;
  m_draws.back().add_entry(count, offset);
  m_indices_written = indices_written;
}

//////////////////////////////////////
// BlendModeTracker methods
BlendModeTracker::
BlendModeTracker(void)
{
}

unsigned int
BlendModeTracker::
blend_index(const fastuidraw::gl::BlendMode &blend_mode)
{
  std::map<fastuidraw::gl::BlendMode, unsigned int>::const_iterator iter;
  unsigned int return_value;

  iter = m_map.find(blend_mode);
  if(iter != m_map.end())
    {
      return_value = iter->second;
    }
  else
    {
      return_value = m_modes.size();
      m_modes.push_back(blend_mode);
      m_map[blend_mode] = return_value;
    }

  return return_value;
}

/////////////////////////////////////////
// PainterBackendGLPrivate methods
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

  /* now allocate m_pool after adjusting m_params
   */
  m_pool = FASTUIDRAWnew painter_vao_pool(m_params, m_tex_buffer_support);

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      m_have_dual_src_blending = m_ctx_properties.has_extension("GL_EXT_blend_func_extended");
      m_have_framebuffer_fetch = m_ctx_properties.has_extension("GL_EXT_shader_framebuffer_fetch");
    }
  #else
    {
      m_have_dual_src_blending = true;
      m_have_framebuffer_fetch = m_ctx_properties.has_extension("GL_EXT_shader_framebuffer_fetch");
    }
  #endif

  /*
    TODO: have a params options so that one can select how to perform
    blending shaders.
   */
  if(m_have_framebuffer_fetch and false)
    {
      m_blend_enabled = false;
      m_shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH";
      m_blend_type = fastuidraw::gl::detail::shader_builder::framebuffer_fetch_blending;
    }
  else if(m_have_dual_src_blending)
    {
      m_blend_enabled = true;
      m_shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND";
      m_blend_type = fastuidraw::gl::detail::shader_builder::dual_src_blending;
    }
  else
    {
      m_blend_enabled = true;
      m_shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND";
      m_blend_type = fastuidraw::gl::detail::shader_builder::single_src_blending;
    }
}

void
PainterBackendGLPrivate::
update_varying_size(const fastuidraw::gl::varying_list &plist)
{
  m_number_uint_varyings = std::max(m_number_uint_varyings, plist.uints().size());
  m_number_int_varyings = std::max(m_number_int_varyings, plist.ints().size());
  for(unsigned int i = 0; i < fastuidraw::gl::varying_list::interpolation_number_types; ++i)
    {
      enum fastuidraw::gl::varying_list::interpolation_qualifier_t q;
      q = static_cast<enum fastuidraw::gl::varying_list::interpolation_qualifier_t>(i);
      m_number_float_varyings[q] = std::max(m_number_float_varyings[q], plist.floats(q).size());
    }
}

void
PainterBackendGLPrivate::
build_program(void)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  using namespace fastuidraw::gl::detail;
  using namespace fastuidraw::gl::detail::shader_builder;
  using namespace fastuidraw::PainterPacking;

  fastuidraw::gl::Shader::shader_source vert, frag;
  std::ostringstream declare_varyings;

  fastuidraw::gl::GlyphAtlasGL *glyphs;
  assert(dynamic_cast<fastuidraw::gl::GlyphAtlasGL*>(m_p->glyph_atlas().get()));
  glyphs = static_cast<fastuidraw::gl::GlyphAtlasGL*>(m_p->glyph_atlas().get());

  if(m_params.unpack_header_and_brush_in_frag_shader())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
      frag.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
    }

  stream_declare_varyings(declare_varyings, m_number_uint_varyings,
                          m_number_int_varyings, m_number_float_varyings);

  add_enums(m_params.m_config.alignment(), vert);
  add_texture_size_constants(vert, m_params);

  if(m_number_clip_planes > 0)
    {
      vert.add_macro("FASTUIDRAW_PAINTER_USE_HW_CLIP_PLANES");
      frag.add_macro("FASTUIDRAW_PAINTER_USE_HW_CLIP_PLANES");

      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          vert.specify_extension("GL_APPLE_clip_distance", fastuidraw::gl::Shader::require_extension);
          frag.specify_extension("GL_APPLE_clip_distance", fastuidraw::gl::Shader::require_extension);
        }
      #endif
    }

  switch(m_params.data_store_backing())
    {
    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      {
        const char *uint_types[]=
          {
            "",
            "uint",
            "uvec2",
            "uvec3",
            "uvec4"
          };

        unsigned int alignment(m_params.m_config.alignment());
        assert(alignment <= 4 && alignment > 0);

        vert
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", m_params.data_blocks_per_store_buffer())
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_UINT_TYPE", uint_types[alignment]);

        frag
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", m_params.data_blocks_per_store_buffer())
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_UINT_TYPE", uint_types[alignment]);
      }
      break;

    case fastuidraw::gl::PainterBackendGL::data_store_tbo:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
        frag.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
      }
      break;
    }

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if(m_ctx_properties.version() >= fastuidraw::ivec2(3, 2))
        {
          vert
            .specify_version("320 es");
          frag
            .specify_version("320 es")
            .specify_extension("GL_EXT_blend_func_extended", fastuidraw::gl::Shader::enable_extension);
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

          vert
            .specify_version(version.c_str())
            .specify_extension("GL_EXT_texture_buffer", fastuidraw::gl::Shader::enable_extension)
            .specify_extension("GL_OES_texture_buffer", fastuidraw::gl::Shader::enable_extension);
          frag
            .specify_version(version.c_str())
            .specify_extension("GL_EXT_blend_func_extended", fastuidraw::gl::Shader::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", fastuidraw::gl::Shader::enable_extension)
            .specify_extension("GL_OES_texture_buffer", fastuidraw::gl::Shader::enable_extension);
        }
    }
  #endif

  if(glyphs->texel_texture(false) == 0)
    {
      vert.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
      frag.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
    }

  switch(glyphs->param_values().glyph_geometry_backing_store_type())
    {
    case fastuidraw::gl::GlyphAtlasGL::glyph_geometry_texture_2d_array:
      {
        fastuidraw::ivec2 log2_glyph_geom;
        log2_glyph_geom = glyphs->geometry_texture_as_2d_array_log2_dims();

        vert
          .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_ARRAY")
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_WIDTH_LOG2", log2_glyph_geom.x())
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_HEIGHT_LOG2", log2_glyph_geom.y());

        frag
          .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_ARRAY")
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_WIDTH_LOG2", log2_glyph_geom.x())
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_HEIGHT_LOG2", log2_glyph_geom.y());
      }
      break;

    case fastuidraw::gl::GlyphAtlasGL::glyph_geometry_texture_buffer:
      {
        vert.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
        frag.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
      }
      break;
    }

  vert
    .add_source("fastuidraw_painter_gles_precision.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", m_params.image_atlas()->param_values().log2_index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", m_color_tile_size)
    .add_macro("fastuidraw_varying", "out")
    .add_source(declare_varyings.str().c_str(), fastuidraw::gl::Shader::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_types.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_unpacked_values.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_forward_declares.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_main.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source(m_vert_shader_utils);
  stream_unpack_code(m_params.m_config.alignment(), vert);
  stream_uber_vert_shader(m_params.vert_shader_use_switch(), vert, make_c_array(m_item_shaders));


  fastuidraw::gl::ImageAtlasGL *image_atlas_gl;
  assert(dynamic_cast<fastuidraw::gl::ImageAtlasGL*>(m_p->image_atlas().get()));
  image_atlas_gl = static_cast<fastuidraw::gl::ImageAtlasGL*>(m_p->image_atlas().get());

  add_enums(m_params.m_config.alignment(), frag);
  add_texture_size_constants(frag, m_params);
  frag
    .add_macro(m_shader_blend_macro.c_str())
    .add_source("fastuidraw_painter_gles_precision.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", m_params.image_atlas()->param_values().log2_index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", m_color_tile_size)
    .add_macro("fastuidraw_varying", "in")
    .add_source(declare_varyings.str().c_str(), fastuidraw::gl::Shader::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_types.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_unpacked_values.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_forward_declares.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource);

  if(m_params.unpack_header_and_brush_in_frag_shader())
    {
      frag
        .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
        .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", fastuidraw::gl::Shader::from_resource);
    }

  frag
    .add_source("fastuidraw_painter_brush.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source(image_atlas_gl->glsl_compute_coord_src("fastuidraw_compute_image_atlas_coord", "fastuidraw_imageIndexAtlas"))
    .add_source("fastuidraw_painter_main.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_anisotropic.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source(m_frag_shader_utils);

  if(m_params.unpack_header_and_brush_in_frag_shader())
    {
      stream_unpack_code(m_params.m_config.alignment(), frag);
    }

  stream_uber_frag_shader(m_params.frag_shader_use_switch(), frag, make_c_array(m_item_shaders));
  stream_uber_blend_shader(m_params.blend_shader_use_switch(), frag,
                           make_c_array(m_blend_shaders), m_blend_type);

  m_program = FASTUIDRAWnew fastuidraw::gl::Program(vert, frag,
                                                    fastuidraw::gl::PreLinkActionArray()
                                                    .add_binding("fastuidraw_primary_attribute", 0)
                                                    .add_binding("fastuidraw_secondary_attribute", 1)
                                                    .add_binding("fastuidraw_uint_attribute", 2)
                                                    .add_binding("fastuidraw_header_attribute", 3),
                                                    fastuidraw::gl::ProgramInitializerArray()
                                                    .add_sampler_initializer("fastuidraw_imageAtlas", bind_image_color_unfiltered)
                                                    .add_sampler_initializer("fastuidraw_imageAtlasFiltered", bind_image_color_filtered)
                                                    .add_sampler_initializer("fastuidraw_imageIndexAtlas", bind_image_index)
                                                    .add_sampler_initializer("fastuidraw_glyphTexelStoreUINT", bind_glyph_texel_usampler)
                                                    .add_sampler_initializer("fastuidraw_glyphTexelStoreFLOAT", bind_glyph_texel_sampler)
                                                    .add_sampler_initializer("fastuidraw_glyphGeometryDataStore", bind_glyph_geomtry)
                                                    .add_sampler_initializer("fastuidraw_colorStopAtlas", bind_colorstop)
                                                    .add_sampler_initializer("fastuidraw_painterStore_tbo", bind_painter_data_store_tbo)
                                                    .add_uniform_block_binding("fastuidraw_painterStore_ubo", bind_painter_data_store_ubo)
                                                    );

  m_target_resolution_loc = m_program->uniform_location("fastuidraw_viewport_pixels");
  m_target_resolution_recip_loc = m_program->uniform_location("fastuidraw_viewport_recip_pixels");
  m_target_resolution_recip_magnitude_loc = m_program->uniform_location("fastuidraw_viewport_recip_pixels_magnitude");
}

PainterBackendGLPrivate::
PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::params &P,
                        fastuidraw::gl::PainterBackendGL *p):
  m_params(P),
  m_color_tile_size(1 << P.image_atlas()->param_values().log2_color_tile_size()),
  m_linear_filter_sampler(0),
  m_rebuild_program(true),
  m_next_item_shader_ID(1),
  m_next_blend_shader_ID(1),
  m_number_float_varyings(0),
  m_number_uint_varyings(0),
  m_number_int_varyings(0),
  m_backend_configured(false),
  m_number_clip_planes(0),
  m_clip_plane0(GL_INVALID_ENUM),
  m_ctx_properties(false),
  m_pool(NULL),
  m_p(p)
{
  m_vert_shader_utils
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_compute_local_distance_from_pixel_distance.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_align.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource);

  m_frag_shader_utils
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string",
                fastuidraw::gl::Shader::from_resource)
    .add_source(fastuidraw::gl::GlyphAtlasGL::glsl_curvepair_compute_pseudo_distance(m_params.glyph_atlas()->param_values().alignment(),
                                                                                     "fastuidraw_curvepair_pseudo_distance",
                                                                                     "fastuidraw_fetch_glyph_data",
                                                                                     false))
    .add_source(fastuidraw::gl::GlyphAtlasGL::glsl_curvepair_compute_pseudo_distance(m_params.glyph_atlas()->param_values().alignment(),
                                                                                     "fastuidraw_curvepair_pseudo_distance",
                                                                                     "fastuidraw_fetch_glyph_data",
                                                                                     true));
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

#define paramsSetGet(type, name)                                    \
  fastuidraw::gl::PainterBackendGL::params&                          \
  fastuidraw::gl::PainterBackendGL::params::                         \
  name(type v)                                                      \
  {                                                                 \
    PainterBackendGLParamsPrivate *d;                               \
    d = reinterpret_cast<PainterBackendGLParamsPrivate*>(m_d);      \
    d->m_##name = v;                                                \
    return *this;                                                   \
  }                                                                 \
                                                                    \
  type                                                              \
  fastuidraw::gl::PainterBackendGL::params::                         \
  name(void) const                                                  \
  {                                                                 \
    PainterBackendGLParamsPrivate *d;                               \
    d = reinterpret_cast<PainterBackendGLParamsPrivate*>(m_d);      \
    return d->m_##name;                                             \
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
  PainterBackend(P.glyph_atlas(),
                 P.image_atlas(),
                 P.colorstop_atlas(),
                 P.m_config,
                 PainterBackendGLPrivate::create_shader_set())
{
  PainterBackendGLPrivate *d;
  m_d = d = FASTUIDRAWnew PainterBackendGLPrivate(P, this);
  set_hints().clipping_via_hw_clip_planes(d->m_number_clip_planes > 0);
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
add_vertex_shader_util(const Shader::shader_source &src)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);
  d->m_vert_shader_utils.add_source(src);
}

void
fastuidraw::gl::PainterBackendGL::
add_fragment_shader_util(const Shader::shader_source &src)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);
  d->m_frag_shader_utils.add_source(src);
}

fastuidraw::PainterShader::Tag
fastuidraw::gl::PainterBackendGL::
absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  reference_counted_ptr<PainterItemShaderGL> h;
  fastuidraw::PainterShader::Tag return_value;

  assert(!shader->parent());
  assert(shader.dynamic_cast_ptr<PainterItemShaderGL>());
  h = shader.static_cast_ptr<PainterItemShaderGL>();

  d->m_rebuild_program = true;
  d->m_item_shaders.push_back(h);
  d->update_varying_size(h->varyings());

  return_value.m_ID = d->m_next_item_shader_ID;
  d->m_next_item_shader_ID += h->number_sub_shaders();
  return_value.m_group = d->m_params.break_on_shader_change() ? return_value.m_ID : 0;
  return return_value;
}

uint32_t
fastuidraw::gl::PainterBackendGL::
compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  return d->m_params.break_on_shader_change() ? shader->ID() : 0;
}

fastuidraw::PainterShader::Tag
fastuidraw::gl::PainterBackendGL::
absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  reference_counted_ptr<PainterBlendShaderGL> h;
  fastuidraw::PainterShader::Tag return_value;
  uint32_t group, sub_shader;
  reference_counted_ptr<BlendShaderSourceCode> blend_code;

  assert(!shader->parent());
  assert(shader.dynamic_cast_ptr<PainterBlendShaderGL>());
  h = shader.static_cast_ptr<PainterBlendShaderGL>();

  switch(d->m_blend_type)
    {
    default:
      assert("Invalid blend_type");
      //fall through
    case fastuidraw::gl::detail::shader_builder::single_src_blending:
      {
        blend_code = h->single_src_blender().m_src;
        sub_shader = h->single_src_blender().m_sub_shader;
        group = d->m_blend_mode_tracker.blend_index(h->single_src_blender().m_blend_mode);
      }
      break;

    case fastuidraw::gl::detail::shader_builder::dual_src_blending:
      {
        blend_code = h->dual_src_blender().m_src;
        sub_shader = h->dual_src_blender().m_sub_shader;
        group = d->m_blend_mode_tracker.blend_index(h->dual_src_blender().m_blend_mode);
      }
      break;

    case fastuidraw::gl::detail::shader_builder::framebuffer_fetch_blending:
      {
        blend_code = h->fetch_blender().m_src;
        sub_shader = h->fetch_blender().m_sub_shader;
        group = 0;
      }
      break;
    }

  if(blend_code->registered_to() == NULL)
    {
      uint32_t shader_id;

      shader_id = d->m_next_blend_shader_ID;
      d->m_next_blend_shader_ID += blend_code->number_sub_shaders();

      blend_code->register_shader_code(shader_id, this);
      d->m_blend_shaders.push_back(blend_code);

      d->m_rebuild_program = true;
    }
  else
    {
      assert(blend_code->registered_to() == this);
    }
  
  return_value.m_ID = blend_code->ID() + sub_shader;
  return_value.m_group = group;

  return return_value;
}

uint32_t
fastuidraw::gl::PainterBackendGL::
compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader)
{
  return shader->group();
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
  if(d->m_rebuild_program)
    {
      d->build_program();
      d->m_rebuild_program = false;
      std::cout << "Took " << d->m_program->program_build_time()
                << " seconds to build uber-shader\n";
    }
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

  if(d->m_blend_enabled)
    {
      glEnable(GL_BLEND);
    }
  else
    {
      glDisable(GL_BLEND);
    }
  
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

  glActiveTexture(GL_TEXTURE0 + bind_image_color_unfiltered);
  glBindSampler(bind_image_color_unfiltered, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, image->color_texture());

  glActiveTexture(GL_TEXTURE0 + bind_image_color_filtered);
  glBindSampler(bind_image_color_filtered, d->m_linear_filter_sampler);
  glBindTexture(GL_TEXTURE_2D_ARRAY, image->color_texture());

  glActiveTexture(GL_TEXTURE0 + bind_image_index);
  glBindSampler(bind_image_index, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, image->index_texture());

  glActiveTexture(GL_TEXTURE0 + bind_glyph_texel_usampler);
  glBindSampler(bind_glyph_texel_usampler, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, glyphs->texel_texture(true));

  glActiveTexture(GL_TEXTURE0 + bind_glyph_texel_sampler);
  glBindSampler(bind_glyph_texel_sampler, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, glyphs->texel_texture(false));

  glActiveTexture(GL_TEXTURE0 + bind_glyph_geomtry);
  glBindSampler(bind_glyph_geomtry, 0);
  glBindTexture(glyphs->geometry_texture_binding_point(), glyphs->geometry_texture());

  glActiveTexture(GL_TEXTURE0 + bind_colorstop);
  glBindSampler(bind_colorstop, 0);
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

  glActiveTexture(GL_TEXTURE0 + bind_image_color_unfiltered);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_image_index);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_glyph_texel_usampler);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  fastuidraw::gl::GlyphAtlasGL *glyphs;
  assert(dynamic_cast<fastuidraw::gl::GlyphAtlasGL*>(glyph_atlas().get()));
  glyphs = static_cast<fastuidraw::gl::GlyphAtlasGL*>(glyph_atlas().get());

  glActiveTexture(GL_TEXTURE0 + bind_glyph_geomtry);
  glBindTexture(glyphs->geometry_texture_binding_point(), 0);

  glActiveTexture(GL_TEXTURE0 + bind_colorstop);
  glBindTexture(ColorStopAtlasGL::texture_bind_target(), 0);

  glActiveTexture(GL_TEXTURE0 + bind_image_color_filtered);
  glBindSampler(bind_image_color_filtered, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_glyph_texel_sampler);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  switch(d->m_params.data_store_backing())
    {
    case fastuidraw::gl::PainterBackendGL::data_store_tbo:
      {
        glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
      }
      break;
    case fastuidraw::gl::PainterBackendGL::data_store_ubo:
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, bind_painter_data_store_ubo, 0);
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

  return FASTUIDRAWnew DrawCommand(d->m_pool,
                                   d->m_params,
                                   d->m_blend_mode_tracker.blend_modes());
}
