/*!
 * \file colorstop_atlas_gl.cpp
 * \brief file colorstop_atlas_gl.cpp
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


#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>

#include <private/util_private.hpp>
#include <private/gl_backend/texture_gl.hpp>
#include <private/gl_backend/colorstop_atlas_gl.hpp>

namespace
{
  class BackingStore:public fastuidraw::ColorStopBackingStore
  {
  public:
    BackingStore(int w, int l);
    ~BackingStore();

    virtual
    void
    set_data(int x, int l, int w,
             fastuidraw::c_array<const fastuidraw::u8vec4> data);

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
    create(int w, int l)
    {
      BackingStore *p;
      p = FASTUIDRAWnew BackingStore(w, l);
      return fastuidraw::reference_counted_ptr<fastuidraw::ColorStopBackingStore>(p);
    }

  private:
    #ifdef FASTUIDRAW_GL_USE_GLES

    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_2D_ARRAY,
                                              GL_RGBA8, GL_RGBA,
                                              GL_UNSIGNED_BYTE,
                                              GL_LINEAR, GL_LINEAR> TextureGL;

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
                                              GL_LINEAR, GL_LINEAR> TextureGL;

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
}

//////////////////////////
// BackingStore methods
BackingStore::
BackingStore(int w, int l):
  fastuidraw::ColorStopBackingStore(w, l),
  m_backing_store(dimensions_for_store(w, l), true)
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
         fastuidraw::c_array<const fastuidraw::u8vec4> data)
{
  FASTUIDRAWassert(data.size() == static_cast<unsigned int>(w) );
  TextureGL::EntryLocation V;
  fastuidraw::c_array<const uint8_t> pdata;

  V.m_location = location_for_store(x, l);
  V.m_size = dimensions_for_store(w, 1);
  pdata = data.reinterpret_pointer<const uint8_t>();

  m_backing_store.set_data_c_array(V, pdata);
}

//////////////////////////////////////////////////////
// fastuidraw::gl::detail::ColorStopAtlasGL methods
fastuidraw::gl::detail::ColorStopAtlasGL::
ColorStopAtlasGL(const PainterEngineGL::ColorStopAtlasParams &P):
  fastuidraw::ColorStopAtlas(BackingStore::create(P.width(), P.num_layers()))
{
}

fastuidraw::gl::detail::ColorStopAtlasGL::
~ColorStopAtlasGL()
{
}

GLuint
fastuidraw::gl::detail::ColorStopAtlasGL::
texture(void) const
{
  flush();
  const BackingStore *p;
  FASTUIDRAWassert(dynamic_cast<const BackingStore*>(backing_store().get()));
  p = static_cast<const BackingStore*>(backing_store().get());
  return p->texture();
}

GLenum
fastuidraw::gl::detail::ColorStopAtlasGL::
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
