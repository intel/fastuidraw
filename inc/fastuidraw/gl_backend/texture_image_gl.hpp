/*!
 * \file texture_image_gl.hpp
 * \brief file texture_image_gl.hpp
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


#ifndef FASTUIDRAW_TEXTURE_IMAGE_GL_HPP
#define FASTUIDRAW_TEXTURE_IMAGE_GL_HPP

#include <fastuidraw/image.hpp>
#include <fastuidraw/image_atlas.hpp>
#include <fastuidraw/gl_backend/gl_header.hpp>

namespace fastuidraw
{
namespace gl
{
/*!\addtogroup GLBackend
 * @{
 */

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
    create(ImageAtlas &patlas, int w, int h, unsigned int m,
           GLuint texture, bool object_owns_texture,
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
    create(ImageAtlas &patlas, int w, int h, unsigned int m,
           GLenum tex_magnification, GLenum tex_minification,
           enum format_t fmt = rgba_format,
           bool allow_bindless = true);

    /*!
     * Create a GL texture and use it to back a TextureImage; the
     * created TextureImage will own the GL texture.
     * \param w width of the image to create
     * \param h height of the image to create
     * \param patlas the ImageAtlas that the created image is part of
     * \param image_data image data to which to initialize the image
     * \param tex_magnification magnification filter to get the texture
     * \param tex_minification minification filter to get the texture
     * \param allow_bindless if both this is true and the GL/GLES implementation
     *                       supports bindless texturing, return an object whose
     *                       \ref type() returns \ref bindless_texture2d.
     */
    static
    reference_counted_ptr<TextureImage>
    create(ImageAtlas &patlas, int w, int h,
           const ImageSourceBase &image_data,
           GLenum tex_magnification, GLenum tex_minification,
           bool allow_bindless = true);

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
    TextureImage(ImageAtlas &patlas, int w, int h, unsigned int m,
                 bool object_owns_texture, GLuint texture,
                 enum format_t fmt);
    TextureImage(ImageAtlas &patlas, int w, int h, unsigned int m,
                 bool object_owns_texture, GLuint texture, GLuint64 handle,
                 enum format_t fmt);

    void *m_d;
  };
/*! @} */

} //namespace gl
} //namespace fastuidraw

#endif
