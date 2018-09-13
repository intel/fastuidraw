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
assert_fail(c_string str, c_string file, int line)
{
  std::cerr << file << ":" << line << ": Assertion '" << str << "' failed\n";
  std::abort();
}
