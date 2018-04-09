/*!
 * \file texture_gl.hpp
 * \brief file texture_gl.hpp
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


#pragma once

#include <list>
#include <vector>
#include <algorithm>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

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

class ClearImageSubData
{
public:
  ClearImageSubData(void);

  template<GLenum texture_target>
  void
  clear(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
        GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type);
private:
  enum type_t
    {
      use_clear_texture,
      use_tex_sub_image,
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
};
#endif

template<>
class TextureTargetDimension<GL_TEXTURE_2D_ARRAY>
{
public:
  enum { N = 3 };
  enum { Binding = GL_TEXTURE_BINDING_2D_ARRAY };
};

#ifdef GL_TEXTURE_CUBE_MAP_ARRAY
template<>
class TextureTargetDimension<GL_TEXTURE_CUBE_MAP_ARRAY>
{
public:
  enum { N = 3 };
  enum { Binding = GL_TEXTURE_BINDING_CUBE_MAP_ARRAY };
};
#endif

inline
void
tex_storage(bool use_tex_storage,
            GLenum texture_target,
            GLint internalformat,
            vecN<GLsizei, 3> size)
{
  if(use_tex_storage)
    {
      glTexStorage3D(texture_target, 1, internalformat,
                     size.x(), size.y(), size.z());
    }
  else
    {
      glTexImage3D(texture_target,
                   0,
                   internalformat,
                   size.x(), size.y(), size.z(), 0,
                   format_from_internal_format(internalformat),
                   type_from_internal_format(internalformat),
                   nullptr);
    }
}

inline
void
tex_sub_image(GLenum texture_target, vecN<GLint, 3> offset,
              vecN<GLsizei, 3> size, GLenum format, GLenum type,
              const void *pixels)
{
  glTexSubImage3D(texture_target, 0,
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
};

#ifdef GL_TEXTURE_1D_ARRAY
template<>
class TextureTargetDimension<GL_TEXTURE_1D_ARRAY>
{
public:
  enum { N = 2 };
  enum { Binding = GL_TEXTURE_BINDING_1D_ARRAY };
};
#endif

#ifdef GL_TEXTURE_RECTANGLE
template<>
class TextureTargetDimension<GL_TEXTURE_RECTANGLE>
{
public:
  enum { N = 2 };
  enum { Binding = GL_TEXTURE_BINDING_RECTANGLE };
};
#endif

inline
void
tex_storage(bool use_tex_storage,
            GLenum texture_target, GLint internalformat, vecN<GLsizei, 2> size)
{
  if(use_tex_storage)
    {
      glTexStorage2D(texture_target, 1, internalformat, size.x(), size.y());
    }
  else
    {
      glTexImage2D(texture_target,
                   0,
                   internalformat,
                   size.x(), size.y(), 0,
                   format_from_internal_format(internalformat),
                   type_from_internal_format(internalformat),
                   nullptr);
    }
}

inline
void
tex_sub_image(GLenum texture_target,
              vecN<GLint, 2> offset,
              vecN<GLsizei, 2> size,
              GLenum format, GLenum type, const void *pixels)
{
  glTexSubImage2D(texture_target, 0,
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
};
#endif

#if defined(glTexStorage1D)

inline
void
tex_storage(bool use_tex_storage,
            GLenum texture_target, GLint internalformat, vecN<GLsizei, 1> size)
{
  if(use_tex_storage)
    {
      glTexStorage1D(texture_target, 1, internalformat, size.x());
    }
  else
    {
      glTexImage1D(texture_target,
                   0,
                   internalformat,
                   size.x(), 0,
                   format_from_internal_format(internalformat),
                   type_from_internal_format(internalformat),
                   nullptr);
    }
}

inline
void
tex_sub_image(GLenum texture_target, vecN<GLint, 1> offset,
              vecN<GLsizei, 1> size, GLenum format, GLenum type,
              const void *pixels)
{
  glTexSubImage1D(texture_target, 0, offset.x(), size.x(), format, type, pixels);
}

#endif

template<size_t N>
class EntryLocationN
{
public:
  typedef std::pair<EntryLocationN, std::vector<uint8_t> > with_data;
  vecN<int, N> m_location;
  vecN<GLsizei, N> m_size;
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
                   GLenum filter,
                   DimensionType dims, bool delayed);
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
  resize(vecN<int, N> new_num_layers)
  {
    m_dims = new_num_layers;
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
  GLenum m_filter;

  bool m_delayed;
  vecN<int, N> m_dims;
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
                 GLenum filter,
                 vecN<int, N> dims, bool delayed):
  m_internal_format(internal_format),
  m_external_format(external_format),
  m_external_type(external_type),
  m_filter(filter),
  m_delayed(delayed),
  m_dims(dims),
  m_texture(0),
  m_number_times_create_texture_called(0)
{
  if(!m_delayed)
    {
      create_texture();
    }
  m_texture_dimension = m_dims;
}

template<GLenum texture_target>
TextureGLGeneric<texture_target>::
~TextureGLGeneric()
{
  if(m_texture != 0)
    {
      delete_texture();
    }
}

template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
flush_size_change(void)
{
  if(m_texture_dimension != m_dims)
    {
      /* only need to issue GL commands to resize
         the underlying GL texture IF we do not have
         a texture yet.
       */
      if(m_texture != 0)
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
                 we need to permute [2] and [1].
                 "Slices of a TEXTURE_1D_ARRAY, TEXTURE_2D_ARRAY, TEXTURE_CUBE_MAP_ARRAY
                 TEXTURE_3D and faces of TEXTURE_CUBE_MAP are all compatible provided
                 they share a compatible internal format, and multiple slices or faces
                 may be copied between these objects with a single call by specifying the
                 starting slice with <srcZ> and <dstZ>, and the number of slices to
                 be copied with <srcDepth>.
              */
              if(texture_target == GL_TEXTURE_1D_ARRAY)
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
          glDeleteTextures(1, &old_texture);
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
  glDeleteTextures(1, &m_texture);
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
  glGenTextures(1, &m_texture);
  FASTUIDRAWassert(m_texture!=0);
  glBindTexture(texture_target, m_texture);
  if(m_number_times_create_texture_called == 0)
    {
      ContextProperties ctx;
      m_use_tex_storage = ctx.is_es() || ctx.version() >= ivec2(4, 2)
        || ctx.has_extension("GL_ARB_texture_storage");
    }
  tex_storage(m_use_tex_storage, texture_target, m_internal_format, m_dims);
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, m_filter);
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, m_filter);
  ++m_number_times_create_texture_called;
}


template<GLenum texture_target>
void
TextureGLGeneric<texture_target>::
flush(void)
{
  flush_size_change();
  if(m_texture == 0)
    {
      create_texture();
    }

  if(!m_unflushed_commands.empty())
    {
      glBindTexture(texture_target, m_texture);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      for(typename list_type::iterator iter = m_unflushed_commands.begin(),
            end = m_unflushed_commands.end(); iter != end; ++iter)
        {
          FASTUIDRAWassert(!iter->second.empty());
          tex_sub_image(texture_target,
                        iter->first.m_location,
                        iter->first.m_size,
                        m_external_format, m_external_type,
                        &iter->second[0]);
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
  if(data.empty())
    {
      return;
    }

  if(m_delayed)
    {
      m_unflushed_commands.push_back(typename EntryLocation::with_data());
      typename EntryLocation::with_data &R(m_unflushed_commands.back());
      R.first = loc;
      R.second.swap(data);
    }
  else
    {
      flush_size_change();
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glBindTexture(texture_target, m_texture);
      tex_sub_image(texture_target,
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
  if(data.empty())
    {
      return;
    }

  if(m_delayed)
    {
      std::vector<uint8_t> data_copy;
      data_copy.resize(data.size());
      std::copy(data.begin(), data.end(), data_copy.begin());
      set_data_vector(loc, data_copy);
    }
  else
    {
      flush_size_change();
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glBindTexture(texture_target, m_texture);
      tex_sub_image(texture_target,
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
         GLenum filter>
class TextureGL:public TextureGLGeneric<texture_target>
{
public:
  TextureGL(typename TextureGLGeneric<texture_target>::DimensionType dims, bool delayed):
    TextureGLGeneric<texture_target>(internal_format, external_format,
                                     external_type, filter,
                                     dims, delayed)
  {}
};

////////////////////////////////
// ClearImageSubData methods
template<GLenum texture_target>
void
ClearImageSubData::
clear(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
      GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type)
{
  if (m_type == uninited)
    {
      m_type = compute_type();
    }

  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      if (m_type == use_clear_texture)
        {
          vecN<uint32_t, 4> zero(0, 0, 0, 0);
          glClearTexSubImage(texture, level,
                             xoffset, yoffset, zoffset,
                             width, height, depth,
                             format, type,
                             &zero);
          return;
        }
    }
  #endif

  vecN<uint32_t, 4> zero(0, 0, 0, 0);
  unsigned int num_texels(width * height * depth);
  std::vector<vecN<uint32_t, 4> > zeros(num_texels, zero);
  GLint old_texture;
  vecN<GLsizei, TextureTargetDimension<texture_target>::N> offset, sz;

  sz[0] = width;
  offset[0] = xoffset;

  if(TextureTargetDimension<texture_target>::N >= 2)
    {
      sz[1] = height;
      offset[1] = yoffset;
    }

  if(TextureTargetDimension<texture_target>::N >= 3)
    {
      sz[2] = depth;
      offset[2] = zoffset;
    }

  glGetIntegerv(TextureTargetDimension<texture_target>::Binding, &old_texture);
  glBindTexture(texture_target, texture);
  tex_sub_image(texture_target, offset, sz, format, type, &zeros[0]);
  glBindTexture(texture_target, old_texture);
}

} //namespace detail
} //namespace gl
} //namespace fastuidraw
