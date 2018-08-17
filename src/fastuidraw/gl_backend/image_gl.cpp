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
#include "private/bindless.hpp"
#include "../private/util_private.hpp"



namespace
{
  template<GLenum internal_format,
           GLenum external_format,
           GLenum mag_filter,
           GLenum min_filter>
  class Texture
  {
  public:
    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_2D_ARRAY,
                                              internal_format, external_format,
                                              GL_UNSIGNED_BYTE,
                                              mag_filter, min_filter> type;
  };

  class ColorBackingStoreGL:public fastuidraw::AtlasColorBackingStoreBase
  {
  public:
    ColorBackingStoreGL(int log2_tile_size, int log2_num_tiles_per_row_per_col, int number_layers, bool delayed);
    ~ColorBackingStoreGL() {}

    virtual
    void
    set_data(int mipmap_level, fastuidraw::ivec2 dst_xy, int dst_l, fastuidraw::ivec2 src_xy,
             unsigned int size, const fastuidraw::ImageSourceBase &data);
    virtual
    void
    set_data(int mipmap_level, fastuidraw::ivec2 dst_xy, int dst_l,
             unsigned int size, fastuidraw::u8vec4 color_value);

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
    typedef Texture<GL_RGBA8, GL_RGBA, GL_LINEAR, GL_NEAREST_MIPMAP_LINEAR>::type TextureGL;
    TextureGL m_backing_store;
    unsigned int m_number_mipmap_levels;
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
             fastuidraw::c_array<const fastuidraw::ivec3> data,
             int slack,
             const fastuidraw::AtlasColorBackingStoreBase *C,
             int pcolor_tile_size);

    virtual
    void
    set_data(int x, int y, int l,
             int w, int h,
             fastuidraw::c_array<const fastuidraw::ivec3> data);

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
     * Data packed is:
     *  - .x    ===> which x-tile.
     *  - .y    ===> which y-tile.
     *  - .z/.w ===> which layer packed as layer = 2*z + w
     * Note:
     *  ColorBackingStore must be no larger than 2^8 * color_tile_size
     *  For color_tile_size = 2^5, then value is 2^13 = 8192
     */
    typedef Texture<GL_RGBA8UI, GL_RGBA_INTEGER, GL_NEAREST, GL_NEAREST>::type TextureGL;
    TextureGL m_backing_store;
  };

  class TextureImagePrivate
  {
  public:
    TextureImagePrivate(GLuint tex, bool owns):
      m_texture(tex),
      m_owns_texture(owns)
    {}

    GLuint m_texture;
    bool m_owns_texture;
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
  m_backing_store(dimensions(), delayed, log2_tile_size),
  m_number_mipmap_levels(log2_tile_size)
{}

void
ColorBackingStoreGL::
set_data(int mipmap_level, fastuidraw::ivec2 dst_xy, int dst_l, fastuidraw::ivec2 src_xy,
         unsigned int size, const fastuidraw::ImageSourceBase &image_data)
{
  using namespace fastuidraw;

  if (mipmap_level >= m_backing_store.num_mipmaps())
    {
      return;
    }

  TextureGL::EntryLocation V;
  std::vector<u8vec4> data_storage(size * size);
  fastuidraw::c_array<u8vec4> data(make_c_array(data_storage));
  fastuidraw::c_array<const uint8_t> raw_data;

  raw_data = data.reinterpret_pointer<const uint8_t>();
  V.m_mipmap_level = mipmap_level;
  V.m_location.x() = dst_xy.x();
  V.m_location.y() = dst_xy.y();
  V.m_location.z() = dst_l;
  V.m_size.x() = size;
  V.m_size.y() = size;
  V.m_size.z() = 1;

  image_data.fetch_texels(V.m_mipmap_level, src_xy,
                          V.m_size.x(), V.m_size.y(), data);
  m_backing_store.set_data_c_array(V, raw_data);
}

void
ColorBackingStoreGL::
set_data(int mipmap_level, fastuidraw::ivec2 dst_xy, int dst_l,
         unsigned int size, fastuidraw::u8vec4 color_value)
{
  using namespace fastuidraw;

  if (mipmap_level >= m_backing_store.num_mipmaps())
    {
      return;
    }

  TextureGL::EntryLocation V;
  std::vector<u8vec4> data_storage(size * size, color_value);
  fastuidraw::c_array<u8vec4> data(make_c_array(data_storage));
  fastuidraw::c_array<const uint8_t> raw_data;

  raw_data = data.reinterpret_pointer<const uint8_t>();
  V.m_mipmap_level = mipmap_level;
  V.m_location.x() = dst_xy.x();
  V.m_location.y() = dst_xy.y();
  V.m_location.z() = dst_l;
  V.m_size.x() = size;
  V.m_size.y() = size;
  V.m_size.z() = 1;

  m_backing_store.set_data_c_array(V, raw_data);
}

fastuidraw::ivec3
ColorBackingStoreGL::
store_size(int log2_tile_size, int log2_num_tiles_per_row_per_col, int num_layers)
{
  /* Because the index type is an 8-bit integer, log2_num_tiles_per_row_per_col
   * must be clamped to 8.
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
         fastuidraw::c_array<const fastuidraw::ivec3> data,
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
         fastuidraw::c_array<const fastuidraw::ivec3> data)
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
   * however, because index is an 8 bit integer, log2_num_index_tiles_per_row_per_col
   * must be capped to 8
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
  d = static_cast<ImageAtlasGLParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ImageAtlasGLParamsPrivate(*d);
}

fastuidraw::gl::ImageAtlasGL::params::
~params()
{
  ImageAtlasGLParamsPrivate *d;
  d = static_cast<ImageAtlasGLParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
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

assign_swap_implement(fastuidraw::gl::ImageAtlasGL::params)
setget_implement(fastuidraw::gl::ImageAtlasGL::params,
                 ImageAtlasGLParamsPrivate,
                 int, log2_color_tile_size)
setget_implement(fastuidraw::gl::ImageAtlasGL::params,
                 ImageAtlasGLParamsPrivate,
                 int, log2_num_color_tiles_per_row_per_col)
setget_implement(fastuidraw::gl::ImageAtlasGL::params,
                 ImageAtlasGLParamsPrivate,
                 int, num_color_layers)
setget_implement(fastuidraw::gl::ImageAtlasGL::params,
                 ImageAtlasGLParamsPrivate,
                 int, log2_index_tile_size)
setget_implement(fastuidraw::gl::ImageAtlasGL::params,
                 ImageAtlasGLParamsPrivate,
                 int, log2_num_index_tiles_per_row_per_col)
setget_implement(fastuidraw::gl::ImageAtlasGL::params,
                 ImageAtlasGLParamsPrivate,
                 int, num_index_layers)
setget_implement(fastuidraw::gl::ImageAtlasGL::params,
                 ImageAtlasGLParamsPrivate,
                 bool, delayed)

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
  d = static_cast<ImageAtlasGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

const fastuidraw::gl::ImageAtlasGL::params&
fastuidraw::gl::ImageAtlasGL::
param_values(void) const
{
  ImageAtlasGLPrivate *d;
  d = static_cast<ImageAtlasGLPrivate*>(m_d);
  return d->m_params;
}

GLuint
fastuidraw::gl::ImageAtlasGL::
color_texture(void) const
{
  flush();
  const ColorBackingStoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const ColorBackingStoreGL*>(color_store().get()));
  p = static_cast<const ColorBackingStoreGL*>(color_store().get());
  return p->texture();
}

GLuint
fastuidraw::gl::ImageAtlasGL::
index_texture(void) const
{
  flush();
  const IndexBackingStoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const IndexBackingStoreGL*>(index_store().get()));
  p = static_cast<const IndexBackingStoreGL*>(index_store().get());
  return p->texture();
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL::TextureImage>
fastuidraw::gl::ImageAtlasGL::
create_bindless(int pw, int ph, unsigned int m, const ImageSourceBase &image,
                GLenum min_filter, GLenum mag_filter, GLuint *tex)
{
  reference_counted_ptr<TextureImage> p;
  if (pw > 0 && ph > 0 && m > 0 && !detail::bindless().not_supported())
    {
      p = TextureImage::create(pw, ph, m, image, min_filter, mag_filter, true);
    }

  if (tex)
    {
      *tex = (p) ? p->texture() : 0u;
    }

  return p;
}

//////////////////////////////////////////////////////
// fastuidraw::gl::ImageAtlasGL::TextureImage methods
fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL::TextureImage>
fastuidraw::gl::ImageAtlasGL::TextureImage::
create(int w, int h, unsigned int m, GLuint texture,
       bool object_owns_texture)
{
  if (w <= 0 || h <= 0 || m <= 0 || texture == 0)
    {
      return nullptr;
    }

  if (detail::bindless().not_supported())
    {
      return FASTUIDRAWnew TextureImage(w, h, m, object_owns_texture, texture);
    }
  else
    {
      GLuint64 handle;

      handle = detail::bindless().get_texture_handle(texture);
      detail::bindless().make_texture_handle_resident(handle);
      return FASTUIDRAWnew TextureImage(w, h, m, object_owns_texture, texture, handle);
    }
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL::TextureImage>
fastuidraw::gl::ImageAtlasGL::TextureImage::
create(int pw, int ph, unsigned int m, const ImageSourceBase &image,
       GLenum min_filter, GLenum mag_filter,
       bool object_owns_texture)
{
  if (pw <= 0 || ph <= 0 || m <= 0)
    {
      return nullptr;
    }

  GLuint texture;
  int w(pw), h(ph);
  std::vector<u8vec4> data_storage(w * h);
  c_array<u8vec4> data(make_c_array(data_storage));

  glGenTextures(1, &texture);
  FASTUIDRAWassert(texture != 0u);
  glBindTexture(GL_TEXTURE_2D, texture);

  detail::tex_storage<GL_TEXTURE_2D>(true, GL_RGBA8, ivec2(w, h), m);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

  m = t_min(m, image.num_mipmap_levels());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m - 1);
  for (unsigned int l = 0; l < m && w > 0 && h > 0; ++l, w /= 2, h /= 2)
    {
      image.fetch_texels(l, ivec2(0, 0), t_max(w, 1), t_max(h, 1), data);
      glTexSubImage2D(GL_TEXTURE_2D, l, 0, 0,
                      t_max(w, 1), t_max(h, 1),
                      GL_RGBA, GL_UNSIGNED_BYTE,
                      data.c_ptr());
    }
  glBindTexture(GL_TEXTURE_2D, 0);
  return create(pw, ph, m, texture, object_owns_texture);
}

fastuidraw::gl::ImageAtlasGL::TextureImage::
TextureImage(int w, int h, unsigned int m,
             bool object_owns_texture, GLuint texture):
  Image(w, h, m, fastuidraw::Image::context_texture2d, -1)
{
  m_d = FASTUIDRAWnew TextureImagePrivate(texture, object_owns_texture);
}

fastuidraw::gl::ImageAtlasGL::TextureImage::
TextureImage(int w, int h, unsigned int m,
             bool object_owns_texture, GLuint texture, GLuint64 handle):
  Image(w, h, m, fastuidraw::Image::bindless_texture2d, handle)
{
  m_d = FASTUIDRAWnew TextureImagePrivate(texture, object_owns_texture);
}

fastuidraw::gl::ImageAtlasGL::TextureImage::
~TextureImage()
{
  TextureImagePrivate *d;
  d = static_cast<TextureImagePrivate*>(m_d);

  if (type() == bindless_texture2d)
    {
      detail::bindless().make_texture_handle_non_resident(bindless_handle());
    }

  if (d->m_owns_texture)
    {
      glDeleteTextures(1, &d->m_texture);
    }

  FASTUIDRAWdelete(d);
}

GLuint
fastuidraw::gl::ImageAtlasGL::TextureImage::
texture(void) const
{
  TextureImagePrivate *d;
  d = static_cast<TextureImagePrivate*>(m_d);
  return d->m_texture;
}
