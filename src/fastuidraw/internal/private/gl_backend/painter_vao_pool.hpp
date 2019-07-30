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
  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
  unsigned int m_data_store_binding_point;
  unsigned int m_pool;
};

class painter_vao_pool:public reference_counted<painter_vao_pool>::non_concurrent
{
public:
  explicit
  painter_vao_pool(const PainterEngineGL::ConfigurationGL &params,
                   enum tex_buffer_support_t tex_buffer_support,
                   unsigned int data_store_binding);

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

  /*
   * returns the UBO used to hold the values filled
   * by PainterShaderRegistrarGLSL::fill_uniform_buffer().
   * There is only one such UBO per VAO. It is assumed
   * that the ubo_size NEVER changes once this is
   * called once.
   */
  GLuint
  uniform_ubo(unsigned int ubo_size, GLenum target);

  void
  release_vao(painter_vao &V);

  static
  void
  prepare_index_vertex_sources(GLuint attribute_bo,
			       GLuint header_attribute_bo,
			       GLuint index_bo);

private:
  GLuint
  generate_tbo(GLuint src_buffer, GLenum fmt, unsigned int unit);

  GLuint
  generate_bo(GLenum bind_target, GLsizei psize);

  void
  create_vao(painter_vao &V);

  void
  release_vao_resources(const painter_vao &V);

  unsigned int m_attribute_buffer_size, m_header_buffer_size;
  unsigned int m_index_buffer_size;
  int m_blocks_per_data_buffer;
  unsigned int m_data_buffer_size;
  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
  enum tex_buffer_support_t m_tex_buffer_support;
  unsigned int m_data_store_binding;
  bool m_assume_single_gl_context;

  unsigned int m_current_pool;
  std::vector<std::vector<painter_vao> > m_free_vaos;
  std::vector<GLuint> m_ubos;
};

}}}

#endif
