/*!
 * \file colorstop_atlas_gl.hpp
 * \brief file colorstop_atlas_gl.hpp
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

#include <fastuidraw/colorstop_atlas.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace fastuidraw
{
namespace gl
{
/*!\addtogroup GLBackend
  @{
 */

  /*!
    \brief
    An ColorStopAtlasGL is the GL(and GLES) backend implementation
    for \ref ColorStopAtlas.

    A ColorStopAtlasGL uses a GL texture for the underlying
    store. In GL, the texture type is GL_TEXTURE_1D_ARRAY,
    in GLES it is GL_TEXTURE_2D_ARRAY (because GLES does not support
    1D texture).

    The method flush() must be called with a GL context current.
    If the ColorStopAtlasGL was constructed as delayed,
    then the loading of data to the GL textures is delayed until
    flush, otherwise it is done immediately and then must be done
    with a GL context current.
   */
  class ColorStopAtlasGL:public ColorStopAtlas
  {
  public:
    /*!
      \brief
      Class to hold the construction parameters for creating
      a ColorStopAtlasGL.
     */
    class params
    {
    public:
      /*!
        Ctor.
       */
      params(void);

      /*!
        Copy ctor.
        \param obj value from which to copy
       */
      params(const params &obj);

      ~params();

      /*!
        Assignment operator.
        \param rhs value from which to copy
       */
      params&
      operator=(const params &rhs);

      /*!
        width of underlying 1D-texture array, initial
        value is 1024
       */
      int
      width(void) const;

      /*!
        Set the value for width(void) const
       */
      params&
      width(int v);

      /*!
        number of layers of underling 1D texture, initial
        value is 32
       */
      int
      num_layers(void) const;

      /*!
        Set the value for num_layers(void) const
       */
      params&
      num_layers(int v);

      /*!
        Query the current GL context and set the value for
        width() const to GL_MAX_TEXTURE_SIZE.
       */
      params&
      optimal_width(void);

      /*!
        if true, upload of texture data is delayed until
        ColorStopAtlasGL::flush() is called, initial
        value is false.
       */
      bool
      delayed(void) const;

      /*!
        Set the value for delayed(void) const
       */
      params&
      delayed(bool v);

    private:
      void *m_d;
    };

    /*!
      Ctor.
      \param P parameters of construction.
     */
    explicit
    ColorStopAtlasGL(const params &P);

    ~ColorStopAtlasGL();

    /*!
      Returns the underlying GL texture ID of the
      texture of the backing store.
     */
    GLuint
    texture(void) const;

    /*!
      Returns the params value used to construct
      the ColorStopAtlasGL.
     */
    const params&
    param_values(void);

    /*!
      Returns the texture bind target of the underlying texture;
      for GLES this is GL_TEXTURE_2D_ARRAY, for GL this is
      GL_TEXTURE_1D_ARRAY
     */
    static
    GLenum
    texture_bind_target(void);

  private:
    void *m_d;
  };
/*! @} */

}

}
