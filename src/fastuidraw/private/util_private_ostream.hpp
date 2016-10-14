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
#include <boost/thread.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{

  template<typename T, size_t N>
  std::ostream&
  operator<<(std::ostream &ostr, const vecN<T, N> &obj)
  {
    ostr << "(";
    for(size_t i = 0; i < N; ++i)
      {
        if(i != 0)
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
  operator<<(std::ostream &ostr, c_array<T> obj)
  {
    ostr << "(";
    for(size_t i = 0; i < obj.size(); ++i)
      {
        if(i != 0)
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
  operator<<(std::ostream &ostr, const_c_array<T> obj)
  {
    ostr << "(";
    for(size_t i = 0; i < obj.size(); ++i)
      {
        if(i != 0)
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
        if(i != 0)
          {
            ostr << ", ";
          }
        ostr << obj[i];
      }
    ostr << ")";
    return ostr;
  }
}
