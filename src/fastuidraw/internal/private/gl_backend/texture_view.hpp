/*!
 * \file texture_view.hpp
 * \brief file texture_view.hpp
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


#pragma once

#include <fastuidraw/gl_backend/ngl_header.hpp>

namespace fastuidraw { namespace gl { namespace detail {

enum texture_view_support_t
  {
    texture_view_without_extension,
    texture_view_oes_extension,
    texture_view_ext_extension,
    texture_view_not_supported
  };

enum texture_view_support_t
compute_texture_view_support(void);


void
texture_view(enum texture_view_support_t md,
             GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat,
             GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);

} //namespace detail
} //namespace gl
} //namespace fastuidraw
