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
#include <private/util_private.hpp>

#include <private/gl_backend/texture_gl.hpp>
#include <private/gl_backend/bindless.hpp>
#include <private/gl_backend/image_gl.hpp>

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

  unsigned int
  compute_color_tile_size(const fastuidraw::gl::PainterEngineGL::ImageAtlasParams &P)
  {
    bool no_atlas;

    no_atlas = P.log2_color_tile_size() < 0
      || P.log2_index_tile_size() < 0
      || P.log2_num_color_tiles_per_row_per_col() < 0;

    return no_atlas ? 0 : (1 << P.log2_color_tile_size());
  }

  unsigned int
  compute_index_tile_size(const fastuidraw::gl::PainterEngineGL::ImageAtlasParams &P)
  {
    bool no_atlas;

    no_atlas = P.log2_color_tile_size() < 0
      || P.log2_index_tile_size() < 0
      || P.log2_num_color_tiles_per_row_per_col() < 0;

    return no_atlas ? 0 : (1 << P.log2_index_tile_size());
  }

  class ColorBackingStoreGL:public fastuidraw::AtlasColorBackingStoreBase
  {
  public:
    ColorBackingStoreGL(int log2_tile_size, int log2_num_tiles_per_row_per_col, int number_layers);
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
    create(int log2_tile_size, int log2_num_tiles_per_row_per_col, int num_layers)
    {
      if (log2_tile_size < 0 || log2_num_tiles_per_row_per_col < 0)
        {
          return nullptr;
        }

      ColorBackingStoreGL *p;
      p = FASTUIDRAWnew ColorBackingStoreGL(log2_tile_size, log2_num_tiles_per_row_per_col, num_layers);
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
    typedef Texture<GL_RGBA8, GL_RGBA, GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST>::type TextureGL;
    TextureGL m_backing_store;
  };

  class IndexBackingStoreGL:public fastuidraw::AtlasIndexBackingStoreBase
  {
  public:
    IndexBackingStoreGL(int log2_tile_size,
                        int log2_num_index_tiles_per_row_per_col,
                        int num_layers);

    ~IndexBackingStoreGL()
    {}

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
           int num_layers)
    {
      if (log2_tile_size < 0 || log2_num_index_tiles_per_row_per_col < 0)
        {
          return nullptr;
        }

      IndexBackingStoreGL *p;
      p = FASTUIDRAWnew IndexBackingStoreGL(log2_tile_size,
                                           log2_num_index_tiles_per_row_per_col,
                                           num_layers);
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

} //namespace

////////////////////////////////////////////
// ColorBackingStoreGL methods
ColorBackingStoreGL::
ColorBackingStoreGL(int log2_tile_size,
                    int log2_num_tiles_per_row_per_col,
                    int number_layers):
  fastuidraw::AtlasColorBackingStoreBase(store_size(log2_tile_size, log2_num_tiles_per_row_per_col, number_layers),
                                         true),
  m_backing_store(dimensions(), true, log2_tile_size)
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
                    int num_layers):
  fastuidraw::AtlasIndexBackingStoreBase(store_size(log2_tile_size,
                                                    log2_num_index_tiles_per_row_per_col,
                                                    num_layers),
                                         true),
  m_backing_store(dimensions(), true)
{}

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
// fastuidraw::gl::detail::ImageAtlasGL methods
fastuidraw::gl::detail::ImageAtlasGL::
ImageAtlasGL(const PainterEngineGL::ImageAtlasParams &P):
  fastuidraw::ImageAtlas(compute_color_tile_size(P),
                         compute_index_tile_size(P),
                         ColorBackingStoreGL::create(P.log2_color_tile_size(),
                                                     P.log2_num_color_tiles_per_row_per_col(),
                                                     P.num_color_layers()),
                         IndexBackingStoreGL::create(P.log2_index_tile_size(),
                                                     P.log2_num_index_tiles_per_row_per_col(),
                                                     P.num_index_layers()))
{
}

fastuidraw::gl::detail::ImageAtlasGL::
~ImageAtlasGL()
{
}

GLuint
fastuidraw::gl::detail::ImageAtlasGL::
color_texture(void) const
{
  flush();
  const ColorBackingStoreGL *p;
  FASTUIDRAWassert(!color_store() || dynamic_cast<const ColorBackingStoreGL*>(color_store().get()));
  p = static_cast<const ColorBackingStoreGL*>(color_store().get());
  return (p) ? p->texture() : 0u;
}

GLuint
fastuidraw::gl::detail::ImageAtlasGL::
index_texture(void) const
{
  flush();
  const IndexBackingStoreGL *p;
  FASTUIDRAWassert(!index_store() || dynamic_cast<const IndexBackingStoreGL*>(index_store().get()));
  p = static_cast<const IndexBackingStoreGL*>(index_store().get());
  return (p) ? p->texture() : 0u;
}

fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::gl::detail::ImageAtlasGL::
create_image_bindless(int w, int h, const ImageSourceBase &image_data)
{
  if (detail::bindless().not_supported())
    {
      return nullptr;
    }

  reference_counted_ptr<TextureImage> return_value;

  return_value = TextureImage::create(this, w, h, image_data,
                                      GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST);
  FASTUIDRAWassert(return_value->type() == Image::bindless_texture2d);

  return return_value;
}

fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::gl::detail::ImageAtlasGL::
create_image_context_texture2d(int w, int h, const ImageSourceBase &image_data)
{
  reference_counted_ptr<TextureImage> return_value;

  return_value = TextureImage::create(this, w, h, image_data,
                                      GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST,
                                      false);
  FASTUIDRAWassert(return_value->type() == Image::context_texture2d);

  return return_value;
}
