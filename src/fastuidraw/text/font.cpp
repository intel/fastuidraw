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

    std::mutex m_mutex;
    unsigned int m_number_fonts_alive;

  private:
    GlyphGenerateParamValues(void):
      m_distance_field_pixel_size(48),
      m_distance_field_max_distance(1.5f),
      m_number_fonts_alive(0)
    {}
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

///////////////////////////////////////////
// fastuidraw::FontBase methods
fastuidraw::FontBase::
FontBase(const FontProperties &pprops):
  m_props(pprops)
{
  std::lock_guard<std::mutex> m(GlyphGenerateParamValues::object().m_mutex);
  ++GlyphGenerateParamValues::object().m_number_fonts_alive;
}

fastuidraw::FontBase::
~FontBase()
{
  std::lock_guard<std::mutex> m(GlyphGenerateParamValues::object().m_mutex);
  --GlyphGenerateParamValues::object().m_number_fonts_alive;
}
