/*!
 * \file shader_code.cpp
 * \brief file shader_code.cpp
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

#include <sstream>

#include <fastuidraw/util/math.hpp>
#include <fastuidraw/glsl/shader_code.hpp>

/////////////////////////////////
// fastuidraw::glsl::code methods
fastuidraw::glsl::ShaderSource
fastuidraw::glsl::code::
image_atlas_compute_coord(c_string function_name,
                          c_string index_texture,
                          unsigned int index_tile_size,
                          unsigned int color_tile_size)
{
  ShaderSource return_value;

  return_value
    .add_macro("FASTUIDRAW_INDEX_TILE_SIZE", index_tile_size)
    .add_macro("FASTUIDRAW_COLOR_TILE_SIZE", color_tile_size)
    .add_macro("FASTUIDRAW_INDEX_ATLAS", index_texture)
    .add_macro("FASTUIDRAW_ATLAS_COMPUTE_COORD", function_name)
    .add_source("fastuidraw_atlas_image_fetch.glsl.resource_string", glsl::ShaderSource::from_resource)
    .remove_macro("FASTUIDRAW_INDEX_TILE_SIZE")
    .remove_macro("FASTUIDRAW_COLOR_TILE_SIZE")
    .remove_macro("FASTUIDRAW_INDEX_ATLAS")
    .remove_macro("FASTUIDRAW_ATLAS_COMPUTE_COORD");

  return return_value;
}

fastuidraw::glsl::ShaderSource
fastuidraw::glsl::code::
compute_interval(c_string function_name)
{
  ShaderSource return_value;

  return_value
    .add_macro("FASTUIDRAW_COMPUTE_INTERVAL_NAME", function_name)
    .add_source("fastuidraw_compute_interval.glsl.resource_string", ShaderSource::from_resource)
    .remove_macro("FASTUIDRAW_COMPUTE_INTERVAL_NAME");

  return return_value;
}
