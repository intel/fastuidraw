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
#include <fastuidraw/glsl/shader_source.hpp>

template<typename T>
fastuidraw::glsl::ShaderSource&
operator<<(fastuidraw::glsl::ShaderSource &src, const T &obj)
{
  std::ostringstream str;
  str << obj;
  src.add_source(str.str().c_str(), fastuidraw::glsl::ShaderSource::from_string);
  return src;
}

template<typename T>
std::ostream&
operator<<(std::ostream &ostr, const fastuidraw::range_type<T> &obj)
{
  ostr << "[" << obj.m_begin << ", " << obj.m_end << ")";
  return ostr;
}

template<typename T, size_t N>
std::ostream&
operator<<(std::ostream &ostr, const fastuidraw::vecN<T, N> &obj)
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
std::ostream&
operator<<(std::ostream &ostr, fastuidraw::c_array<T> obj)
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
std::ostream&
operator<<(std::ostream &ostr, const std::vector<T> &obj)
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
