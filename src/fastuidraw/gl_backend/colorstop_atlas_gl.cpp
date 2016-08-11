/*!
 * \file colorstop_atlas_gl.cpp
 * \brief file colorstop_atlas_gl.cpp
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


#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/colorstop_atlas_gl.hpp>

#include "private/texture_gl.hpp"

namespace
{
  class BackingStore:public fastuidraw::ColorStopBackingStore
  {
  public:
    BackingStore(int w, int l, bool delayed);
    ~BackingStore();

    virtual
    void
    set_data(int x, int l, int w,
             fastuidraw::const_c_array<fastuidraw::u8vec4> data);

    virtual
    void
    flush(void)
    {
      m_backing_store.flush();
    }

    GLuint
    texture(void) const
    {
      return m_backing_store.texture();
    }

    virtual
    void
    resize_implement(int new_num_layers);

    static
    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopBackingStore>
    create(int w, int l, bool delayed)
    {
      BackingStore *p;
      p = FASTUIDRAWnew BackingStore(w, l, delayed);
      return fastuidraw::reference_counted_ptr<fastuidraw::ColorStopBackingStore>(p);
    }

  private:
    #ifdef FASTUIDRAW_GL_USE_GLES

    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_2D_ARRAY,
                                             GL_RGBA8, GL_RGBA,
                                             GL_UNSIGNED_BYTE,
                                             GL_LINEAR> TextureGL;

    static
    fastuidraw::ivec3
    dimensions_for_store(int w, int l)
    {
      return fastuidraw::ivec3(w, 1, l);
    }

    static
    fastuidraw::ivec3
    location_for_store(int x, int l)
    {
      return fastuidraw::ivec3(x, 0, l);
    }

    #else

    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_1D_ARRAY,
                                             GL_RGBA8, GL_RGBA,
                                             GL_UNSIGNED_BYTE,
                                             GL_LINEAR> TextureGL;

    static
    fastuidraw::ivec2
    dimensions_for_store(int w, int l)
    {
      return fastuidraw::ivec2(w, l);
    }

    static
    fastuidraw::ivec2
    location_for_store(int x, int l)
    {
      return fastuidraw::ivec2(x, l);
    }

    #endif

    TextureGL m_backing_store;
  };

  class ColorStopAtlasGLParamsPrivate
  {
  public:
    ColorStopAtlasGLParamsPrivate(void):
      m_width(1024),
      m_num_layers(32),
      m_delayed(false)
    {}

    int m_width;
    int m_num_layers;
    bool m_delayed;
  };

  class ColorStopAtlasGLPrivate
  {
  public:
    explicit
    ColorStopAtlasGLPrivate(const fastuidraw::gl::ColorStopAtlasGL::params &P):
      m_params(P)
    {}

    fastuidraw::gl::ColorStopAtlasGL::params m_params;
  };
}

//////////////////////////
// BackingStore methods
BackingStore::
BackingStore(int w, int l, bool delayed):
  fastuidraw::ColorStopBackingStore(w, l, true),
  m_backing_store(dimensions_for_store(w, l), delayed)
{
}

BackingStore::
~BackingStore()
{
}

void
BackingStore::
resize_implement(int new_num_layers)
{
  fastuidraw::ivec2 dims(dimensions());
  m_backing_store.resize(dimensions_for_store(dims.x(), new_num_layers));
}

void
BackingStore::
set_data(int x, int l,
         int w,
         fastuidraw::const_c_array<fastuidraw::u8vec4> data)
{
  assert(data.size() == static_cast<unsigned int>(w) );
  TextureGL::EntryLocation V;

  V.m_location = location_for_store(x, l);
  V.m_size = dimensions_for_store(w, 1);

  m_backing_store.set_data_c_array(V, data.reinterpret_pointer<uint8_t>());

}

///////////////////////////////////////////////
// fastuidraw::gl::ColorStopAtlasGL::params methods
fastuidraw::gl::ColorStopAtlasGL::params::
params(void)
{
  m_d = FASTUIDRAWnew ColorStopAtlasGLParamsPrivate();
}

fastuidraw::gl::ColorStopAtlasGL::params::
params(const params &obj)
{
  ColorStopAtlasGLParamsPrivate *d;
  d = reinterpret_cast<ColorStopAtlasGLParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ColorStopAtlasGLParamsPrivate(*d);
}

fastuidraw::gl::ColorStopAtlasGL::params::
~params()
{
  ColorStopAtlasGLParamsPrivate *d;
  d = reinterpret_cast<ColorStopAtlasGLParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::gl::ColorStopAtlasGL::params&
fastuidraw::gl::ColorStopAtlasGL::params::
operator=(const params &rhs)
{
  if(this != &rhs)
    {
      ColorStopAtlasGLParamsPrivate *d, *rhs_d;
      d = reinterpret_cast<ColorStopAtlasGLParamsPrivate*>(m_d);
      rhs_d = reinterpret_cast<ColorStopAtlasGLParamsPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define paramsSetGet(type, name)                                    \
  fastuidraw::gl::ColorStopAtlasGL::params&                          \
  fastuidraw::gl::ColorStopAtlasGL::params::                         \
  name(type v)                                                      \
  {                                                                 \
    ColorStopAtlasGLParamsPrivate *d;                               \
    d = reinterpret_cast<ColorStopAtlasGLParamsPrivate*>(m_d);      \
    d->m_##name = v;                                                \
    return *this;                                                   \
  }                                                                 \
                                                                    \
  type                                                              \
  fastuidraw::gl::ColorStopAtlasGL::params::                         \
  name(void) const                                                  \
  {                                                                 \
    ColorStopAtlasGLParamsPrivate *d;                               \
    d = reinterpret_cast<ColorStopAtlasGLParamsPrivate*>(m_d);      \
    return d->m_##name;                                             \
  }

paramsSetGet(int, width)
paramsSetGet(int, num_layers)
paramsSetGet(bool, delayed)

#undef paramsSetGet


fastuidraw::gl::ColorStopAtlasGL::params&
fastuidraw::gl::ColorStopAtlasGL::params::
optimal_width(void)
{
  return width(fastuidraw::gl::context_get<GLint>(GL_MAX_TEXTURE_SIZE));
}

//////////////////////////////////////////////////////
// fastuidraw::gl::ColorStopAtlasGL methods
fastuidraw::gl::ColorStopAtlasGL::
ColorStopAtlasGL(const params &P):
  fastuidraw::ColorStopAtlas(BackingStore::create(P.width(), P.num_layers(), P.delayed()))
{
  m_d = FASTUIDRAWnew ColorStopAtlasGLPrivate(P);
}

fastuidraw::gl::ColorStopAtlasGL::
~ColorStopAtlasGL()
{
  ColorStopAtlasGLPrivate *d;
  d = reinterpret_cast<ColorStopAtlasGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::gl::ColorStopAtlasGL::params&
fastuidraw::gl::ColorStopAtlasGL::
param_values(void)
{
  ColorStopAtlasGLPrivate *d;
  d = reinterpret_cast<ColorStopAtlasGLPrivate*>(m_d);
  return d->m_params;
}

GLuint
fastuidraw::gl::ColorStopAtlasGL::
texture(void) const
{
  flush();
  const BackingStore *p;
  assert(dynamic_cast<const BackingStore*>(backing_store().get()));
  p = static_cast<const BackingStore*>(backing_store().get());
  return p->texture();
}

GLenum
fastuidraw::gl::ColorStopAtlasGL::
texture_bind_target(void)
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      return GL_TEXTURE_2D_ARRAY;
    }
  #else
    {
      return GL_TEXTURE_1D_ARRAY;
    }
  #endif
}
