/*!
 * \file image_gl.cpp
 * \brief file image_gl.cpp
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


#include <vector>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/image_gl.hpp>
#include "private/texture_gl.hpp"
#include "../private/util_private.hpp"



namespace
{
  template<GLenum internal_format,
           GLenum external_format,
           GLenum filter>
  class Texture
  {
  public:
    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_2D_ARRAY,
                                              internal_format, external_format,
                                              GL_UNSIGNED_BYTE, filter> type;
  };

  class ColorBackingStoreGL:public fastuidraw::AtlasColorBackingStoreBase
  {
  public:
    ColorBackingStoreGL(int log2_tile_size, int log2_num_tiles_per_row_per_col, int number_layers, bool delayed);
    ~ColorBackingStoreGL() {}

    virtual
    void
    set_data(int x, int y, int l,
             int w, int h,
             fastuidraw::const_c_array<fastuidraw::u8vec4> pdata);

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

    static
    fastuidraw::ivec3
    store_size(int log2_tile_size, int log2_num_tiles_per_row_per_col, int num_layers);

    static
    fastuidraw::reference_counted_ptr<fastuidraw::AtlasColorBackingStoreBase>
    create(int log2_tile_size, int log2_num_tiles_per_row_per_col, int num_layers, bool delayed)
    {
      ColorBackingStoreGL *p;
      p = FASTUIDRAWnew ColorBackingStoreGL(log2_tile_size, log2_num_tiles_per_row_per_col, num_layers, delayed);
      return fastuidraw::reference_counted_ptr<fastuidraw::AtlasColorBackingStoreBase>(p);
    }

  protected:
    virtual
    void
    resize_implement(int new_num_layers)
    {
      fastuidraw::ivec3 dims(dimensions());
      dims.z() = new_num_layers;
      m_backing_store.resize(dims);
    }

  private:
    typedef Texture<GL_RGBA8, GL_RGBA, GL_NEAREST>::type TextureGL;
    TextureGL m_backing_store;
  };


  class IndexBackingStoreGL:public fastuidraw::AtlasIndexBackingStoreBase
  {
  public:
    IndexBackingStoreGL(int log2_tile_size,
                        int log2_num_index_tiles_per_row_per_col,
                        int num_layers,
                        bool delayed);

    ~IndexBackingStoreGL()
    {}

    virtual
    void
    set_data(int x, int y, int l,
             int w, int h,
             fastuidraw::const_c_array<fastuidraw::ivec3> data,
             int slack,
             const fastuidraw::AtlasColorBackingStoreBase *C,
             int pcolor_tile_size);

    virtual
    void
    set_data(int x, int y, int l,
             int w, int h,
             fastuidraw::const_c_array<fastuidraw::ivec3> data);

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

    static
    fastuidraw::ivec3
    store_size(int log2_tile_size,
               int log2_num_index_tiles_per_row_per_col,
               int num_layers);

    static
    fastuidraw::reference_counted_ptr<fastuidraw::AtlasIndexBackingStoreBase>
    create(int log2_tile_size,
           int log2_num_index_tiles_per_row_per_col,
           int num_layers, bool delayed)
    {
      IndexBackingStoreGL *p;
      p = FASTUIDRAWnew IndexBackingStoreGL(log2_tile_size,
                                           log2_num_index_tiles_per_row_per_col,
                                           num_layers, delayed);
      return fastuidraw::reference_counted_ptr<fastuidraw::AtlasIndexBackingStoreBase>(p);
    }

  protected:
    virtual
    void
    resize_implement(int new_num_layers)
    {
      fastuidraw::ivec3 dims(dimensions());
      dims.z() = new_num_layers;
      m_backing_store.resize(dims);
    }

  private:
    /*
      Data packed is:
        - .x    ===> which x-tile.
        - .y    ===> which y-tile.
        - .z/.w ===> which layer packed as layer = 2*z + w
      Note:
        ColorBackingStore must be no larger than 2^8 * color_tile_size
        For color_tile_size = 2^5, then value is 2^13 = 8192
     */
    typedef Texture<GL_RGBA8UI, GL_RGBA_INTEGER, GL_NEAREST>::type TextureGL;
    TextureGL m_backing_store;
  };

  class ImageAtlasGLParamsPrivate
  {
  public:
    ImageAtlasGLParamsPrivate(void):
      m_log2_color_tile_size(5),
      m_log2_num_color_tiles_per_row_per_col(8),
      m_num_color_layers(1),
      m_log2_index_tile_size(2),
      m_log2_num_index_tiles_per_row_per_col(6),
      m_num_index_layers(4),
      m_delayed(false)
    {}

    int m_log2_color_tile_size;
    int m_log2_num_color_tiles_per_row_per_col;
    int m_num_color_layers;
    int m_log2_index_tile_size;
    int m_log2_num_index_tiles_per_row_per_col;
    int m_num_index_layers;
    bool m_delayed;
  };

  class ImageAtlasGLPrivate
  {
  public:
    ImageAtlasGLPrivate(const fastuidraw::gl::ImageAtlasGL::params &P):
      m_params(P)
    {}

    fastuidraw::gl::ImageAtlasGL::params m_params;
  };

} //namespace




////////////////////////////////////////////
// ColorBackingStoreGL methods
ColorBackingStoreGL::
ColorBackingStoreGL(int log2_tile_size,
                    int log2_num_tiles_per_row_per_col,
                    int number_layers,
                    bool delayed):
  fastuidraw::AtlasColorBackingStoreBase(store_size(log2_tile_size, log2_num_tiles_per_row_per_col, number_layers),
                                         true),
  m_backing_store(dimensions(), delayed)
{}

void
ColorBackingStoreGL::
set_data(int x, int y, int l,
         int w, int h,
         fastuidraw::const_c_array<fastuidraw::u8vec4> pdata)
{
  TextureGL::EntryLocation V;
  fastuidraw::const_c_array<uint8_t> data;

  V.m_location.x() = x;
  V.m_location.y() = y;
  V.m_location.z() = l;
  V.m_size.x() = w;
  V.m_size.y() = h;
  V.m_size.z() = 1;
  data = pdata.reinterpret_pointer<uint8_t>();
  m_backing_store.set_data_c_array(V, data);
}

fastuidraw::ivec3
ColorBackingStoreGL::
store_size(int log2_tile_size, int log2_num_tiles_per_row_per_col, int num_layers)
{
  /* Because the index type is an 8-bit integer, log2_num_tiles_per_row_per_col
     must be clamped to 8.
   */
  log2_num_tiles_per_row_per_col = std::max(1, std::min(8, log2_num_tiles_per_row_per_col));
  int v(1 << (log2_num_tiles_per_row_per_col + log2_tile_size));
  return fastuidraw::ivec3(v, v, num_layers);
}

//////////////////////////////////////////
// IndexBackingStoreGL methods
IndexBackingStoreGL::
IndexBackingStoreGL(int log2_tile_size,
                    int log2_num_index_tiles_per_row_per_col,
                    int num_layers,
                    bool delayed):
  fastuidraw::AtlasIndexBackingStoreBase(store_size(log2_tile_size, log2_num_index_tiles_per_row_per_col, num_layers),
                                        true),
  m_backing_store(dimensions(), delayed)
{}

void
IndexBackingStoreGL::
set_data(int x, int y, int l,
         int w, int h,
         fastuidraw::const_c_array<fastuidraw::ivec3> data,
         int slack,
         const fastuidraw::AtlasColorBackingStoreBase *C,
         int pcolor_tile_size)
{
  (void)slack;
  (void)C;
  (void)pcolor_tile_size;
  set_data(x, y, l, w, h, data);
}

void
IndexBackingStoreGL::
set_data(int x, int y, int l,
         int w, int h,
         fastuidraw::const_c_array<fastuidraw::ivec3> data)
{
  TextureGL::EntryLocation V;
  std::vector<uint8_t> data_store(4*w*h);
  fastuidraw::c_array<uint8_t> ptr(fastuidraw::make_c_array(data_store));
  fastuidraw::c_array<fastuidraw::u8vec4> converted;
  converted = ptr.reinterpret_pointer<fastuidraw::u8vec4>();

  V.m_location.x() = x;
  V.m_location.y() = y;
  V.m_location.z() = l;
  V.m_size.x() = w;
  V.m_size.y() = h;
  V.m_size.z() = 1;
  for(int idx = 0, b = 0; b < h; ++b)
    {
      for(int a = 0; a < w; ++a, ++idx)
        {
          converted[idx].x() = data[idx].x();
          converted[idx].y() = data[idx].y();
          converted[idx].z() = ( data[idx].z() ) & 0xFF;
          converted[idx].w() = ( data[idx].z() ) >> 8;
        }
    }
  m_backing_store.set_data_vector(V, data_store);
}

fastuidraw::ivec3
IndexBackingStoreGL::
store_size(int log2_tile_size, int log2_num_index_tiles_per_row_per_col, int num_layers)
{
  /* Size is just 2^(log2_tile_size + log2_num_index_tiles_per_row_per_col),
     however, because index is an 8 bit integer, log2_num_index_tiles_per_row_per_col
     must be capped to 8
  */
  log2_num_index_tiles_per_row_per_col = std::min(8, std::max(1, log2_num_index_tiles_per_row_per_col)); 
  int v(1 << (log2_num_index_tiles_per_row_per_col + log2_tile_size));
  return fastuidraw::ivec3(v, v, num_layers);
}

//////////////////////////////////////////////
// fastuidraw::gl::ImageAtlasGL::params methods
fastuidraw::gl::ImageAtlasGL::params::
params(void)
{
  m_d = FASTUIDRAWnew ImageAtlasGLParamsPrivate();
}

fastuidraw::gl::ImageAtlasGL::params::
params(const params &obj)
{
  ImageAtlasGLParamsPrivate *d;
  d = reinterpret_cast<ImageAtlasGLParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ImageAtlasGLParamsPrivate(*d);
}

fastuidraw::gl::ImageAtlasGL::params::
~params()
{
  ImageAtlasGLParamsPrivate *d;
  d = reinterpret_cast<ImageAtlasGLParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::gl::ImageAtlasGL::params&
fastuidraw::gl::ImageAtlasGL::params::
operator=(const params &rhs)
{
  if(this != &rhs)
    {
      ImageAtlasGLParamsPrivate *d, *rhs_d;
      d = reinterpret_cast<ImageAtlasGLParamsPrivate*>(m_d);
      rhs_d = reinterpret_cast<ImageAtlasGLParamsPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

fastuidraw::gl::ImageAtlasGL::params&
fastuidraw::gl::ImageAtlasGL::params::
optimal_color_sizes(int log2_color_tile_size)
{
  int m, log2m, c;
  m = context_get<GLint>(GL_MAX_TEXTURE_SIZE);
  log2m = uint32_log2(m);
  c = std::min(8, std::max(1, log2m - log2_color_tile_size));
  return log2_num_color_tiles_per_row_per_col(c);
}

#define paramsSetGet(type, name)				 \
  fastuidraw::gl::ImageAtlasGL::params&                          \
  fastuidraw::gl::ImageAtlasGL::params::                         \
  name(type v)							 \
  {								 \
    ImageAtlasGLParamsPrivate *d;				 \
    d = reinterpret_cast<ImageAtlasGLParamsPrivate*>(m_d);	 \
    d->m_##name = v;						 \
    return *this;						 \
  }								 \
  								 \
  type								 \
  fastuidraw::gl::ImageAtlasGL::params::                         \
  name(void) const						 \
  {								 \
    ImageAtlasGLParamsPrivate *d;				 \
    d = reinterpret_cast<ImageAtlasGLParamsPrivate*>(m_d);	 \
    return d->m_##name;						 \
  }								 \


paramsSetGet(int, log2_color_tile_size)
paramsSetGet(int, log2_num_color_tiles_per_row_per_col)
paramsSetGet(int, num_color_layers)
paramsSetGet(int, log2_index_tile_size)
paramsSetGet(int, log2_num_index_tiles_per_row_per_col)
paramsSetGet(int, num_index_layers)
paramsSetGet(bool, delayed)

#undef paramsSetGet



//////////////////////////////////////////////
// fastuidraw::gl::ImageAtlasGL methods
fastuidraw::gl::ImageAtlasGL::
ImageAtlasGL(const params &P):
  fastuidraw::ImageAtlas(1 << P.log2_color_tile_size(), //color tile size
                        1 << P.log2_index_tile_size(), //index tile size
                        ColorBackingStoreGL::create(P.log2_color_tile_size(), P.log2_num_color_tiles_per_row_per_col(),
                                                    P.num_color_layers(), P.delayed()),
                        IndexBackingStoreGL::create(P.log2_index_tile_size(),
                                                    P.log2_num_index_tiles_per_row_per_col(),
                                                    P.num_index_layers(), P.delayed()))
{
  m_d = FASTUIDRAWnew ImageAtlasGLPrivate(P);
}

fastuidraw::gl::ImageAtlasGL::
~ImageAtlasGL()
{
  ImageAtlasGLPrivate *d;
  d = reinterpret_cast<ImageAtlasGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::gl::ImageAtlasGL::params&
fastuidraw::gl::ImageAtlasGL::
param_values(void) const
{
  ImageAtlasGLPrivate *d;
  d = reinterpret_cast<ImageAtlasGLPrivate*>(m_d);
  return d->m_params;
}

GLuint
fastuidraw::gl::ImageAtlasGL::
color_texture(void) const
{
  flush();
  const ColorBackingStoreGL *p;
  assert(dynamic_cast<const ColorBackingStoreGL*>(color_store().get()));
  p = static_cast<const ColorBackingStoreGL*>(color_store().get());
  return p->texture();
}

GLuint
fastuidraw::gl::ImageAtlasGL::
index_texture(void) const
{
  flush();
  const IndexBackingStoreGL *p;
  assert(dynamic_cast<const IndexBackingStoreGL*>(index_store().get()));
  p = static_cast<const IndexBackingStoreGL*>(index_store().get());
  return p->texture();
}

fastuidraw::gl::Shader::shader_source
fastuidraw::gl::ImageAtlasGL::
glsl_compute_coord_src(const char *function_name,
                       const char *index_texture) const
{
  Shader::shader_source return_value;

  return_value
    .add_macro("FASTUIDRAW_INDEX_TILE_SIZE", index_tile_size())
    .add_macro("FASTUIDRAW_COLOR_TILE_SIZE", color_tile_size())
    .add_macro("FASTUIDRAW_INDEX_ATLAS", index_texture)
    .add_macro("FASTUIDRAW_ATLAS_COMPUTE_COORD", function_name)
    .add_source("fastuidraw_atlas_image_fetch.glsl.resource_string", Shader::from_resource)
    .remove_macro("FASTUIDRAW_INDEX_TILE_SIZE")
    .remove_macro("FASTUIDRAW_COLOR_TILE_SIZE")
    .remove_macro("FASTUIDRAW_INDEX_ATLAS")
    .remove_macro("FASTUIDRAW_ATLAS_COMPUTE_COORD");

  return return_value;
}

fastuidraw::vecN<fastuidraw::vec2, 2>
fastuidraw::gl::ImageAtlasGL::
shader_coords(reference_counted_ptr<Image> image)
{
  ivec2 master_index_tile(image->master_index_tile());

  assert(image->number_index_lookups() > 0);

  vec2 wh(image->master_index_tile_dims());
  float f(image->atlas()->index_tile_size());
  vec2 fmaster_index_tile(master_index_tile);
  vec2 c0(f * fmaster_index_tile);
  return vecN<vec2, 2>(c0, c0 + wh);
}
