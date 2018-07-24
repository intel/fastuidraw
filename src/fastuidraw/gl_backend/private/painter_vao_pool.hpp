/*!
 * \file painter_vao_pool.hpp
 * \brief file painter_vao_pool.hpp
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

#pragma once

#include <vector>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>

#include "tex_buffer.hpp"

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
};

class painter_vao_pool:noncopyable
{
public:
  explicit
  painter_vao_pool(const PainterBackendGL::ConfigurationGL &params,
                   const PainterBackend::ConfigurationBase &params_base,
                   enum tex_buffer_support_t tex_buffer_support,
                   const glsl::PainterShaderRegistrarGLSL::BindingPoints &binding_points);

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
   * by PainterBackendGLSL::fill_uniform_buffer().
   * There is only one such UBO per VAO. It is assumed
   * that the ubo_size NEVER changes once this is
   * called once.
   */
  GLuint
  uniform_ubo(unsigned int ubo_size, GLenum target);

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
  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
  enum tex_buffer_support_t m_tex_buffer_support;
  glsl::PainterShaderRegistrarGLSL::BindingPoints m_binding_points;

  unsigned int m_current, m_pool;
  std::vector<std::vector<painter_vao> > m_vaos;
  std::vector<GLuint> m_ubos;
};

}}}
