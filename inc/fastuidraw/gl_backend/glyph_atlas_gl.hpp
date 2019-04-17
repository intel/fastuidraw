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
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
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
   * GlyphAtlasBackingStoreBase.
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
       * Number floats that can be held in the data
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
       * the glyph data. Default value is
       * \ref glsl::PainterShaderRegistrarGLSL::glyph_data_tbo.
       */
      enum glsl::PainterShaderRegistrarGLSL::glyph_data_backing_t
      glyph_data_backing_store_type(void) const;

      /*!
       * Set glyph_data_backing_store() to \ref
       * glsl::PainterShaderRegistrarGLSL::glyph_data_tbo,
       * i.e. for the glyph data to be stored
       * on a GL texture buffer object.
       */
      params&
      use_texture_buffer_store(void);

      /*!
       * Set glyph_data_backing_store() to \ref
       * glsl::PainterShaderRegistrarGLSL::glyph_data_ssbo,
       * i.e. for the glyph data to be stored
       * on a GL texture buffer object.
       */
      params&
      use_storage_buffer_store(void);

      /*!
       * Set glyph_data_backing_store() to \ref
       * glsl::PainterShaderRegistrarGLSL::glyph_data_texture_array,
       * i.e. to use a 2D texture array to store the
       * glyph data. The depth of the
       * array is set implicitely by the size given by
       * GlyphAtlasBackingStoreBase::size().
       * NOTE: if either parameter is made negative, the
       * call is ignored.
       * \param log2_width Log2 of the width of the 2D texture array
       * \param log2_height Log2 of the height of the 2D texture array
       */
      params&
      use_texture_2d_array_store(int log2_width, int log2_height);

      /*!
       * Set glyph_data_backing_store() to \ref
       * glsl::PainterShaderRegistrarGLSL::glyph_data_texture_array,
       * i.e. to use a 2D texture array to store the
       * glyph data. The depth of the
       * array is set implicitely by the size given by
       * GlyphAtlasBackingStoreBase::size(). The width and height
       * of the texture will be selected from examining the context
       * properties.
       */
      params&
      use_texture_2d_array_store(void);

      /*!
       * If glyph_data_backing_store() returns \ref
       * glsl::PainterShaderRegistrarGLSL::glyph_data_texture_array,
       * returns the values
       * set in use_texture_2d_array_store(), otherwise
       * returns a value where both components are -1.
       */
      ivec2
      texture_2d_array_store_log2_dims(void) const;

      /*!
       * Query the GL context to decide what is the optimal settings
       * to back the GlyphAtlasBackingStoreBase returned by
       * GlyphAtlas::store(). A GL context must be current
       * so that GL capabilities may be queried.
       */
      params&
      use_optimal_store_backing(void);

    private:
      void *m_d;
    };

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
    GlyphAtlasGL(const params &P);

    ~GlyphAtlasGL();

    /*!
     * Returns true if and only if the binding point of the
     * GlyphAtlasBackingStoreBase is a texture unit.
     */
    bool
    data_binding_point_is_texture_unit(void) const;

    /*!
     * Returns the GL object ID of the GlyphAtlasBackingStoreBase
     * derived object used by this GlyphAtlasGL. If the
     * GlyphAtlasGL was constructed as delayed, then the first time
     * data_texture() is called, a GL context must be current (and that
     * GL context is the context to which the texture will belong).
     * If backed by a texture, returns the name of a texture. If backed
     * by a buffer returns the name of a GL buffer object.
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
