/*!
 * \file opengl_trait.cpp
 * \brief file opengl_trait.cpp
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

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>

namespace fastuidraw {
namespace gl {

  void
  VertexAttribPointer(GLint index, const opengl_trait_value &v, GLboolean normalized)
  {
    glVertexAttribPointer(index, v.m_count, v.m_type, normalized, v.m_stride, v.m_offset);
  }

  void
  VertexAttribIPointer(GLint index, const opengl_trait_value &v)
  {
    glVertexAttribIPointer(index, v.m_count, v.m_type, v.m_stride, v.m_offset);
  }

} //namespace gl
} //namespace fastuidraw
