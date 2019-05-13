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

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>

#include <private/util_private.hpp>
#include <private/gl_backend/texture_gl.hpp>
#include <private/gl_backend/buffer_object_gl.hpp>
#include <private/gl_backend/tex_buffer.hpp>
#include <private/gl_backend/texture_view.hpp>
#include <private/gl_backend/glyph_atlas_gl.hpp>

namespace
{
  class StoreGL:public fastuidraw::GlyphAtlasBackingStoreBase
  {
  public:
    StoreGL(unsigned int number,
            GLenum pbinding_point, const fastuidraw::ivec2 &log2_dims,
            bool binding_point_is_texture_unit):
      fastuidraw::GlyphAtlasBackingStoreBase(number),
      m_binding_point(pbinding_point),
      m_log2_dims(log2_dims),
      m_binding_point_is_texture_unit(binding_point_is_texture_unit)
    {
    }

    virtual
    GLuint
    gl_backing(enum fastuidraw::gl::detail::GlyphAtlasGL::backing_fmt_t fmt) const = 0;

    static
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasBackingStoreBase>
    create(const fastuidraw::gl::PainterEngineGL::GlyphAtlasParams &P);

    GLenum m_binding_point;
    fastuidraw::ivec2 m_log2_dims;
    bool m_binding_point_is_texture_unit;
  };

  class StoreGL_StorageBuffer:public StoreGL
  {
  public:
    explicit
    StoreGL_StorageBuffer(unsigned int number);

    virtual
    void
    set_values(unsigned int location,
               fastuidraw::c_array<const uint32_t> pdata);

    virtual
    void
    flush(void);

    virtual
    GLuint
    gl_backing(enum fastuidraw::gl::detail::GlyphAtlasGL::backing_fmt_t fmt) const;

  protected:

    virtual
    void
    resize_implement(unsigned int new_size);

  private:

    typedef fastuidraw::gl::detail::BufferGL<GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW> BufferGL;
    BufferGL m_backing_store;
  };

  class StoreGL_TextureBuffer:public StoreGL
  {
  public:
    explicit
    StoreGL_TextureBuffer(unsigned int number);

    ~StoreGL_TextureBuffer();

    virtual
    void
    set_values(unsigned int location,
               fastuidraw::c_array<const uint32_t> pdata);

    virtual
    void
    flush(void);

    virtual
    GLuint
    gl_backing(enum fastuidraw::gl::detail::GlyphAtlasGL::backing_fmt_t fmt) const;

  protected:

    virtual
    void
    resize_implement(unsigned int new_size);

  private:

    typedef fastuidraw::gl::detail::BufferGL<GL_TEXTURE_BUFFER, GL_STATIC_DRAW> BufferGL;
    BufferGL m_backing_store;
    mutable GLuint m_texture, m_texture_fp16;
    mutable bool m_tbo_dirty;
  };

  class StoreGL_Texture:public StoreGL
  {
  public:
    explicit
    StoreGL_Texture(fastuidraw::ivec2 log2_wh, unsigned int number);

    ~StoreGL_Texture();

    virtual
    void
    set_values(unsigned int location,
               fastuidraw::c_array<const uint32_t> pdata);

    virtual
    void
    flush(void);

    virtual
    GLuint
    gl_backing(enum fastuidraw::gl::detail::GlyphAtlasGL::backing_fmt_t fmt) const;

  protected:

    virtual
    void
    resize_implement(unsigned int new_size);

  private:

    static
    fastuidraw::ivec3
    texture_size(fastuidraw::uvec2 wh, unsigned int number_texels);

    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_2D_ARRAY,
                                              GL_R32UI,
                                              GL_RED_INTEGER,
                                              GL_UNSIGNED_INT,
                                              GL_NEAREST,
                                              GL_NEAREST> TextureGL;
    fastuidraw::uvec2 m_layer_dims;
    int m_texels_per_layer;
    TextureGL m_backing_store;
    mutable GLuint m_texture_fp16;
  };
}

///////////////////////////////////////////////
// StoreGL_Texture methods
StoreGL_Texture::
StoreGL_Texture(fastuidraw::ivec2 log2_wh, unsigned int number_texels):
  StoreGL(number_texels, GL_TEXTURE_2D_ARRAY, log2_wh, true),
  m_layer_dims(1 << log2_wh.x(), 1 << log2_wh.y()),
  m_texels_per_layer(m_layer_dims.x() * m_layer_dims.y()),
  m_backing_store(texture_size(m_layer_dims, number_texels), true),
  m_texture_fp16(0)
{
}

StoreGL_Texture::
~StoreGL_Texture()
{
  if (m_texture_fp16)
    {
      fastuidraw_glDeleteTextures(1, &m_texture_fp16);
    }
}

fastuidraw::ivec3
StoreGL_Texture::
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

void
StoreGL_Texture::
resize_implement(unsigned int new_size)
{
  if (m_texture_fp16)
    {
      fastuidraw_glDeleteTextures(1, &m_texture_fp16);
      m_texture_fp16 = 0;
    }
    m_backing_store.resize(texture_size(m_layer_dims, new_size));
}

void
StoreGL_Texture::
flush(void)
{
  m_backing_store.flush();
  if (m_texture_fp16 == 0)
    {
      fastuidraw_glGenTextures(1, &m_texture_fp16);
      FASTUIDRAWassert(m_texture_fp16 != 0);

      fastuidraw::gl::detail::texture_view(fastuidraw::gl::detail::compute_texture_view_support(),
                                           m_texture_fp16, GL_TEXTURE_2D_ARRAY,
                                           m_backing_store.texture(), GL_RG16F,
                                           0, 1,
                                           0, m_backing_store.dims().z());
    }
}

GLuint
StoreGL_Texture::
gl_backing(enum fastuidraw::gl::detail::GlyphAtlasGL::backing_fmt_t fmt) const
{
  return (fmt == fastuidraw::gl::detail::GlyphAtlasGL::backing_uint32_fmt) ?
    m_backing_store.texture() :
    m_texture_fp16;
}

void
StoreGL_Texture::
set_values(unsigned int location,
           fastuidraw::c_array<const uint32_t> pdata)
{
  unsigned int num_texels;
  fastuidraw::uvec3 p;

  num_texels = pdata.size();
  p.x() = fastuidraw::unpack_bits(0, m_log2_dims.x(), location);
  p.y() = fastuidraw::unpack_bits(m_log2_dims.x(), m_log2_dims.y(), location);
  p.z() = location >> (m_log2_dims.x() + m_log2_dims.y());

  while(num_texels > 0)
    {
      unsigned int num_take;
      fastuidraw::c_array<const uint8_t> take_data;
      TextureGL::EntryLocation loc;

      num_take = std::min(m_layer_dims.x() - p.x(), num_texels);
      take_data = pdata.sub_array(0, num_take).reinterpret_pointer<const uint8_t>();

      loc.m_location.x() = p.x();
      loc.m_location.y() = p.y();
      loc.m_location.z() = p.z();
      loc.m_size.x() = num_take;
      loc.m_size.y() = 1;
      loc.m_size.z() = 1;

      m_backing_store.set_data_c_array(loc, take_data);

      FASTUIDRAWassert(num_texels >= num_take);
      num_texels -= num_take;

      pdata = pdata.sub_array(num_take);
      FASTUIDRAWassert(num_texels == pdata.size());

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
// StoreGL_TextureBuffer methods
StoreGL_TextureBuffer::
StoreGL_TextureBuffer(unsigned int number):
  StoreGL(number, GL_TEXTURE_BUFFER,
          fastuidraw::ivec2(-1, -1), true),
  m_backing_store(number * sizeof(float), true),
  m_texture(0),
  m_texture_fp16(0),
  m_tbo_dirty(true)
{
}

StoreGL_TextureBuffer::
~StoreGL_TextureBuffer()
{
  if (m_texture)
    {
      FASTUIDRAWassert(m_texture_fp16);
      fastuidraw_glDeleteTextures(1, &m_texture);
      fastuidraw_glDeleteTextures(1, &m_texture_fp16);
    }
}

void
StoreGL_TextureBuffer::
set_values(unsigned int location,
           fastuidraw::c_array<const uint32_t> pdata)
{
  m_backing_store.set_data(location * sizeof(float),
                           pdata.reinterpret_pointer<const uint8_t>());
}

void
StoreGL_TextureBuffer::
flush(void)
{
  m_backing_store.flush();
}

void
StoreGL_TextureBuffer::
resize_implement(unsigned int new_size)
{
  m_backing_store.resize(new_size * sizeof(float));
  m_tbo_dirty = true;
}

GLuint
StoreGL_TextureBuffer::
gl_backing(enum fastuidraw::gl::detail::GlyphAtlasGL::backing_fmt_t fmt) const
{
  if (m_texture == 0)
    {
      fastuidraw_glGenTextures(1, &m_texture);
      FASTUIDRAWassert(m_texture != 0);

      fastuidraw_glGenTextures(1, &m_texture_fp16);
      FASTUIDRAWassert(m_texture_fp16 != 0);
    }

  if (m_tbo_dirty)
    {
      GLuint bo;

      bo = m_backing_store.buffer();
      FASTUIDRAWassert(bo != 0);
      fastuidraw_glBindTexture(GL_TEXTURE_BUFFER, m_texture);
      fastuidraw::gl::detail::tex_buffer(fastuidraw::gl::detail::compute_tex_buffer_support(),
                                         GL_TEXTURE_BUFFER, GL_R32UI, bo);
      fastuidraw_glBindTexture(GL_TEXTURE_BUFFER, m_texture_fp16);
      fastuidraw::gl::detail::tex_buffer(fastuidraw::gl::detail::compute_tex_buffer_support(),
                                         GL_TEXTURE_BUFFER, GL_RG16F, bo);
      m_tbo_dirty = false;
    }

  return (fmt == fastuidraw::gl::detail::GlyphAtlasGL::backing_uint32_fmt) ?
    m_texture :
    m_texture_fp16;
}

///////////////////////////////////////////////
// StoreGL_StorageBuffer methods
StoreGL_StorageBuffer::
StoreGL_StorageBuffer(unsigned int number):
  StoreGL(number, GL_SHADER_STORAGE_BUFFER,
                  fastuidraw::ivec2(-1, -1), false),
  m_backing_store(number * sizeof(float), true)
{
}

void
StoreGL_StorageBuffer::
set_values(unsigned int location,
           fastuidraw::c_array<const uint32_t> pdata)
{
  m_backing_store.set_data(location * sizeof(float),
                           pdata.reinterpret_pointer<const uint8_t>());
}

void
StoreGL_StorageBuffer::
flush(void)
{
  m_backing_store.flush();
}

void
StoreGL_StorageBuffer::
resize_implement(unsigned int new_size)
{
  m_backing_store.resize(new_size * sizeof(float));
}

GLuint
StoreGL_StorageBuffer::
gl_backing(enum fastuidraw::gl::detail::GlyphAtlasGL::backing_fmt_t) const
{
  return m_backing_store.buffer();
}

////////////////////////////////////////
// StoreGL methods
fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasBackingStoreBase>
StoreGL::
create(const fastuidraw::gl::PainterEngineGL::GlyphAtlasParams &P)
{
  unsigned int number;
  StoreGL *p(nullptr);

  number = P.number_floats();

  switch(P.glyph_data_backing_store_type())
    {
    case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_tbo:
      p = FASTUIDRAWnew StoreGL_TextureBuffer(number);
      break;

    case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_ssbo:
      p = FASTUIDRAWnew StoreGL_StorageBuffer(number);
      break;

    case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_texture_array:
      p = FASTUIDRAWnew StoreGL_Texture(P.texture_2d_array_store_log2_dims(),
                                        number);
      break;

    default:
      FASTUIDRAWassert(!"Bad glyph data store backing type");
    }
  return fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlasBackingStoreBase>(p);
}

//////////////////////////////////////////////////////////////////
// fastuidraw::gl::detail::GlyphAtlasGL methods
fastuidraw::gl::detail::GlyphAtlasGL::
GlyphAtlasGL(const PainterEngineGL::GlyphAtlasParams &P):
  GlyphAtlas(StoreGL::create(P))
{
}

fastuidraw::gl::detail::GlyphAtlasGL::
~GlyphAtlasGL()
{
}

GLenum
fastuidraw::gl::detail::GlyphAtlasGL::
data_binding_point(void) const
{
  const StoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const StoreGL*>(store().get()));
  p = static_cast<const StoreGL*>(store().get());
  return p->m_binding_point;
}

GLuint
fastuidraw::gl::detail::GlyphAtlasGL::
data_backing(enum backing_fmt_t fmt) const
{
  flush();
  const StoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const StoreGL*>(store().get()));
  p = static_cast<const StoreGL*>(store().get());
  return p->gl_backing(fmt);
}

bool
fastuidraw::gl::detail::GlyphAtlasGL::
data_binding_point_is_texture_unit(void) const
{
  const StoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const StoreGL*>(store().get()));
  p = static_cast<const StoreGL*>(store().get());
  return p->m_binding_point_is_texture_unit;
}

fastuidraw::ivec2
fastuidraw::gl::detail::GlyphAtlasGL::
data_texture_as_2d_array_log2_dims(void) const
{
  const StoreGL *p;
  FASTUIDRAWassert(dynamic_cast<const StoreGL*>(store().get()));
  p = static_cast<const StoreGL*>(store().get());
  return p->m_log2_dims;
}
