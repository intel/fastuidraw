/*!
 * \file painter_stroking_data_selector_common.hpp
 * \brief file painter_stroking_data_selector_common.hpp
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

#include <fastuidraw/painter/shader/painter_stroke_shader.hpp>

namespace fastuidraw
{
  namespace detail
  {
    template<typename T>
    class StrokingDataSelectorT:public fastuidraw::StrokingDataSelectorBase
    {
    public:
      explicit
      StrokingDataSelectorT(bool pixel_arc_stroking_possible):
        m_pixel_arc_stroking_possible(pixel_arc_stroking_possible)
      {}

      float
      compute_thresh(const fastuidraw::PainterShaderData::DataBase *data,
                     float path_magnification,
                     float curve_flatness) const override final
      {
        const T *d;
        d = static_cast<const T*>(data);

        if (d->m_radius <= 0.0f)
          {
            /* Not really stroking, just select a LARGE value
             * to get a very low level of detail.
             */
            return 10000.0f;
          }
        else
          {
            float return_value;

            return_value = curve_flatness / fastuidraw::t_max(1.0f, d->m_radius);
            if (d->m_stroking_units == fastuidraw::PainterStrokeParams::path_stroking_units)
              {
                return_value /= path_magnification;
              }
            return return_value;
          }
      }

      void
      stroking_distances(const fastuidraw::PainterShaderData::DataBase *data,
                         fastuidraw::c_array<float> out_geometry_inflation) const override final
      {
        const T *d;
        d = static_cast<const T*>(data);

        float out_pixel_distance, out_item_space_distance;

        if (d->m_stroking_units == fastuidraw::PainterStrokeParams::path_stroking_units)
          {
            out_pixel_distance = 0.0f;
            out_item_space_distance = d->m_radius;
          }
        else
          {
            out_pixel_distance = d->m_radius;
            out_item_space_distance = 0.0f;
          }

        out_geometry_inflation[pixel_space_distance] = out_pixel_distance;
        out_geometry_inflation[item_space_distance] = out_item_space_distance;
        out_geometry_inflation[pixel_space_distance_miter_joins] = d->m_miter_limit * out_pixel_distance;
        out_geometry_inflation[item_space_distance_miter_joins] = d->m_miter_limit * out_item_space_distance;
      }

      bool
      arc_stroking_possible(const fastuidraw::PainterShaderData::DataBase *data) const override final
      {
        const T *d;
        d = static_cast<const T*>(data);

        return m_pixel_arc_stroking_possible
          || d->m_stroking_units == fastuidraw::PainterStrokeParams::path_stroking_units;
      }

      bool
      data_compatible(const fastuidraw::PainterShaderData::DataBase *data) const override final
      {
        return dynamic_cast<const T*>(data);
      }

    private:
      bool m_pixel_arc_stroking_possible;
    };
  }
}
