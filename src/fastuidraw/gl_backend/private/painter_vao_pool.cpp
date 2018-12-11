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

#include "painter_vao_pool.hpp"

///////////////////////////////////////////
// fastuidraw::gl::detail::painter_vao_pool methods
fastuidraw::gl::detail::painter_vao_pool::
painter_vao_pool(const PainterBackendGL::ConfigurationGL &params,
                 enum tex_buffer_support_t tex_buffer_support,
                 const glsl:: PainterShaderRegistrarGLSL::BindingPoints &binding_points):
  m_attribute_buffer_size(params.attributes_per_buffer() * sizeof(PainterAttribute)),
  m_header_buffer_size(params.attributes_per_buffer() * sizeof(uint32_t)),
  m_index_buffer_size(params.indices_per_buffer() * sizeof(PainterIndex)),
  m_blocks_per_data_buffer(params.data_blocks_per_store_buffer()),
  m_data_buffer_size(m_blocks_per_data_buffer * 4 * sizeof(generic_data)),
  m_data_store_backing(params.data_store_backing()),
  m_tex_buffer_support(tex_buffer_support),
  m_binding_points(binding_points),
  m_current(0),
  m_pool(0),
  m_vaos(params.number_pools()),
  m_ubos(params.number_pools(), 0)
{}

fastuidraw::gl::detail::painter_vao_pool::
~painter_vao_pool()
{
  FASTUIDRAWassert(m_ubos.size() == m_vaos.size());
  for(unsigned int p = 0, endp = m_vaos.size(); p < endp; ++p)
    {
      for(const painter_vao &vao : m_vaos[p])
        {
          if (vao.m_data_tbo != 0)
            {
              fastuidraw_glDeleteTextures(1, &vao.m_data_tbo);
            }
          fastuidraw_glDeleteBuffers(1, &vao.m_attribute_bo);
          fastuidraw_glDeleteBuffers(1, &vao.m_header_bo);
          fastuidraw_glDeleteBuffers(1, &vao.m_index_bo);
          fastuidraw_glDeleteBuffers(1, &vao.m_data_bo);
          fastuidraw_glDeleteVertexArrays(1, &vao.m_vao);
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
  if (m_ubos[m_pool] == 0)
    {
      m_ubos[m_pool] = generate_bo(target, sz);
    }
  else
    {
      fastuidraw_glBindBuffer(target, m_ubos[m_pool]);
    }

  #ifndef NDEBUG
    {
      GLint psize(0);
      fastuidraw_glGetBufferParameteriv(target, GL_BUFFER_SIZE, &psize);
      FASTUIDRAWassert(psize >= static_cast<int>(sz));
    }
  #endif

  return m_ubos[m_pool];
}

fastuidraw::gl::detail::painter_vao
fastuidraw::gl::detail::painter_vao_pool::
request_vao(void)
{
  painter_vao return_value;

  if (m_current == m_vaos[m_pool].size())
    {
      opengl_trait_value v;

      m_vaos[m_pool].resize(m_current + 1);
      fastuidraw_glGenVertexArrays(1, &m_vaos[m_pool][m_current].m_vao);

      FASTUIDRAWassert(m_vaos[m_pool][m_current].m_vao != 0);
      fastuidraw_glBindVertexArray(m_vaos[m_pool][m_current].m_vao);

      m_vaos[m_pool][m_current].m_data_store_backing = m_data_store_backing;

      switch(m_data_store_backing)
        {
        case glsl::PainterShaderRegistrarGLSL::data_store_tbo:
          {
            m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_ARRAY_BUFFER, m_data_buffer_size);
            m_vaos[m_pool][m_current].m_data_store_binding_point = m_binding_points.data_store_buffer_tbo();
            generate_tbos(m_vaos[m_pool][m_current]);
          }
          break;

        case glsl::PainterShaderRegistrarGLSL::data_store_ubo:
          {
            m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_ARRAY_BUFFER, m_data_buffer_size);
            m_vaos[m_pool][m_current].m_data_store_binding_point = m_binding_points.data_store_buffer_ubo();
          }
          break;

        case glsl::PainterShaderRegistrarGLSL::data_store_ssbo:
          {
            m_vaos[m_pool][m_current].m_data_bo = generate_bo(GL_ARRAY_BUFFER, m_data_buffer_size);
            m_vaos[m_pool][m_current].m_data_store_binding_point = m_binding_points.data_store_buffer_ssbo();
          }
          break;
        }

      /* generate_bo leaves the returned buffer object bound to
       * the passed binding target.
       */
      m_vaos[m_pool][m_current].m_attribute_bo = generate_bo(GL_ARRAY_BUFFER, m_attribute_buffer_size);
      m_vaos[m_pool][m_current].m_index_bo = generate_bo(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_size);

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

      m_vaos[m_pool][m_current].m_header_bo = generate_bo(GL_ARRAY_BUFFER, m_header_buffer_size);
      fastuidraw_glEnableVertexAttribArray(glsl::PainterShaderRegistrarGLSL::header_attrib_slot);
      v = opengl_trait_values<uint32_t>();
      VertexAttribIPointer(glsl::PainterShaderRegistrarGLSL::header_attrib_slot, v);

      fastuidraw_glBindVertexArray(0);
    }

  return_value = m_vaos[m_pool][m_current];
  ++m_current;

  return return_value;
}

void
fastuidraw::gl::detail::painter_vao_pool::
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
fastuidraw::gl::detail::painter_vao_pool::
generate_tbos(painter_vao &vao)
{
  vao.m_data_tbo = generate_tbo(vao.m_data_bo, GL_RGBA32UI,
                                m_binding_points.data_store_buffer_tbo());
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
