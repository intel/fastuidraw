/*!
 * \file buffer_object_gl.hpp
 * \brief file buffer_object_gl.hpp
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

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>

namespace fastuidraw { namespace gl { namespace detail {

class BufferGLEntryLocation
{
public:
  int m_location;
  std::vector<uint8_t> m_data;
};

/*!\class BufferGL
 * Wrapper over the GL buffer API providing ability to delay updates to
 * undlering buffer until flush().
 * \tparam binding_point GL binding point (ala glBindBuffer) to use for GL
 *                       operations
 * \tparam usage GL usage parameter to pass to glBufferData when buffer
 *               object is created
 */
template<GLenum binding_point,
         GLenum usage>
class BufferGL
{
public:
  BufferGL(GLsizei psize, bool delayed):
    m_size(psize),
    m_buffer_size(psize),
    m_delayed(delayed),
    m_buffer(0)
  {
    FASTUIDRAWassert(m_size > 0);
    if (!m_delayed)
      {
        create_buffer();
      }
  }

  ~BufferGL()
  {
    if (m_buffer != 0)
      {
        delete_buffer();
      }
  }

  void
  delete_buffer(void)
  {
    FASTUIDRAWassert(m_buffer != 0);
    glBindBuffer(binding_point, 0);
    glDeleteBuffers(1, &m_buffer);
    m_buffer = 0;
  }

  void
  set_data(int offset, c_array<const uint8_t> data)
  {
    FASTUIDRAWassert(!data.empty());
    if (m_delayed)
      {
        m_unflushed_commands.push_back(BufferGLEntryLocation());
        m_unflushed_commands.back().m_location = offset;
        m_unflushed_commands.back().m_data.resize(data.size());
        std::copy(data.begin(), data.end(), m_unflushed_commands.back().m_data.begin());
      }
    else
      {
        flush_size_change();
        glBindBuffer(binding_point, m_buffer);
        glBufferSubData(binding_point, offset, data.size(), &data[0]);
      }
  }

  void
  set_data_vector(int offset, std::vector<uint8_t> &data)
  {
    FASTUIDRAWassert(!data.empty());
    if (m_delayed)
      {
        m_unflushed_commands.push_back(BufferGLEntryLocation());
        m_unflushed_commands.back().m_location = offset;
        m_unflushed_commands.back().m_data.swap(data);
      }
    else
      {
        set_data(offset, data);
      }
  }

  void
  flush(void)
  {
    flush_size_change();
    if (m_buffer == 0)
      {
        create_buffer();
      }

    if (!m_unflushed_commands.empty())
      {
        glBindBuffer(binding_point, m_buffer);
        for(BufferGLEntryLocation &B : m_unflushed_commands)
          {
            FASTUIDRAWassert(!B.m_data.empty());
            glBufferSubData(binding_point, B.m_location, B.m_data.size(), &B.m_data[0]);
          }
        m_unflushed_commands.clear();
      }
  }

  GLuint
  buffer(void) const
  {
    FASTUIDRAWassert(m_buffer != 0);
    return m_buffer;
  }

  GLsizei
  size(void) const
  {
    return m_size;
  }

  void
  resize(GLsizei new_size)
  {
    m_size = new_size;
  }

private:

  void
  flush_size_change(void)
  {
    if (m_size != m_buffer_size)
      {
        if (m_buffer != 0)
          {
            GLuint old_buffer, prev_buffer(0);
            GLenum src_binding_point, src_binding_point_query;

            old_buffer = m_buffer;
            m_buffer = 0;
            create_buffer();

            /* The values of GL_COPY_READ/WRITE_BUFFER
             * and GL_COPY_READ/WRITE_BUFFER_BINDING are
             * the -same-. However, some GL headers (namely
             * Apples OpenGL/gl3.h do not define the macro
             * values with the _BINDING suffix.
             */
            if (binding_point != GL_COPY_READ_BUFFER)
              {
                src_binding_point = GL_COPY_READ_BUFFER;
                src_binding_point_query = GL_COPY_READ_BUFFER;
              }
            else
              {
                src_binding_point = GL_COPY_WRITE_BUFFER;
                src_binding_point_query = GL_COPY_WRITE_BUFFER;
              }
            prev_buffer = fastuidraw::gl::context_get<GLint>(src_binding_point_query);
            glBindBuffer(binding_point, m_buffer);
            glBindBuffer(src_binding_point, old_buffer);

            glCopyBufferSubData(src_binding_point, binding_point,
                                0, 0, std::min(m_buffer_size, m_size));

            glBindBuffer(src_binding_point, prev_buffer);
            glDeleteBuffers(1, &old_buffer);
          }

        m_buffer_size = m_size;
      }
  }

  void
  create_buffer(void)
  {
    FASTUIDRAWassert(m_buffer == 0);
    glGenBuffers(1, &m_buffer);
    FASTUIDRAWassert(m_buffer != 0);
    glBindBuffer(binding_point, m_buffer);
    glBufferData(binding_point, m_size, nullptr, usage);
  }

  GLsizei m_size;
  GLsizei m_buffer_size;
  bool m_delayed;
  mutable GLuint m_buffer;
  std::list<BufferGLEntryLocation> m_unflushed_commands;
};

} //namespace detail
} //namespace gl
} //namespace fastuidraw
