/*!
 * \file texture_view.cpp
 * \brief file texture_view.cpp
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

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include "texture_view.hpp"

enum fastuidraw::gl::detail::texture_view_support_t
fastuidraw::gl::detail::
compute_texture_view_support(void)
{
  ContextProperties ctx;
  if (ctx.is_es())
    {
      if (ctx.has_extension("GL_OES_texture_view"))
        {
          return texture_view_oes_extension;
        }
      if (ctx.has_extension("GL_EXT_texture_view"))
        {
          return texture_view_ext_extension;
        }
    }
  else
    {
      #ifndef __APPLE__
        {
          if (ctx.version() >= ivec2(4, 3) || ctx.has_extension("GL_ARB_texture_view"))
            {
              return texture_view_without_extension;
            }
        }
      #endif
    }

  return texture_view_not_supported;
}

void
fastuidraw::gl::detail::
texture_view(enum texture_view_support_t md,
             GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat,
             GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
  #ifdef __APPLE__
    {
      FASTUIDRAWunused(md);
      FASTUIDRAWunused(texture);
      FASTUIDRAWunused(target);
      FASTUIDRAWunused(origtexture);
      FASTUIDRAWunused(internalformat);
      FASTUIDRAWunused(minlevel);
      FASTUIDRAWunused(numlevels);
      FASTUIDRAWunused(minlayer);
      FASTUIDRAWunused(numlayers);
      FASTUIDRAWassert(!"glTextureView not supported by GL context!\n");
    }
  #elif !defined(FASTUIDRAW_GL_USE_GLES)
    {
      if (md == texture_view_without_extension)
        {
          glTextureView(texture, target, origtexture, internalformat,
                        minlevel, numlevels, minlayer, numlayers);
        }
      else
        {
          FASTUIDRAWassert(!"glTextureView not supported by GL context!\n");
        }
    }
  #else
    {
      switch(md)
        {
        case texture_view_oes_extension:
          glTextureViewOES(texture, target, origtexture, internalformat,
                           minlevel, numlevels, minlayer, numlayers);
          break;

        case texture_view_ext_extension:
          glTextureViewEXT(texture, target, origtexture, internalformat,
                           minlevel, numlevels, minlayer, numlayers);
          break;

        default:
          FASTUIDRAWassert(!"glTextureView not supported by GL context!\n");
        }
    }
  #endif
}
