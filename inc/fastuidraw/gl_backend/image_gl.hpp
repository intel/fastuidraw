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
   * If the ImageAtlasGL was constructed as delayed,
   * then the loading of data to the GL textures is delayed until
   * flush(), otherwise it is done immediately and then must be done
   * with a GL context current.
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

      /*!
       * if true, creation of GL objects and uploading of data
       * to GL objects is performed when flush() is called.
       * If false, creation of GL objects is at construction
       * and upload of data to GL is done immediately, initial
       * value is false.
       */
      bool
      delayed(void) const;

      /*!
       * Set the value for delayed(void) const
       */
      params&
      delayed(bool v);

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
       * \param w width of the texture
       * \param h height of the texture
       * \param m number of mipmap levels of the texture
       * \param object_owns_texture the created TextureImage will own the
       *                            GL texture and will delete the GL texture
       *                            when the returned TextureImage is deleted.
       *                            If false, the GL texture must be deleted
       *                            by the caller AFTER the TextureImage is
       *                            deleted.
       */
      static
      reference_counted_ptr<TextureImage>
      create(int w, int h, unsigned int m, GLuint texture,
             bool object_owns_texture = true);

      /*!
       * Create a TextureImage from image data.
       * \param w width of the image
       * \param h height of the image
       * \param m number of mipmap levels for the texture to have
       * \param image image data to copy to the texture
       * \param min_filter value to pass to GL for the minification filter
       * \param mag_filter value to pass to GL for the magnification filter
       * \param object_owns_texture the created TextureImage will own the
       *                            GL texture and will delete the GL texture
       *                            when the returned TextureImage is deleted.
       *                            If false, the GL texture must be deleted
       *                            by the caller AFTER the TextureImage is
       *                            deleted.
       */
      static
      reference_counted_ptr<TextureImage>
      create(int w, int h, unsigned int m, const ImageSourceBase &image,
             GLenum min_filter = GL_LINEAR_MIPMAP_LINEAR,
             GLenum mag_filter = GL_LINEAR,
             bool object_owns_texture = true);

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
       * pair, one must modify it from a PainterDraw::Action
       * so that the texture is consumed by the gfx API
       * before it is modified.
       */
      GLuint
      texture(void) const;

    private:
      TextureImage(int w, int h, unsigned int m,
                   bool object_owns_texture, GLuint texture);
      TextureImage(int w, int h, unsigned int m,
                   bool object_owns_texture, GLuint texture, GLuint64 handle);

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
     * derived object used by this ImageAtlasGL. If the
     * ImageAtlasGL was constructed as delayed, then the first time
     * color_texture() is called, a GL context must be current (and
     * that GL context is the context to which the texture will belong).
     */
    GLuint
    color_texture(void) const;

    /*!
     * Returns the GL texture ID of the AtlasIndexBackingStoreBase
     * derived object used by this ImageAtlasGL. If the
     * ImageAtlasGL was constructed as delayed, then the first time
     * index_texture() is called, a GL context must be current (and
     * that GL context is the context to which the texture will belong).
     */
    GLuint
    index_texture(void) const;

    /*!
     * Returns the params value used to construct
     * the ImageAtlasGL.
     */
    const params&
    param_values(void) const;

    /*!
     * Create an Image that is bindless that is backed directly by
     * a texture. The returned Image object will own the GL texture;
     * i.e. the texture is deleted when the Image is. Will return a
     * nullptr if the current GL-context does not support bindless
     * texturing.
     * \param w width of the image
     * \param h height of the image
     * \param m number of mipmap levels for the texture to have
     * \param image image data to copy to the texture
     * \param min_filter value to pass to GL for the minification filter
     * \param mag_filter value to pass to GL for the magnification filter
     * \param tex if non-NULL write the name of the texture to tex;
     *            do NOT delete the texture or reallocate its backing
     *            store (i.e. glTexImage2D or glTexStorage2D). However,
     *            one can change texture parameters (via glTexParameter)
     *            or image data (via glTexSubImage2D) freely.
     */
    static
    reference_counted_ptr<TextureImage>
    create_bindless(int w, int h, unsigned int m, const ImageSourceBase &image,
                    GLenum min_filter = GL_LINEAR_MIPMAP_LINEAR,
                    GLenum mag_filter = GL_LINEAR,
                    GLuint *tex = nullptr);

  private:
    void *m_d;
  };
/*! @} */

} //namespace gl
} //namespace fastuidraw
