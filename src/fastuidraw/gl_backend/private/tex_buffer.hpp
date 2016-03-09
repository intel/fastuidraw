/*!
 * \file tex_buffer.hpp
 * \brief file tex_buffer.hpp
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

#include <fastuidraw/gl_backend/gl_header.hpp>

namespace fastuidraw { namespace gl { namespace detail {

enum tex_buffer_support_t
  {
    tex_buffer_no_extension,
    tex_buffer_oes_extension,
    tex_buffer_ext_extension,

    tex_buffer_not_supported,
  };

enum tex_buffer_support_t
compute_tex_buffer_support(void);

void
tex_buffer(enum tex_buffer_support_t md, GLenum target, GLenum format, GLuint bo);



} //namespace detail
} //namespace gl
} //namespace fastuidraw
