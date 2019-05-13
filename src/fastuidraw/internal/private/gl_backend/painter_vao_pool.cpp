/*!
 * \file painter_vao_pool.cpp
 * \brief file painter_vao_pool.cpp
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

#include <fastuidraw/gl_backend/gl_binding.hpp>
#include <private/gl_backend/painter_vao_pool.hpp>

///////////////////////////////////////////
// fastuidraw::gl::detail::painter_vao_pool methods
fastuidraw::gl::detail::painter_vao_pool::
painter_vao_pool(const PainterEngineGL::ConfigurationGL &params,
                 enum tex_buffer_support_t tex_buffer_support,
                 unsigned int data_store_binding):
  m_attribute_buffer_size(params.attributes_per_buffer() * sizeof(PainterAttribute)),
  m_header_buffer_size(params.attributes_per_buffer() * sizeof(uint32_t)),
  m_index_buffer_size(params.indices_per_buffer() * sizeof(PainterIndex)),
  m_blocks_per_data_buffer(params.data_blocks_per_store_buffer()),
  m_data_buffer_size(m_blocks_per_data_buffer * 4 * sizeof(uint32_t)),
  m_data_store_backing(params.data_store_backing()),
  m_tex_buffer_support(tex_buffer_support),
  m_data_store_binding(data_store_binding),
  m_current_pool(0),
  m_free_vaos(params.number_pools()),
  m_ubos(params.number_pools(), 0)
{}

fastuidraw::gl::detail::painter_vao_pool::
~painter_vao_pool()
{
  FASTUIDRAWassert(m_ubos.size() == m_free_vaos.size());
  for(unsigned int p = 0, endp = m_free_vaos.size(); p < endp; ++p)
    {
      for(const painter_vao &vao : m_free_vaos[p])
        {
          release_vao_resources(vao);
        }

      if (m_ubos[p] != 0)
        {
          fastuidraw_glDeleteBuffers(1, &m_ubos[p]);
        }
    }
}

GLuint
fastuidraw::gl::detail::painter_vao_pool::
uniform_ubo(unsigned int sz, GLenum target)
{
  if (m_ubos[m_current_pool] == 0)
    {
      m_ubos[m_current_pool] = generate_bo(target, sz);
    }
  else
    {
      fastuidraw_glBindBuffer(target, m_ubos[m_current_pool]);
    }

  #ifndef NDEBUG
    {
      GLint psize(0);
      fastuidraw_glGetBufferParameteriv(target, GL_BUFFER_SIZE, &psize);
      FASTUIDRAWassert(psize >= static_cast<int>(sz));
    }
  #endif

  return m_ubos[m_current_pool];
}

fastuidraw::gl::detail::painter_vao
fastuidraw::gl::detail::painter_vao_pool::
request_vao(void)
{
  painter_vao return_value;

  if (m_free_vaos[m_current_pool].empty())
    {
      return_value.m_data_store_backing = m_data_store_backing;
      return_value.m_data_store_binding_point = m_data_store_binding;
      return_value.m_data_bo = generate_bo(GL_ARRAY_BUFFER, m_data_buffer_size);
      return_value.m_attribute_bo = generate_bo(GL_ARRAY_BUFFER, m_attribute_buffer_size);
      return_value.m_index_bo = generate_bo(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_size);
      return_value.m_header_bo = generate_bo(GL_ARRAY_BUFFER, m_header_buffer_size);
      if (m_data_store_backing == glsl::PainterShaderRegistrarGLSL::data_store_tbo)
        {
          return_value.m_data_tbo = generate_tbo(return_value.m_data_bo, GL_RGBA32UI,
                                                 return_value.m_data_store_binding_point);
        }
      return_value.m_pool = m_current_pool;
    }
  else
    {
      return_value = m_free_vaos[m_current_pool].back();
      m_free_vaos[m_current_pool].pop_back();
    }

  /* We re-create the VAO in case GL contexts have changed */
  create_vao(return_value);
  FASTUIDRAWassert(return_value.m_pool == m_current_pool);
  return return_value;
}

void
fastuidraw::gl::detail::painter_vao_pool::
create_vao(painter_vao &return_value)
{
  opengl_trait_value v;

  FASTUIDRAWassert(return_value.m_vao == 0);

  fastuidraw_glGenVertexArrays(1, &return_value.m_vao);
  fastuidraw_glBindVertexArray(return_value.m_vao);

  fastuidraw_glBindBuffer(GL_ARRAY_BUFFER, return_value.m_attribute_bo);
  fastuidraw_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, return_value.m_index_bo);

  fastuidraw_glEnableVertexAttribArray(glsl::PainterShaderRegistrarGLSL::attribute0_slot);
  v = opengl_trait_values<uvec4>(sizeof(PainterAttribute),
                                 offsetof(PainterAttribute, m_attrib0));
  VertexAttribIPointer(glsl::PainterShaderRegistrarGLSL::attribute0_slot, v);

  fastuidraw_glEnableVertexAttribArray(glsl::PainterShaderRegistrarGLSL::attribute1_slot);
  v = opengl_trait_values<uvec4>(sizeof(PainterAttribute),
                                 offsetof(PainterAttribute, m_attrib1));
  VertexAttribIPointer(glsl::PainterShaderRegistrarGLSL::attribute1_slot, v);

  fastuidraw_glEnableVertexAttribArray(glsl::PainterShaderRegistrarGLSL::attribute2_slot);
  v = opengl_trait_values<uvec4>(sizeof(PainterAttribute),
                                 offsetof(PainterAttribute, m_attrib2));
  VertexAttribIPointer(glsl::PainterShaderRegistrarGLSL::attribute2_slot, v);

  fastuidraw_glBindBuffer(GL_ARRAY_BUFFER, return_value.m_header_bo);
  fastuidraw_glEnableVertexAttribArray(glsl::PainterShaderRegistrarGLSL::header_attrib_slot);
  v = opengl_trait_values<uint32_t>();
  VertexAttribIPointer(glsl::PainterShaderRegistrarGLSL::header_attrib_slot, v);
  fastuidraw_glBindVertexArray(0);
}

void
fastuidraw::gl::detail::painter_vao_pool::
release_vao_resources(const painter_vao &V)
{
  if (V.m_data_tbo != 0)
    {
      fastuidraw_glDeleteTextures(1, &V.m_data_tbo);
    }
  fastuidraw_glDeleteBuffers(1, &V.m_attribute_bo);
  fastuidraw_glDeleteBuffers(1, &V.m_header_bo);
  fastuidraw_glDeleteBuffers(1, &V.m_index_bo);
  fastuidraw_glDeleteBuffers(1, &V.m_data_bo);
  FASTUIDRAWassert(V.m_vao == 0);
}

void
fastuidraw::gl::detail::painter_vao_pool::
next_pool(void)
{
  ++m_current_pool;
  if (m_current_pool == m_free_vaos.size())
    {
      m_current_pool = 0;
    }
}

void
fastuidraw::gl::detail::painter_vao_pool::
release_vao(painter_vao &V)
{
  FASTUIDRAWassert(V.m_pool < m_free_vaos.size());
  fastuidraw_glDeleteVertexArrays(1, &V.m_vao);
  V.m_vao = 0;
  m_free_vaos[V.m_pool].push_back(V);
}

GLuint
fastuidraw::gl::detail::painter_vao_pool::
generate_tbo(GLuint src_buffer, GLenum fmt, unsigned int unit)
{
  GLuint return_value(0);

  fastuidraw_glGenTextures(1, &return_value);
  FASTUIDRAWassert(return_value != 0);

  fastuidraw_glActiveTexture(GL_TEXTURE0 + unit);
  fastuidraw_glBindTexture(GL_TEXTURE_BUFFER, return_value);
  tex_buffer(m_tex_buffer_support, GL_TEXTURE_BUFFER, fmt, src_buffer);

  return return_value;
}

GLuint
fastuidraw::gl::detail::painter_vao_pool::
generate_bo(GLenum bind_target, GLsizei psize)
{
  GLuint return_value(0);
  fastuidraw_glGenBuffers(1, &return_value);
  FASTUIDRAWassert(return_value != 0);
  fastuidraw_glBindBuffer(bind_target, return_value);
  fastuidraw_glBufferData(bind_target, psize, nullptr, GL_STREAM_DRAW);
  return return_value;
}
