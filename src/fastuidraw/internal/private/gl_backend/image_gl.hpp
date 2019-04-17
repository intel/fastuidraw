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
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>


namespace fastuidraw
{
namespace gl
{
namespace detail
{

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
     * Ctor.
     * \param P parameters for ImageAtlasGL
     */
    explicit
    ImageAtlasGL(const PainterEngineGL::ImageAtlasParams &P);

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

  private:
    virtual
    reference_counted_ptr<Image>
    create_image_bindless(int w, int h, const ImageSourceBase &image_data);

    virtual
    reference_counted_ptr<Image>
    create_image_context_texture2d(int w, int h, const ImageSourceBase &image_data);
  };
/*! @} */

} //namespace detail
} //namespace gl
} //namespace fastuidraw
