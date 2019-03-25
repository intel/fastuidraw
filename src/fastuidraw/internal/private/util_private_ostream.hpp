/*!
 * \file util_private_ostream.hpp
 * \brief file util_private_ostream.hpp
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

#include <vector>
#include <iostream>
#include <sstream>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/blend_mode.hpp>
#include <fastuidraw/glsl/shader_source.hpp>

#include "bounding_box.hpp"

namespace fastuidraw { namespace detail {

class PrintBytes
{
public:
  enum rounding_mode_t
    {
      round_to_highest_unit = 0,
      round_to_mb_or_highest_unit = 1,
      round_to_kb_or_highest_unit = 2,
      do_not_round= 3,
    };

  explicit
  PrintBytes(uint64_t v, enum rounding_mode_t r = round_to_kb_or_highest_unit):
    m_gb(fastuidraw::uint64_unpack_bits(30u, 34u, v)),
    m_mb(fastuidraw::uint64_unpack_bits(20u, 10u, v)),
    m_kb(fastuidraw::uint64_unpack_bits(10u, 10u, v)),
    m_b(fastuidraw::uint64_unpack_bits(0u, 10u, v)),
    m_rounding_mode(r)
  {}

  uint64_t m_gb, m_mb, m_kb, m_b;
  enum rounding_mode_t m_rounding_mode;
};

}}

namespace std {
inline
ostream&
operator<<(ostream &str, const fastuidraw::detail::PrintBytes &obj)
{
  bool print_spe(false), print(true);
  char spe(' ');

  if (obj.m_gb && print)
    {
      str << obj.m_gb << "GB";
      print_spe = true;
      print = (obj.m_rounding_mode > fastuidraw::detail::PrintBytes::round_to_highest_unit);
    }

  if (obj.m_mb && print)
    {
      if (print_spe)
        {
          str << spe;
        }
      str << obj.m_mb << "MB";
      print_spe = true;
      print = (obj.m_rounding_mode > fastuidraw::detail::PrintBytes::round_to_mb_or_highest_unit);
    }

  if (obj.m_kb && print)
    {
      if (print_spe)
        {
          str << spe;
        }
      str << obj.m_kb << "KB";
      print_spe = true;
      print = (obj.m_rounding_mode > fastuidraw::detail::PrintBytes::round_to_kb_or_highest_unit);
    }

  if (obj.m_b && print)
    {
      if (print_spe)
        {
          str << spe;
        }
      str << obj.m_b << "B";
    }
  return str;
}

template<typename T>
ostream&
operator<<(ostream &ostr, const fastuidraw::range_type<T> &obj)
{
  ostr << "[" << obj.m_begin << ", " << obj.m_end << ")";
  return ostr;
}

template<typename T, size_t N>
ostream&
operator<<(ostream &ostr, const fastuidraw::vecN<T, N> &obj)
{
  ostr << "(";
  for(size_t i = 0; i < N; ++i)
    {
      if (i != 0)
        {
          ostr << ", ";
        }
      ostr << obj[i];
    }
  ostr << ")";
  return ostr;
}

template<typename T>
ostream&
operator<<(ostream &ostr, fastuidraw::c_array<T> obj)
{
  ostr << "(";
  for(size_t i = 0; i < obj.size(); ++i)
    {
      if (i != 0)
        {
          ostr << ", ";
        }
      ostr << obj[i];
    }
  ostr << ")";
  return ostr;
}

template<typename T>
ostream&
operator<<(ostream &ostr, const vector<T> &obj)
{
  ostr << "(";
  for(size_t i = 0; i < obj.size(); ++i)
    {
      if (i != 0)
        {
          ostr << ", ";
        }
      ostr << obj[i];
    }
  ostr << ")";
  return ostr;
}

template<typename T>
ostream&
operator<<(ostream &str, const fastuidraw::BoundingBox<T> &obj)
{
  if (obj.empty())
    {
      str << "{}";
    }
  else
    {
      str << "[" << obj.min_point() << " -- " << obj.max_point() << "]";
    }
  return str;
}

template<typename T>
ostream&
operator<<(ostream &str, const fastuidraw::RectT<T> &obj)
{
  str << "[" << obj.m_min_point << " -- " << obj.m_max_point << "]";
  return str;
}

inline
std::ostream&
operator<<(ostream &str, fastuidraw::BlendMode obj)
{
  if (obj.is_valid())
    {
      if (obj.blending_on())
        {
          str << "[equation_rgb = " << fastuidraw::BlendMode::label(obj.equation_rgb())
              << ", equation_alpha = " << fastuidraw::BlendMode::label(obj.equation_alpha())
              << ", func_src_rgb = " << fastuidraw::BlendMode::label(obj.func_src_rgb())
              << ", func_src_alpha = " << fastuidraw::BlendMode::label(obj.func_src_alpha())
              << ", func_dst_rgb = " << fastuidraw::BlendMode::label(obj.func_dst_rgb())
              << ", func_dst_alpha = " << fastuidraw::BlendMode::label(obj.func_dst_alpha())
              << "]";
        }
      else
        {
          str << "[BlendingOff]";
        }
    }
  else
    {
      str << "InvalidBlendMode";
    }
  return str;
}
}

template<typename T>
fastuidraw::glsl::ShaderSource&
operator<<(fastuidraw::glsl::ShaderSource &src, const T &obj)
{
  std::ostringstream str;
  str << obj;
  src.add_source(str.str().c_str(), fastuidraw::glsl::ShaderSource::from_string);
  return src;
}
