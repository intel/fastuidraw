/*!
 * \file scratch_renderer.cpp
 * \brief file scratch_renderer.cpp
 *
 * Copyright 2019 by Intel.
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

#include <private/gl_backend/scratch_renderer.hpp>
#include <private/gl_backend/opengl_trait.hpp>

fastuidraw::gl::detail::ScratchRenderer::
ScratchRenderer(void):
  m_vao(0),
  m_vbo(0),
  m_ibo(0)
{
  const char *vert_src =
    "in vec2 p;\n"
    "void main(void)\n"
    "{\n"
    "\tgl_Position=vec4(p, p);\n"
    "}\n";

  const char *frag_src =
    "out TYPE v;\n"
    "void main(void)\n"
    "{\n"
    "\tv = VALUE;\n"
    "}\n";

  const char *TYPES[number_renders] =
    {
      [float_render] = "vec4",
      [int_render] = "ivec4",
      [uint_render] = "uvec4",
    };

  const char *VALUES[number_renders] =
    {
      [float_render] = "vec4(1.0, 0.5, 1.0, 0.75)",
      [int_render] = "ivec4(1, 5, 1, 75)",
      [uint_render] = "uvec4(1, 5, 1, 75)",
    };

  const char *version;

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      version = "300 es";
    }
  #else
    {
      version = "330";
    }
  #endif

  for (unsigned int i = 0; i < number_renders; ++i)
    {
      m_programs[i] = FASTUIDRAWnew Program(glsl::ShaderSource()
                                            .specify_version(version)
                                            .add_source(vert_src, glsl::ShaderSource::from_string),
                                            glsl::ShaderSource()
                                            .specify_version(version)
                                            .add_macro("TYPE", TYPES[i])
                                            .add_macro("VALUE", VALUES[i])
                                            .add_source(frag_src, glsl::ShaderSource::from_string),
                                            PreLinkActionArray()
                                            .add_binding("p", 0));
    }
}

fastuidraw::gl::detail::ScratchRenderer::
~ScratchRenderer()
{
  if (m_vao != 0)
    {
      fastuidraw_glDeleteVertexArrays(1, &m_vao);
      fastuidraw_glDeleteBuffers(1, &m_vbo);
      fastuidraw_glDeleteBuffers(1, &m_ibo);
    }
}

void
fastuidraw::gl::detail::ScratchRenderer::
ready_vao(void)
{
  if (m_vao != 0)
    {
      fastuidraw_glBindVertexArray(m_vao);
      return;
    }

  vec2 verts[4] =
    {
      vec2(+0.1f, +0.1f),
      vec2(-0.1f, +0.1f),
      vec2(-0.1f, -0.1f),
      vec2(+0.1f, -0.1f)
    };

  GLushort indices[6] =
    {
      0, 1, 2,
      0, 2, 3
    };

  fastuidraw_glGenVertexArrays(1, &m_vao);
  FASTUIDRAWassert(m_vao != 0);

  fastuidraw_glGenBuffers(1, &m_vbo);
  FASTUIDRAWassert(m_vbo !=0 );

  fastuidraw_glGenBuffers(1, &m_ibo);
  FASTUIDRAWassert(m_ibo !=0 );

  fastuidraw_glBindVertexArray(m_vao);
  fastuidraw_glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  fastuidraw_glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

  fastuidraw_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  fastuidraw_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  VertexAttribPointer(0, opengl_trait_values<vec2>());
}

void
fastuidraw::gl::detail::ScratchRenderer::
draw(enum render_type_t t)
{
  ready_vao();
  m_programs[t]->use_program();
  fastuidraw_glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
  fastuidraw_glUseProgram(0);
  fastuidraw_glBindVertexArray(0);
}
