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
#include "../private/util_private.hpp"

#ifndef GL_SRC1_COLOR
#define GL_SRC1_COLOR GL_SRC1_COLOR_EXT
#endif

namespace
{
  class LocalBlendShader:public fastuidraw::gl::PainterBlendShaderGL
  {
  public:
    typedef fastuidraw::gl::Shader::shader_source shader_source;
    typedef fastuidraw::gl::BlendMode BlendMode;
    typedef fastuidraw::gl::SingleSourceBlenderShader SingleSourceBlenderShader;
    typedef fastuidraw::gl::DualSourceBlenderShader DualSourceBlenderShader;
    typedef fastuidraw::gl::FramebufferFetchBlendShader FramebufferFetchBlendShader;

    LocalBlendShader(const BlendMode &single_md,
                     const shader_source &dual_src,
                     const BlendMode &dual_md):
      fastuidraw::gl::PainterBlendShaderGL(SingleSourceBlenderShader(single_md, shader_source().
                                                                     add_source("fastuidraw_fall_through.glsl.resource_string",
                                                                                fastuidraw::gl::Shader::from_resource)),
                                           DualSourceBlenderShader(dual_md, dual_src),
                                           FramebufferFetchBlendShader())
    {}
      
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

  enum blend_type
    {
      blend_single_src_blend,
      blend_dual_src_blend,
      blend_framebuffer_fetch,
    };

  /* Enumerations that provide bit masks and shifts
     to read from PainterShader::Tag::m_group to
     extract the index into m_dual_src_blend_modes
     and m_single_src_blend_modes of PainterBackendGLPrivate
   */
  enum
    {      
      dual_src_blend_bit0 = 0,
      dual_src_blend_num_bits = 16,
      dual_src_blend_max = FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(dual_src_blend_num_bits),
      dual_src_blend_mask = dual_src_blend_max << dual_src_blend_bit0,
      
      single_src_blend_bit0 = dual_src_blend_bit0 + dual_src_blend_num_bits,
      single_src_blend_num_bits = 16,
      single_src_blend_max = FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(single_src_blend_num_bits),
      single_src_blend_mask = single_src_blend_max << single_src_blend_bit0,
    };

  /*!
    Values for stroke shading uber-shader
   */
  enum uber_stroke_render_pass_t
    {
      uber_stroke_opaque_pass,
      uber_stroke_aa_pass,
      uber_stroke_non_aa,

      uber_number_passes
    };

  class PainterBackendGLPrivate
  {
  public:
    class ShaderSetCreatorConstants
    {
    public:
      ShaderSetCreatorConstants(void);
      
      uint32_t m_stroke_render_pass_num_bits, m_stroke_dash_num_bits;
      uint32_t m_stroke_width_pixels_bit0, m_stroke_render_pass_bit0, m_stroke_dash_style_bit0;
    };

    class ShaderSetCreator:public ShaderSetCreatorConstants
    {
    public:
      ShaderSetCreator(void);

      fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>
      create_glyph_item_shader(const std::string &vert_src,
                               const std::string &frag_src,
                               const fastuidraw::gl::varying_list &varyings);

      fastuidraw::PainterGlyphShader
      create_glyph_shader(bool anisotropic);

      /*
        stroke_dash_style having value number_dashed_cap_styles means
        to not have dashed stroking.
      */
      fastuidraw::PainterStrokeShader
      create_stroke_shader(enum fastuidraw::PainterEnums::dashed_cap_style stroke_dash_style,
                           bool pixel_width_stroking);

      fastuidraw::PainterDashedStrokeShaderSet
      create_dashed_stroke_shader_set(bool pixel_width_stroking);

      fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>
      create_stroke_item_shader(enum fastuidraw::PainterEnums::dashed_cap_style stroke_dash_style,
                                bool pixel_width_stroking,
                                enum uber_stroke_render_pass_t render_pass_macro);

      fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>
      create_fill_shader(void);
    
      fastuidraw::PainterBlendShaderSet
      create_blend_shaders(void);

      fastuidraw::PainterShaderSet
      create_shader_set(void);

      fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader> m_uber_stroke_shader;
    };

    template<typename T>
    class UberShaderStreamer
    {
    public:
      typedef fastuidraw::reference_counted_ptr<T> ref_type;
      typedef fastuidraw::const_c_array<ref_type> array_type;
      typedef fastuidraw::gl::Shader::shader_source shader_source;
      typedef const shader_source& (T::*get_src_type)(void) const;
      typedef void (*pre_post_stream_type)(shader_source &dst, const ref_type &sh);

      static
      void
      stream_nothing(shader_source &, const ref_type &)
      {}

      static
      void
      stream_uber(bool use_switch, shader_source &dst, array_type shaders,
                  get_src_type get_src,
                  pre_post_stream_type pre_stream,
                  pre_post_stream_type post_stream,
                  const std::string &return_type,
                  const std::string &uber_func_with_args,
                  const std::string &shader_main,
                  const std::string &shader_args, //of the form ", arg1, arg2,..,argN" or empty string
                  const std::string &shader_id);

      static
      void
      stream_uber(bool use_switch, shader_source &dst, array_type shaders,
                  get_src_type get_src,
                  const std::string &return_type,
                  const std::string &uber_func_with_args,
                  const std::string &shader_main,
                  const std::string &shader_args,
                  const std::string &shader_id)
      {
        stream_uber(use_switch, dst, shaders, get_src,
                    &stream_nothing, &stream_nothing,
                    return_type, uber_func_with_args,
                    shader_main, shader_args, shader_id);
      }
    };

    PainterBackendGLPrivate(const fastuidraw::gl::PainterBackendGL::params &P,
                            fastuidraw::gl::PainterBackendGL *p);

    ~PainterBackendGLPrivate();

    static
    unsigned int
    number_data_blocks(unsigned int alignment, unsigned int sz);

    static
    void
    add_enums(unsigned int alignment, fastuidraw::gl::Shader::shader_source &src);

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
    pre_stream_varyings(fastuidraw::gl::Shader::shader_source &dst,
                        const fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterItemShaderGL> &sh)
    {
      stream_alias_varyings(dst, sh->varyings(), true);
    }

    static
    void
    post_stream_varyings(fastuidraw::gl::Shader::shader_source &dst,
                         const fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterItemShaderGL> &sh)
    {
      stream_alias_varyings(dst, sh->varyings(), false);
    }

    static
    void
    stream_uber_vert_shader(bool use_switch,
                            fastuidraw::gl::Shader::shader_source &vert,
                            fastuidraw::const_c_array<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterItemShaderGL> > item_shaders);

    static
    void
    stream_uber_frag_shader(bool use_switch,
                            fastuidraw::gl::Shader::shader_source &frag,
                            fastuidraw::const_c_array<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterItemShaderGL> > item_shaders);

    static
    void
    stream_uber_blend_shader(bool use_switch,
                             fastuidraw::gl::Shader::shader_source &frag,
                             fastuidraw::const_c_array<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBlendShaderGL> > blend_shaders,
                             enum blend_type tp);
    static
    fastuidraw::PainterShaderSet
    create_shader_set(void)
    {
      ShaderSetCreator cr;
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
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBlendShaderGL> > m_blend_shaders;
    unsigned int m_next_blend_shader_ID;
    fastuidraw::gl::Shader::shader_source m_vert_shader_utils;
    fastuidraw::gl::Shader::shader_source m_frag_shader_utils;
    BlendModeTracker m_dual_src_blend_modes, m_single_src_blend_modes, m_no_blending;
    uint32_t m_blend_shader_group_mask, m_blend_shader_group_shift;
    BlendModeTracker *m_blender_to_use;
    bool m_blend_enabled;
    std::string m_shader_blend_macro;
    enum blend_type m_blend_type;
    
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
                fastuidraw::const_c_array<fastuidraw::gl::BlendMode> blend_modes,
                uint32_t blend_mask, uint32_t blend_shift);

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
    uint32_t m_blend_mask, m_blend_shift;
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
      m_vert_shader_use_switch(true),
      m_frag_shader_use_switch(true),
      m_blend_shader_use_switch(true),
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
            fastuidraw::const_c_array<fastuidraw::gl::BlendMode> blend_modes,
            uint32_t blend_mask, uint32_t blend_shift):
  m_vao(hnd->request_vao()),
  m_blend_modes(blend_modes),
  m_blend_mask(blend_mask),
  m_blend_shift(blend_shift),
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
  if(old_shaders.blend_group() != new_shaders.blend_group())
    {
      uint32_t old_mode, new_mode;
      
      old_mode = (old_shaders.blend_group() & m_blend_mask) >> m_blend_shift;
      new_mode = (new_shaders.blend_group() & m_blend_mask) >> m_blend_shift;
      if(old_mode != new_mode)
        {
          if(!m_draws.empty())
            {
              add_entry(indices_written);
            }
          m_draws.push_back(m_blend_modes[new_mode]);
        }
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

///////////////////////////////////////////////
// PainterBackendGLPrivate::ShaderSetCreatorConstants methods
PainterBackendGLPrivate::ShaderSetCreatorConstants::
ShaderSetCreatorConstants(void)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterEnums;

  m_stroke_render_pass_num_bits = number_bits_required(uber_number_passes - 1);
  m_stroke_dash_num_bits = number_bits_required(number_dashed_cap_styles);
  assert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_render_pass_num_bits) >= uber_number_passes - 1);
  assert(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(m_stroke_dash_num_bits) >= number_dashed_cap_styles);

  m_stroke_width_pixels_bit0 = 0;
  m_stroke_render_pass_bit0 = m_stroke_width_pixels_bit0 + 1;
  m_stroke_dash_style_bit0 = m_stroke_render_pass_bit0 + m_stroke_render_pass_num_bits;
}

//////////////////////////////////////////
//  PainterBackendGLPrivate::ShaderSetCreator methods
PainterBackendGLPrivate::ShaderSetCreator::
ShaderSetCreator(void)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  unsigned int num_sub_shaders;

  num_sub_shaders = 1u << (m_stroke_render_pass_num_bits + m_stroke_dash_num_bits + 1u);
  m_uber_stroke_shader = FASTUIDRAWnew PainterItemShaderGL(num_sub_shaders,
                                                           Shader::shader_source()
                                                           .add_source("fastuidraw_painter_stroke.vert.glsl.resource_string",
                                                                       Shader::from_resource),
                                                           Shader::shader_source()
                                                           .add_source("fastuidraw_painter_stroke.frag.glsl.resource_string",
                                                                       Shader::from_resource),
                                                           varying_list()
                                                           .add_float_varying("fastuidraw_stroking_on_boundary")
                                                           .add_float_varying("fastuidraw_stroking_distance"));
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>
PainterBackendGLPrivate::ShaderSetCreator::
create_glyph_item_shader(const std::string &vert_src,
                         const std::string &frag_src,
                         const fastuidraw::gl::varying_list &varyings)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  reference_counted_ptr<PainterItemShader> shader;
  shader = FASTUIDRAWnew PainterItemShaderGL(Shader::shader_source()
                                             .add_source(vert_src.c_str(), Shader::from_resource),
                                             Shader::shader_source()
                                             .add_source(frag_src.c_str(), Shader::from_resource),
                                             varyings);
  return shader;
}

fastuidraw::PainterGlyphShader
PainterBackendGLPrivate::ShaderSetCreator::
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

  return_value
    .shader(coverage_glyph,
            create_glyph_item_shader("fastuidraw_painter_glyph_coverage.vert.glsl.resource_string",
                                     "fastuidraw_painter_glyph_coverage.frag.glsl.resource_string",
                                     varyings));

  if(anisotropic)
    {
      return_value
        .shader(distance_field_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_distance_field_anisotropic.frag.glsl.resource_string",
                                         varyings))
        .shader(curve_pair_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_curve_pair.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_curve_pair_anisotropic.frag.glsl.resource_string",
                                         varyings));
    }
  else
    {
      return_value
        .shader(distance_field_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string",
                                         varyings))
        .shader(curve_pair_glyph,
                create_glyph_item_shader("fastuidraw_painter_glyph_curve_pair.vert.glsl.resource_string",
                                         "fastuidraw_painter_glyph_curve_pair.frag.glsl.resource_string",
                                         varyings));
    }

  return return_value;
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>
PainterBackendGLPrivate::ShaderSetCreator::
create_stroke_item_shader(enum fastuidraw::PainterEnums::dashed_cap_style stroke_dash_style,
                          bool pixel_width_stroking,
                          enum uber_stroke_render_pass_t render_pass)
{
  using namespace fastuidraw;
  reference_counted_ptr<PainterItemShader> shader;
  uint32_t sub_shader;

  sub_shader = (stroke_dash_style << m_stroke_dash_style_bit0)
    | (render_pass << m_stroke_render_pass_bit0)
    | (uint32_t(pixel_width_stroking) << m_stroke_width_pixels_bit0);
  shader = FASTUIDRAWnew PainterItemShader(sub_shader, m_uber_stroke_shader);

  return shader;
}



fastuidraw::PainterStrokeShader
PainterBackendGLPrivate::ShaderSetCreator::
create_stroke_shader(enum fastuidraw::PainterEnums::dashed_cap_style stroke_dash_style,
                     bool pixel_width_stroking)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterEnums;

  PainterStrokeShader return_value;
  return_value
    .aa_shader_pass1(create_stroke_item_shader(stroke_dash_style, pixel_width_stroking, uber_stroke_opaque_pass))
    .aa_shader_pass2(create_stroke_item_shader(stroke_dash_style, pixel_width_stroking, uber_stroke_aa_pass))
    .non_aa_shader(create_stroke_item_shader(stroke_dash_style, pixel_width_stroking, uber_stroke_non_aa));
  return return_value;
}

fastuidraw::PainterDashedStrokeShaderSet
PainterBackendGLPrivate::ShaderSetCreator::
create_dashed_stroke_shader_set(bool pixel_width_stroking)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterEnums;
  PainterDashedStrokeShaderSet return_value;

  return_value
    .shader(dashed_no_caps_closed, create_stroke_shader(dashed_no_caps_closed, pixel_width_stroking))
    .shader(dashed_rounded_caps_closed, create_stroke_shader(dashed_rounded_caps_closed, pixel_width_stroking))
    .shader(dashed_square_caps_closed, create_stroke_shader(dashed_square_caps_closed, pixel_width_stroking))
    .shader(dashed_no_caps, create_stroke_shader(dashed_no_caps, pixel_width_stroking))
    .shader(dashed_rounded_caps, create_stroke_shader(dashed_rounded_caps, pixel_width_stroking))
    .shader(dashed_square_caps, create_stroke_shader(dashed_square_caps, pixel_width_stroking));
  return return_value;
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterItemShader>
PainterBackendGLPrivate::ShaderSetCreator::
create_fill_shader(void)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  reference_counted_ptr<PainterItemShader> shader;
  varying_list varyings;

  varyings.add_float_varying("fastuidraw_stroking_on_boundary");
  shader = FASTUIDRAWnew PainterItemShaderGL(Shader::shader_source()
                                             .add_source("fastuidraw_painter_fill.vert.glsl.resource_string",
                                                         Shader::from_resource),
                                             Shader::shader_source()
                                             .add_source("fastuidraw_painter_fill.frag.glsl.resource_string",
                                                         Shader::from_resource),
                                             varyings);
  return shader;
}

fastuidraw::PainterBlendShaderSet
PainterBackendGLPrivate::ShaderSetCreator::
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
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ONE, GL_ONE_MINUS_SRC_ALPHA),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_src_over.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_over,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ONE_MINUS_DST_ALPHA, GL_ONE),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_dst_over.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_minus_dst_alpha_src1));
  
  
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_clear,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ZERO, GL_ZERO),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_clear.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ONE, GL_ZERO),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_src.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ZERO, GL_ONE),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_dst.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src_in,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_DST_ALPHA, GL_ZERO),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_src_in.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                dst_alpha_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_in,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ZERO, GL_SRC_ALPHA),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_dst_in.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src_out,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ONE_MINUS_DST_ALPHA, GL_ZERO),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_src_out.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_minus_dst_alpha_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_out,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_dst_out.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_src_atop,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_src_atop.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                dst_alpha_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_dst_atop,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_dst_atop.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_minus_dst_alpha_src1));
  
  shaders.shader(fastuidraw::PainterEnums::blend_porter_duff_xor,
                 FASTUIDRAWnew LocalBlendShader(fastuidraw::gl::BlendMode()
                                                .equation(GL_FUNC_ADD)
                                                .func(GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                                                fastuidraw::gl::Shader::shader_source()
                                                .add_source("fastuidraw_porter_duff_xor.glsl.resource_string",
                                                            fastuidraw::gl::Shader::from_resource),
                                                one_minus_dst_alpha_src1));
  
  return shaders;
}

fastuidraw::PainterShaderSet
PainterBackendGLPrivate::ShaderSetCreator::
create_shader_set(void)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterEnums;
  PainterShaderSet return_value;

  return_value
    .glyph_shader(create_glyph_shader(false))
    .glyph_shader_anisotropic(create_glyph_shader(true))
    .stroke_shader(create_stroke_shader(number_dashed_cap_styles, false))
    .pixel_width_stroke_shader(create_stroke_shader(number_dashed_cap_styles, true))
    .dashed_stroke_shader(create_dashed_stroke_shader_set(false))
    .pixel_width_dashed_stroke_shader(create_dashed_stroke_shader_set(true))
    .fill_shader(create_fill_shader())
    .blend_shaders(create_blend_shaders());
  return return_value;
}

/////////////////////////////////////////
// PainterBackendGLPrivate methods
unsigned int
PainterBackendGLPrivate::
number_data_blocks(unsigned int alignment, unsigned int sz)
{
  unsigned int number_blocks;
  number_blocks = sz / alignment;
  if(number_blocks * alignment < sz)
    {
      ++number_blocks;
    }
  return number_blocks;
}

void
PainterBackendGLPrivate::
add_enums(unsigned int alignment, fastuidraw::gl::Shader::shader_source &src)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterPacking;
  using namespace fastuidraw::PainterEnums;

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

  ShaderSetCreatorConstants cr;

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

    .add_macro("fastuidraw_shader_pen_num_blocks", number_data_blocks(alignment, Brush::pen_data_size))
    .add_macro("fastuidraw_shader_image_num_blocks", number_data_blocks(alignment, Brush::image_data_size))
    .add_macro("fastuidraw_shader_linear_gradient_num_blocks", number_data_blocks(alignment, Brush::linear_gradient_data_size))
    .add_macro("fastuidraw_shader_radial_gradient_num_blocks", number_data_blocks(alignment, Brush::radial_gradient_data_size))
    .add_macro("fastuidraw_shader_repeat_window_num_blocks", number_data_blocks(alignment, Brush::repeat_window_data_size))
    .add_macro("fastuidraw_shader_transformation_matrix_num_blocks", number_data_blocks(alignment, Brush::transformation_matrix_data_size))
    .add_macro("fastuidraw_shader_transformation_translation_num_blocks", number_data_blocks(alignment, Brush::transformation_translation_data_size))

    .add_macro("fastuidraw_z_bit0", z_bit0)
    .add_macro("fastuidraw_z_num_bits", z_num_bits)
    .add_macro("fastuidraw_blend_shader_bit0", blend_shader_bit0)
    .add_macro("fastuidraw_blend_shader_num_bits", blend_shader_num_bits)

    .add_macro("fastuidraw_stroke_edge_point", StrokedPath::edge_point)
    .add_macro("fastuidraw_stroke_start_edge_point", StrokedPath::start_edge_point)
    .add_macro("fastuidraw_stroke_end_edge_point", StrokedPath::end_edge_point)
    .add_macro("fastuidraw_stroke_number_edge_point_types", StrokedPath::number_edge_point_types)
    .add_macro("fastuidraw_stroke_start_contour_point", StrokedPath::start_contour_point)
    .add_macro("fastuidraw_stroke_end_contour_point", StrokedPath::end_contour_point)
    .add_macro("fastuidraw_stroke_rounded_join_point", StrokedPath::rounded_join_point)
    .add_macro("fastuidraw_stroke_miter_join_point", StrokedPath::miter_join_point)
    .add_macro("fastuidraw_stroke_rounded_cap_point", StrokedPath::rounded_cap_point)
    .add_macro("fastuidraw_stroke_square_cap_point", StrokedPath::square_cap_point)
    .add_macro("fastuidraw_stroke_point_type_mask", StrokedPath::point_type_mask)
    .add_macro("fastuidraw_stroke_sin_sign_mask", StrokedPath::sin_sign_mask)
    .add_macro("fastuidraw_stroke_normal0_y_sign_mask", StrokedPath::normal0_y_sign_mask)
    .add_macro("fastuidraw_stroke_normal1_y_sign_mask", StrokedPath::normal1_y_sign_mask)

    .add_macro("fastuidraw_stroke_dashed_no_caps_close", PainterEnums::dashed_no_caps_closed)
    .add_macro("fastuidraw_stroke_dashed_rounded_caps_closed", PainterEnums::dashed_rounded_caps_closed)
    .add_macro("fastuidraw_stroke_dashed_square_caps_closed", PainterEnums::dashed_square_caps_closed)
    .add_macro("fastuidraw_stroke_dashed_no_caps", PainterEnums::dashed_no_caps)
    .add_macro("fastuidraw_stroke_dashed_rounded_caps", PainterEnums::dashed_rounded_caps)
    .add_macro("fastuidraw_stroke_dashed_square_caps", PainterEnums::dashed_square_caps)
    .add_macro("fastuidraw_stroke_no_dashes", PainterEnums::number_dashed_cap_styles)

    .add_macro("fastuidraw_stroke_sub_shader_width_pixels_bit0", cr.m_stroke_width_pixels_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_bit0", cr.m_stroke_render_pass_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_render_pass_num_bits", cr.m_stroke_render_pass_num_bits)
    .add_macro("fastuidraw_stroke_sub_shader_dash_style_bit0", cr.m_stroke_dash_style_bit0)
    .add_macro("fastuidraw_stroke_sub_shader_dash_num_bits", cr.m_stroke_dash_num_bits)

    .add_macro("fastuidraw_stroke_opaque_pass", uber_stroke_opaque_pass)
    .add_macro("fastuidraw_stroke_aa_pass", uber_stroke_aa_pass)
    .add_macro("fastuidraw_stroke_non_aa", uber_stroke_non_aa);
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
  using namespace fastuidraw;

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
      .set(item_shader_data_offset, ".item_shader_data_location", glsl_shader_unpack_value::uint_type)
      .set(blend_shader_data_offset, ".blend_shader_data_location", glsl_shader_unpack_value::uint_type)
      .set(item_shader_offset, ".item_shader", glsl_shader_unpack_value::uint_type)
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
    glsl_shader_unpack_value_set<PainterStrokeParams::stroke_data_size> labels;
    labels
      .set(PainterStrokeParams::stroke_width_offset, ".width")
      .set(PainterStrokeParams::stroke_miter_limit_offset, ".miter_limit")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_stroking_params",
                              "fastuidraw_stroking_params",
                              true);
  }
}

template<typename T>
void
PainterBackendGLPrivate::UberShaderStreamer<T>::
stream_uber(bool use_switch, shader_source &dst, array_type shaders,
            get_src_type get_src,
            pre_post_stream_type pre_stream, pre_post_stream_type post_stream,
            const std::string &return_type,
            const std::string &uber_func_with_args,
            const std::string &shader_main,
            const std::string &shader_args, //of the form ", arg1, arg2,..,argN" or empty string
            const std::string &shader_id)
{
  /* first stream all of the item_shaders with predefined macros. */
  for(unsigned int i = 0; i < shaders.size(); ++i)
    {
      pre_stream(dst, shaders[i]);

      std::ostringstream str;
      str << shader_main << shaders[i]->ID();
      dst
        .add_macro(shader_main.c_str(), str.str().c_str())
        .add_source((shaders[i].get()->*get_src)())
        .remove_macro(shader_main.c_str());

      post_stream(dst, shaders[i]);
    }

  std::ostringstream str;
  bool has_sub_shaders(false), has_return_value(return_type != "void");
  std::string tab;

  str << return_type << "\n"
      << uber_func_with_args << "\n"
      << "{\n";

  if(has_return_value)
    {
      str << "    " << return_type << " p;\n";
    }

  for(unsigned int i = 0; i < shaders.size(); ++i)
    {
      if(shaders[i]->number_sub_shaders() > 1)
        {
          unsigned int start, end;
          start = shaders[i]->ID();
          end = start + shaders[i]->number_sub_shaders();
          if(has_sub_shaders)
            {
              str << "    else";
            }
          else
            {
              str << "    ";
            }

          str << "if(" << shader_id << " >= uint(" << start
              << ") && " << shader_id << " < uint(" << end << "))\n"
              << "    {\n"
              << "        ";
          if(has_return_value)
            {
              str << "p = ";
            }
          str << shader_main << shaders[i]->ID()
              << "(" << shader_id << " - uint(" << start << ")" << shader_args << ");\n"
              << "    }\n";
          has_sub_shaders = true;
        }
    }

  if(has_sub_shaders && use_switch)
    {
      str << "    else\n"
          << "    {\n";
      tab = "        ";
    }
  else
    {
      tab = "    ";
    }

  if(use_switch)
    {
      str << tab << "switch(" << shader_id << ")\n"
          << tab << "{\n";
    }

  for(unsigned int i = 0; i < shaders.size(); ++i)
    {
      if(shaders[i]->number_sub_shaders() == 1)
        {
          if(use_switch)
            {
              str << tab << "case uint(" << shaders[i]->ID() << "):\n";
              str << tab << "    {\n"
                  << tab << "        ";
            }
          else
            {
              if(i != 0)
                {
                  str << tab << "else if";
                }
              else
                {
                  str << tab << "if";
                }
              str << "(" << shader_id << " == uint(" << shaders[i]->ID() << "))\n";
              str << tab << "{\n"
                  << tab << "    ";
            }

          if(has_return_value)
            {
              str << "p = ";
            }

          str << shader_main << shaders[i]->ID()
              << "(uint(0)" << shader_args << ");\n";

          if(use_switch)
            {
              str << tab << "    }\n"
                  << tab << "    break;\n\n";
            }
          else
            {
              str << tab << "}\n";
            }
        }
    }

  if(use_switch)
    {
      str << tab << "}\n";
    }

  if(has_sub_shaders && use_switch)
    {
      str << "    }\n";
    }

  if(has_return_value)
    {
      str << "    return p;\n";
    }

  str << "}\n";
  dst.add_source(str.str().c_str(), fastuidraw::gl::Shader::from_string);
}

void
PainterBackendGLPrivate::
stream_uber_vert_shader(bool use_switch,
                        fastuidraw::gl::Shader::shader_source &vert,
                        fastuidraw::const_c_array<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterItemShaderGL> > item_shaders)
{
  UberShaderStreamer<fastuidraw::gl::PainterItemShaderGL>::stream_uber(use_switch, vert, item_shaders,
                                                                       &fastuidraw::gl::PainterItemShaderGL::vertex_src,
                                                                       &pre_stream_varyings, &post_stream_varyings,
                                                                       "vec4", "fastuidraw_run_vert_shader(in fastuidraw_shader_header h, out uint add_z)",
                                                                       "fastuidraw_gl_vert_main",
                                                                       ", fastuidraw_primary_attribute, fastuidraw_secondary_attribute, "
                                                                       "fastuidraw_uint_attribute, h.item_shader_data_location, add_z",
                                                                       "h.item_shader");
}

void
PainterBackendGLPrivate::
stream_uber_frag_shader(bool use_switch,
                        fastuidraw::gl::Shader::shader_source &frag,
                        fastuidraw::const_c_array<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterItemShaderGL> > item_shaders)
{
  UberShaderStreamer<fastuidraw::gl::PainterItemShaderGL>::stream_uber(use_switch, frag, item_shaders,
                                                                       &fastuidraw::gl::PainterItemShaderGL::fragment_src,
                                                                       &pre_stream_varyings, &post_stream_varyings,
                                                                       "vec4",
                                                                       "fastuidraw_run_frag_shader(in uint frag_shader, in uint frag_shader_data_location)",
                                                                       "fastuidraw_gl_frag_main", ", frag_shader_data_location",
                                                                       "frag_shader");
}

void
PainterBackendGLPrivate::
stream_uber_blend_shader(bool use_switch,
                         fastuidraw::gl::Shader::shader_source &frag,
                         fastuidraw::const_c_array<fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBlendShaderGL> > blend_shaders,
                         enum blend_type tp)
{
  std::string sub_func_name, func_name, sub_func_args;
  UberShaderStreamer<fastuidraw::gl::PainterBlendShaderGL>::get_src_type get_src;

  switch(tp)
    {
    case blend_dual_src_blend:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 color0, out vec4 src0, out vec4 src1)";
      sub_func_name = "fastuidraw_gl_compute_blend_factors";
      sub_func_args = ", blend_shader_data_location, color0, src0, src1";
      get_src = &fastuidraw::gl::PainterBlendShaderGL::dual_src_blender_shader;
      break;

    case blend_framebuffer_fetch:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 in_src, in vec4 in_fb, out vec4 out_src)";
      sub_func_name = "fastuidraw_gl_compute_post_blended_value";
      sub_func_args = ", blend_shader_data_location, in_src, in_fb, out_src";
      get_src = &fastuidraw::gl::PainterBlendShaderGL::fetch_blender_shader;
      break;

    case blend_single_src_blend:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 in_src, out vec4 out_src)";
      sub_func_name = "fastuidraw_gl_compute_blend_value";
      sub_func_args = ", blend_shader_data_location, in_src, out_src";
      get_src = &fastuidraw::gl::PainterBlendShaderGL::single_src_blender_shader;
      break;
    }
  UberShaderStreamer<fastuidraw::gl::PainterBlendShaderGL>::stream_uber(use_switch, frag, blend_shaders, get_src,
                                                                        "void", func_name,
                                                                        sub_func_name, sub_func_args, "blend_shader");
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
      m_blender_to_use = &m_no_blending;
      m_blend_shader_group_mask = 0u;
      m_blend_shader_group_shift = 0u;
      m_blend_enabled = false;
      m_shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH";
      m_blend_type = blend_framebuffer_fetch;
    }
  else if(m_have_dual_src_blending)
    {
      m_blender_to_use = &m_dual_src_blend_modes;
      m_blend_shader_group_mask = dual_src_blend_mask;
      m_blend_shader_group_shift = dual_src_blend_bit0;
      m_blend_enabled = true;
      m_shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND";
      m_blend_type = blend_dual_src_blend;
    }
  else
    {
      m_blender_to_use = &m_single_src_blend_modes;
      m_blend_shader_group_mask = single_src_blend_mask;
      m_blend_shader_group_shift = single_src_blend_bit0;
      m_blend_enabled = true;
      m_shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND";
      m_blend_type = blend_single_src_blend;
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
  stream_uber_blend_shader(m_params.blend_shader_use_switch(), frag, make_c_array(m_blend_shaders), m_blend_type);

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
  m_blend_shader_group_mask(0u),
  m_blend_shader_group_shift(0u),
  m_blender_to_use(NULL),  
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
  uint32_t dual_src, single_src;

  assert(!shader->parent());
  assert(shader.dynamic_cast_ptr<PainterBlendShaderGL>());
  h = shader.static_cast_ptr<PainterBlendShaderGL>();

  d->m_rebuild_program = true;
  d->m_blend_shaders.push_back(h);

  dual_src = d->m_dual_src_blend_modes.blend_index(h->dual_src_blender().m_blend_mode);
  assert(dual_src < dual_src_blend_max);
  
  single_src = d->m_single_src_blend_modes.blend_index(h->single_src_blender().m_blend_mode);
  assert(single_src < single_src_blend_max);
  
  return_value.m_ID = d->m_next_blend_shader_ID;
  d->m_next_blend_shader_ID += h->number_sub_shaders();
  return_value.m_group = pack_bits(dual_src_blend_bit0, dual_src_blend_num_bits, dual_src)
    | pack_bits(single_src_blend_bit0, single_src_blend_num_bits, single_src);
  
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

  if(!d->m_backend_configured)
    {
      d->configure_backend();
    }

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
  /* we need to know properties about the GL
     context so that we can do the right thing
     for DrawCommand
   */
  PainterBackendGLPrivate *d;
  d = reinterpret_cast<PainterBackendGLPrivate*>(m_d);
  if(!d->m_backend_configured)
    {
      d->configure_backend();
      /* Set performance rendering hints
       */
      set_hints().clipping_via_hw_clip_planes(d->m_number_clip_planes > 0);
    }
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
                                   d->m_blender_to_use->blend_modes(),
                                   d->m_blend_shader_group_mask,
                                   d->m_blend_shader_group_shift);
}
