/*!
 * \file glyph_atlas_gl.hpp
 * \brief file glyph_atlas_gl.hpp
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

#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>
#include <fastuidraw/gl_backend/gl_header.hpp>

namespace fastuidraw
{
namespace gl
{
namespace detail
{

  /*!
   * \brief
   * A GlyphAtlasGL is the GL(and GLES) backend implementation
   * for \ref GlyphAtlas.
   *
   * A GlyphAtlasGL on creation, creates an object derived from \ref
   * GlyphAtlasBackingStoreBase.
   *
   * The method flush() must be called with a GL context current.
   */
  class GlyphAtlasGL:public GlyphAtlas
  {
  public:
    /*!
     * Format enumeration to specify format to
     * view the data backing as
     */
    enum backing_fmt_t
      {
        /*!
         * Specifies to view the data as an array
         * of uint32_t values (i.e. GL_R32UI).
         */
        backing_uint32_fmt,

        /*!
         * Specifes to view the data as an array
         * of fp16-x2 values (i.e. GL_RG16F).
         */
        backing_fp16x2_fmt,
      };

    /*!
     * Ctor.
     * \param P parameters for constrution
     */
    explicit
    GlyphAtlasGL(const PainterEngineGL::GlyphAtlasParams &P);

    ~GlyphAtlasGL();

    /*!
     * Returns true if and only if the binding point of the
     * GlyphAtlasBackingStoreBase is a texture unit.
     */
    bool
    data_binding_point_is_texture_unit(void) const;

    /*!
     * Returns the GL object ID of the GlyphAtlasBackingStoreBase
     * derived object used by this GlyphAtlasGL. The first time
     * data_texture() is called, a GL context must be current (and that
     * GL context is the context to which the texture or buffer will
     * belong). If backed by a texture, returns the name of a texture.
     * If backed by a buffer returns the name of a GL buffer object.
     */
    GLuint
    data_backing(enum backing_fmt_t fmt) const;

    /*!
     * Returns the binding point to which to bind the objected returned
     * by data_backing().
     */
    GLenum
    data_binding_point(void) const;

    /*!
     * In the case that the data is stored in a texture
     * array (GL_TEXTURE_2D_ARRAY), returns the log2 of
     * the width and height of the backing texture 2D
     * array.
     */
    ivec2
    data_texture_as_2d_array_log2_dims(void) const;
  };
/*! @} */

} //namespace detail
} //namespace gl
} //namespace fastuidraw
