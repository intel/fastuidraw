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
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
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
compute_interval(c_string function_name,
                 c_string fetch_macro_function)
{
  ShaderSource return_value;

  return_value
    .add_macro("FASTUIDRAW_COMPUTE_INTERVAL_NAME", function_name)
    .add_macro("FASTUIDRAW_COMPUTE_INTERVAL_FETCH_DATA", fetch_macro_function)
    .add_source("fastuidraw_compute_interval.glsl.resource_string", ShaderSource::from_resource)
    .remove_macro("FASTUIDRAW_COMPUTE_INTERVAL_FETCH_DATA")
    .remove_macro("FASTUIDRAW_COMPUTE_INTERVAL_NAME");

  return return_value;
}

fastuidraw::glsl::ShaderSource
fastuidraw::glsl::code::
restricted_rays_compute_coverage(c_string fetch_macro_function)
{
  ShaderSource return_value;

  return_value
    .add_macro("fastuidraw_restricted_rays_hierarchy_node_bit", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_is_node_bit))
    .add_macro("fastuidraw_restricted_rays_hierarchy_node_mask", uint32_t(FASTUIDRAW_MASK(GlyphRenderDataRestrictedRays::hierarchy_is_node_bit, 1u)))
    .add_macro("fastuidraw_restricted_rays_hierarchy_split_coord_bit", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_splitting_coordinate_bit))
    .add_macro("fastuidraw_restricted_rays_hierarchy_child0_bit", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_child0_offset_bit0))
    .add_macro("fastuidraw_restricted_rays_hierarchy_child1_bit", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_child1_offset_bit0))
    .add_macro("fastuidraw_restricted_rays_hierarchy_child_num_bits", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_child_offset_numbits))
    .add_macro("fastuidraw_restricted_rays_hierarchy_curve_list_bit0", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_bit0))
    .add_macro("fastuidraw_restricted_rays_hierarchy_curve_list_num_bits", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_numbits))
    .add_macro("fastuidraw_restricted_rays_hierarchy_curve_list_size_bit0", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_size_bit0))
    .add_macro("fastuidraw_restricted_rays_hierarchy_curve_list_size_num_bits", uint32_t(GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_size_numbits))
    .add_macro("fastuidraw_restricted_rays_point_coordinate_num_bits", uint32_t(GlyphRenderDataRestrictedRays::point_coordinate_numbits))
    .add_macro("fastuidraw_restricted_rays_point_x_coordinate_bit0", uint32_t(GlyphRenderDataRestrictedRays::point_x_coordinate_bit0))
    .add_macro("fastuidraw_restricted_rays_point_y_coordinate_bit0", uint32_t(GlyphRenderDataRestrictedRays::point_y_coordinate_bit0))
    .add_macro("fastuidraw_restricted_rays_winding_value_bias", uint32_t(GlyphRenderDataRestrictedRays::winding_bias))
    .add_macro("fastuidraw_restricted_rays_winding_value_bit0", uint32_t(GlyphRenderDataRestrictedRays::winding_value_bit0))
    .add_macro("fastuidraw_restricted_rays_winding_value_num_bits", uint32_t(GlyphRenderDataRestrictedRays::winding_value_numbits))
    .add_macro("fastuidraw_restricted_rays_position_delta_divide", uint32_t(GlyphRenderDataRestrictedRays::delta_div_factor))
    .add_macro("fastuidraw_restricted_rays_position_delta_x_bit0", uint32_t(GlyphRenderDataRestrictedRays::delta_x_bit0))
    .add_macro("fastuidraw_restricted_rays_position_delta_y_bit0", uint32_t(GlyphRenderDataRestrictedRays::delta_y_bit0))
    .add_macro("fastuidraw_restricted_rays_position_delta_num_bits", uint32_t(GlyphRenderDataRestrictedRays::delta_numbits))
    .add_macro("fastuidraw_restricted_rays_curve_entry_num_bits", uint32_t(GlyphRenderDataRestrictedRays::curve_numbits))
    .add_macro("fastuidraw_restricted_rays_curve_entry0_bit0", uint32_t(GlyphRenderDataRestrictedRays::curve_entry0_bit0))
    .add_macro("fastuidraw_restricted_rays_curve_entry1_bit0", uint32_t(GlyphRenderDataRestrictedRays::curve_entry1_bit0))
    .add_macro("fastuidraw_restricted_rays_curve_is_quadratic_bit", uint32_t(GlyphRenderDataRestrictedRays::curve_is_quadratic_bit))
    .add_macro("fastuidraw_restricted_rays_curve_is_quadratic_mask", uint32_t(FASTUIDRAW_MASK(GlyphRenderDataRestrictedRays::curve_is_quadratic_bit, 1u)))
    .add_macro("fastuidraw_restricted_rays_curve_bit0", uint32_t(GlyphRenderDataRestrictedRays::curve_location_bit0))
    .add_macro("fastuidraw_restricted_rays_curve_num_bits", uint32_t(GlyphRenderDataRestrictedRays::curve_location_numbits))
    .add_macro("FASTUIDRAW_RESTRICTED_RAYS_FETCH_DATA", fetch_macro_function)
    .add_source("fastuidraw_restricted_rays.glsl.resource_string", ShaderSource::from_resource)
    .remove_macro("FASTUIDRAW_RESTRICTED_RAYS_FETCH_DATA");

  return return_value;
}
