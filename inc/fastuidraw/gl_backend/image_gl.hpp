/*!
 * \file image_gl.hpp
 * \brief file image_gl.hpp
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

#include <fastuidraw/image.hpp>
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
   * An ImageAtlasGL is the GL(and GLES) backend implementation
   * for \ref ImageAtlas.
   *
   * An ImageAtlasGL on creation, creates an
   * \ref AtlasColorBackingStoreBase and an \ref AtlasIndexBackingStoreBase
   * itself that are backed by GL textures. Both of the backing
   * stores use GL_TEXTURE_2D_ARRAY textures. On deletion,
   * deletes the backing color and index stores.
   *
   * The method flush() must be called with a GL context current.
   */
  class ImageAtlasGL:public ImageAtlas
  {
  public:
    /*!
     * \brief
     * Class to hold the construction parameters for creating
     * a ImageAtlasGL.
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
       * The log2 of the width and height of the color tile
       * size, initial value is 5
       */
      int
      log2_color_tile_size(void) const;

      /*!
       * Set the value for log2_color_tile_size(void) const
       */
      params&
      log2_color_tile_size(int v);

      /*!
       * The log2 of the number of color tiles across and
       * down per layer, initial value is 8. Effective
       * value is clamped to 8.
       */
      int
      log2_num_color_tiles_per_row_per_col(void) const;

      /*!
       * Set the value for log2_num_color_tiles_per_row_per_col(void) const
       */
      params&
      log2_num_color_tiles_per_row_per_col(int v);

      /*!
       * Sets log2_color_tile_size() and log2_num_color_tiles_per_row_per_col()
       * to a size that is optimal for the GL implementation given a value
       * for log2_color_tile_size().
       */
      params&
      optimal_color_sizes(int log2_color_tile_size);

      /*!
       * The initial number of color layers, initial value is 1
       */
      int
      num_color_layers(void) const;

      /*!
       * Set the value for num_color_layers(void) const
       */
      params&
      num_color_layers(int v);

      /*!
       * The log2 of the width and height of the index tile
       * size, initial value is 2
       */
      int
      log2_index_tile_size(void) const;

      /*!
       * Set the value for log2_index_tile_size(void) const
       */
      params&
      log2_index_tile_size(int v);

      /*!
       * The log2 of the number of index tiles across and
       * down per layer, initial value is 6
       */
      int
      log2_num_index_tiles_per_row_per_col(void) const;

      /*!
       * Set the value for log2_num_index_tiles_per_row_per_col(void) const
       */
      params&
      log2_num_index_tiles_per_row_per_col(int v);

      /*!
       * The initial number of index layers, initial value is 4
       */
      int
      num_index_layers(void) const;

      /*!
       * Set the value for num_index_layers(void) const
       */
      params&
      num_index_layers(int v);

    private:
      void *m_d;
    };

    /*!
     * A TextureImage is an Image that is backed by a GL texture.
     * Creating a TextureImage requires that a GL context is
     * current. If the GL context supports bindless (i.e one
     * of the GL_ARB_bindless_texture or GL_NV_bindless_texture
     * is present), then the created TextureImage will have that
     * Image::type() is Image::bindless_texture2d, otherwise it
     * will be Image::context_texture2d.
     */
    class TextureImage:public Image
    {
    public:
      /*!
       * Create a TextureImage from a previously created GL texture
       * whose binding target is GL_TEXTURE_2D.
       * \param patlas the ImageAtlas that the created image is part of
       * \param w width of the texture
       * \param h height of the texture
       * \param m number of mipmap levels of the texture
       * \param texture GL texture name
       * \param object_owns_texture the created TextureImage will own the
       *                            GL texture and will delete the GL texture
       *                            when the returned TextureImage is deleted.
       *                            If false, the GL texture must be deleted
       *                            by the caller AFTER the TextureImage is
       *                            deleted.
       * \param fmt format of the RGBA of the texture
       * \param allow_bindless if both this is true and the GL/GLES implementation
       *                       supports bindless texturing, return an object whose
       *                       \ref type() returns \ref bindless_texture2d.
       */
      static
      reference_counted_ptr<TextureImage>
      create(const reference_counted_ptr<ImageAtlas> &patlas,
             int w, int h, unsigned int m, GLuint texture,
             bool object_owns_texture,
             enum format_t fmt = rgba_format,
             bool allow_bindless = true);
      /*!
       * Create a GL texture and use it to back a TextureImage; the
       * created TextureImage will own the GL texture.
       * \param patlas the ImageAtlas that the created image is part of
       * \param w width of the texture
       * \param h height of the texture
       * \param m number of mipmap levels of the texture
       * \param tex_magnification magnification filter to get the texture
       * \param tex_minification minification filter to get the texture
       * \param fmt format of the RGBA of the texture
       * \param allow_bindless if both this is true and the GL/GLES implementation
       *                       supports bindless texturing, return an object whose
       *                       \ref type() returns \ref bindless_texture2d.
       */
      static
      reference_counted_ptr<TextureImage>
      create(const reference_counted_ptr<ImageAtlas> &patlas,
             int w, int h, unsigned int m,
             GLenum tex_magnification,
             GLenum tex_minification,
             enum format_t fmt = rgba_format,
             bool allow_bindless = true);
      /*!
       * Create a GL texture with no mipmapping and use it to back
       * a TextureImage; the created TextureImage will own the GL
       * texture. Equivalent to
       * \code
       * create(patlas, w, h, 1, filter, filter, fmt);
       * \endcode
       * \param patlas the ImageAtlas that the created image is part of
       * \param w width of the texture
       * \param h height of the texture
       * \param filter magnification and minification filter to give
       *               the texture
       * \param fmt format of the RGBA of the texture
       * \param allow_bindless if both this is true and the GL/GLES implementation
       *                       supports bindless texturing, return an object whose
       *                       \ref type() returns \ref bindless_texture2d.
       */
      static
      reference_counted_ptr<TextureImage>
      create(const reference_counted_ptr<ImageAtlas> &patlas,
             int w, int h, GLenum filter,
             enum format_t fmt = rgba_format,
             bool allow_bindless = true)
      {
        return create(patlas, w, h, 1, filter, filter, fmt, allow_bindless);
      }

      ~TextureImage();

      /*!
       * Returns the GL texture backing the TextureImage.
       * The texture binding target is always GL_TEXTURE_2D.
       * One can modify the -contents- of the texture via
       * glGetTexParameter familiy of functions or the
       * contents of the backing store via glTexSubImage2D,
       * but one should never change its backing store
       * (via glTexImage2D) or delete it (via glDeleteTextures).
       * Lastly, recall that \ref Painter works by generating
       * index and draw buffers that are sent to the GL/GLES
       * API at Painter::end(), thus if one wants to modify
       * the texture within a Painter::begin() / Painter::end()
       * pair, one must modify it from a PainterDrawBreakAction
       * so that the texture is consumed by the gfx API
       * before it is modified.
       */
      GLuint
      texture(void) const;

    private:
      TextureImage(const reference_counted_ptr<ImageAtlas> &patlas,
                   int w, int h, unsigned int m,
                   bool object_owns_texture, GLuint texture,
                   enum format_t fmt);
      TextureImage(const reference_counted_ptr<ImageAtlas> &patlas,
                   int w, int h, unsigned int m,
                   bool object_owns_texture, GLuint texture, GLuint64 handle,
                   enum format_t fmt);

      void *m_d;
    };

    /*!
     * Ctor.
     * \param P parameters for ImageAtlasGL
     */
    explicit
    ImageAtlasGL(const params &P);

    ~ImageAtlasGL(void);

    /*!
     * Returns the GL texture ID of the AtlasColorBackingStoreBase
     * derived object used by this ImageAtlasGL. A GL context must
     * be current (and that GL context is the context to which the
     * texture will belong).
     */
    GLuint
    color_texture(void) const;

    /*!
     * Returns the GL texture ID of the AtlasIndexBackingStoreBase
     * derived object used by this ImageAtlasGL. A GL context must be
     * current (and that GL context is the context to which the texture
     * will belong).
     */
    GLuint
    index_texture(void) const;

    /*!
     * Returns the params value used to construct
     * the ImageAtlasGL.
     */
    const params&
    param_values(void) const;

  private:
    void *m_d;
  };
/*! @} */

} //namespace gl
} //namespace fastuidraw
