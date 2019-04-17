/*!
 * \file scratch_renderer.hpp
 * \brief file scratch_renderer.hpp
 *
 * Copyright 2019 by Intel.
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

#include <list>
#include <vector>
#include <algorithm>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

namespace fastuidraw {
namespace gl {
namespace detail {

class ScratchRenderer:public reference_counted<ScratchRenderer>::concurrent
{
public:
  enum render_type_t
    {
      float_render,
      int_render,
      uint_render,

      number_renders
    };

  ScratchRenderer(void);
  ~ScratchRenderer();

  void
  draw(enum render_type_t t);

private:
  void
  ready_vao(void);

  vecN<reference_counted_ptr<Program>,  number_renders> m_programs;
  GLuint m_vao, m_vbo, m_ibo;
};

}}}
