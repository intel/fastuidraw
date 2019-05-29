/*!
 * \file texture_gl.hpp
 * \brief file texture_gl.hpp
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


#pragma once

#include <list>
#include <vector>
#include <algorithm>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

#include <private/gl_backend/scratch_renderer.hpp>

namespace fastuidraw { namespace gl { namespace detail {

GLenum
format_from_internal_format(GLenum fmt);

GLenum
type_from_internal_format(GLenum fmt);

class CopyImageSubData
{
public:
  CopyImageSubData(void);

  void
  operator()(GLuint srcName, GLenum srcTarget, GLint srcLevel,
             GLint srcX, GLint srcY, GLint srcZ,
             GLuint dstName, GLenum dstTarget, GLint dstLevel,
             GLint dstX, GLint dstY, GLint dstZ,
             GLsizei width, GLsizei height, GLsizei depth) const;

private:
  enum type_t
    {
      unextended_function,
      #ifdef FASTUIDRAW_GL_USE_GLES
        oes_function,
        ext_function,
      #endif
      emulate_function,

      uninited,
    };

  static
  enum type_t
  compute_type(void);

  mutable enum type_t m_type;
};

template<GLenum texture_target>
class TextureTargetDimension
{};

///////////////////////////////////
// 3D
#ifdef GL_TEXTURE_3D
template<>
class TextureTargetDimension<GL_TEXTURE_3D>
{
public:
  enum { N = 3 };
  enum { Binding = GL_TEXTURE_BINDING_3D };

  static
  vecN<GLsizei, 3>
  next_lod_size(vecN<GLsizei, 3> dims)
  {
    vecN<GLsizei, 3> R(t_max(1, dims.x() / 2),
                       t_max(1, dims.y() / 2),
                       t_max(1, dims.z() / 2));
    return R;
  }
};
#endif

template<>
class TextureTargetDimension<GL_TEXTURE_2D_ARRAY>
{
public:
  enum { N = 3 };
  enum { Binding = GL_TEXTURE_BINDING_2D_ARRAY };

  static
  vecN<GLsizei, 3>
  next_lod_size(vecN<GLsizei, 3> dims)
  {
    vecN<GLsizei, 3> R(t_max(1, dims.x() / 2),
                       t_max(1, dims.y() / 2),
                       dims.z());
    return R;
  }
};

#ifdef GL_TEXTURE_CUBE_MAP_ARRAY
template<>
class TextureTargetDimension<GL_TEXTURE_CUBE_MAP_ARRAY>
{
public:
  enum { N = 3 };
  enum { Binding = GL_TEXTURE_BINDING_CUBE_MAP_ARRAY };

  static
  vecN<GLsizei, 3>
  next_lod_size(vecN<GLsizei, 3> dims)
  {
    vecN<GLsizei, 3> R(t_max(1, dims.x() / 2),
                       t_max(1, dims.y() / 2),
                       dims.z());
    return R;
  }
};
#endif

template<GLenum texture_target>
inline
void
tex_storage(bool use_tex_storage, GLint internalformat, vecN<GLsizei, 3> size,
            unsigned int num_levels = 1)
{
  if (use_tex_storage)
    {
      fastuidraw_glTexStorage3D(texture_target, num_levels, internalformat,
                                size.x(), size.y(), size.z());
    }
  else
    {
      for (unsigned int i = 0; i < num_levels; ++i)
        {
          fastuidraw_glTexImage3D(texture_target,
                                  i,
                                  internalformat,
                                  size.x(), size.y(), size.z(), 0,
                                  format_from_internal_format(internalformat),
                                  type_from_internal_format(internalformat),
                                  nullptr);
          size = TextureTargetDimension<texture_target>::next_lod_size(size);
        }
    }
}

template<GLenum texture_target>
inline
void
tex_sub_image(int level, vecN<GLint, 3> offset,
              vecN<GLsizei, 3> size, GLenum format, GLenum type,
              const void *pixels)
{
  fastuidraw_glTexSubImage3D(texture_target, level,
                             offset.x(), offset.y(), offset.z(),
                             size.x(), size.y(), size.z(),
                             format, type, pixels);
}

//////////////////////////////////////////////
// 2D

template<>
class TextureTargetDimension<GL_TEXTURE_2D>
{
public:
  enum { N = 2 };
  enum { Binding = GL_TEXTURE_BINDING_2D };

  static
  vecN<GLsizei, 2>
  next_lod_size(vecN<GLsizei, 2> dims)
  {
    vecN<GLsizei, 2> R(t_max(1, dims.x() / 2),
                       t_max(1, dims.y() / 2));
    return R;
  }
};

#ifdef GL_TEXTURE_1D_ARRAY
template<>
class TextureTargetDimension<GL_TEXTURE_1D_ARRAY>
{
public:
  enum { N = 2 };
  enum { Binding = GL_TEXTURE_BINDING_1D_ARRAY };

  static
  vecN<GLsizei, 2>
  next_lod_size(vecN<GLsizei, 2> dims)
  {
    vecN<GLsizei, 2> R(t_max(1, dims.x() / 2),
                       dims.y());
    return R;
  }
};
#endif

#ifdef GL_TEXTURE_RECTANGLE
template<>
class TextureTargetDimension<GL_TEXTURE_RECTANGLE>
{
public:
  enum { N = 2 };
  enum { Binding = GL_TEXTURE_BINDING_RECTANGLE };

  vecN<GLsizei, 2>
  next_lod_size(vecN<GLsizei, 2> dims)
  {
    return dims;
  }
};
#endif

template<GLenum texture_target>
inline
void
tex_storage(bool use_tex_storage, GLint internalformat, vecN<GLsizei, 2> size,
            unsigned int num_levels = 1)
{
  if (use_tex_storage)
    {
      fastuidraw_glTexStorage2D(texture_target, num_levels, internalformat, size.x(), size.y());
    }
  else
    {
      for (unsigned int i = 0; i < num_levels; ++i)
        {
          fastuidraw_glTexImage2D(texture_target,
                                  i,
                                  internalformat,
                                  size.x(), size.y(), 0,
                                  format_from_internal_format(internalformat),
                                  type_from_internal_format(internalformat),
                                  nullptr);
          size = TextureTargetDimension<texture_target>::next_lod_size(size);
        }
    }
}

template<GLenum texture_target>
inline
void
tex_sub_image(int level,
              vecN<GLint, 2> offset,
              vecN<GLsizei, 2> size,
              GLenum format, GLenum type, const void *pixels)
{
  fastuidraw_glTexSubImage2D(texture_target, level,
                             offset.x(), offset.y(),
                             size.x(), size.y(),
                             format, type, pixels);
}


//////////////////////////////////////////
// 1D
#ifdef GL_TEXTURE_1D
template<>
class TextureTargetDimension<GL_TEXTURE_1D>
{
public:
  enum { N = 1 };
  enum { Binding = GL_TEXTURE_BINDING_1D };

  static
  vecN<GLsizei, 1>
  next_lod_size(vecN<GLsizei, 1> dims)
  {
    vecN<GLsizei, 1> R(t_max(dims.x() / 2, 1));
    return R;
  }
};
#endif

#if defined(glTexStorage1D)

template<GLenum texture_target>
inline
void
tex_storage(bool use_tex_storage, GLint internalformat, vecN<GLsizei, 1> size,
            unsigned int num_levels = 1)
{
  if (use_tex_storage)
    {
      fastuidraw_glTexStorage1D(texture_target, 1, internalformat, size.x());
    }
  else
    {
      for (unsigned int i = 0; i < num_levels; ++i)
        {
          fastuidraw_glTexImage1D(texture_target,
                                  i,
                                  internalformat,
                                  size.x(), 0,
                                  format_from_internal_format(internalformat),
                                  type_from_internal_format(internalformat),
                                  nullptr);
          size = TextureTargetDimension<texture_target>::next_lod_size(size);
        }
    }
}

template<GLenum texture_target>
inline
void
tex_sub_image(int level, vecN<GLint, 1> offset,
              vecN<GLsizei, 1> size, GLenum format, GLenum type,
              const void *pixels)
{
  fastuidraw_glTexSubImage1D(texture_target, level, offset.x(), size.x(), format, type, pixels);
}

#endif

template<size_t N>
class EntryLocationN
{
public:
  typedef std::pair<EntryLocationN, std::vector<uint8_t> > with_data;

  EntryLocationN(void):
    m_mipmap_level(0u)
  {}

  vecN<int, N> m_location;
  vecN<GLsizei, N> m_size;
  unsigned int m_mipmap_level;
};

class UseTexStorage
{
public:
  UseTexStorage(void)
  {
    ContextProperties ctx;

    m_use_tex_storage = ctx.is_es() || ctx.version() >= ivec2(4, 2)
      || ctx.has_extension("GL_ARB_texture_storage");
  }

  operator bool() const { return m_use_tex_storage; }

private:
  bool m_use_tex_storage;
};

template<GLenum texture_target>
class TextureGLGeneric
{
public:
  enum { N = TextureTargetDimension<texture_target>::N };
  enum { BindingPoint = texture_target };

  typedef EntryLocationN<N> EntryLocation;
  typedef vecN<int, N> DimensionType;

  TextureGLGeneric(GLenum internal_format,
                   GLenum external_format,
                   GLenum external_type,
                   GLenum mag_filter,
                   GLenum min_filter,
                   DimensionType dims, bool delayed,
                   unsigned int mipmap_levels = 1);
  ~TextureGLGeneric();

  void
  delete_texture(void);

  GLuint
  texture(void) const;

  void
  flush(void);

  void
  set_data_vector(const EntryLocation &loc,
                  std::vector<uint8_t> &data);

  void
  set_data_c_array(const EntryLocation &loc,
                   c_array<const uint8_t> data);

  void
  resize(vecN<int, N> new_dims)
  {
    m_dims = new_dims;
  }

  const vecN<int, N>&
  dims(void) const
  {
    return m_dims;
  }

  int
  num_mipmaps(void) const
  {
    return m_num_mipmaps;
  }

private:

  void
  create_texture(void) const;

  void
  tex_subimage(const EntryLocation &loc,
               c_array<const uint8_t> data);

  void
  flush_size_change(void);

  GLenum m_internal_format;
  GLenum m_external_format;
  GLenum m_external_type;
  GLenum m_mag_filter;
  GLenum m_min_filter;

  bool m_delayed;
  vecN<int, N> m_dims;
  unsigned int m_num_mipmaps;
  vecN<int, N> m_texture_dimension;
  mutable GLuint m_texture;
  mutable bool m_use_tex_storage;
  mutable int m_number_times_create_texture_called;
  CopyImageSubData m_blitter;

  typedef typename EntryLocation::with_data with_data;
  typedef std::list<with_data> list_type;

  list_type m_unflushed_commands;
};

///////////////////////////////////////
//TextureGLGeneric methods
template<GLenum texture_target>
TextureGLGeneric<texture_target>::
TextureGLGeneric(GLenum internal_format,
                 GLenum external_format,
                 GLenum external_type,
                 GLenum mag_filter,
                 GLenum min_filter,
                 vecN<int, N> dims, bool delayed,
                 unsigned int mipmap_levels):
  m_internal_format(internal_format),
  m_external_format(external_format),
  m_external_type(external_type),
  m_mag_filter(mag_filter),
  m_min_filter(min_filter),
  m_delayed(delayed),
  m_dims(dims),
  m_num_mipmaps(mipmap_levels),
  m_texture(0),
  m_number_times_create_texture_called(0)
{
  if (!m_delayed)
    {
      create_texture();
    }
  m_texture_dimension = m_dims;
}

template<GLenum texture_target>
TextureGLGeneric<texture_target>::
~TextureGLGeneric()
{
  if (m_texture != 0)
    {
      delete_texture();
    }
}

template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
flush_size_change(void)
{
  if (m_texture_dimension != m_dims)
    {
      /* only need to issue GL commands to resize
       * the underlying GL texture IF we do not have
       * a texture yet.
       */
      if (m_texture != 0)
        {
          GLuint old_texture;

          old_texture = m_texture;
          /* create a new texture for the new size,
           */
          m_texture = 0;
          create_texture();

          /* copy the contents of old_texture to m_texture
           */
          vecN<GLint, 3> blit_dims;
          for(unsigned int i = 0; i < N; ++i)
            {
              blit_dims[i] = std::min(m_dims[i], m_texture_dimension[i]);
            }
          for(unsigned int i = N; i < 3; ++i)
            {
              blit_dims[i] = 1;
            }

          #ifdef GL_TEXTURE_1D_ARRAY
            {
              /* Sighs. The GL API is utterly wonky. For GL_TEXTURE_1D_ARRAY,
               * we need to permute [2] and [1].
               * "Slices of a TEXTURE_1D_ARRAY, TEXTURE_2D_ARRAY, TEXTURE_CUBE_MAP_ARRAY
               * TEXTURE_3D and faces of TEXTURE_CUBE_MAP are all compatible provided
               * they share a compatible internal format, and multiple slices or faces
               * may be copied between these objects with a single call by specifying the
               * starting slice with <srcZ> and <dstZ>, and the number of slices to
               * be copied with <srcDepth>.
               */
              if (texture_target == GL_TEXTURE_1D_ARRAY)
                {
                  std::swap(blit_dims[1], blit_dims[2]);
                }
            }
          #endif

          m_blitter(old_texture, texture_target, 0,
                    0, 0, 0, //src
                    m_texture, texture_target, 0,
                    0, 0, 0, //dst
                    blit_dims[0], blit_dims[1], blit_dims[2]);

          /* now delete old_texture
           */
          fastuidraw_glDeleteTextures(1, &old_texture);
        }
      m_texture_dimension = m_dims;
    }
}

template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
delete_texture(void)
{
  FASTUIDRAWassert(m_texture != 0);
  fastuidraw_glDeleteTextures(1, &m_texture);
  m_texture = 0;
}


template<GLenum texture_target>
GLuint
TextureGLGeneric<texture_target>::
texture(void) const
{
  FASTUIDRAWassert(m_texture != 0);
  return m_texture;
}


template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
create_texture(void) const
{
  FASTUIDRAWassert(m_texture == 0);
  fastuidraw_glGenTextures(1, &m_texture);
  FASTUIDRAWassert(m_texture!=0);
  fastuidraw_glBindTexture(texture_target, m_texture);
  if (m_number_times_create_texture_called == 0)
    {
      ContextProperties ctx;
      m_use_tex_storage = ctx.is_es() || ctx.version() >= ivec2(4, 2)
        || ctx.has_extension("GL_ARB_texture_storage");
    }
  tex_storage<texture_target>(m_use_tex_storage, m_internal_format, m_dims, m_num_mipmaps);
  fastuidraw_glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, m_min_filter);
  fastuidraw_glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, m_mag_filter);
  fastuidraw_glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, m_num_mipmaps - 1);
  ++m_number_times_create_texture_called;
}


template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
flush(void)
{
  flush_size_change();
  if (m_texture == 0)
    {
      create_texture();
    }

  if (!m_unflushed_commands.empty())
    {
      fastuidraw_glBindTexture(texture_target, m_texture);
      fastuidraw_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      for(const auto &cmd : m_unflushed_commands)
        {
          FASTUIDRAWassert(!cmd.second.empty());
          tex_sub_image<texture_target>(cmd.first.m_mipmap_level,
                                        cmd.first.m_location,
                                        cmd.first.m_size,
                                        m_external_format, m_external_type,
                                        &cmd.second[0]);
        }
      m_unflushed_commands.clear();
    }
}


template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
set_data_vector(const EntryLocation &loc,
                std::vector<uint8_t> &data)
{
  if (data.empty())
    {
      return;
    }

  if (m_delayed)
    {
      m_unflushed_commands.push_back(typename EntryLocation::with_data());
      typename EntryLocation::with_data &R(m_unflushed_commands.back());
      R.first = loc;
      R.second.swap(data);
    }
  else
    {
      flush_size_change();
      fastuidraw_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      fastuidraw_glBindTexture(texture_target, m_texture);
      tex_sub_image<texture_target>(loc.m_mipmap_level,
                                    loc.m_location,
                                    loc.m_size,
                                    m_external_format, m_external_type,
                                    &data[0]);
    }
}

template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
set_data_c_array(const EntryLocation &loc,
                 fastuidraw::c_array<const uint8_t> data)
{
  if (data.empty())
    {
      return;
    }

  if (m_delayed)
    {
      std::vector<uint8_t> data_copy;
      data_copy.resize(data.size());
      std::copy(data.begin(), data.end(), data_copy.begin());
      set_data_vector(loc, data_copy);
    }
  else
    {
      flush_size_change();
      fastuidraw_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      fastuidraw_glBindTexture(texture_target, m_texture);
      tex_sub_image<texture_target>(loc.m_mipmap_level,
                                    loc.m_location,
                                    loc.m_size,
                                    m_external_format, m_external_type,
                                    data.c_ptr());

    }
}

template<GLenum texture_target,
         GLenum internal_format,
         GLenum external_format,
         GLenum external_type,
         GLenum mag_filter,
         GLenum min_filter>
class TextureGL:public TextureGLGeneric<texture_target>
{
public:
  TextureGL(typename TextureGLGeneric<texture_target>::DimensionType dims, bool delayed,
            unsigned int num_mip_map_levels = 1):
    TextureGLGeneric<texture_target>(internal_format, external_format,
                                     external_type, mag_filter, min_filter,
                                     dims, delayed, num_mip_map_levels)
  {}
};

////////////////////////////////
// non-member methods
enum texture_type_t
  {
      decimal_color_texture_type,
      integer_color_texture_type,
      unsigned_integer_color_texture_type,
      depth_texture_type,
      depth_stencil_texture_type,
  };


enum texture_type_t
compute_texture_type(GLenum external_format, GLenum external_type);

inline
enum texture_type_t
compute_texture_type_from_internal_format(GLenum internal_format)
{
  return compute_texture_type(format_from_internal_format(internal_format),
                              type_from_internal_format(internal_format));
}

/* \param texture GL name of texture
 * \param level mipmap level to clear
 * \param render_scratch if non-null, render a little junk to the texture
 *                       to encourage an implementation to attach
 *                       auxiliary surfaces for those buggy GL
 *                       implementations that forget to attach
 *                       auxiliary surfaces to a texture's bindless
 *                       description if it attached the auxliary
 *                       surface after the bindless handle was made.
 */
void
clear_texture_2d(GLuint texture, GLint level, enum texture_type_t,
                 ScratchRenderer *render_scratch = nullptr);

inline
void
clear_texture_2d(GLuint texture, GLint level,
                 GLenum external_format, GLenum external_type,
                 ScratchRenderer *render_scratch = nullptr)
{
  clear_texture_2d(texture, level, compute_texture_type(external_format, external_type), render_scratch);
}

inline
void
clear_texture_2d(GLuint texture, GLint level, GLenum internal_format,
                 ScratchRenderer *render_scratch = nullptr)
{
  clear_texture_2d(texture, level, compute_texture_type_from_internal_format(internal_format), render_scratch);
}


} //namespace detail
} //namespace gl
} //namespace fastuidraw
