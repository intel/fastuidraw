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
       * \param fetch_macro_function function or macro taking one argument
       *                             that returns a uvec4 of data
       */
      ShaderSource
      compute_interval(c_string function_name,
                       c_string fetch_macro_function);

      /*!
       * Construct/returns a ShaderSource value that implements the function:
       * \code
       * float
       * fastuidraw_restricted_rays_compute_coverage(in uint glyph_data_location,
       *                                             in vec2 glyph_coord,
       *                                             in vec2 glyph_coord_dx,
       *                                             in vec2 glyph_coord_dy)
       * \endcode
       * That compute the coverage from glyph data as packed by \ref
       * GlyphRenderDataRestrictedRays. The paremeters of the GLSL function
       * mean:
       *   - glyph_data_location location of the glyph data within the GlyphAtlas
       *   - glyph_coord the coordinate within the glyph where the min-value
       *     in each dimension is -GlyphRenderDataRestrictedRays::glyph_coord_value
       *     and the max-value in each dimension is GlyphRenderDataRestrictedRays::glyph_coord_value
       *   - glyph_coord_dx is the value of dFdx(glyph_coord)
       *   - glyph_coord_dy is the value of dFdy(glyph_coord)
       *
       * The returned \ref ShaderSource also includes a large number of utility
       * functions and structs each prefixed with fastuidraw_restricted_rays_ that
       * assist in implementing the fastuidraw_restricted_rays_compute_coverage()
       * function.
       *
       * \param fetch_macro_function function or macro taking one argument
       *                             that returns a single uint of data
       * \param fetch_macro_function_fp16x2 function or macro taking one argument
       *                                    that returns a single uint of data
       *                                    bit casted to an (fp16, fp16) pair
       *                                    realized as an vec2 in GLSL
       */
      ShaderSource
      restricted_rays_compute_coverage(c_string fetch_macro_function,
				       c_string fetch_macro_function_fp16x2);

       /*!
       * Construct/returns a ShaderSource value that implements the function:
       * \code
       * float
       * fastuidraw_banded_rays_compute_coverage(in uint glyph_data_location,
       *                                         in vec2 glyph_coord,
       *                                         in vec2 glyph_coord_fwidth,
       *                                         in uint num_vertical_bands,
       *                                         in uint num_horizontal_bands,
       *                                         in bool use_odd_even_rule)
       *
       * \endcode
       * That compute the coverage from glyph data as packed by \ref
       * GlyphRenderDataBandedRays. The paremeters of the GLSL function
       * mean:
       *   - glyph_data_location location of the glyph data within the GlyphAtlas
       *   - glyph_coord the coordinate within the glyph where the min-value
       *     in each dimension is -GlyphRenderDataBandedRays::glyph_coord_value
       *     and the max-value in each dimension is GlyphRenderDataBandedRays::glyph_coord_value
       *   - glyph_coord_fwidth is the value of fwidth(glyph_coord)
       *   - num_vertical_bands is the number of verical bands of the banded ray glyph,
       *     i.e. the value packed into the glyph attribute \ref
       *     GlyphRenderDataBandedRays::glyph_num_vertical_bands
       *   - num_horizontal_bands is the number of verical bands of the banded ray glyph,
       *     i.e. the value packed into the glyph attribute \ref
       *     GlyphRenderDataBandedRays::glyph_num_horizontal_bands
       *   - use_odd_even_rule if true, fill with the odd-even fill rule, otherwise
       *     fill with the non-zero fill rule.
       *
       * The returned \ref ShaderSource also includes a large number of utility
       * functions and structs each prefixed with fastuidraw_banded_rays_ that
       * assist in implementing the fastuidraw_banded_rays_compute_coverage()
       * function.
       * \param fetch_macro_function function or macro taking one argument
       *                             that returns a single uint of data
       */
      ShaderSource
      banded_rays_compute_coverage(c_string fetch_macro_function);
    }
/*! @} */
  }

}
