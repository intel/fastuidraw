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
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>

#include "private/tex_buffer.hpp"
#include "../private/util_private.hpp"

#ifndef GL_SRC1_COLOR
#define GL_SRC1_COLOR GL_SRC1_COLOR_EXT
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
      m_data_tbo_float(0),
      m_data_tbo_uint(0),
      m_data_tbo_int(0)
    {}

    GLuint m_vao;
    GLuint m_attribute_bo, m_header_bo, m_index_bo, m_data_bo;
    GLuint m_data_tbo_float, m_data_tbo_uint, m_data_tbo_int;
  };

  class painter_vao_pool:fastuidraw::noncopyable
  {
  public:
    explicit
    painter_vao_pool(const fastuidraw::gl::PainterBackendGL::params &params);

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
    unsigned int m_index_buffer_size, m_data_buffer_size;
    int m_alignment;

    unsigned int m_current, m_pool;
    std::vector<std::vector<painter_vao> > m_vaos;
    enum fastuidraw::gl::detail::tex_buffer_support_t m_tex_buffer_support;
  };

  class PainterBackendGLPrivate
  {
  public:
    PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::params &P,
                            fastuidraw::gl::PainterBackendGL *p);

    ~PainterBackendGLPrivate();

    static
    void
    add_enums(fastuidraw::gl::Shader::shader_source &src);

    static
    void
    add_texture_size_constants(fastuidraw::gl::Shader::shader_source &src,
                               const fastuidraw::gl::PainterBackendGL::params &P);

    static
    const char*
    float_varying_label(unsigned int t);

    static
    const char*
    int_varying_label(void);

    static
    const char*
    uint_varying_label(void);

    static
    void
    stream_declare_varyings(std::ostream &str, unsigned int cnt,
                            const std::string &qualifier,
                            const std::string &type,
                            const std::string &name);

    static
    void
    stream_declare_varyings(std::ostream &str,
                            size_t uint_count, size_t int_count,
                            fastuidraw::const_c_array<size_t> float_counts);

    static
    void
    stream_alias_varyings(fastuidraw::gl::Shader::shader_source &vert,
                          fastuidraw::const_c_array<const char*> p,
                          const std::string &s, bool define);

    static
    void
    stream_alias_varyings(fastuidraw::gl::Shader::shader_source &shader,
                          const fastuidraw::gl::varying_list &p,
                          bool define);

    static
    void
    stream_unpack_code(unsigned int alignment,
                       fastuidraw::gl::Shader::shader_source &str);

    static
    void
    stream_uber_vert_shader(fastuidraw::gl::Shader::shader_source &vert,
                            fastuidraw::const_c_array<fastuidraw::gl::PainterShaderGL::handle> vert_shaders);

    static
    void
    stream_uber_frag_shader(fastuidraw::gl::Shader::shader_source &frag,
                            fastuidraw::const_c_array<fastuidraw::gl::PainterShaderGL::handle> frag_shaders);

    static
    void
    stream_uber_blend_shader(fastuidraw::gl::Shader::shader_source &frag,
                             fastuidraw::const_c_array<fastuidraw::gl::PainterBlendShaderGL::handle> blend_shaders);

    static
    fastuidraw::PainterGlyphShader
    create_glyph_shader(bool anisotropic);

    static
    fastuidraw::PainterItemShader
    create_stroke_item_shader(const std::string &macro1, const std::string &macro2,
                              bool include_boundary_varying);

    static
    fastuidraw::PainterStrokeShader
    create_stroke_shader(const std::string &macro1);

    static
    fastuidraw::PainterItemShader
    create_fill_shader(void);

    static
    fastuidraw::PainterBlendShaderSet
    create_blend_shaders(void);

    static
    fastuidraw::PainterShaderSet
    create_shader_set(void);

    void
    build_program(void);

    void
    build_vao_tbos(void);

    void
    update_varying_size(const fastuidraw::gl::varying_list &plist);

    void
    query_extension_support(void);

    unsigned int
    fetch_blend_mode_index(const fastuidraw::gl::BlendMode &blend_mode);

    fastuidraw::gl::PainterBackendGL::params m_params;
    unsigned int m_color_tile_size;

    GLuint m_linear_filter_sampler;

    bool m_rebuild_program;
    std::vector<fastuidraw::gl::PainterShaderGL::handle> m_vert_shaders;
    std::vector<fastuidraw::gl::PainterShaderGL::handle> m_frag_shaders;
    std::vector<fastuidraw::gl::PainterBlendShaderGL::handle> m_blend_shaders;
    fastuidraw::gl::Shader::shader_source m_vert_shader_utils;
    fastuidraw::gl::Shader::shader_source m_frag_shader_utils;
    std::map<fastuidraw::gl::BlendMode, uint32_t> m_map_blend_modes;
    std::vector<fastuidraw::gl::BlendMode> m_blend_modes;
    fastuidraw::vecN<size_t, fastuidraw::gl::varying_list::interpolation_number_types> m_number_float_varyings;
    size_t m_number_uint_varyings;
    size_t m_number_int_varyings;

    bool m_extension_support_queried;
    int m_number_clip_planes; //0 indicates no hw clip planes.
    GLenum m_clip_plane0;
    fastuidraw::gl::ContextProperties m_ctx_properties;

    fastuidraw::gl::Program::handle m_program;
    GLint m_target_resolution_loc, m_target_resolution_recip_loc;
    GLint m_target_resolution_recip_magnitude_loc;
    fastuidraw::vec2 m_target_resolution;
    fastuidraw::vec2 m_target_resolution_recip;
    float m_target_resolution_recip_magnitude;
    painter_vao_pool m_pool;

    fastuidraw::gl::PainterBackendGL *m_p;
  };

  enum
    {
      bind_image_color_unfiltered = 0,
      bind_image_color_filtered,
      bind_image_index,
      bind_glyph_texel_usampler,
      bind_glyph_texel_sampler,
      bind_glyph_geomtry,
      bind_colorstop,
      bind_painter_data_store_float,
      bind_painter_data_store_uint,
      bind_painter_data_store_int,
    };

  enum
    {
      store_texture_float,
      store_texture_uint,
      store_texture_int,

      number_store_texture_modes
    };

  class DrawEntry
  {
  public:
    DrawEntry(fastuidraw::gl::BlendMode mode);

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
      m_number_pools(3),
      m_break_on_vertex_shader_change(false),
      m_break_on_fragment_shader_change(false),
      m_use_hw_clip_planes(true)
    {}

    unsigned int m_attributes_per_buffer;
    unsigned int m_indices_per_buffer;
    unsigned int m_data_blocks_per_store_buffer;
    unsigned int m_number_pools;
    bool m_break_on_vertex_shader_change;
    bool m_break_on_fragment_shader_change;
    fastuidraw::gl::ImageAtlasGL::handle m_image_atlas;
    fastuidraw::gl::ColorStopAtlasGL::handle m_colorstop_atlas;
    fastuidraw::gl::GlyphAtlasGL::handle m_glyph_atlas;
    bool m_use_hw_clip_planes;
  };
}

///////////////////////////////////////////
// painter_vao_pool methods
painter_vao_pool::
painter_vao_pool(const fastuidraw::gl::PainterBackendGL::params &params):
  m_attribute_buffer_size(params.attributes_per_buffer() * sizeof(fastuidraw::PainterAttribute)),
  m_header_buffer_size(params.attributes_per_buffer() * sizeof(uint32_t)),
  m_index_buffer_size(params.indices_per_buffer() * sizeof(fastuidraw::PainterIndex)),
  m_data_buffer_size(params.m_config.alignment() * params.data_blocks_per_store_buffer() * sizeof(fastuidraw::generic_data)),
  m_alignment(params.m_config.alignment()),
  m_current(0),
  m_pool(0),
  m_vaos(params.number_pools()),
  m_tex_buffer_support(fastuidraw::gl::detail::tex_buffer_not_supported)
{}

painter_vao_pool::
~painter_vao_pool()
{
  for(unsigned int p = 0, endp = m_vaos.size(); p < endp; ++p)
    {
      for(unsigned int i = 0, endi = m_vaos[p].size(); i < endi; ++i)
        {
          glDeleteTextures(1, &m_vaos[p][i].m_data_tbo_float);
          glDeleteTextures(1, &m_vaos[p][i].m_data_tbo_uint);
          glDeleteTextures(1, &m_vaos[p][i].m_data_tbo_int);
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

  if(m_tex_buffer_support == fastuidraw::gl::detail::tex_buffer_not_supported)
    {
      m_tex_buffer_support = fastuidraw::gl::detail::compute_tex_buffer_support();
      assert(m_tex_buffer_support != fastuidraw::gl::detail::tex_buffer_not_supported);
    }

  if(m_current == m_vaos[m_pool].size())
    {
      fastuidraw::gl::opengl_trait_value v;

      m_vaos[m_pool].resize(m_current + 1);
      glGenVertexArrays(1, &m_vaos[m_pool][m_current].m_vao);

      assert(m_vaos[m_pool][m_current].m_vao != 0);
      glBindVertexArray(m_vaos[m_pool][m_current].m_vao);

      /* generate_bo leaves the returned buffer object bound to
         the passed binding target.
      */
      m_vaos[m_pool][m_current].m_attribute_bo = generate_bo(GL_ARRAY_BUFFER, m_attribute_buffer_size);
      m_vaos[m_pool][m_current].m_index_bo = generate_bo(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_size);
      m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_TEXTURE_BUFFER, m_data_buffer_size);
      generate_tbos(m_vaos[m_pool][m_current]);

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
  const GLenum float_fmts[4] =
    {
      GL_R32F,
      GL_RG32F,
      GL_RGB32F,
      GL_RGBA32F,
    };

  const GLenum uint_fmts[4] =
    {
      GL_R32UI,
      GL_RG32UI,
      GL_RGB32UI,
      GL_RGBA32UI,
    };

  const GLenum int_fmts[4] =
    {
      GL_R32I,
      GL_RG32I,
      GL_RGB32I,
      GL_RGBA32I,
    };
  vao.m_data_tbo_float = generate_tbo(vao.m_data_bo, float_fmts[m_alignment - 1], bind_painter_data_store_float);
  vao.m_data_tbo_uint = generate_tbo(vao.m_data_bo, uint_fmts[m_alignment - 1], bind_painter_data_store_uint);
  vao.m_data_tbo_int = generate_tbo(vao.m_data_bo, int_fmts[m_alignment - 1], bind_painter_data_store_int);
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
DrawEntry(fastuidraw::gl::BlendMode mode):
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

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_header_bo);
  header_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->header_buffer_size(), flags);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vao.m_index_bo);
  index_bo = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, hnd->index_buffer_size(), flags);

  glBindBuffer(GL_TEXTURE_BUFFER, m_vao.m_data_bo);
  data_bo = glMapBufferRange(GL_TEXTURE_BUFFER, 0, hnd->data_buffer_size(), flags);

  m_attributes = fastuidraw::c_array<fastuidraw::PainterAttribute>(reinterpret_cast<fastuidraw::PainterAttribute*>(attr_bo),
                                                                 params.attributes_per_buffer());
  m_indices = fastuidraw::c_array<fastuidraw::PainterIndex>(reinterpret_cast<fastuidraw::PainterIndex*>(index_bo),
                                                          params.indices_per_buffer());
  m_store = fastuidraw::c_array<fastuidraw::generic_data>(reinterpret_cast<fastuidraw::generic_data*>(data_bo),
                                                        params.m_config.alignment() * params.data_blocks_per_store_buffer());

  m_header_attributes = fastuidraw::c_array<uint32_t>(reinterpret_cast<uint32_t*>(header_bo),
                                                     params.attributes_per_buffer());

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

void
DrawCommand::
draw_break(const fastuidraw::PainterShaderGroup &old_shaders,
           const fastuidraw::PainterShaderGroup &new_shaders,
           unsigned int attributes_written, unsigned int indices_written) const
{
  /* if the blend mode changes, then we need to start a new DrawEntry
   */
  if(old_shaders.blend_group() != new_shaders.blend_group())
    {
      if(!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(m_blend_modes[new_shaders.blend_group()]);
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
  glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_float);
  glBindTexture(GL_TEXTURE_BUFFER, m_vao.m_data_tbo_float);
  glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_uint);
  glBindTexture(GL_TEXTURE_BUFFER, m_vao.m_data_tbo_uint);
  glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_int);
  glBindTexture(GL_TEXTURE_BUFFER, m_vao.m_data_tbo_int);

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

  glBindBuffer(GL_TEXTURE_BUFFER, m_vao.m_data_bo);
  glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, data_store_written * sizeof(fastuidraw::generic_data));
  glUnmapBuffer(GL_TEXTURE_BUFFER);
}

void
DrawCommand::
add_entry(unsigned int indices_written) const
{
  unsigned int count;
  const fastuidraw::PainterIndex *offset(NULL);

  if(m_draws.empty())
    {
      m_draws.push_back(m_blend_modes[0]);
    }
  assert(indices_written >= m_indices_written);
  count = indices_written - m_indices_written;
  offset += m_indices_written;
  m_draws.back().add_entry(count, offset);
  m_indices_written = indices_written;
}

/////////////////////////////////////////
// PainterBackendGLPrivate methods
void
PainterBackendGLPrivate::
add_enums(fastuidraw::gl::Shader::shader_source &src)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterPacking;

  /* fp32 can store a 24-bit integer exactly,
     however, the operation of converting from
     uint to normalized fp32 may lose a bit,
     so 23-bits it is.
     TODO: go through the requirements of IEEE754,
     what a compiler of a driver might do and
     what a GPU does to see how many bits we
     really have.
  */
  uint32_t z_bits_supported;
  z_bits_supported = std::min(23u, static_cast<uint32_t>(z_num_bits));

  src
    .add_macro("fastuidraw_half_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported - 1))
    .add_macro("fastuidraw_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported))
    .add_macro("fastuidraw_shader_image_mask", PainterBrush::image_mask)
    .add_macro("fastuidraw_shader_image_filter_bit0", PainterBrush::image_filter_bit0)
    .add_macro("fastuidraw_shader_image_filter_num_bits", PainterBrush::image_filter_num_bits)
    .add_macro("fastuidraw_shader_image_filter_nearest", PainterBrush::image_filter_nearest)
    .add_macro("fastuidraw_shader_image_filter_linear", PainterBrush::image_filter_linear)
    .add_macro("fastuidraw_shader_image_filter_cubic", PainterBrush::image_filter_cubic)
    .add_macro("fastuidraw_shader_linear_gradient_mask", PainterBrush::gradient_mask)
    .add_macro("fastuidraw_shader_radial_gradient_mask", PainterBrush::radial_gradient_mask)
    .add_macro("fastuidraw_shader_gradient_repeat_mask", PainterBrush::gradient_repeat_mask)
    .add_macro("fastuidraw_shader_repeat_window_mask", PainterBrush::repeat_window_mask)
    .add_macro("fastuidraw_shader_transformation_translation_mask", PainterBrush::transformation_translation_mask)
    .add_macro("fastuidraw_shader_transformation_matrix_mask", PainterBrush::transformation_matrix_mask)
    .add_macro("fastuidraw_image_number_index_lookup_bit0", PainterBrush::image_number_index_lookups_bit0)
    .add_macro("fastuidraw_image_number_index_lookup_num_bits", PainterBrush::image_number_index_lookups_num_bits)
    .add_macro("fastuidraw_image_slack_bit0", PainterBrush::image_slack_bit0)
    .add_macro("fastuidraw_image_slack_num_bits", PainterBrush::image_slack_num_bits)
    .add_macro("fastuidraw_image_master_index_x_bit0",     Brush::image_atlas_location_x_bit0)
    .add_macro("fastuidraw_image_master_index_x_num_bits", Brush::image_atlas_location_x_num_bits)
    .add_macro("fastuidraw_image_master_index_y_bit0",     Brush::image_atlas_location_y_bit0)
    .add_macro("fastuidraw_image_master_index_y_num_bits", Brush::image_atlas_location_y_num_bits)
    .add_macro("fastuidraw_image_master_index_z_bit0",     Brush::image_atlas_location_z_bit0)
    .add_macro("fastuidraw_image_master_index_z_num_bits", Brush::image_atlas_location_z_num_bits)
    .add_macro("fastuidraw_image_size_x_bit0",     Brush::image_size_x_bit0)
    .add_macro("fastuidraw_image_size_x_num_bits", Brush::image_size_x_num_bits)
    .add_macro("fastuidraw_image_size_y_bit0",     Brush::image_size_y_bit0)
    .add_macro("fastuidraw_image_size_y_num_bits", Brush::image_size_y_num_bits)
    .add_macro("fastuidraw_color_stop_x_bit0",     Brush::gradient_color_stop_x_bit0)
    .add_macro("fastuidraw_color_stop_x_num_bits", Brush::gradient_color_stop_x_num_bits)
    .add_macro("fastuidraw_color_stop_y_bit0",     Brush::gradient_color_stop_y_bit0)
    .add_macro("fastuidraw_color_stop_y_num_bits", Brush::gradient_color_stop_y_num_bits)
    .add_macro("fastuidraw_vert_shader_bit0", vert_shader_bit0)
    .add_macro("fastuidraw_vert_shader_num_bits", vert_shader_num_bits)
    .add_macro("fastuidraw_frag_shader_bit0", frag_shader_bit0)
    .add_macro("fastuidraw_frag_shader_num_bits", frag_shader_num_bits)
    .add_macro("fastuidraw_z_bit0", z_bit0)
    .add_macro("fastuidraw_z_num_bits", z_num_bits)
    .add_macro("fastuidraw_blend_shader_bit0", blend_shader_bit0)
    .add_macro("fastuidraw_blend_shader_num_bits", blend_shader_num_bits)
    .add_macro("fastuidraw_stroke_edge_point", StrokedPath::edge_point)
    .add_macro("fastuidraw_stroke_rounded_join_point", StrokedPath::rounded_join_point)
    .add_macro("fastuidraw_stroke_miter_join_point", StrokedPath::miter_join_point)
    .add_macro("fastuidraw_stroke_rounded_cap_point", StrokedPath::rounded_cap_point)
    .add_macro("fastuidraw_stroke_square_cap_point", StrokedPath::square_cap_point)
    .add_macro("fastuidraw_stroke_point_type_mask", StrokedPath::point_type_mask)
    .add_macro("fastuidraw_stroke_normal0_y_sign_mask", StrokedPath::normal0_y_sign_mask)
    .add_macro("fastuidraw_stroke_normal1_y_sign_mask", StrokedPath::normal1_y_sign_mask)
    .add_macro("fastuidraw_stroke_sin_sign_mask", StrokedPath::sin_sign_mask);
}

void
PainterBackendGLPrivate::
add_texture_size_constants(fastuidraw::gl::Shader::shader_source &src,
                           const fastuidraw::gl::PainterBackendGL::params &P)
{
  fastuidraw::ivec2 glyph_atlas_size;
  unsigned int image_atlas_size, colorstop_atlas_size;

  glyph_atlas_size = fastuidraw::ivec2(P.glyph_atlas()->param_values().texel_store_dimensions());
  image_atlas_size = 1 << (P.image_atlas()->param_values().log2_color_tile_size()
                           + P.image_atlas()->param_values().log2_num_color_tiles_per_row_per_col());
  colorstop_atlas_size = P.colorstop_atlas()->param_values().width();

  src
    .add_macro("fastuidraw_glyphTexelStore_size_x", glyph_atlas_size.x())
    .add_macro("fastuidraw_glyphTexelStore_size_y", glyph_atlas_size.y())
    .add_macro("fastuidraw_glyphTexelStore_size", "ivec2(fastuidraw_glyphTexelStore_size_x, fastuidraw_glyphTexelStore_size_y)")
    .add_macro("fastuidraw_imageAtlas_size", image_atlas_size)
    .add_macro("fastuidraw_colorStopAtlas_size", colorstop_atlas_size)
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_x", "(1.0 / float(fastuidraw_glyphTexelStore_size_x) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_y", "(1.0 / float(fastuidraw_glyphTexelStore_size_y) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal",
               "vec2(fastuidraw_glyphTexelStore_size_reciprocal_x, fastuidraw_glyphTexelStore_size_reciprocal_y)")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal", "(1.0 / float(fastuidraw_imageAtlas_size) )")
    .add_macro("fastuidraw_colorStopAtlas_size_reciprocal", "(1.0 / float(fastuidraw_colorStopAtlas_size) )");
}

const char*
PainterBackendGLPrivate::
float_varying_label(unsigned int t)
{
  using namespace fastuidraw::gl;
  switch(t)
    {
    case varying_list::interpolation_smooth:
      return "fastuidraw_varying_float_smooth";
    case varying_list::interpolation_flat:
      return "fastuidraw_varying_float_flat";
    case varying_list::interpolation_noperspective:
      return "fastuidraw_varying_float_noperspective";
    }
  assert(!"Invalid varying_list::interpolation_qualifier_t");
  return "";
}

const char*
PainterBackendGLPrivate::
int_varying_label(void)
{
  return "fastuidraw_varying_int";
}

const char*
PainterBackendGLPrivate::
uint_varying_label(void)
{
  return "fastuidraw_varying_uint";
}

void
PainterBackendGLPrivate::
stream_declare_varyings(std::ostream &str, unsigned int cnt,
                        const std::string &qualifier,
                        const std::string &type,
                        const std::string &name)
{
  for(unsigned int i = 0; i < cnt; ++i)
    {
      str << qualifier << " fastuidraw_varying " << type << " " << name << i << ";\n";
    }
}

void
PainterBackendGLPrivate::
stream_declare_varyings(std::ostream &str,
                        size_t uint_count, size_t int_count,
                        fastuidraw::const_c_array<size_t> float_counts)
{
  using namespace fastuidraw::gl;
  stream_declare_varyings(str, uint_count, "flat", "uint", uint_varying_label());
  stream_declare_varyings(str, int_count, "flat", "int", int_varying_label());

  stream_declare_varyings(str, float_counts[varying_list::interpolation_smooth],
                          "", "float", float_varying_label(varying_list::interpolation_smooth));

  stream_declare_varyings(str, float_counts[varying_list::interpolation_flat],
                          "flat", "float", float_varying_label(varying_list::interpolation_flat));

  stream_declare_varyings(str, float_counts[varying_list::interpolation_noperspective],
                          "noperspective", "float", float_varying_label(varying_list::interpolation_noperspective));
}

void
PainterBackendGLPrivate::
stream_alias_varyings(fastuidraw::gl::Shader::shader_source &vert,
                      fastuidraw::const_c_array<const char*> p,
                      const std::string &s, bool define)
{
  for(unsigned int i = 0; i < p.size(); ++i)
    {
      std::ostringstream str;
      str << s << i;
      if(define)
        {
          vert.add_macro(p[i], str.str().c_str());
        }
      else
        {
          vert.remove_macro(p[i]);
        }
    }
}

void
PainterBackendGLPrivate::
stream_alias_varyings(fastuidraw::gl::Shader::shader_source &shader,
                      const fastuidraw::gl::varying_list &p,
                      bool define)
{
  using namespace fastuidraw::gl;
  stream_alias_varyings(shader, p.uints(), uint_varying_label(), define);
  stream_alias_varyings(shader, p.ints(), int_varying_label(), define);

  for(unsigned int i = 0; i < varying_list::interpolation_number_types; ++i)
    {
      enum varying_list::interpolation_qualifier_t q;
      q = static_cast<enum varying_list::interpolation_qualifier_t>(i);
      stream_alias_varyings(shader, p.floats(q), float_varying_label(q), define);
    }
}

void
PainterBackendGLPrivate::
stream_unpack_code(unsigned int alignment,
                   fastuidraw::gl::Shader::shader_source &str)
{
  using namespace fastuidraw::PainterPacking;
  using namespace fastuidraw::PainterPacking::Brush;
  using namespace fastuidraw::gl;

  {
    glsl_shader_unpack_value_set<pen_data_size> labels;
    labels
      .set(pen_red_offset, ".r")
      .set(pen_green_offset, ".g")
      .set(pen_blue_offset, ".b")
      .set(pen_alpha_offset, ".a")
      .stream_unpack_function(alignment, str, "fastuidraw_read_pen_color", "vec4");
  }

  {
    /* Matrics in GLSL are [column][row], that is why
       one sees the transposing to the loads
    */
    glsl_shader_unpack_value_set<transformation_matrix_data_size> labels;
    labels
      .set(transformation_matrix_m00_offset, "[0][0]")
      .set(transformation_matrix_m10_offset, "[0][1]")
      .set(transformation_matrix_m01_offset, "[1][0]")
      .set(transformation_matrix_m11_offset, "[1][1]")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_transformation_matrix",
                              "mat2");
  }

  {
    glsl_shader_unpack_value_set<transformation_translation_data_size> labels;
    labels
      .set(transformation_translation_x_offset, ".x")
      .set(transformation_translation_y_offset, ".y")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_transformation_translation",
                              "vec2");
  }

  {
    glsl_shader_unpack_value_set<repeat_window_data_size> labels;
    labels
      .set(repeat_window_x_offset, ".xy.x")
      .set(repeat_window_y_offset, ".xy.y")
      .set(repeat_window_width_offset, ".wh.x")
      .set(repeat_window_height_offset, ".wh.y")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_repeat_window",
                              "fastuidraw_brush_repeat_window");
  }

  {
    glsl_shader_unpack_value_set<image_data_size> labels;
    labels
      .set(image_atlas_location_xyz_offset, ".image_atlas_location_xyz", glsl_shader_unpack_value::uint_type)
      .set(image_size_xy_offset, ".image_size_xy", glsl_shader_unpack_value::uint_type)
      .set(image_start_xy_offset, ".image_start_xy", glsl_shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_image_raw_data",
                              "fastuidraw_brush_image_data_raw");
  }

  {
    glsl_shader_unpack_value_set<linear_gradient_data_size> labels;
    labels
      .set(gradient_p0_x_offset, ".p0.x")
      .set(gradient_p0_y_offset, ".p0.y")
      .set(gradient_p1_x_offset, ".p1.x")
      .set(gradient_p1_y_offset, ".p1.y")
      .set(gradient_color_stop_xy_offset, ".color_stop_sequence_xy", glsl_shader_unpack_value::uint_type)
      .set(gradient_color_stop_length_offset, ".color_stop_sequence_length", glsl_shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_linear_gradient_data",
                              "fastuidraw_brush_gradient_raw");
  }

  {
    glsl_shader_unpack_value_set<radial_gradient_data_size> labels;
    labels
      .set(gradient_p0_x_offset, ".p0.x")
      .set(gradient_p0_y_offset, ".p0.y")
      .set(gradient_p1_x_offset, ".p1.x")
      .set(gradient_p1_y_offset, ".p1.y")
      .set(gradient_color_stop_xy_offset, ".color_stop_sequence_xy", glsl_shader_unpack_value::uint_type)
      .set(gradient_color_stop_length_offset, ".color_stop_sequence_length", glsl_shader_unpack_value::uint_type)
      .set(gradient_start_radius_offset, ".r0")
      .set(gradient_end_radius_offset, ".r1")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_radial_gradient_data",
                              "fastuidraw_brush_gradient_raw");
  }

  {
    glsl_shader_unpack_value_set<header_size> labels;
    labels
      .set(clip_equations_offset, ".clipping_location", glsl_shader_unpack_value::uint_type)
      .set(item_matrix_offset, ".item_matrix_location", glsl_shader_unpack_value::uint_type)
      .set(brush_shader_data_offset, ".brush_shader_data_location", glsl_shader_unpack_value::uint_type)
      .set(vert_shader_data_offset, ".vert_shader_data_location", glsl_shader_unpack_value::uint_type)
      .set(frag_shader_data_offset, ".frag_shader_data_location", glsl_shader_unpack_value::uint_type)
      .set(vert_frag_shader_offset, ".vert_frag_shader", glsl_shader_unpack_value::uint_type)
      .set(brush_shader_offset, ".brush_shader", glsl_shader_unpack_value::uint_type)
      .set(z_blend_shader_offset, ".z_blend_shader_raw", glsl_shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_header",
                              "fastuidraw_shader_header", false);
  }

  {
    glsl_shader_unpack_value_set<clip_equations_data_size> labels;
    labels
      .set(clip0_coeff_x, ".clip0.x")
      .set(clip0_coeff_y, ".clip0.y")
      .set(clip0_coeff_w, ".clip0.z")

      .set(clip1_coeff_x, ".clip1.x")
      .set(clip1_coeff_y, ".clip1.y")
      .set(clip1_coeff_w, ".clip1.z")

      .set(clip2_coeff_x, ".clip2.x")
      .set(clip2_coeff_y, ".clip2.y")
      .set(clip2_coeff_w, ".clip2.z")

      .set(clip3_coeff_x, ".clip3.x")
      .set(clip3_coeff_y, ".clip3.y")
      .set(clip3_coeff_w, ".clip3.z")

      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_clipping",
                              "fastuidraw_clipping_data", false);
  }

  {
    /* Matrics in GLSL are [column][row], that is why
       one sees the transposing to the loads
    */
    glsl_shader_unpack_value_set<item_matrix_data_size> labels;
    labels
      .set(item_matrix_m00_offset, "[0][0]")
      .set(item_matrix_m10_offset, "[0][1]")
      .set(item_matrix_m20_offset, "[0][2]")
      .set(item_matrix_m01_offset, "[1][0]")
      .set(item_matrix_m11_offset, "[1][1]")
      .set(item_matrix_m21_offset, "[1][2]")
      .set(item_matrix_m02_offset, "[2][0]")
      .set(item_matrix_m12_offset, "[2][1]")
      .set(item_matrix_m22_offset, "[2][2]")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_item_matrix", "mat3", false);
  }

  {
    glsl_shader_unpack_value_set<stroke_data_size> labels;
    labels
      .set(stroke_width_offset, ".width")
      .set(stroke_miter_limit_offset, ".miter_limit")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_stroking_params",
                              "fastuidraw_stroking_params",
                              true);
  }
}

void
PainterBackendGLPrivate::
stream_uber_vert_shader(fastuidraw::gl::Shader::shader_source &vert,
                        fastuidraw::const_c_array<fastuidraw::gl::PainterShaderGL::handle> vert_shaders)
{
  /* first stream all of the vert_shaders with predefined macros. */
  for(unsigned int i = 0; i < vert_shaders.size(); ++i)
    {
      stream_alias_varyings(vert, vert_shaders[i]->varyings(), true);

      std::ostringstream str;
      str << "fastuidraw_gl_vert_main" << vert_shaders[i]->ID();
      vert
        .add_macro("fastuidraw_gl_vert_main", str.str().c_str())
        .add_source(vert_shaders[i]->src())
        .remove_macro("fastuidraw_gl_vert_main");

      stream_alias_varyings(vert, vert_shaders[i]->varyings(), false);
    }

  std::ostringstream str;
  str << "vec4\n"
      << "fastuidraw_run_vert_shader(in fastuidraw_shader_header h, out uint add_z)\n"
      << "{\n"
      << "    vec4 p = vec4(0.0, 0.0, 0.0, 0.0);\n";

  for(unsigned int i = 0; i < vert_shaders.size(); ++i)
    {
      if(i != 0)
        {
          str << "    else if";
        }
      else
        {
          str << "    if";
        }

      str << "(h.vert_shader == uint(" << vert_shaders[i]->ID() << "))\n"
          << "    {\n"
          << "        p = vec4(fastuidraw_gl_vert_main" << vert_shaders[i]->ID()
          << "(fastuidraw_primary_attribute, "
          << "fastuidraw_secondary_attribute, "
          << "fastuidraw_uint_attribute, h.vert_shader_data_location, add_z));\n"
          << "    }\n";
    }
  str << "    return p;\n"
      << "}\n";

  vert.add_source(str.str().c_str(), fastuidraw::gl::Shader::from_string);
}

void
PainterBackendGLPrivate::
stream_uber_frag_shader(fastuidraw::gl::Shader::shader_source &frag,
                        fastuidraw::const_c_array<fastuidraw::gl::PainterShaderGL::handle> frag_shaders)
{
  /* first stream all of the vert_shaders with predefined macros. */
  for(unsigned int i = 0; i < frag_shaders.size(); ++i)
    {
      stream_alias_varyings(frag, frag_shaders[i]->varyings(), true);

      std::ostringstream str;
      str << "fastuidraw_gl_frag_main" << frag_shaders[i]->ID();
      frag
        .add_macro("fastuidraw_gl_frag_main", str.str().c_str())
        .add_source(frag_shaders[i]->src())
        .remove_macro("fastuidraw_gl_frag_main");
      stream_alias_varyings(frag, frag_shaders[i]->varyings(), false);
    }

  std::ostringstream str;
  str << "vec4\n"
      << "fastuidraw_run_frag_shader(in uint frag_shader, in uint frag_shader_data_location)\n"
      << "{\n"
      << "    vec4 p = vec4(1.0, 1.0, 1.0, 1.0);\n";

  for(unsigned int i = 0; i < frag_shaders.size(); ++i)
    {
      if(i != 0)
        {
          str << "    else if";
        }
      else
        {
          str << "    if";
        }
      str << "(frag_shader == uint(" << frag_shaders[i]->ID() << "))\n"
          << "    {\n"
          << "        p = vec4(fastuidraw_gl_frag_main" << frag_shaders[i]->ID()
          << "(frag_shader_data_location));\n"
          << "    }\n";
    }
  str << "    return p;\n"
      << "}\n";

  frag.add_source(str.str().c_str(), fastuidraw::gl::Shader::from_string);
}

void
PainterBackendGLPrivate::
stream_uber_blend_shader(fastuidraw::gl::Shader::shader_source &frag,
                         fastuidraw::const_c_array<fastuidraw::gl::PainterBlendShaderGL::handle> blend_shaders)
{
  /* first stream all of the vert_shaders with predefined macros. */
  for(unsigned int i = 0; i < blend_shaders.size(); ++i)
    {
      std::ostringstream str;
      str << "fastuidraw_gl_compute_blend_factors" << blend_shaders[i]->ID();
      frag
        .add_macro("fastuidraw_gl_compute_blend_factors", str.str().c_str())
        .add_source(blend_shaders[i]->src())
        .remove_macro("fastuidraw_gl_compute_blend_factors");
    }

  std::ostringstream str;
  str << "void\n"
      << "fastuidraw_run_blend_shader(in uint blend_shader, in vec4 color0, out vec4 src0, out vec4 src1)\n"
      << "{\n";

  for(unsigned int i = 0; i < blend_shaders.size(); ++i)
    {
      if(i != 0)
        {
          str << "    else if";
        }
      else
        {
          str << "    if";
        }
      str << "(blend_shader == uint(" << blend_shaders[i]->ID() << "))\n"
          << "    {\n"
          << "        fastuidraw_gl_compute_blend_factors" << blend_shaders[i]->ID()
          << "(color0, src0, src1);\n"
          << "    }\n";
    }

  if(!blend_shaders.empty())
    {
      str << "    else\n";
    }

  str << "    {\n"
      << "        src0 = color0;\n"
      << "        src1 = color0;\n"
      << "    }\n"
      << "}\n";

  frag.add_source(str.str().c_str(), fastuidraw::gl::Shader::from_string);
}

fastuidraw::PainterGlyphShader
PainterBackendGLPrivate::
create_glyph_shader(bool anisotropic)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  PainterGlyphShader return_value;
  varying_list varyings;

  varyings
    .add_float_varying("fastuidraw_glyph_tex_coord_x")
    .add_float_varying("fastuidraw_glyph_tex_coord_y")
    .add_float_varying("fastuidraw_glyph_secondary_tex_coord_x")
    .add_float_varying("fastuidraw_glyph_secondary_tex_coord_y")
    .add_uint_varying("fastuidraw_glyph_tex_coord_layer")
    .add_uint_varying("fastuidraw_glyph_secondary_tex_coord_layer")
    .add_uint_varying("fastuidraw_glyph_geometry_data_location");

  {
    PainterItemShader shader;
    shader
      .vert_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                                .add_source("fastuidraw_painter_glyph_coverage.vert.glsl.resource_string",
                                                            Shader::from_resource),
                                                varyings))
      .frag_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                                .add_source("fastuidraw_painter_glyph_coverage.frag.glsl.resource_string",
                                                            Shader::from_resource),
                                                varyings));
    return_value.shader(coverage_glyph, shader);
  }

  {
    std::string frag_src;
    PainterItemShader shader;

    if(anisotropic)
      {
        frag_src = "fastuidraw_painter_glyph_distance_field_anisotropic.frag.glsl.resource_string";
      }
    else
      {
        frag_src = "fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string";
      }

    shader
      .vert_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                                .add_source("fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string",
                                                            Shader::from_resource),
                                                varyings))
      .frag_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                                .add_source(frag_src.c_str(), Shader::from_resource),
                                                varyings));
    return_value.shader(distance_field_glyph, shader);
  }

  {
    std::string frag_src;
    PainterItemShader shader;

    if(anisotropic)
      {
        frag_src = "fastuidraw_painter_glyph_curve_pair_anisotropic.frag.glsl.resource_string";
      }
    else
      {
        frag_src = "fastuidraw_painter_glyph_curve_pair.frag.glsl.resource_string";
      }

    shader
      .vert_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                                .add_source("fastuidraw_painter_glyph_curve_pair.vert.glsl.resource_string",
                                                            Shader::from_resource),
                                                varyings))
      .frag_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                                .add_source(frag_src.c_str(), Shader::from_resource),
                                                varyings));
    return_value.shader(curve_pair_glyph, shader);
  }

  return return_value;
}

fastuidraw::PainterItemShader
PainterBackendGLPrivate::
create_stroke_item_shader(const std::string &macro1, const std::string &macro2,
                          bool include_boundary_varying)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  PainterItemShader shader;
  varying_list varyings;

  if(include_boundary_varying)
    {
      varyings.add_float_varying("fastuidraw_stroking_on_boundary");
    }

  shader
    .vert_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                               .add_macro(macro1.c_str())
                                               .add_macro(macro2.c_str())
                                               .add_source("fastuidraw_painter_stroke.vert.glsl.resource_string",
                                                           Shader::from_resource)
                                               .remove_macro(macro2.c_str())
                                               .remove_macro(macro1.c_str()),
                                               varyings))
    .frag_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                               .add_macro(macro1.c_str())
                                               .add_macro(macro2.c_str())
                                               .add_source("fastuidraw_painter_stroke.frag.glsl.resource_string",
                                                           Shader::from_resource)
                                               .remove_macro(macro2.c_str())
                                               .remove_macro(macro1.c_str()),
                                               varyings));
  return shader;
}



fastuidraw::PainterStrokeShader
PainterBackendGLPrivate::
create_stroke_shader(const std::string &macro1)
{
  fastuidraw::PainterStrokeShader return_value;
  return_value
    .aa_shader_pass1(create_stroke_item_shader(macro1, "FASTUIDRAW_STROKE_OPAQUE_PASS", false))
    .aa_shader_pass2(create_stroke_item_shader(macro1, "FASTUIDRAW_STROKE_AA_PASS", true))
    .non_aa_shader(create_stroke_item_shader(macro1, "FASTUIDRAW_STROKE_NO_AA", false));
  return return_value;
}

fastuidraw::PainterItemShader
PainterBackendGLPrivate::
create_fill_shader(void)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  PainterItemShader shader;
  varying_list varyings;

  varyings.add_float_varying("fastuidraw_stroking_on_boundary");
  shader
    .vert_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                              .add_source("fastuidraw_painter_fill.vert.glsl.resource_string",
                                                          Shader::from_resource),
                                              varyings))
    .frag_shader(FASTUIDRAWnew PainterShaderGL(Shader::shader_source()
                                              .add_source("fastuidraw_painter_fill.frag.glsl.resource_string",
                                                          Shader::from_resource),
                                              varyings));
  return shader;
}

fastuidraw::PainterBlendShaderSet
PainterBackendGLPrivate::
create_blend_shaders(void)
{
  /* try to use as few blend modes as possible so that
     we have fewer draw call breaks. The convention is as
     follows:
     - src0 is GL_ONE and the GLSL code handles the multiply
     - src1 is computed by the GLSL code as needed
     This is fine for those modes that do not need DST values
  */
  fastuidraw::gl::BlendMode one_src1, dst_alpha_src1, one_minus_dst_alpha_src1;
  fastuidraw::PainterBlendShaderSet shaders;

  one_src1
    .equation(GL_FUNC_ADD)
    .func_src(GL_ONE)
    .func_dst(GL_SRC1_COLOR);

  dst_alpha_src1
    .equation(GL_FUNC_ADD)
    .func_src(GL_DST_ALPHA)
    .func_dst(GL_SRC1_COLOR);

  one_minus_dst_alpha_src1
    .equation(GL_FUNC_ADD)
    .func_src(GL_ONE_MINUS_DST_ALPHA)
    .func_dst(GL_SRC1_COLOR);

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src_over,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_src_over.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_over,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_dst_over.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_minus_dst_alpha_src1));



  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_clear,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_clear.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_src.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_dst.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src_in,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_src_in.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  dst_alpha_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_in,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_dst_in.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src_out,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_src_out.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_minus_dst_alpha_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_out,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_dst_out.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src_atop,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_src_atop.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  dst_alpha_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_atop,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_dst_atop.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_minus_dst_alpha_src1));

  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_xor,
                 FASTUIDRAWnew fastuidraw::gl::PainterBlendShaderGL(fastuidraw::gl::Shader::shader_source()
                                                                  .add_source("fastuidraw_porter_duff_xor.glsl.resource_string",
                                                                              fastuidraw::gl::Shader::from_resource),
                                                                  one_minus_dst_alpha_src1));

  return shaders;
}

fastuidraw::PainterShaderSet
PainterBackendGLPrivate::
create_shader_set(void)
{
  fastuidraw::PainterShaderSet return_value;
  return_value
    .glyph_shader(create_glyph_shader(false))
    .glyph_shader_anisotropic(create_glyph_shader(true))
    .stroke_shader(create_stroke_shader("FASTUIDRAW_STROKE_WIDTH_NOT_IN_PIXELS"))
    .pixel_width_stroke_shader(create_stroke_shader("FASTUIDRAW_STROKE_WIDTH_IN_PIXELS"))
    .fill_shader(create_fill_shader())
    .blend_shaders(create_blend_shaders());

  return return_value;
}

unsigned int
PainterBackendGLPrivate::
fetch_blend_mode_index(const fastuidraw::gl::BlendMode &blend_mode)
{
  std::map<fastuidraw::gl::BlendMode, unsigned int>::const_iterator iter;
  unsigned int return_value;

  iter = m_map_blend_modes.find(blend_mode);
  if(iter != m_map_blend_modes.end())
    {
      return_value = iter->second;
    }
  else
    {
      return_value = m_blend_modes.size();
      m_blend_modes.push_back(blend_mode);
      m_map_blend_modes[blend_mode] = return_value;
    }
  return return_value;
}

void
PainterBackendGLPrivate::
query_extension_support(void)
{
  assert(!m_extension_support_queried);

  m_extension_support_queried = true;

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
            }
        }
      #else
        {
          m_number_clip_planes = fastuidraw::gl::context_get<GLint>(GL_MAX_CLIP_DISTANCES);
          m_clip_plane0 = GL_CLIP_DISTANCE0;
        }
      #endif
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
  using namespace fastuidraw::PainterPacking;
  fastuidraw::gl::Shader::shader_source vert, frag;
  std::ostringstream declare_varyings;

  if(!m_extension_support_queried)
    {
      query_extension_support();
    }

  stream_declare_varyings(declare_varyings, m_number_uint_varyings,
                          m_number_int_varyings, m_number_float_varyings);

  add_enums(vert);
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

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if(m_ctx_properties.version() >= fastuidraw::ivec2(3, 2))
        {
          vert
            .specify_version("320 es");
          frag
            .specify_version("320 es")
            .specify_extension("GL_OES_texture_buffer", fastuidraw::gl::Shader::require_extension);
        }
      else
        {
          vert
            .specify_version("310 es")
            .specify_extension("GL_OES_texture_buffer", fastuidraw::gl::Shader::require_extension);
          frag
            .specify_version("310 es")
            .specify_extension("GL_EXT_blend_func_extended", fastuidraw::gl::Shader::require_extension)
            .specify_extension("GL_OES_texture_buffer", fastuidraw::gl::Shader::require_extension);
        }
    }
  #endif

  fastuidraw::gl::GlyphAtlasGL *glyphs;
  glyphs = dynamic_cast<fastuidraw::gl::GlyphAtlasGL*>(m_p->glyph_atlas().get());
  assert(glyphs != NULL);

  if(glyphs->texel_texture(false) == 0)
    {
      vert.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
      frag.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
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
    .add_source("fastuidraw_painter_brush_types.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_varyings.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_forward_declares.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_unpack.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_main.vert.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source(m_vert_shader_utils);
  stream_unpack_code(m_params.m_config.alignment(), vert);
  stream_uber_vert_shader(vert, make_c_array(m_vert_shaders));


  fastuidraw::gl::ImageAtlasGL *image_atlas_gl;
  image_atlas_gl = dynamic_cast<fastuidraw::gl::ImageAtlasGL*>(m_p->image_atlas().get());
  assert(image_atlas_gl != NULL);

  add_enums(frag);
  add_texture_size_constants(frag, m_params);
  frag
    .add_source("fastuidraw_painter_gles_precision.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", m_params.image_atlas()->param_values().log2_index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", m_color_tile_size)
    .add_macro("fastuidraw_varying", "in")
    .add_source(declare_varyings.str().c_str(), fastuidraw::gl::Shader::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush_varyings.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_forward_declares.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_brush.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source(image_atlas_gl->glsl_compute_coord_src("fastuidraw_compute_image_atlas_coord", "fastuidraw_imageIndexAtlas"))
    .add_source("fastuidraw_painter_main.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source("fastuidraw_painter_anisotropic.frag.glsl.resource_string", fastuidraw::gl::Shader::from_resource)
    .add_source(m_frag_shader_utils);
  stream_uber_frag_shader(frag, make_c_array(m_frag_shaders));
  stream_uber_blend_shader(frag, make_c_array(m_blend_shaders));

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
                                                  .add_sampler_initializer("fastuidraw_painterStoreFLOAT", bind_painter_data_store_float)
                                                  .add_sampler_initializer("fastuidraw_painterStoreUINT", bind_painter_data_store_uint)
                                                  .add_sampler_initializer("fastuidraw_painterStoreINT", bind_painter_data_store_int) );

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
  m_number_float_varyings(0),
  m_number_uint_varyings(0),
  m_number_int_varyings(0),
  m_extension_support_queried(false),
  m_number_clip_planes(0),
  m_clip_plane0(GL_INVALID_ENUM),
  m_ctx_properties(false),
  m_pool(m_params),
  m_p(p)
{
  unsigned int entry;
  entry = fetch_blend_mode_index(fastuidraw::gl::BlendMode()
                                 .equation_rgb(GL_FUNC_ADD)
                                 .equation_alpha(GL_FUNC_ADD)
                                 .func_src_rgb(GL_ONE)
                                 .func_src_alpha(GL_ONE)
                                 .func_dst_rgb(GL_ONE_MINUS_SRC_ALPHA)
                                 .func_dst_alpha(GL_ONE_MINUS_SRC_ALPHA));
  assert(entry == 0);
  FASTUIDRAWunused(entry);

  m_vert_shader_utils
    .add_source("fastuidraw_compute_local_distance_from_pixel_distance.glsl.resource_string",
                fastuidraw::gl::Shader::from_resource);

  m_frag_shader_utils
    .add_source(fastuidraw::gl::GlyphAtlasGL::glsl_curvepair_compute_pseudo_distance(m_params.glyph_atlas()->param_values().alignment(),
                                                                                     "fastuidraw_curvepair_pseudo_distance",
                                                                                     "fastuidraw_glyphGeometryDataStore",
                                                                                     false))
    .add_source(fastuidraw::gl::GlyphAtlasGL::glsl_curvepair_compute_pseudo_distance(m_params.glyph_atlas()->param_values().alignment(),
                                                                                     "fastuidraw_curvepair_pseudo_distance",
                                                                                     "fastuidraw_glyphGeometryDataStore",
                                                                                     true));
}

PainterBackendGLPrivate::
~PainterBackendGLPrivate()
{
  if(m_linear_filter_sampler != 0)
    {
      glDeleteSamplers(1, &m_linear_filter_sampler);
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
params(const params &obj)
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
paramsSetGet(bool, break_on_vertex_shader_change)
paramsSetGet(bool, break_on_fragment_shader_change)
paramsSetGet(const fastuidraw::gl::ImageAtlasGL::handle&, image_atlas)
paramsSetGet(const fastuidraw::gl::ColorStopAtlasGL::handle&, colorstop_atlas)
paramsSetGet(const fastuidraw::gl::GlyphAtlasGL::handle&, glyph_atlas)
paramsSetGet(bool, use_hw_clip_planes)

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
absorb_vert_shader(const PainterShader::handle &shader)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  PainterShaderGL::handle h;
  fastuidraw::PainterShader::Tag return_value;

  h = shader.dynamic_cast_ptr<PainterShaderGL>();
  assert(h);

  d->m_rebuild_program = true;
  d->m_vert_shaders.push_back(h);
  d->update_varying_size(h->varyings());

  return_value.m_ID = d->m_vert_shaders.size();
  return_value.m_group = d->m_params.break_on_vertex_shader_change() ? return_value.m_ID : 0;
  return return_value;
}


fastuidraw::PainterShader::Tag
fastuidraw::gl::PainterBackendGL::
absorb_frag_shader(const PainterShader::handle &shader)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  PainterShaderGL::handle h;
  fastuidraw::PainterShader::Tag return_value;

  h = shader.dynamic_cast_ptr<PainterShaderGL>();
  assert(h);

  d->m_rebuild_program = true;
  d->m_frag_shaders.push_back(h);
  d->update_varying_size(h->varyings());

  return_value.m_ID = d->m_frag_shaders.size();
  return_value.m_group = d->m_params.break_on_fragment_shader_change() ? return_value.m_ID : 0;
  return return_value;
}

fastuidraw::PainterShader::Tag
fastuidraw::gl::PainterBackendGL::
absorb_blend_shader(const PainterShader::handle &shader)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  PainterBlendShaderGL::handle h;
  fastuidraw::PainterShader::Tag return_value;

  h = shader.dynamic_cast_ptr<PainterBlendShaderGL>();
  assert(h);

  d->m_rebuild_program = true;
  d->m_blend_shaders.push_back(h);

  return_value.m_ID = d->m_blend_shaders.size();
  return_value.m_group = d->fetch_blend_mode_index(h->blend_mode());

  return return_value;
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

fastuidraw::gl::Program::handle
fastuidraw::gl::PainterBackendGL::
program(void)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  if(d->m_rebuild_program)
    {
      d->build_program();
      d->m_rebuild_program = false;
    }
  return d->m_program;
}

void
fastuidraw::gl::PainterBackendGL::
on_begin(void)
{}

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
  glEnable(GL_BLEND);

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
  glyphs = dynamic_cast<GlyphAtlasGL*>(glyph_atlas().get());
  assert(glyphs != NULL);

  ImageAtlasGL *image;
  image = dynamic_cast<ImageAtlasGL*>(image_atlas().get());
  assert(image != NULL);

  ColorStopAtlasGL *color;
  color = dynamic_cast<ColorStopAtlasGL*>(colorstop_atlas().get());
  assert(color != NULL);

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
  glBindTexture(GL_TEXTURE_BUFFER, glyphs->geometry_texture());

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
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  glActiveTexture(GL_TEXTURE0 + bind_image_color_unfiltered);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_image_index);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_glyph_texel_usampler);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_glyph_geomtry);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  glActiveTexture(GL_TEXTURE0 + bind_colorstop);
  glBindTexture(ColorStopAtlasGL::texture_bind_target(), 0);

  glActiveTexture(GL_TEXTURE0 + bind_image_color_filtered);
  glBindSampler(bind_image_color_filtered, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_glyph_texel_sampler);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_float);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_uint);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  glActiveTexture(GL_TEXTURE0 + bind_painter_data_store_int);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  d->m_pool.next_pool();
}

fastuidraw::PainterDrawCommand::const_handle
fastuidraw::gl::PainterBackendGL::
map_draw_command(void)
{
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);

  return FASTUIDRAWnew DrawCommand(&d->m_pool,
                                  d->m_params,
                                  make_c_array(d->m_blend_modes));
}
