/*!
 * \file painter_surface_gl_private.hpp
 * \brief file painter_surface_gl_private.hpp
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

#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>

namespace fastuidraw { namespace gl { namespace detail {

class SurfaceGLPrivate:noncopyable
{
public:
  enum fbo_tp_bits
    {
      fbo_color_buffer_bit,
      fbo_auxiliary_buffer_bit,
      fbo_num_bits,

      fbo_color_buffer = FASTUIDRAW_MASK(fbo_color_buffer_bit, 1),
      fbo_auxiliary_buffer = FASTUIDRAW_MASK(fbo_auxiliary_buffer_bit, 1),

      number_fbo_t = FASTUIDRAW_MASK(0, fbo_num_bits) + 1
    };

  enum auxiliary_buffer_t
    {
      auxiliary_buffer_u8,
      auxiliary_buffer_u32,

      number_auxiliary_buffer_t
    };

  explicit
  SurfaceGLPrivate(GLuint texture,
                   const PainterBackendGL::SurfaceGL::Properties &props);

  ~SurfaceGLPrivate();

  static
  PainterBackendGL::SurfaceGL*
  surface_gl(const reference_counted_ptr<PainterBackend::Surface> &surface);

  GLuint
  auxiliary_buffer(enum auxiliary_buffer_t tp);

  static
  GLenum
  auxiliaryBufferInternalFmt(enum auxiliary_buffer_t tp)
  {
    return tp == auxiliary_buffer_u8 ?
      GL_R8 :
      GL_R32UI;
  }

  GLuint
  color_buffer(void)
  {
    return buffer(buffer_color);
  }

  static
  uint32_t
  fbo_bits(enum PainterBackendGL::auxiliary_buffer_t aux,
           enum PainterBackendGL::compositing_type_t compositing);

  GLuint
  fbo(uint32_t tp);

  c_array<const GLenum>
  draw_buffers(uint32_t tp);

  GLuint
  fbo(enum PainterBackendGL::auxiliary_buffer_t aux,
      enum PainterBackendGL::compositing_type_t compositing)
  {
    return fbo(fbo_bits(aux, compositing));
  }

  c_array<const GLenum>
  draw_buffers(enum PainterBackendGL::auxiliary_buffer_t aux,
               enum PainterBackendGL::compositing_type_t compositing)
  {
    return draw_buffers(fbo_bits(aux, compositing));
  }

  PainterBackend::Surface::Viewport m_viewport;
  vec4 m_clear_color;
  PainterBackendGL::SurfaceGL::Properties m_properties;

private:
  enum buffer_t
    {
      buffer_color,
      buffer_depth,

      number_buffer_t
    };

  GLuint
  buffer(enum buffer_t);

  vecN<GLuint, number_auxiliary_buffer_t> m_auxiliary_buffer;
  vecN<GLuint, number_buffer_t> m_buffers;
  vecN<GLuint, number_fbo_t> m_fbo;
  vecN<vecN<GLenum, 2>, 4> m_draw_buffer_values;
  vecN<c_array<const GLenum>, 4> m_draw_buffers;

  bool m_own_texture;
};

}}}
