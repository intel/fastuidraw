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
/*!\addtogroup GLSL
 * @{
 */
    /*!
     * \brief Namespace to encapsulate GLSL shader source
     *        code used in rendering, part of the main
     *        library libFastUIDraw
     */
    namespace code
    {
      /*!
       * Construct/returns a ShaderSource value that
       * implements the function:
       * \code
       * float
       * function_name(in int texel_value,
       *               in vec2 texture_coordinate,
       *               in int geometry_offset)
       * \endcode
       *
       * which returns the signed pseudo-distance to the glyph boundary
       * for a glyph of type \ref curve_pair_glyph (these glyphs are
       * backed by data produced from a \ref GlyphRenderDataCurvePair).
       * The value texel_value is value in the texel store from the
       * position texture_coordinate (the bottom left for the glyph being
       * at Glyph::atlas_location().location() and the top right
       * being at that value + Glyph::layout().m_texel_size.
       * The value geometry_offset is from Glyph::geometry_offset().
       * \param function_name name for the function
       * \param geometry_store_fetch the macro function (that returns a vec4)
       *                             to use in the produced GLSL code to fetch
       *                             the geometry store data.
       * \param derivative_function if true, give the GLSL function with the
       *                            argument signature (in int, in vec2, in int, out vec2)
       *                            where the last argument is the gradient of
       *                            the function with repsect to texture_coordinate.
       */
      ShaderSource
      curvepair_compute_pseudo_distance(c_string function_name,
                                        c_string geometry_store_fetch,
                                        bool derivative_function = false);
      /*!
       * Gives the shader source code for a function with
       * the signature:
       * \code
       * void
       * function(in vec2 punnormalized_index_tex_coord,
       *          in int pindex_layer,
       *          in int pnum_levels,
       *          in int tile_slack,
       *          out vec2 return_value_unnormalized_texcoord_xy,
       *          out int return_value_layer)
       * \endcode
       *
       * which computes the unnormalized texture coordinate and
       * layer into the color atlas.
       *
       * \param function_name name to give the function
       * \param index_texture name to give the index texture atlas
       * \param index_tile_size the size of each index tile (see ImageAtlas::index_tile_size())
       * \param color_tile_size the size of each color tile (see ImageAtlas::color_tile_size())
       */
      ShaderSource
      image_atlas_compute_coord(c_string function_name,
                                c_string index_texture,
                                unsigned int index_tile_size,
                                unsigned int color_tile_size);

      /*!
       * Construct/returns a ShaderSource value that
       * implements the function:
       * \code
       * float
       * function_name(in uint intervals_location,
       *               in float total_distance,
       *               in float first_interval_start,
       *               in float in_distance,
       *               in int number_intervals,
       *               out int interval_id,
       *               out float interval_begin,
       *               out float interval_end)
       * \endcode
       * that compute the interval a distance value lies upon from
       * a repeated interval pattern. The parameter meanins are:
       * - intervals_location gives the location into the data store buffer where the
       *   interval data is packed as (TODO: document format).
       * - total_distance the period of the repeat interval pattern
       * - first_interval_start
       * - in_distance distance value to evaluate
       * - number_intervals number of intervals in the interval pattern
       * - interval_id (output) ID of interval
       * - interval_begin (output) interval start of interval
       * - interval_end (output) interval end of interval
       * - return -1 if on an odd interval, +1 if on an even interval
       *
       * \param function_name name to give to the function
       * \param data_alignment the alignment of the data store (see PainterBackend::ConfigurationBase::alignemnt()).
       */
      ShaderSource
      compute_interval(c_string function_name, unsigned int data_alignment);
    }
/*! @} */
  }

}
