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
  class GeometryStoreGL:public fastuidraw::GlyphAtlasGeometryBackingStoreBase
  {
  public:
    GeometryStoreGL(unsigned int number,
                    GLenum pbinding_point, const fastuidraw::ivec2 &log2_dims,
                    bool backed_by_texture):
      fastuidraw::GlyphAtlasGeometryBackingStoreBase(number, true),
      m_binding_point(pbinding_point),
      m_log2_dims(log2_dims),
      m_backed_by_texture(backed_by_texture)
    {
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
    GeometryStoreGL_StorageBuffer(unsigned int number, bool delayed);

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
    GeometryStoreGL_TextureBuffer(unsigned int number, bool delayed);

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
    GeometryStoreGL_Texture(fastuidraw::ivec2 log2_wh, unsigned int number, bool delayed);

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
  };

  class GlyphAtlasGLParamsPrivate
  {
  public:
    GlyphAtlasGLParamsPrivate(void):
      m_number_floats(1024 * 1024),
      m_delayed(false),
      m_type(fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_tbo),
      m_log2_dims_geometry_store(-1, -1)
    {}

    unsigned int m_number_floats;
    bool m_delayed;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_backing_t m_type;
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

///////////////////////////////////////////////
// GeometryStoreGL_Texture methods
GeometryStoreGL_Texture::
GeometryStoreGL_Texture(fastuidraw::ivec2 log2_wh, unsigned int number_texels, bool delayed):
  GeometryStoreGL(number_texels, GL_TEXTURE_2D_ARRAY, log2_wh, true),
  m_layer_dims(1 << log2_wh.x(), 1 << log2_wh.y()),
  m_texels_per_layer(m_layer_dims.x() * m_layer_dims.y()),
  m_backing_store(texture_size(m_layer_dims, number_texels), delayed)
{
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
// GeometryStoreGL_TextureBuffer methods
GeometryStoreGL_TextureBuffer::
GeometryStoreGL_TextureBuffer(unsigned int number, bool delayed):
  GeometryStoreGL(number, GL_TEXTURE_BUFFER,
                  fastuidraw::ivec2(-1, -1), true),
  m_backing_store(number * sizeof(float), delayed),
  m_texture(0),
  m_tbo_dirty(true)
{
}

void
GeometryStoreGL_TextureBuffer::
set_values(unsigned int location,
           fastuidraw::c_array<const fastuidraw::generic_data> pdata)
{
  m_backing_store.set_data(location * sizeof(float),
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
  m_backing_store.resize(new_size * sizeof(float));
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
      GLuint bo;

      bo = m_backing_store.buffer();
      FASTUIDRAWassert(bo != 0);
      glBindTexture(GL_TEXTURE_BUFFER, m_texture);
      fastuidraw::gl::detail::tex_buffer(fastuidraw::gl::detail::compute_tex_buffer_support(),
                                         GL_TEXTURE_BUFFER, GL_R32UI, bo);
      m_tbo_dirty = false;
    }

  return m_texture;
}

///////////////////////////////////////////////
// GeometryStoreGL_StorageBuffer methods
GeometryStoreGL_StorageBuffer::
GeometryStoreGL_StorageBuffer(unsigned int number, bool delayed):
  GeometryStoreGL(number, GL_SHADER_STORAGE_BUFFER,
                  fastuidraw::ivec2(-1, -1), false),
  m_backing_store(number * sizeof(float), delayed)
{
}

void
GeometryStoreGL_StorageBuffer::
set_values(unsigned int location,
           fastuidraw::c_array<const fastuidraw::generic_data> pdata)
{
  m_backing_store.set_data(location * sizeof(float),
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
  m_backing_store.resize(new_size * sizeof(float));
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
  unsigned int number;
  GeometryStoreGL *p(nullptr);
  bool delayed;

  number = P.number_floats();
  delayed = P.delayed();

  switch(P.glyph_geometry_backing_store_type())
    {
    case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_tbo:
      p = FASTUIDRAWnew GeometryStoreGL_TextureBuffer(number, delayed);
      break;

    case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_ssbo:
      p = FASTUIDRAWnew GeometryStoreGL_StorageBuffer(number, delayed);
      break;

    case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_texture_array:
      p = FASTUIDRAWnew GeometryStoreGL_Texture(P.texture_2d_array_geometry_store_log2_dims(),
                                                number, delayed);
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

enum fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_backing_t
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
  d->m_type = glsl::PainterShaderRegistrarGLSL::glyph_geometry_tbo;
  d->m_log2_dims_geometry_store = ivec2(-1, -1);
  return *this;
}

fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::params::
use_storage_buffer_geometry_store(void)
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  d->m_type = glsl::PainterShaderRegistrarGLSL::glyph_geometry_ssbo;
  d->m_log2_dims_geometry_store = ivec2(-1, -1);
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
      d->m_type = glsl::PainterShaderRegistrarGLSL::glyph_geometry_texture_array;
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

fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::params::
use_optimal_geometry_store_backing(void)
{
  GlyphAtlasGLParamsPrivate *d;
  d = static_cast<GlyphAtlasGLParamsPrivate*>(m_d);

  /* Required maximal size of 64MB */
  const int32_t required_max_size(64u << 20u);

  if (context_get<int>(GL_MAX_SHADER_STORAGE_BLOCK_SIZE) >= required_max_size)
    {
      d->m_type = glsl::PainterShaderRegistrarGLSL::glyph_geometry_ssbo;
      d->m_log2_dims_geometry_store = ivec2(-1, -1);
    }
  else if (detail::compute_tex_buffer_support() != detail::tex_buffer_not_supported
           && context_get<int>(GL_MAX_TEXTURE_BUFFER_SIZE) >= required_max_size)
    {
      d->m_type = glsl::PainterShaderRegistrarGLSL::glyph_geometry_tbo;
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
      required_height = t_min(max_wh, required_area_per_layer / width);
      if (required_height * width < required_area_per_layer)
        {
          ++required_height;
        }

      d->m_type = glsl::PainterShaderRegistrarGLSL::glyph_geometry_texture_array;
      d->m_log2_dims_geometry_store.x() = uint32_log2(width);
      d->m_log2_dims_geometry_store.y() = (required_height <= 1) ?
        0 : uint32_log2(required_height);
    }
  return *this;
}

setget_implement(fastuidraw::gl::GlyphAtlasGL::params,
                 GlyphAtlasGLParamsPrivate,
                 unsigned int, number_floats);
setget_implement(fastuidraw::gl::GlyphAtlasGL::params,
                 GlyphAtlasGLParamsPrivate,
                 bool, delayed);

//////////////////////////////////////////////////////////////////
// fastuidraw::gl::GlyphAtlasGL methods
fastuidraw::gl::GlyphAtlasGL::
GlyphAtlasGL(const params &P):
  GlyphAtlas(GeometryStoreGL::create(P))
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
