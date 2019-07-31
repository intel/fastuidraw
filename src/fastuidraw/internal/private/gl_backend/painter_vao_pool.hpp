/*!
 * \file painter_vao_pool.hpp
 * \brief file painter_vao_pool.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef FASTUIDRAW_PAINTER_VAO_POOL_HPP
#define FASTUIDRAW_PAINTER_VAO_POOL_HPP

#include <vector>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>

#include <private/gl_backend/tex_buffer.hpp>
#include <private/gl_backend/opengl_trait.hpp>

namespace fastuidraw { namespace gl { namespace detail {

class client_buffers:public reference_counted<client_buffers>::non_concurrent
{
public:
  client_buffers(uint32_t num_attributes,
                 uint32_t num_indices,
                 uint32_t num_data):
    m_attributes_store(num_attributes),
    m_header_attributes_store(num_attributes),
    m_indices_store(num_indices),
    m_data_store(num_data)
  {}

  std::vector<PainterAttribute> m_attributes_store;
  std::vector<uint32_t> m_header_attributes_store;
  std::vector<PainterIndex> m_indices_store;
  std::vector<uvec4> m_data_store;
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
  
  c_array<PainterAttribute>
  attributes(void) const
  {
    return m_attributes;
  }
  c_array<uint32_t>
  header_attributes(void) const
  {
    return m_header_attributes;
  }

  c_array<PainterIndex>
  indices(void) const
  {
    return m_indices;
  }

  c_array<uvec4>
  data(void) const
  {
    return m_data;
  }

  GLuint
  vao(void) const
  {
    return m_vao;
  }

  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t
  data_store_backing(void) const
  {
    return m_data_store_backing;
  }

  unsigned int
  data_store_binding_point(void) const
  {
    return m_data_store_binding_point;
  }

  GLuint
  data_bo(void) const
  {
    return m_data_bo;
  }

  GLuint
  data_tbo(void) const
  {
    return m_data_tbo;
  }
  
private:
  friend class painter_vao_pool;

  GLuint m_vao;
  GLuint m_attribute_bo, m_header_bo, m_index_bo, m_data_bo;
  GLuint m_data_tbo;
  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
  unsigned int m_data_store_binding_point;
  unsigned int m_pool;
  reference_counted_ptr<client_buffers> m_buffers;
  c_array<PainterAttribute> m_attributes;
  c_array<uint32_t> m_header_attributes;
  c_array<PainterIndex> m_indices;
  c_array<uvec4> m_data;
};

class painter_vao_pool:public reference_counted<painter_vao_pool>::non_concurrent
{
public:
  explicit
  painter_vao_pool(const PainterEngineGL::ConfigurationGL &params,
                   enum tex_buffer_support_t tex_buffer_support,
                   unsigned int data_store_binding);

  ~painter_vao_pool();

  painter_vao
  request_vao(void);

  void
  release_vao(painter_vao &V);

  void
  next_pool(void);

  /*
   * returns the UBO used to hold the values filled
   * by PainterShaderRegistrarGLSL::fill_uniform_buffer().
   * There is only one such UBO per VAO. It is assumed
   * that the ubo_size NEVER changes once this is
   * called once.
   */
  GLuint
  uniform_ubo(unsigned int ubo_size, GLenum target);

  static
  void
  prepare_index_vertex_sources(GLuint attribute_bo,
                               GLuint header_attribute_bo,
                               GLuint index_bo);

  void
  unmap_vao_buffers(unsigned int attributes_written,
                    unsigned int indices_written,
                    unsigned int data_store_written,
                    const painter_vao &vao);

private:
  GLuint
  generate_tbo(GLuint src_buffer, GLenum fmt, unsigned int unit);

  GLuint
  generate_bo(GLenum bind_target, GLsizei psize);

  void
  create_vao(painter_vao &V);

  void
  release_vao_resources(const painter_vao &V);

  unsigned int m_num_attributes, m_num_indices, m_blocks_per_data_buffer;
  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
  enum tex_buffer_support_t m_tex_buffer_support;
  unsigned int m_data_store_binding;
  bool m_assume_single_gl_context;
  enum PainterEngineGL::buffer_streaming_type_t m_buffer_streaming_type;

  unsigned int m_current_pool;
  std::vector<std::vector<painter_vao> > m_free_vaos;
  std::vector<GLuint> m_ubos;
};

}}}

#endif
