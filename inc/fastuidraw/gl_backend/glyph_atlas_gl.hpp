/*!
 * \file glyph_atlas_gl.hpp
 * \brief file glyph_atlas_gl.hpp
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

#include <fastuidraw/text/glyph_atlas.hpp>
#include <fastuidraw/glsl/painter_backend_glsl.hpp>
#include <fastuidraw/gl_backend/gl_header.hpp>

namespace fastuidraw
{
namespace gl
{
/*!\addtogroup GLBackend
 * @{
 */

  /*!
   * \brief
   * A GlyphAtlasGL is the GL(and GLES) backend implementation
   * for \ref GlyphAtlas.
   *
   * A GlyphAtlasGL on creation, creates an object derived from \ref
   * GlyphAtlasTexelBackingStoreBase and an object derived from \ref
   * GlyphAtlasGeometryBackingStoreBase. The texels of the \ref
   * GlyphAtlasTexelBackingStoreBase derived object are backed by a
   * GL_TEXTURE_2D_ARRAY texture.
   *
   * The method flush() must be called with a GL context current.
   * If the GlyphAtlasGL was constructed delayed, then the loading
   * of data to the GL texture and buffer object are delayed until
   * flush() is called, otherwise it is done immediately and then
   * must be done with a GL context current.
   */
  class GlyphAtlasGL:public GlyphAtlas
  {
  public:
    /*!
     * \brief
     * Class to hold the construction parameters for creating
     * a GlyphAtlasGL.
     */
    class params
    {
    public:
      /*!
       * Ctor.
       */
      params(void);

      /*!
       * Copy ctor.
       * \param obj value from which to copy
       */
      params(const params &obj);

      ~params();

      /*!
       * Assignment operator.
       * \param obj value from which to copy
       */
      params&
      operator=(const params &obj);

      /*!
       * Swap operation
       * \param obj object with which to swap
       */
      void
      swap(params &obj);

      /*!
       * Initial dimension for the texel backing store,
       * initial value is (1024, 1024, 16)
       */
      ivec3
      texel_store_dimensions(void) const;

      /*!
       * Set the value for texel_store_dimensions(void) const
       */
      params&
      texel_store_dimensions(ivec3 v);

      /*!
       * Number floats that can be held in the geometry data
       * backing store, initial value is 1024 * 1024
       */
      unsigned int
      number_floats(void) const;

      /*!
       * Set the value for number_floats(void) const
       */
      params&
      number_floats(unsigned int v);

      /*!
       * if true, creation of GL objects and uploading of data
       * to GL objects is performed when flush() is called.
       * If false, creation of GL objects is at construction
       * and upload of data to GL is done immediately,
       * initial value is false.
       */
      bool
      delayed(void) const;

      /*!
       * Set the value for delayed(void) const
       */
      params&
      delayed(bool v);

      /*!
       * Returns what kind of GL object is used to back
       * the glyph geometry data. Default value is
       * \ref glsl::PainterBackendGLSL::glyph_geometry_tbo.
       */
      enum glsl::PainterBackendGLSL::glyph_geometry_backing_t
      glyph_geometry_backing_store_type(void) const;

      /*!
       * Set glyph_geometry_backing_store() to \ref
       * glsl::PainterBackendGLSL::glyph_geometry_tbo,
       * i.e. for the glyph geometry data to be stored
       * on a GL texture buffer object.
       */
      params&
      use_texture_buffer_geometry_store(void);

      /*!
       * Set glyph_geometry_backing_store() to \ref
       * glsl::PainterBackendGLSL::glyph_geometry_ssbo,
       * i.e. for the glyph geometry data to be stored
       * on a GL texture buffer object.
       */
      params&
      use_storage_buffer_geometry_store(void);

      /*!
       * Set glyph_geometry_backing_store() to \ref
       * glsl::PainterBackendGLSL::glyph_geometry_texture_array,
       * i.e. to use a 2D texture array to store the
       * glyph geometry data. The depth of the
       * array is set implicitely by the size given by
       * GlyphAtlasGeometryBackingStoreBase::size().
       * NOTE: if either parameter is made negative, the
       * call is ignored.
       * \param log2_width Log2 of the width of the 2D texture array
       * \param log2_height Log2 of the height of the 2D texture array
       */
      params&
      use_texture_2d_array_geometry_store(int log2_width,
                                          int log2_height = 0);

      /*!
       * If glyph_geometry_backing_store() returns \ref
       * glsl::PainterBackendGLSL::glyph_geometry_texture_array,
       * returns the values
       * set in use_texture_2d_array_geometry_store(), otherwise
       * returns a value where both components are -1.
       */
      ivec2
      texture_2d_array_geometry_store_log2_dims(void) const;

      /*!
       * Query the GL context to decide what is the optimal settings
       * to back the GlyphAtlasGeometryBackingStoreBase returned by
       * GlyphAtlas::geometry_store(). A GL context must be current
       * so that GL capabilities may be queried.
       */
      params&
      use_optimal_geometry_store_backing(void);

      /*!
       * The alignment for the
       * GlyphAtlasGeometryBackingStoreBase of the
       * constructed GlyphAtlasGL. The value must be 1,
       * 2, 3 or 4. The value is the number of channels
       * the underlying texture buffer has, initial value
       * is 4.
       */
      unsigned int
      alignment(void) const;

      /*!
       * Set the value for alignment(void) const
       */
      params&
      alignment(unsigned int v);

    private:
      void *m_d;
    };

    /*!
     * Ctor.
     * \param P parameters for constrution
     */
    GlyphAtlasGL(const params &P);

    ~GlyphAtlasGL();

    /*!
     * Returns the GL texture ID of the GlyphAtlasTexelBackingStoreBase
     * derived object used by this GlyphAtlasGL. If the
     * GlyphAtlasGL was constructed as delayed, then the first time
     * texel_texture() is called, a GL context must be current (and that
     * GL context is the context to which the texture will belong).
     * \param as_integer if true, returns a view to the texture whose internal
     *                   format is GL_R8UI. If false returns a view to the
     *                   texture whose internal format is GL_R8. NOTE:
     *                   if the GL implementation does not support the
     *                   glTextureView API (in ES via GL_OES_texture_view),
     *                   will return 0 if as_integer is false.
     */
    GLuint
    texel_texture(bool as_integer) const;

    /*!
     * Returns true if and only if the GlyphAtlasGeometryBackingStoreBase
     * is backed by a texture.
     */
    bool
    geometry_backed_by_texture(void) const;

    /*!
     * Returns the GL object ID of the GlyphAtlasGeometryBackingStoreBase
     * derived object used by this GlyphAtlasGL. If the
     * GlyphAtlasGL was constructed as delayed, then the first time
     * geometry_texture() is called, a GL context must be current (and that
     * GL context is the context to which the texture will belong).
     * If backed by a texture, returns the name of a texture. If backed
     * by a buffer returns the name of a GL buffer object.
     */
    GLuint
    geometry_backing(void) const;

    /*!
     * Returns the binding point to which to bind the texture returned
     * by geometry_texture().
     */
    GLenum
    geometry_binding_point(void) const;

    /*!
     * In the case that the geometry data is stored in a texture
     * array (GL_TEXTURE_2D_ARRAY) instead of a texture buffer
     * object, returns the log2 of the width and height of the
     * backing texture 2D array.
     */
    ivec2
    geometry_texture_as_2d_array_log2_dims(void) const;

    /*!
     * Returns the params value used to construct
     * the GlyphAtlasGL.
     */
    const params&
    param_values(void) const;

  private:
    void *m_d;
  };
/*! @} */

} //namespace gl
} //namespace fastuidraw
