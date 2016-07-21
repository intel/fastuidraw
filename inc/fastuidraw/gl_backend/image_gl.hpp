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
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace fastuidraw
{
namespace gl
{
/*!\addtogroup GLBackend
  @{
 */

  /*!
    An ImageAtlasGL on creation, creates an
    AtlasColorBackingStoreBase and an AtlasIndexBackingStoreBase
    itself that are backed by GL textures. Both of the backing
    stores use GL_TEXTURE_2D_ARRAY textures. On deletion,
    deletes the backing color and index stores.

    The method flush() must be called with a GL context current.
    If the ImageAtlasGL was constructed as delayed,
    then the loading of data to the GL textures is delayed until
    flush(), otherwise it is done immediately and then must be done
    with a GL context current.
   */
  class ImageAtlasGL:public ImageAtlas
  {
  public:
    /*!
      Class to hold the construction parameters for creating
      a ImageAtlasGL.
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
        The log2 of the width and height of the color tile
        size, initial value is 5
       */
      int
      log2_color_tile_size(void) const;

      /*!
        Set the value for log2_color_tile_size(void) const
       */
      params&
      log2_color_tile_size(int v);

      /*!
        The log2 of the number of color tiles across and
        down per layer, initial value is 8. Effective
	value is clamped to 8.
       */
      int
      log2_num_color_tiles_per_row_per_col(void) const;

      /*!
        Set the value for log2_num_color_tiles_per_row_per_col(void) const
       */
      params&
      log2_num_color_tiles_per_row_per_col(int v);

      /*!
	Sets log2_color_tile_size() and log2_num_color_tiles_per_row_per_col()
	to a size that is optimal for the GL implementation given a value
	for log2_color_tile_size().
       */
      params&
      optimal_color_sizes(int log2_color_tile_size);

      /*!
        The initial number of color layers, initial value is 1
       */
      int
      num_color_layers(void) const;

      /*!
        Set the value for num_color_layers(void) const
       */
      params&
      num_color_layers(int v);

      /*!
        The log2 of the width and height of the index tile
        size, initial value is 2
       */
      int
      log2_index_tile_size(void) const;

      /*!
        Set the value for log2_index_tile_size(void) const
       */
      params&
      log2_index_tile_size(int v);

      /*!
        The log2 of the number of index tiles across and
        down per layer, initial value is 6
       */
      int
      log2_num_index_tiles_per_row_per_col(void) const;

      /*!
        Set the value for log2_num_index_tiles_per_row_per_col(void) const
       */
      params&
      log2_num_index_tiles_per_row_per_col(int v);

      /*!
        The initial number of index layers, initial value is 4
       */
      int
      num_index_layers(void) const;

      /*!
        Set the value for num_index_layers(void) const
       */
      params&
      num_index_layers(int v);

      /*!
        if true, creation of GL objects and uploading of data
        to GL objects is performed when flush() is called.
        If false, creation of GL objects is at construction
        and upload of data to GL is done immediately, initial
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
      \param P parameters for ImageAtlasGL
    */
    explicit
    ImageAtlasGL(const params &P);

    ~ImageAtlasGL(void);

    /*!
      Returns the GL texture ID of the AtlasColorBackingStoreBase
      derived object used by this ImageAtlasGL. If the
      ImageAtlasGL was constructed as delayed, then the first time
      color_texture() is called, a GL context must be current (and
      that GL context is the context to which the texture will belong).
     */
    GLuint
    color_texture(void) const;

    /*!
      Returns the GL texture ID of the AtlasIndexBackingStoreBase
      derived object used by this ImageAtlasGL. If the
      ImageAtlasGL was constructed as delayed, then the first time
      index_texture() is called, a GL context must be current (and
      that GL context is the context to which the texture will belong).
     */
    GLuint
    index_texture(void) const;

    /*!
      Returns the params value used to construct
      the ImageAtlasGL.
     */
    const params&
    param_values(void) const;

    /*!
      Returns the coordinates to use for the corners
      of drawing an image that are fed as the 1st
      argument to the GLSL function defined by
      glsl_compute_coord_src().
      \param image from which coordinates are requested
     */
    static
    vecN<vec2, 2>
    shader_coords(reference_counted_ptr<Image> image);

    /*!
      Gives the shader source code for a function with
      the signature:
      \code
      void
      function(in vec2 punnormalized_index_tex_coord,
               in int pindex_layer,
               in int pnum_levels,
               in int tile_slack,
               out vec2 return_value_unnormalized_texcoord_xy,
               out int return_value_layer)
      \endcode

      which computes the unnormalized texture coordinate and
      layer into the color atlas.

      \param function_name name to give the function
      \param index_texture name to give the index texture atlas
     */
    Shader::shader_source
    glsl_compute_coord_src(const char *function_name,
                           const char *index_texture) const;

  private:
    void *m_d;
  };
/*! @} */

} //namespace gl
} //namespace fastuidraw
