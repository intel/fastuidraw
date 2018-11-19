/*!
 * \file bindless.hpp
 * \brief file bindless.hpp
 *
 * Copyright 2018 by Intel.
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
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include "bindless.hpp"

fastuidraw::gl::detail::Bindless::
Bindless(void)
{
  gl::ContextProperties ctx;
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if(ctx.has_extension("GL_NV_bindless_texture"))
        {
          m_type = nv_bindless_texture;
        }
      else
        {
          m_type = no_bindless_texture;
        }
    }
  #else
    {
      if (ctx.has_extension("GL_ARB_bindless_texture"))
        {
          m_type = arb_bindless_texture;
        }
      else if (ctx.has_extension("GL_NV_bindless_texture"))
        {
          m_type = nv_bindless_texture;
        }
      else
        {
          m_type = no_bindless_texture;
        }
    }
  #endif
}

GLuint64
fastuidraw::gl::detail::Bindless::
get_texture_handle(GLuint tex) const
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      FASTUIDRAWassert(m_type == nv_bindless_texture);
      return glGetTextureHandleNV(tex);
    }
  #else
    {
      FASTUIDRAWassert(m_type != no_bindless_texture);
      if (m_type == nv_bindless_texture)
        {
          return glGetTextureHandleNV(tex);
        }
      else
        {
          return glGetTextureHandleARB(tex);
        }
    }
  #endif
}

void
fastuidraw::gl::detail::Bindless::
make_texture_handle_resident(GLuint64 h) const
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      FASTUIDRAWassert(m_type == nv_bindless_texture);
      glMakeTextureHandleResidentNV(h);
    }
  #else
    {
      FASTUIDRAWassert(m_type != no_bindless_texture);
      if (m_type == nv_bindless_texture)
        {
          glMakeTextureHandleResidentNV(h);
        }
      else
        {
          glMakeTextureHandleResidentARB(h);
        }
    }
  #endif
}

void
fastuidraw::gl::detail::Bindless::
make_texture_handle_non_resident(GLuint64 h) const
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      FASTUIDRAWassert(m_type == nv_bindless_texture);
      glMakeTextureHandleNonResidentNV(h);
    }
  #else
    {
      FASTUIDRAWassert(m_type != no_bindless_texture);
      if (m_type == nv_bindless_texture)
        {
          glMakeTextureHandleNonResidentNV(h);
        }
      else
        {
          glMakeTextureHandleNonResidentARB(h);
        }
    }
  #endif
}

const fastuidraw::gl::detail::Bindless&
fastuidraw::gl::detail::
bindless(void)
{
  static Bindless B;
  return B;
}
