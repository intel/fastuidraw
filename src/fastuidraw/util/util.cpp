/*!
 * \file util.cpp
 * \brief file util.cpp
 *
 * Adapted from: WRATHUtil.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */
#include <fastuidraw/util/util.hpp>

#include <vector>
#include <string>
#include <iostream>

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>

#include "../../3rd_party/ieeehalfprecision/ieeehalfprecision.hpp"

uint32_t
fastuidraw::
uint32_log2(uint32_t v)
{
  uint32_t return_value(0);
  while(v >>= 1)
    {
      ++return_value;
    }
  return return_value;
}

uint32_t
fastuidraw::
number_bits_required(uint32_t v)
{
  if (v == 0)
    {
      return 0;
    }

  uint32_t return_value(0);
  for(uint32_t mask = 1u; mask <= v; mask <<= 1u, ++return_value)
    {}

  return return_value;
}

uint64_t
fastuidraw::
uint64_log2(uint64_t v)
{
  uint64_t return_value(0);
  while(v >>= 1)
    {
      ++return_value;
    }
  return return_value;
}

uint64_t
fastuidraw::
uint64_number_bits_required(uint64_t v)
{
  if (v == 0)
    {
      return 0;
    }

  uint64_t return_value(0);
  for(uint64_t mask = 1u; mask <= v; mask <<= 1u, ++return_value)
    {}

  return return_value;
}

void
fastuidraw::
convert_to_fp16(c_array<const float> src, c_array<uint16_t> dst)
{
  c_array<const uint32_t> srcU;

  FASTUIDRAWassert(src.size() == dst.size());
  srcU = src.reinterpret_pointer<const uint32_t>();
  ieeehalfprecision::singles2halfp(dst.c_ptr(), srcU.c_ptr(), src.size());
}

void
fastuidraw::
convert_to_fp32(c_array<const uint16_t> src, c_array<float> dst)
{
  c_array<uint32_t> dstU;

  FASTUIDRAWassert(src.size() == dst.size());
  dstU = dst.reinterpret_pointer<uint32_t>();
  ieeehalfprecision::halfp2singles(dstU.c_ptr(), src.c_ptr(), src.size());
}

void
fastuidraw::
assert_fail(c_string str, c_string file, int line)
{
  std::cerr << file << ":" << line << ": Assertion '" << str << "' failed\n";
  std::abort();
}
