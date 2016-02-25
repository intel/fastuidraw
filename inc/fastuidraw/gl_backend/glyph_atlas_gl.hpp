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
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace fastuidraw
{
namespace gl
{
/*!\addtogroup GLBackend
  @{
 */

  /*!
    A GlyphAtlasGL on creation, creates a GlyphAtlasTexelBackingStoreBase
    (backed by a GL_TEXTURE_2D_ARRAY) and a GlyphAtlasGeometryBackingStoreBase
    (backed by a buffer object linked to a textuer via GL_TEXTURE_BUFFER).
    On deletion the GL backing stores are deleted.

    The method flush() must be called with a GL context current.
    If the GlyphAtlasGL was constructed delayed, then the loading
    of data to the GL texture and buffer object are delayed until
    flush() is called, otherwise it is done immediately and then
    must be done with a GL context current.
   */
  class GlyphAtlasGL:public GlyphAtlas
  {
  public:
    /*!
      Overload typedef for handle
    */
    typedef reference_counted_ptr<GlyphAtlasGL> handle;

    /*!
      Overload typedef for const_handle
    */
    typedef reference_counted_ptr<const GlyphAtlasGL> const_handle;

    /*!
      Class to hold the construction parameters for creating
      a GlyphAtlasGL.
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
        \param obj value from which to copy
       */
      params&
      operator=(const params &obj);

      /*!
        Initial dimension for the texel backing store,
        initial value is (1024, 1024, 16)
       */
      ivec3
      texel_store_dimensions(void) const;

      /*!
        Set the value for texel_store_dimensions(void) const
       */
      params&
      texel_store_dimensions(ivec3 v);

      /*!
        Number floats that can be held in the geometry data
        backing store, initial value is 1024 * 1024
       */
      unsigned int
      number_floats(void) const;

      /*!
        Set the value for number_floats(void) const
       */
      params&
      number_floats(unsigned int v);

      /*!
        if true, creation of GL objects and uploading of data
        to GL objects is performed when flush() is called.
        If false, creation of GL objects is at construction
        and upload of data to GL is done immediately,
        initial value is false.
       */
      bool
      delayed(void) const;

      /*!
        Set the value for delayed(void) const
       */
      params&
      delayed(bool v);

      /*!
        The alignment for the
        GlyphAtlasGeometryBackingStoreBase of the
        constructed GlyphAtlasGL. The value must be 1,
        2, 3 or 4. The value is the number of channels
        the underlying texture buffer has, initial value
        is 4.
       */
      unsigned int
      alignment(void) const;

      /*!
        Set the value for alignment(void) const
       */
      params&
      alignment(unsigned int v);

    private:
      void *m_d;
    };

    /*!
      Ctor.
      \param P parameters for constrution
     */
    GlyphAtlasGL(const params &P);

    ~GlyphAtlasGL();

    /*!
      Returns the GL texture ID of the GlyphAtlasTexelBackingStoreBase
      derived object used by this GlyphAtlasGL. If the
      GlyphAtlasGL was constructed as delayed, then the first time
      texel_texture() is called, a GL context must be current (and that
      GL context is the context to which the texture will belong).
      \param as_integer if true, returns a view to the texture whose internal
                        format is GL_R8UI. If false returns a view to the
                        texture whose internal format is GL_R8. NOTE:
                        if the GL implementation does not support the
                        glTextureView API (in ES via GL_OES_texture_view),
                        will return 0 if as_integer is false.
     */
    GLuint
    texel_texture(bool as_integer) const;

    /*!
      Returns the GL texture ID of the GlyphAtlasGeometryBackingStoreBase
      derived object used by this GlyphAtlasGL. If the
      GlyphAtlasGL was constructed as delayed, then the first time
      geometry_texture() is called, a GL context must be current (and that
      GL context is the context to which the texture will belong).
     */
    GLuint
    geometry_texture(void) const;

    /*!
      Returns the params value used to construct
      the GlyphAtlasGL.
     */
    const params&
    param_values(void) const;

    /*!
      Construct/returns a Shader::shader_source value that
      implements the two functions:
      \code
        float
        function_name(in int texel_value,
                      in vec2 texture_coordinate,
                      in int geometry_offset)
      \endcode

      which returns the signed pseudo-distance to the glyph boundary.
      The value texel_value is value in the texel store from the
      position texture_coordinate (the bottom left for the glyph being
      at Glyph::atlas_location().location() and the top right
      being at that value + Glyph::layout().m_texel_size.
      The value geometry_offset is from Glyph::geometry_offset().

      \param alignment alignment of the backing geometry store,
                       GlyphAtlasGeometryBackingStoreBase::alignment().
      \param function_name name for the function
      \param geometry_store_name the samplerBuffer backed by the texture
                                 ID returned by geometry_texture().
      \param derivative_function if true, give the GLSL function with the
                                 argument signature (in int, in vec2, in int, out vec2)
                                 where the last argument is the gradient of
                                 the function with repsect to texture_coordinate.
     */
    static
    Shader::shader_source
    glsl_curvepair_compute_pseudo_distance(unsigned int alignment,
                                           const char *function_name,
                                           const char *geometry_store_name,
                                           bool derivative_function = false);




    /*!
      Provided as a conveniance, equivalent to
      \code
      glsl_curvepair_compute_pseudo_distance(geometry_store()->alignment(),
                                             function_name, geometry_store_name)
      \endcode

      \param function_name name for the function
      \param geometry_store_name the samplerBuffer backed by the texture
                                 ID returned by geometry_texture().
      \param derivative_function if true, give the GLSL function with the
                                 argument signature (in int, in vec2, in int, out vec2)
                                 where the last argument is the gradient of
                                 the function with repsect to texture_coordinate.
     */
    Shader::shader_source
    glsl_curvepair_compute_pseudo_distance(const char *function_name,
                                           const char *geometry_store_name,
                                           bool derivative_function = false);
  private:
    void *m_d;
  };
/*! @} */

} //namespace gl
} //namespace fastuidraw
