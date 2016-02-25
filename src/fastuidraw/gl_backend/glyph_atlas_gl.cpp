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


#include <sstream>

#include <fastuidraw/text/glyph_render_data_curve_pair.hpp>
#include <fastuidraw/gl_backend/glyph_atlas_gl.hpp>

#include "private/texture_gl.hpp"
#include "private/buffer_object_gl.hpp"
#include "private/tex_buffer.hpp"
#include "private/texture_view.hpp"
#include "../private/util_private.hpp"

namespace
{
  class Ending
  {
  public:
    explicit
    Ending(unsigned int N, enum fastuidraw::GlyphRenderDataCurvePair::geometry_packing pk):
      m_N(N),
      m_pk(pk)
    {}

    unsigned int m_N;
    enum fastuidraw::GlyphRenderDataCurvePair::geometry_packing m_pk;

  };

  std::ostream&
  operator<<(std::ostream &str, Ending obj)
  {
    int tempIndex, cc;
    const char *component[]=
      {
        "r",
        "g",
        "b",
        "a"
      };

    tempIndex = obj.m_pk / obj.m_N;
    cc = obj.m_pk - obj.m_N * tempIndex;
    assert(0<= cc && cc < 4);
    if(obj.m_N > 1)
      {
        str << tempIndex << "." << component[cc];
      }
    else
      {
        str << tempIndex;
      }
    return str;
  }

  class TexelStoreGL:public fastuidraw::GlyphAtlasTexelBackingStoreBase
  {
  public:
    TexelStoreGL(fastuidraw::ivec3 dims, bool delayed);

    ~TexelStoreGL(void);

    void
    set_data(int x, int y, int l, int w, int h,
             fastuidraw::const_c_array<uint8_t> data);

    void
    flush(void)
    {
      m_backing_store.flush();
    }

    GLuint
    texture(bool as_integer) const;

    static
    handle
    create(fastuidraw::ivec3 dims, bool delayed);

  protected:

    virtual
    void
    resize_implement(int new_num_layers);


  private:
    typedef fastuidraw::gl::detail::TextureGL<GL_TEXTURE_2D_ARRAY,
                                             GL_R8UI, GL_RED_INTEGER,
                                             GL_UNSIGNED_BYTE,
                                             GL_NEAREST> TextureGL;
    TextureGL m_backing_store;
    mutable GLuint m_texture_as_r8;
  };

  class GeometryStoreGL:public fastuidraw::GlyphAtlasGeometryBackingStoreBase
  {
  public:
    explicit
    GeometryStoreGL(unsigned int number_vecNs, bool delayed, unsigned int N);

    void
    set_values(unsigned int location,
               fastuidraw::const_c_array<fastuidraw::generic_data> pdata);

    void
    flush(void);

    GLuint
    texture(void) const;

    unsigned int
    alignment(void) const
    {
      return m_N;
    }

    static
    handle
    create(unsigned int number_vecNs, bool delayed, unsigned int N);

  protected:

    void
    resize_implement(unsigned int new_size)
    {
      m_backing_store.resize(new_size * m_N * sizeof(float));
    }

  private:

    typedef fastuidraw::gl::detail::BufferGL<GL_TEXTURE_BUFFER, GL_STATIC_DRAW> BufferGL;
    BufferGL m_backing_store;
    unsigned int m_N;
    mutable GLuint m_texture;
  };

  class LoaderMacro
  {
  public:
    LoaderMacro(unsigned int alignment, const char *geometry_store_name);

    const char*
    value(void) const
    {
      return m_value.c_str();
    }
  private:
    std::string m_value;
  };

  class GlyphAtlasGLParamsPrivate
  {
  public:
    GlyphAtlasGLParamsPrivate(void):
      m_texel_store_dimensions(1024, 1024, 16),
      m_number_floats(1024 * 1024),
      m_delayed(false),
      m_alignment(4)
    {}

    fastuidraw::ivec3 m_texel_store_dimensions;
    unsigned int m_number_floats;
    bool m_delayed;
    unsigned int m_alignment;
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
     of the texture
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
  if(m_texture_as_r8 != 0)
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

  if(m_texture_as_r8 != 0)
    {
      /* a resize generates a new texture which then has
         a new backing store. The texture view would then
         be using the old backing store which is useless.
         We delete the old texture view and let texture()
         recreate the view on demand.
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
         fastuidraw::const_c_array<uint8_t> data)
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

  assert(tex_as_r8ui != 0);

  if(as_integer)
    {
      return tex_as_r8ui;
    }

  if(m_texture_as_r8 == 0)
    {
      enum fastuidraw::gl::detail::texture_view_support_t md;

      md = fastuidraw::gl::detail::compute_texture_view_support();
      if(md != fastuidraw::gl::detail::texture_view_not_supported)
        {
          glGenTextures(1, &m_texture_as_r8);
          assert(m_texture_as_r8 != 0);

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
             right/bottom). This way filtering works.
          */
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
          glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_REPEAT);
        }
    }
  return m_texture_as_r8;
}


TexelStoreGL::handle
TexelStoreGL::
create(fastuidraw::ivec3 dims, bool delayed)
{
  TexelStoreGL *p;
  p = FASTUIDRAWnew TexelStoreGL(dims, delayed);
  return handle(p);
}

///////////////////////////////////////////////
// GeometryStoreGL methods
GeometryStoreGL::
GeometryStoreGL(unsigned int number_vecNs, bool delayed, unsigned int N):
  fastuidraw::GlyphAtlasGeometryBackingStoreBase(N, number_vecNs, true),
  m_backing_store(number_vecNs * N * sizeof(float), delayed),
  m_N(N),
  m_texture(0)
{
  assert(m_N <= 4 && m_N > 0);
}

void
GeometryStoreGL::
set_values(unsigned int location,
           fastuidraw::const_c_array<fastuidraw::generic_data> pdata)
{
  assert(pdata.size() % m_N == 0);
  m_backing_store.set_data(location * m_N * sizeof(float),
                           pdata.reinterpret_pointer<uint8_t>());
}

void
GeometryStoreGL::
flush(void)
{
  m_backing_store.flush();
}

GLuint
GeometryStoreGL::
texture(void) const
{
  if(m_texture == 0)
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
      assert(bo != 0);

      glGenTextures(1, &m_texture);
      assert(m_texture != 0);

      glBindTexture(GL_TEXTURE_BUFFER, m_texture);
      fastuidraw::gl::detail::tex_buffer(fastuidraw::gl::detail::compute_tex_buffer_support(),
                                        GL_TEXTURE_BUFFER, formats[m_N - 1], bo);
    }
  return m_texture;
}

GeometryStoreGL::handle
GeometryStoreGL::
create(unsigned int number_floats, bool delayed, unsigned int N)
{
  unsigned int number_vecNs;

  GeometryStoreGL *p;

  number_vecNs = number_floats / N;
  if(number_vecNs * N < number_floats)
    {
      ++number_vecNs;
    }
  p = FASTUIDRAWnew GeometryStoreGL(number_vecNs, delayed, N);
  return handle(p);
}

////////////////////////////////////////////////////
// LoaderMacro methods
LoaderMacro::
LoaderMacro(unsigned int alignment, const char *geometry_store_name)
{
  /* we are defining a macro, so on end of lines
     in code we need to add \\
   */
  std::ostringstream str;
  const char *texelFetchExt[4] =
    {
      "r",
      "rg",
      "rgb",
      "rgba"
    };
  const char *tempType[4] =
    {
      "float",
      "vec2",
      "vec3",
      "vec4"
    };

  str << "\n#define FASTUIDRAW_LOAD_CURVE_GEOMETRY(geometry_offset, texel_value) \\\n"
      << "\t" << tempType[alignment - 1] << " ";

  for(unsigned int c = 0, j = 0; c < fastuidraw::GlyphRenderDataCurvePair::number_elements_to_pack; c += alignment, ++j)
    {
      if(c != 0)
        {
          str << ", ";
        }
      str << "temp" << j;
    }
  str << ";\\\n"
      << "\tint start_offset;\\\n"
      << "\tstart_offset = int(geometry_offset) + ( int(texel_value) - 2 ) * int("
      << fastuidraw::round_up_to_multiple(fastuidraw::GlyphRenderDataCurvePair::number_elements_to_pack, alignment) / alignment
      <<");\\\n";

  for(unsigned int c = 0, j = 0; c < fastuidraw::GlyphRenderDataCurvePair::number_elements_to_pack; c += alignment, ++j)
    {
      str << "\ttemp" << j << " = texelFetch("  << geometry_store_name << ", start_offset + "
          << j << ")." << texelFetchExt[alignment - 1] << ";\\\n";
    }

  /* this is scary, the shader code will have the macros/whatever defined for us so that
     that assignment from temp "just works".
   */
#define EXTRACT_STREAM(X) str << "\t"#X << " = temp" << Ending(alignment, fastuidraw::GlyphRenderDataCurvePair::pack_offset_##X) << ";\\\n"


  EXTRACT_STREAM(p_x);
  EXTRACT_STREAM(p_y);
  EXTRACT_STREAM(zeta);
  EXTRACT_STREAM(combine_rule);

  EXTRACT_STREAM(curve0_m0);
  EXTRACT_STREAM(curve0_m1);
  EXTRACT_STREAM(curve0_q_x);
  EXTRACT_STREAM(curve0_q_y);
  EXTRACT_STREAM(curve0_quad_coeff);

  EXTRACT_STREAM(curve1_m0);
  EXTRACT_STREAM(curve1_m1);
  EXTRACT_STREAM(curve1_q_x);
  EXTRACT_STREAM(curve1_q_y);
  EXTRACT_STREAM(curve1_quad_coeff);

  str << "\n";

  m_value = str.str();
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
  d = reinterpret_cast<GlyphAtlasGLParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew GlyphAtlasGLParamsPrivate(*d);
}

fastuidraw::gl::GlyphAtlasGL::params::
~params()
{
  GlyphAtlasGLParamsPrivate *d;
  d = reinterpret_cast<GlyphAtlasGLParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::params::
operator=(const params &rhs)
{
  if(this != &rhs)
    {
      GlyphAtlasGLParamsPrivate *d, *rhs_d;
      d = reinterpret_cast<GlyphAtlasGLParamsPrivate*>(m_d);
      rhs_d = reinterpret_cast<GlyphAtlasGLParamsPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define paramsSetGet(type, name)                                \
  fastuidraw::gl::GlyphAtlasGL::params&                          \
  fastuidraw::gl::GlyphAtlasGL::params::                         \
  name(type v)                                                  \
  {                                                             \
    GlyphAtlasGLParamsPrivate *d;                               \
    d = reinterpret_cast<GlyphAtlasGLParamsPrivate*>(m_d);      \
    d->m_##name = v;                                            \
    return *this;                                               \
  }                                                             \
                                                                \
  type                                                          \
  fastuidraw::gl::GlyphAtlasGL::params::                         \
  name(void) const                                              \
  {                                                             \
    GlyphAtlasGLParamsPrivate *d;                               \
    d = reinterpret_cast<GlyphAtlasGLParamsPrivate*>(m_d);      \
    return d->m_##name;                                         \
  }

paramsSetGet(fastuidraw::ivec3, texel_store_dimensions)
paramsSetGet(unsigned int, number_floats)
paramsSetGet(bool, delayed)
paramsSetGet(unsigned int, alignment)


#undef paramsSetGet

//////////////////////////////////////////////////////////////////
// fastuidraw::gl::GlyphAtlasGL methods
fastuidraw::gl::GlyphAtlasGL::
GlyphAtlasGL(const params &P):
  GlyphAtlas(TexelStoreGL::create(P.texel_store_dimensions(), P.delayed()),
             GeometryStoreGL::create(P.number_floats(), P.delayed(), P.alignment()))
{
  m_d = FASTUIDRAWnew GlyphAtlasGLPrivate(P);
}

fastuidraw::gl::GlyphAtlasGL::
~GlyphAtlasGL()
{
  GlyphAtlasGLPrivate *d;
  d = reinterpret_cast<GlyphAtlasGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::gl::GlyphAtlasGL::params&
fastuidraw::gl::GlyphAtlasGL::
param_values(void) const
{
  GlyphAtlasGLPrivate *d;
  d = reinterpret_cast<GlyphAtlasGLPrivate*>(m_d);
  return d->m_params;
}

GLuint
fastuidraw::gl::GlyphAtlasGL::
texel_texture(bool as_integer) const
{
  flush();

  const TexelStoreGL *p;
  p = dynamic_cast<const TexelStoreGL*>(texel_store().get());
  assert(p);
  return p->texture(as_integer);
}

GLuint
fastuidraw::gl::GlyphAtlasGL::
geometry_texture(void) const
{
  flush();

  const GeometryStoreGL *p;
  p = dynamic_cast<const GeometryStoreGL*>(geometry_store().get());
  assert(p);
  return p->texture();
}

fastuidraw::gl::Shader::shader_source
fastuidraw::gl::GlyphAtlasGL::
glsl_curvepair_compute_pseudo_distance(const char *function_name,
                                       const char *geometry_store_name,
                                       bool derivative_function)
{
  return glsl_curvepair_compute_pseudo_distance(geometry_store()->alignment(),
                                                function_name, geometry_store_name,
                                                derivative_function);
}

fastuidraw::gl::Shader::shader_source
fastuidraw::gl::GlyphAtlasGL::
glsl_curvepair_compute_pseudo_distance(unsigned int alignment,
                                       const char *function_name,
                                       const char *geometry_store_name,
                                       bool derivative_function)
{
  Shader::shader_source return_value;

  /* aww, this sucks; we can choose one of 4 options:
       1. dedicated shader for all variations.
       2. a loader function (but we need to prefix its name)
          along with the global values
       3. break function into sections:
          - function declaration and local variables
          - loader code
          - computation and closing code
       4. Define the loading macro in C++ and make sure the
          variable names used match between this C++ code
          and the GLSL shader code
     We choose option 4; which sucks for reading the shader code.
   */

  return_value
    .add_macro("FASTUIDRAW_CURVEPAIR_COMPUTE_NAME", function_name)
    .add_source(LoaderMacro(alignment, geometry_store_name).value(), Shader::from_string);

  if(derivative_function)
    {
      return_value
        .add_source("fastuidraw_curvepair_glyph_derivative.frag.glsl.resource_string", Shader::from_resource);
    }
  else
    {
      return_value
        .add_source("fastuidraw_curvepair_glyph.frag.glsl.resource_string", Shader::from_resource);
    }

  return_value
    .add_source("#undef FASTUIDRAW_LOAD_CURVE_GEOMETRY\n", Shader::from_string)
    .remove_macro("FASTUIDRAW_CURVEPAIR_COMPUTE_NAME");

  return return_value;

}
