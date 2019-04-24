/*!
 * \file binding_points.hpp
 * \brief file binding_points.hpp
 *
 * Copyright 2019 by Intel.
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

namespace fastuidraw { namespace gl { namespace detail {
  class BindingPoints
  {
  public:
    unsigned int m_num_ubo_units;
    unsigned int m_num_ssbo_units;
    unsigned int m_num_texture_units;
    unsigned int m_num_image_units;

    int m_colorstop_atlas_binding;
    int m_image_atlas_color_tiles_nearest_binding;
    int m_image_atlas_color_tiles_linear_binding;
    int m_image_atlas_index_tiles_binding;
    int m_glyph_atlas_store_binding;
    int m_glyph_atlas_store_binding_fp16;
    int m_data_store_buffer_binding;
    int m_context_texture_binding;
    int m_coverage_buffer_texture_binding;
    int m_uniforms_ubo_binding;
    int m_color_interlock_image_buffer_binding;
  };
}}}
