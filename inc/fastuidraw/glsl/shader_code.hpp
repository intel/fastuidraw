/*!
 * \file shader_code.hpp
 * \brief file shader_code.hpp
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

#include <fastuidraw/glsl/shader_source.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSLShaderBuilder
  @{
 */
    /*!
      \brief Namespace to encapsulate GLSL shader source
             code used in rendering, part of the main
             library libFastUIDraw
     */
    namespace code
    {
      /*!
        Construct/returns a ShaderSource value that
        implements the function:
        \code
        float
        function_name(in int texel_value,
                      in vec2 texture_coordinate,
                      in int geometry_offset)
        \endcode

        which returns the signed pseudo-distance to the glyph boundary
        for a glyph of type \ref curve_pair_glyph (these glyphs are
        backed by data produced from a \ref GlyphRenderDataCurvePair).
        The value texel_value is value in the texel store from the
        position texture_coordinate (the bottom left for the glyph being
        at Glyph::atlas_location().location() and the top right
        being at that value + Glyph::layout().m_texel_size.
        The value geometry_offset is from Glyph::geometry_offset().

        \param alignment alignment of the backing geometry store,
                         GlyphAtlasGeometryBackingStoreBase::alignment().
        \param function_name name for the function
        \param geometry_store_fetch the macro function (that returns a vec4)
                                    to use in the produced GLSL code to fetch
                                    the geometry store data.
        \param derivative_function if true, give the GLSL function with the
                                   argument signature (in int, in vec2, in int, out vec2)
                                   where the last argument is the gradient of
                                   the function with repsect to texture_coordinate.
       */
      ShaderSource
      curvepair_compute_pseudo_distance(unsigned int alignment,
                                        const char *function_name,
                                        const char *geometry_store_fetch,
                                        bool derivative_function = false);
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
        \param index_tile_size the size of each index tile (see ImageAtlas::index_tile_size())
        \param color_tile_size the size of each color tile (see ImageAtlas::color_tile_size())
       */
      ShaderSource
      image_atlas_compute_coord(const char *function_name,
                                const char *index_texture,
                                unsigned int index_tile_size,
                                unsigned int color_tile_size);

      /*!
        Construct/returns a ShaderSource value that
        implements the function:
        \code
        float
        function_name(in uint dashed_stroking_data_location,
                      in float total_dash_pattern_distance,
                      in float in_distance,
                      out uint interval_id)
        \endcode
        that returns the signed distance to the nearest dash boundary.
        The parameter dashed_stroking_data_location is the data location
        to the dashed stroking data packed as (TODO specify format),
        the parameter total_dash_pattern_distance is the total length
        of the dash pattern and the paremeter in_distance is the input
        distance from the start of the contour. The out value, interval_id,
        is the interval ID for where in_distance is located.

        \param function_name name to give to the function
        \param data_alignment the alignment of the data store (see PainterBackend::ConfigurationBase::alignemnt()).
       */
      ShaderSource
      dashed_stroking_compute(const char *function_name,
                              unsigned int data_alignment);
    }
/*! @} */
  }

}
