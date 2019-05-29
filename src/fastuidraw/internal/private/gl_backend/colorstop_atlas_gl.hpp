/*!
 * \file colorstop_atlas_gl.hpp
 * \brief file colorstop_atlas_gl.hpp
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

#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>

namespace fastuidraw
{
namespace gl
{
namespace detail
{

  /*!
   * \brief
   * An ColorStopAtlasGL is the GL(and GLES) backend implementation
   * for \ref ColorStopAtlas.
   *
   * A ColorStopAtlasGL uses a GL texture for the underlying
   * store. In GL, the texture type is GL_TEXTURE_1D_ARRAY,
   * in GLES it is GL_TEXTURE_2D_ARRAY (because GLES does not
   * support 1D texture).
   *
   * The method flush() must be called with a GL context current.
   */
  class ColorStopAtlasGL:public ColorStopAtlas
  {
  public:
    /*!
     * Ctor.
     * \param P parameters of construction.
     */
    explicit
    ColorStopAtlasGL(const PainterEngineGL::ColorStopAtlasParams &P);

    ~ColorStopAtlasGL();

    /*!
     * Returns the GL texture ID of the backing texture.
     * A GL context must be current (and that GL context
     * is the context to which the texture will belong).
     */
    GLuint
    texture(void) const;

    /*!
     * Returns the texture bind target of the underlying texture;
     * for GLES this is GL_TEXTURE_2D_ARRAY, for GL this is
     * GL_TEXTURE_1D_ARRAY
     */
    static
    GLenum
    texture_bind_target(void);
  };

} //namespace detail
} //namespace gl
} //namespace fastuidraw
