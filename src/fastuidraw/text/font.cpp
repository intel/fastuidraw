/*!
 * \file font.cpp
 * \brief file font.cpp
 *
 * Copyright 2018 by Intel.
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

#include <mutex>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/text/font.hpp>
#include <private/util_private.hpp>

namespace
{
  class GlyphGenerateParamValues
  {
  public:
    static
    GlyphGenerateParamValues&
    object(void)
    {
      static GlyphGenerateParamValues V;
      return V;
    }

    unsigned int m_distance_field_pixel_size;
    float m_distance_field_max_distance;
    float m_restricted_rays_minimum_render_size;
    int m_restricted_rays_split_thresh;
    int m_restricted_rays_max_recursion;
    unsigned int m_banded_rays_max_recursion;
    float m_banded_rays_average_number_curves_thresh;

    std::mutex m_mutex;
    unsigned int m_number_fonts_alive;
    unsigned int m_current_unqiue_id;

  private:
    GlyphGenerateParamValues(void):
      m_distance_field_pixel_size(48),
      m_distance_field_max_distance(1.5f),
      m_restricted_rays_minimum_render_size(32.0f),
      m_restricted_rays_split_thresh(4),
      m_restricted_rays_max_recursion(12),
      m_banded_rays_max_recursion(11),
      m_banded_rays_average_number_curves_thresh(2.5f),
      m_number_fonts_alive(0),
      m_current_unqiue_id(0)
    {}
  };

  class FontBasePrivate
  {
  public:
    explicit
    FontBasePrivate(const fastuidraw::FontProperties &pprops,
                    const fastuidraw::FontMetrics &metrics):
      m_properties(pprops),
      m_metrics(metrics)
    {
      std::lock_guard<std::mutex> m(GlyphGenerateParamValues::object().m_mutex);
      ++GlyphGenerateParamValues::object().m_number_fonts_alive;
      m_unique_id = ++GlyphGenerateParamValues::object().m_current_unqiue_id;
    }

    ~FontBasePrivate()
    {
      std::lock_guard<std::mutex> m(GlyphGenerateParamValues::object().m_mutex);
      --GlyphGenerateParamValues::object().m_number_fonts_alive;
    }

    unsigned int m_unique_id;
    fastuidraw::FontProperties m_properties;
    fastuidraw::FontMetrics m_metrics;
  };
}

//////////////////////////////////////////
// fastuidraw::GlyphGenerateParams methods
#define IMPLEMENT(T, X)                                                 \
  T                                                                     \
  fastuidraw::GlyphGenerateParams::                                     \
  X(void)                                                               \
  {                                                                     \
    std::lock_guard<std::mutex> m(GlyphGenerateParamValues::object().m_mutex); \
    return GlyphGenerateParamValues::object().m_##X;                    \
  }                                                                     \
  enum fastuidraw::return_code                                          \
  fastuidraw::GlyphGenerateParams::                                     \
  X(T v)                                                                \
  {                                                                     \
    std::lock_guard<std::mutex> m(GlyphGenerateParamValues::object().m_mutex); \
    if (GlyphGenerateParamValues::object().m_number_fonts_alive != 0)   \
      {                                                                 \
        return routine_fail;                                            \
      }                                                                 \
    GlyphGenerateParamValues::object().m_##X = v;                       \
    return routine_success;                                             \
  }

IMPLEMENT(unsigned int, distance_field_pixel_size)
IMPLEMENT(float, distance_field_max_distance)
IMPLEMENT(float, restricted_rays_minimum_render_size)
IMPLEMENT(int, restricted_rays_split_thresh)
IMPLEMENT(int, restricted_rays_max_recursion)
IMPLEMENT(unsigned int, banded_rays_max_recursion)
IMPLEMENT(float, banded_rays_average_number_curves_thresh)

///////////////////////////////////////////
// fastuidraw::FontBase methods
fastuidraw::FontBase::
FontBase(const FontProperties &pprops,
         const FontMetrics &pmetrics)
{
  m_d = FASTUIDRAWnew FontBasePrivate(pprops, pmetrics);
}

fastuidraw::FontBase::
~FontBase()
{
  FontBasePrivate *d;
  d = static_cast<FontBasePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

get_implement(fastuidraw::FontBase, FontBasePrivate,
              const fastuidraw::FontProperties&, properties)

get_implement(fastuidraw::FontBase, FontBasePrivate,
              const fastuidraw::FontMetrics&, metrics)

get_implement(fastuidraw::FontBase, FontBasePrivate,
              unsigned int, unique_id)
