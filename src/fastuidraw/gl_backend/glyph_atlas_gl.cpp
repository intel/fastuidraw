/*!
 * \file glyph_atlas_gl.cpp
 * \brief file glyph_atlas_gl.cpp
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

#include <fastuidraw/glsl/shader_code.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/glyph_atlas_gl.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>

#include "private/texture_gl.hpp"
#include "private/buffer_object_gl.hpp"
#include "private/tex_buffer.hpp"
#include "private/texture_view.hpp"
#include "../private/util_private.hpp"

namespace
{
  class TexelStoreGL:public fastuidraw::GlyphAtlasTexelBackingStoreBase
  {
  public:
    TexelStoreGL(fastuidraw::ivec3 dims, bool delayed);

    ~TexelStoreGL(void);

    void
    set_data(int x, int y, int l, int w, int h,
             fastuidraw::c_array<const uint8_t> data);

    void
    flush(void)
    {
      m_backing_store.flush();
    }

    GLuint
    texture(bool as_integer) const;

    static
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasTexelBackingStoreBase>
    create(fastuidraw::ivec3 dims, bool delayed);

  protected:

    virtual
    void
    resize_implement(int new_num_layers);


  private:
    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_2D_ARRAY,
                                              GL_R8UI, GL_RED_INTEGER,
                                              GL_UNSIGNED_BYTE,
                                              GL_NEAREST, GL_NEAREST> TextureGL;
    TextureGL m_backing_store;
    mutable GLuint m_texture_as_r8;
  };

  class GeometryStoreGL:public fastuidraw::GlyphAtlasGeometryBackingStoreBase
  {
  public:
    GeometryStoreGL(unsigned int number_vecNs, unsigned int N,
                    GLenum pbinding_point, const fastuidraw::ivec2 &log2_dims,
                    bool backed_by_texture):
      fastuidraw::GlyphAtlasGeometryBackingStoreBase(N, number_vecNs, true),
      m_binding_point(pbinding_point),
      m_log2_dims(log2_dims),
      m_backed_by_texture(backed_by_texture)
    {
      FASTUIDRAWassert(N <= 4 && N > 0);
    }

    virtual
    GLuint
    gl_backing(void) const = 0;

    static
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasGeometryBackingStoreBase>
    create(const fastuidraw::gl::GlyphAtlasGL::params &P);

    GLenum m_binding_point;
    fastuidraw::ivec2 m_log2_dims;
    bool m_backed_by_texture;
  };

  class GeometryStoreGL_StorageBuffer:public GeometryStoreGL
  {
  public:
    explicit
    GeometryStoreGL_StorageBuffer(unsigned int number_vecNs, bool delayed);

    virtual
    void
    set_values(unsigned int location,
               fastuidraw::c_array<const fastuidraw::generic_data> pdata);

    virtual
    void
    flush(void);

    virtual
    GLuint
    gl_backing(void) const;

  protected:

    virtual
    void
    resize_implement(unsigned int new_size);

  private:

    typedef fastuidraw::gl::detail::BufferGL<GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW> BufferGL;
    BufferGL m_backing_store;
  };

  class GeometryStoreGL_TextureBuffer:public GeometryStoreGL
  {
  public:
    explicit
    GeometryStoreGL_TextureBuffer(unsigned int number_vecNs, bool delayed, unsigned int N);

    virtual
    void
    set_values(unsigned int location,
               fastuidraw::c_array<const fastuidraw::generic_data> pdata);

    virtual
    void
    flush(void);

    virtual
    GLuint
    gl_backing(void) const;

  protected:

    virtual
    void
    resize_implement(unsigned int new_size);

  private:

    typedef fastuidraw::gl::detail::BufferGL<GL_TEXTURE_BUFFER, GL_STATIC_DRAW> BufferGL;
    BufferGL m_backing_store;
    mutable GLuint m_texture;
    mutable bool m_tbo_dirty;
  };

  class GeometryStoreGL_Texture:public GeometryStoreGL
  {
  public:
    explicit
    GeometryStoreGL_Texture(fastuidraw::ivec2 log2_wh, unsigned int number_vecNs, bool delayed, unsigned int N);

    virtual
    void
    set_values(unsigned int location,
               fastuidraw::c_array<const fastuidraw::generic_data> pdata);

    virtual
    void
    flush(void);

    virtual
    GLuint
    gl_backing(void) const;

  protected:

    virtual
    void
    resize_implement(unsigned int new_size);

  private:

    static
    GLenum
    internal_format(unsigned int N);

    static
    GLenum
    external_format(unsigned int N);

    static
    fastuidraw::ivec3
    texture_size(fastuidraw::uvec2 wh, unsigned int number_texels);

    typedef fastuidraw::gl::detail::TextureGLGeneric<GL_TEXTURE_2D_ARRAY> TextureGL;
    fastuidraw::uvec2 m_layer_dims;
    int m_texels_per_layer;
    TextureGL m_backing_store;
  };

  class GlyphAtlasGLParamsPrivate
  {
  public:
    GlyphAtlasGLParamsPrivate(void):
      m_texel_store_dimensions(1024, 1024, 16),
      m_number_floats(1024 * 1024),
      m_delayed(false),
      m_alignment(4),
      m_type(fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_tbo),
      m_log2_dims_geometry_store(-1, -1)
    {}

    fastuidraw::ivec3 m_texel_store_dimensions;
    unsigned int m_number_floats;
    bool m_delayed;
    unsigned int m_alignment;
    enum fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_backing_t m_type;
    fastuidraw::ivec2 m_log2_dims_geometry_store;
  };

  class GlyphAtlasGLPrivate
  {
  public:
    explicit
    GlyphAtlasGLPrivate(const fastuidraw::gl::GlyphAtlasGL::params &P):
      m_params(P)
    {}

    fastuidraw::gl::GlyphAtlasGL::params m_params;
  };
}



/////////////////////////////////////////
// TexelStoreGL methods
TexelStoreGL::
TexelStoreGL(fastuidraw::ivec3 dims, bool delayed):
  fastuidraw::GlyphAtlasTexelBackingStoreBase(dims, true),
  m_backing_store(dims, delayed),
  m_texture_as_r8(0)
{
  /* clear the right and bottom border
   * of the texture
   */
  std::vector<uint8_t> right(dims.y(), 0), bottom(dims.x(), 0);
  for(int layer = 0; layer < dims.z(); ++layer)
    {
      set_data(dims.x() - 1, 0, layer, //top right
               1, dims.y(), //entire height
               fastuidraw::make_c_array(right));

      set_data(0, dims.y() - 1, layer, //bottom left
               dims.x() - 1, 1, //entire width
               fastuidraw::make_c_array(bottom));
    }
}

TexelStoreGL::
~TexelStoreGL(void)
{
  if (m_texture_as_r8 != 0)
    {
      glDeleteTextures(1, &m_texture_as_r8);
    }
}

void
TexelStoreGL::
resize_implement(int new_num_layers)
{
  fastuidraw::ivec3 dims(dimensions());
  int old_num_layers(dims.z());

  if (m_texture_as_r8 != 0)
    {
      /* a resize generates a new texture which then has
       * a new backing store. The texture view would then
       * be using the old backing store which is useless.
       * We delete the old texture view and let texture()
       * recreate the view on demand.
       */
      glDeleteTextures(1, &m_texture_as_r8);
      m_texture_as_r8 = 0;
    }

  dims.z() = new_num_layers;
  m_backing_store.resize(dims);

  std::vector<uint8_t> right(dims.y(), 0), bottom(dims.x(), 0);
  for(int layer = old_num_layers; layer < new_num_layers; ++layer)
    {
      set_data(dims.x() - 1, 0, layer, //top right
               1, dims.y(), //entire height
               fastuidraw::make_c_array(right));

      set_data(0, dims.y() - 1, layer, //bottom left
               dims.x() - 1, 1, //entire width
               fastuidraw::make_c_array(bottom));
    }
}

void
TexelStoreGL::
set_data(int x, int y, int l, int w, int h,
         fastuidraw::c_array<const uint8_t> data)
{
  TextureGL::EntryLocation V;

  V.m_location.x() = x;
  V.m_location.y() = y;
  V.m_location.z() = l;
  V.m_size.x() = w;
  V.m_size.y() = h;
  V.m_size.z() = 1;
  m_backing_store.set_data_c_array(V, data);
}

GLuint
TexelStoreGL::
texture(bool as_integer) const
{
  GLuint tex_as_r8ui;
  tex_as_r8ui = m_backing_store.texture();

  FASTUIDRAWassert(tex_as_r8ui != 0);

  if (as_integer)
    {
      return tex_as_r8ui;
    }

  if (m_texture_as_r8 == 0)
    {
      enum fastuidraw::gl::detail::texture_view_support_t md;

      md = fastuidraw::gl::detail::compute_texture_view_support();
      if (md != fastuidraw::gl::detail::texture_view_not_supported)
        {
          glGenTextures(1, &m_texture_as_r8);
          FASTUIDRAWassert(m_texture_as_r8 != 0);

          fastuidraw::gl::detail::texture_view(md,
                                              m_texture_as_r8, //texture to become view
                                              GL_TEXTURE_2D_ARRAY, //texture target for m_texture_as_r8
                                              tex_as_r8ui, //source of backing store for m_texture_as_r8
                                              GL_R8, //internal format for m_texture_as_r8
                                              0, //mipmap level start
                                              1, //number of mips to take
                                              0, //min layer
                                              dimensions().z()); //number layers to take

          glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture_as_r8);
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

          /* we do GL_REPEAT because glyphs are stored with padding (always to
           * right/bottom). This way filtering works.
           */
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_REPEAT);
        }
    }
  return m_texture_as_r8;
}

fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasTexelBackingStoreBase>
TexelStoreGL::
create(fastuidraw::ivec3 dims, bool delayed)
{
  TexelStoreGL *p;
  p = FASTUIDRAWnew TexelStoreGL(dims, delayed);
  return fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasTexelBackingStoreBase>(p);
}

///////////////////////////////////////////////
// GeometryStoreGL_Texture methods
GeometryStoreGL_Texture::
GeometryStoreGL_Texture(fastuidraw::ivec2 log2_wh, unsigned int number_texels, bool delayed, unsigned int N):
  GeometryStoreGL(number_texels, N, GL_TEXTURE_2D_ARRAY, log2_wh, true),
  m_layer_dims(1 << log2_wh.x(), 1 << log2_wh.y()),
  m_texels_per_layer(m_layer_dims.x() * m_layer_dims.y()),
  m_backing_store(internal_format(N), external_format(N), GL_FLOAT,
                  GL_NEAREST, GL_NEAREST,
                  texture_size(m_layer_dims, number_texels), delayed)
{
  FASTUIDRAWassert(N <= 4 && N > 0);
}

fastuidraw::ivec3
GeometryStoreGL_Texture::
texture_size(fastuidraw::uvec2 wh, unsigned int number_texels)
{
  unsigned int A, L;

  A = wh.x() * wh.y();
  L = number_texels / A;
  if (number_texels % A != 0)
    {
      ++L;
    }
  return fastuidraw::ivec3(wh.x(), wh.y(), L);
}

GLenum
GeometryStoreGL_Texture::
external_format(unsigned int N)
{
  FASTUIDRAWassert(1 <= N && N <= 4);
  const GLenum fmts[4] =
    {
      GL_RED,
      GL_RG,
      GL_RGB,
      GL_RGBA
    };
  return fmts[N - 1];
}

GLenum
GeometryStoreGL_Texture::
internal_format(unsigned int N)
{
  FASTUIDRAWassert(1 <= N && N <= 4);
  const GLenum fmts[4] =
    {
      GL_R32F,
      GL_RG32F,
      GL_RGB32F,
      GL_RGBA32F
    };
  return fmts[N - 1];
}

void
GeometryStoreGL_Texture::
resize_implement(unsigned int new_size)
{
  m_backing_store.resize(texture_size(m_layer_dims, new_size));
}

void
GeometryStoreGL_Texture::
flush(void)
{
  m_backing_store.flush();
}

GLuint
GeometryStoreGL_Texture::
gl_backing(void) const
{
  return m_backing_store.texture();
}

void
GeometryStoreGL_Texture::
set_values(unsigned int location,
           fastuidraw::c_array<const fastuidraw::generic_data> pdata)
{
  FASTUIDRAWassert(pdata.size() % alignment() == 0);
  unsigned int num_texels;
  fastuidraw::uvec3 p;

  num_texels = pdata.size() / alignment();
  p.x() = fastuidraw::unpack_bits(0, m_log2_dims.x(), location);
  p.y() = fastuidraw::unpack_bits(m_log2_dims.x(), m_log2_dims.y(), location);
  p.z() = location >> (m_log2_dims.x() + m_log2_dims.y());

  while(num_texels > 0)
    {
      unsigned int num_take;
      fastuidraw::c_array<const uint8_t> take_data;
      TextureGL::EntryLocation loc;

      num_take = std::min(m_layer_dims.x() - p.x(), num_texels);
      take_data = pdata.sub_array(0, num_take * alignment()).reinterpret_pointer<const uint8_t>();

      loc.m_location.x() = p.x();
      loc.m_location.y() = p.y();
      loc.m_location.z() = p.z();
      loc.m_size.x() = num_take;
      loc.m_size.y() = 1;
      loc.m_size.z() = 1;

      m_backing_store.set_data_c_array(loc, take_data);

      FASTUIDRAWassert(num_texels >= num_take);
      num_texels -= num_take;

      pdata = pdata.sub_array(num_take * alignment());
      FASTUIDRAWassert(num_texels  * alignment() == pdata.size());

      p.x() += num_take;
      FASTUIDRAWassert(p.x() <= m_layer_dims.x());
      if (p.x() == m_layer_dims.x())
        {
          p.x() = 0;
          ++p.y();
          FASTUIDRAWassert(p.y() <= m_layer_dims.y());
          if (p.y() == m_layer_dims.y())
            {
              p.y() = 0;
              ++p.z();
            }
        }
    }
}

///////////////////////////////////////////////
// GeometryStoreGL_TextureBuffer methods
GeometryStoreGL_TextureBuffer::
GeometryStoreGL_TextureBuffer(unsigned int number_vecNs, bool delayed, unsigned int N):
  GeometryStoreGL(number_vecNs, N, GL_TEXTURE_BUFFER,
                  fastuidraw::ivec2(-1, -1), true),
  m_backing_store(number_vecNs * N * sizeof(float), delayed),
  m_texture(0),
  m_tbo_dirty(true)
{
  FASTUIDRAWassert(N <= 4 && N > 0);
}

void
GeometryStoreGL_TextureBuffer::
set_values(unsigned int location,
           fastuidraw::c_array<const fastuidraw::generic_data> pdata)
{
  FASTUIDRAWassert(pdata.size() % alignment() == 0);
  m_backing_store.set_data(location * alignment() * sizeof(float),
                           pdata.reinterpret_pointer<const uint8_t>());
}

void
GeometryStoreGL_TextureBuffer::
flush(void)
{
  m_backing_store.flush();
}

void
GeometryStoreGL_TextureBuffer::
resize_implement(unsigned int new_size)
{
  m_backing_store.resize(new_size * alignment() * sizeof(float));
  m_tbo_dirty = true;
}

GLuint
GeometryStoreGL_TextureBuffer::
gl_backing(void) const
{
  if (m_texture == 0)
    {
      glGenTextures(1, &m_texture);
      FASTUIDRAWassert(m_texture != 0);
    }

  if (m_tbo_dirty)
    {
      GLenum formats[4]=
        {
          GL_R32F,
          GL_RG32F,
          GL_RGB32F,
          GL_RGBA32F,
        };
      GLuint bo;

      bo = m_backing_store.buffer();
      FASTUIDRAWassert(bo != 0);
      glBindTexture(GL_TEXTURE_BUFFER, m_texture);
      fastuidraw::gl::detail::tex_buffer(fastuidraw::gl::detail::compute_tex_buffer_support(),
                                         GL_TEXTURE_BUFFER, formats[alignment() - 1], bo);
      m_tbo_dirty = false;
    }

  return m_texture;
}

///////////////////////////////////////////////
// GeometryStoreGL_StorageBuffer methods
GeometryStoreGL_StorageBuffer::
GeometryStoreGL_StorageBuffer(unsigned int number_vecNs, bool delayed):
  GeometryStoreGL(number_vecNs, 4, GL_SHADER_STORAGE_BUFFER,
                  fastuidraw::ivec2(-1, -1), false),
  m_backing_store(number_vecNs * 4 * sizeof(float), delayed)
{
}

void
GeometryStoreGL_StorageBuffer::
set_values(unsigned int location,
           fastuidraw::c_array<const fastuidraw::generic_data> pdata)
{
  FASTUIDRAWassert(alignment() == 4);
  FASTUIDRAWassert(pdata.size() % alignment() == 0);
  m_backing_store.set_data(location * 4 * sizeof(float),
                           pdata.reinterpret_pointer<const uint8_t>());
}

void
GeometryStoreGL_StorageBuffer::
flush(void)
{
  m_backing_store.flush();
}

void
GeometryStoreGL_StorageBuffer::
resize_implement(unsigned int new_size)
{
  m_backing_store.resize(new_size * alignment() * sizeof(float));
}

GLuint
GeometryStoreGL_StorageBuffer::
gl_backing(void) const
{
  return m_backing_store.buffer();
}

////////////////////////////////////////
// GeometryStoreGL methods
fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasGeometryBackingStoreBase>
GeometryStoreGL::
create(const fastuidraw::gl::GlyphAtlasGL::params &P)
{
  unsigned int number_vecNs, N;
  GeometryStoreGL *p(nullptr);
  bool delayed;

  N = P.alignment();
  number_vecNs = P.number_floats() / N;
  delayed = P.delayed();

  if (number_vecNs * N < P.number_floats())
    {
      ++number_vecNs;
    }

  switch(P.glyph_geometry_backing_store_type())
    {
    case fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_tbo:
      p = FASTUIDRAWnew GeometryStoreGL_TextureBuffer(number_vecNs, delayed, N);
      break;

    case fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_ssbo:
      p = FASTUIDRAWnew GeometryStoreGL_StorageBuffer(number_vecNs, delayed);
      break;

    case fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_texture_array:
      p = FASTUIDRAWnew GeometryStoreGL_Texture(P.texture_2d_array_geometry_store_log2_dims(),
                                                number_vecNs, delayed, N);
      break;

    default:
      FASTUIDRAWassert(!"Bad glyph geometry store backing type");
    }
  return fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasGeometryBackingStoreBase>(p);
}

//////////////////////////////////////////////////////
// fastuidraw::gl::GlyphAtlasGL::params methods
fastuidraw::gl::GlyphAtlasGL::params::
params(void)
{
  m_d = FASTUIDRAWnew GlyphAtlasGLParamsPrivate();
}

fastuidraw::gl::GlyphAtlasGL::params::
params(const params &obj)
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew GlyphAtlasGLParamsPrivate(*d);
}

fastuidraw::gl::GlyphAtlasGL::params::
~params()
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::gl::GlyphAtlasGL::params);

enum fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_backing_t
fastuidraw::gl::GlyphAtlasGL::params::
glyph_geometry_backing_store_type(void) const
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  return d->m_type;
}

fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::params::
use_texture_buffer_geometry_store(void)
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  d->m_type = glsl::PainterBackendGLSL::glyph_geometry_tbo;
  d->m_log2_dims_geometry_store = ivec2(-1, -1);
  return *this;
}

fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::params::
use_storage_buffer_geometry_store(void)
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  d->m_type = glsl::PainterBackendGLSL::glyph_geometry_ssbo;
  d->m_log2_dims_geometry_store = ivec2(-1, -1);
  d->m_alignment = 4;
  return *this;
}

fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::params::
use_texture_2d_array_geometry_store(int log2_width, int log2_height)
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  if (log2_width >= 0 && log2_height >= 0)
    {
      d->m_log2_dims_geometry_store = ivec2(log2_width, log2_height);
      d->m_type = glsl::PainterBackendGLSL::glyph_geometry_texture_array;
    }
  return *this;
}

fastuidraw::ivec2
fastuidraw::gl::GlyphAtlasGL::params::
texture_2d_array_geometry_store_log2_dims(void) const
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  return d->m_log2_dims_geometry_store;
}

unsigned int
fastuidraw::gl::GlyphAtlasGL::params::
alignment(void) const
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  return (d->m_type == glsl::PainterBackendGLSL::glyph_geometry_ssbo) ?
    4:
    d->m_alignment;
}

fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::params::
use_optimal_geometry_store_backing(void)
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);

  const int32_t required_max_size(1u << 26u);

  if (detail::compute_tex_buffer_support() != detail::tex_buffer_not_supported
     && context_get<int>(GL_MAX_TEXTURE_BUFFER_SIZE) >= required_max_size)
    {
      d->m_type = glsl::PainterBackendGLSL::glyph_geometry_tbo;
      d->m_log2_dims_geometry_store = ivec2(-1, -1);
    }
  else if (context_get<int>(GL_MAX_SHADER_STORAGE_BLOCK_SIZE) >= required_max_size)
    {
      d->m_type = glsl::PainterBackendGLSL::glyph_geometry_ssbo;
      d->m_log2_dims_geometry_store = ivec2(-1, -1);
    }
  else
    {
      uint32_t max_layers(0), max_wh(0), width;
      uint32_t required_area_per_layer, required_height;
      max_layers = context_get<int>(GL_MAX_ARRAY_TEXTURE_LAYERS);
      max_wh = context_get<int>(GL_MAX_TEXTURE_SIZE);

      /* Our selection of size is as follows:
       *  First maximize width (to a power of 2)
       *  Second maximize depth
       *  Last resort, increase height
       * so that we can store required_size texels.
       */
      width = 1u << uint32_log2(max_wh);
      required_area_per_layer = required_max_size / max_layers;
      required_height = std::min(max_wh, required_area_per_layer / width);
      if (required_height * width < required_area_per_layer)
        {
          ++required_height;
        }

      d->m_type = glsl::PainterBackendGLSL::glyph_geometry_texture_array;
      d->m_log2_dims_geometry_store.x() = uint32_log2(width);
      d->m_log2_dims_geometry_store.y() = (required_height <= 1) ?
        0 : uint32_log2(required_height);
    }
  return *this;
}

setget_implement(fastuidraw::gl::GlyphAtlasGL::params,
                 GlyphAtlasGLParamsPrivate,
                 fastuidraw::ivec3, texel_store_dimensions);
setget_implement(fastuidraw::gl::GlyphAtlasGL::params,
                 GlyphAtlasGLParamsPrivate,
                 unsigned int, number_floats);
setget_implement(fastuidraw::gl::GlyphAtlasGL::params,
                 GlyphAtlasGLParamsPrivate,
                 bool, delayed);
set_implement(fastuidraw::gl::GlyphAtlasGL::params,
              GlyphAtlasGLParamsPrivate,
              unsigned int, alignment);

//////////////////////////////////////////////////////////////////
// fastuidraw::gl::GlyphAtlasGL methods
fastuidraw::gl::GlyphAtlasGL::
GlyphAtlasGL(const params &P):
  GlyphAtlas(TexelStoreGL::create(P.texel_store_dimensions(), P.delayed()),
             GeometryStoreGL::create(P))
{
  m_d = FASTUIDRAWnew GlyphAtlasGLPrivate(P);
}

fastuidraw::gl::GlyphAtlasGL::
~GlyphAtlasGL()
{
  GlyphAtlasGLPrivate *d;
  d = static_cast<GlyphAtlasGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

const fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::
param_values(void) const
{
  GlyphAtlasGLPrivate *d;
  d = static_cast<GlyphAtlasGLPrivate*>(m_d);
  return d->m_params;
}

GLuint
fastuidraw::gl::GlyphAtlasGL::
texel_texture(bool as_integer) const
{
  flush();
  const TexelStoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const TexelStoreGL*>(texel_store().get()));
  p = static_cast<const TexelStoreGL*>(texel_store().get());
  return p->texture(as_integer);
}

GLenum
fastuidraw::gl::GlyphAtlasGL::
geometry_binding_point(void) const
{
  const GeometryStoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const GeometryStoreGL*>(geometry_store().get()));
  p = static_cast<const GeometryStoreGL*>(geometry_store().get());
  return p->m_binding_point;
}

GLuint
fastuidraw::gl::GlyphAtlasGL::
geometry_backing(void) const
{
  flush();
  const GeometryStoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const GeometryStoreGL*>(geometry_store().get()));
  p = static_cast<const GeometryStoreGL*>(geometry_store().get());
  return p->gl_backing();
}

bool
fastuidraw::gl::GlyphAtlasGL::
geometry_backed_by_texture(void) const
{
  const GeometryStoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const GeometryStoreGL*>(geometry_store().get()));
  p = static_cast<const GeometryStoreGL*>(geometry_store().get());
  return p->m_backed_by_texture;
}

fastuidraw::ivec2
fastuidraw::gl::GlyphAtlasGL::
geometry_texture_as_2d_array_log2_dims(void) const
{
  const GeometryStoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const GeometryStoreGL*>(geometry_store().get()));
  p = static_cast<const GeometryStoreGL*>(geometry_store().get());
  return p->m_log2_dims;
}
