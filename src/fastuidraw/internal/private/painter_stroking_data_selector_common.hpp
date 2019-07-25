/*!
 * \file painter_stroking_data_selector_common.hpp
 * \brief file painter_stroking_data_selector_common.hpp
 *
 * Copyright 2019 by Intel.
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

#ifndef FASTUIDRAW_PAINTER_STROKING_DATA_SELECTOR_COMMON_HPP
#define FASTUIDRAW_PAINTER_STROKING_DATA_SELECTOR_COMMON_HPP

#include <cmath>
#include <fastuidraw/painter/shader/painter_stroke_shader.hpp>
#include <fastuidraw/painter/shader_data/painter_stroke_params.hpp>

namespace fastuidraw
{
  namespace detail
  {
    template<unsigned int MiterLimitOffset,
             unsigned int RadiusOffset,
             unsigned int UnitsOffset>
    class StrokingDataSelector:public fastuidraw::StrokingDataSelectorBase
    {
    public:
      explicit
      StrokingDataSelector(bool pixel_arc_stroking_possible):
        m_pixel_arc_stroking_possible(pixel_arc_stroking_possible)
      {}

      float
      compute_thresh(fastuidraw::c_array<const fastuidraw::uvec4> pdata,
                     float path_magnification,
                     float curve_flatness) const override final
      {
        fastuidraw::c_array<const uint32_t> data (pdata.flatten_array());
        float radius(unpack_float(data[RadiusOffset]));

        if (radius <= 0.0f)
          {
            /* Not really stroking, just select a LARGE value
             * to get a very low level of detail.
             */
            return 10000.0f;
          }
        else
          {
            float return_value;

            return_value = curve_flatness / fastuidraw::t_max(1.0f, radius);
            if (data[UnitsOffset] == fastuidraw::PainterStrokeParams::path_stroking_units)
              {
                return_value /= path_magnification;
              }
            return return_value;
          }
      }

      void
      stroking_distances(fastuidraw::c_array<const fastuidraw::uvec4> pdata,
                         fastuidraw::c_array<float> out_geometry_inflation) const override final
      {
        fastuidraw::c_array<const uint32_t> data (pdata.flatten_array());
        float out_pixel_distance, out_item_space_distance;

        if (data[UnitsOffset] == fastuidraw::PainterStrokeParams::path_stroking_units)
          {
            out_pixel_distance = 0.0f;
            out_item_space_distance = unpack_float(data[RadiusOffset]);
          }
        else
          {
            out_pixel_distance = unpack_float(data[RadiusOffset]);
            out_item_space_distance = 0.0f;
          }

        out_geometry_inflation[pixel_space_distance] = out_pixel_distance;
        out_geometry_inflation[item_space_distance] = out_item_space_distance;
        out_geometry_inflation[pixel_space_distance_miter_joins] = unpack_float(data[MiterLimitOffset]) * out_pixel_distance;
        out_geometry_inflation[item_space_distance_miter_joins] = unpack_float(data[MiterLimitOffset]) * out_item_space_distance;
      }

      bool
      arc_stroking_possible(fastuidraw::c_array<const fastuidraw::uvec4> pdata) const override final
      {
        fastuidraw::c_array<const uint32_t> data (pdata.flatten_array());
        return m_pixel_arc_stroking_possible
          || data[UnitsOffset] == fastuidraw::PainterStrokeParams::path_stroking_units;
      }

      bool
      data_compatible(fastuidraw::c_array<const fastuidraw::uvec4> pdata) const override final
      {
        fastuidraw::c_array<const uint32_t> data (pdata.flatten_array());
        return data.size() > MiterLimitOffset
          && data.size() > RadiusOffset
          && data.size() > UnitsOffset
          && std::isfinite(unpack_float(data[MiterLimitOffset]))
          && std::isfinite(unpack_float(data[RadiusOffset]))
          && (data[UnitsOffset] == fastuidraw::PainterStrokeParams::path_stroking_units
              || data[UnitsOffset] == fastuidraw::PainterStrokeParams::pixel_stroking_units);
      }

    private:
      bool m_pixel_arc_stroking_possible;
    };
  }
}

#endif
