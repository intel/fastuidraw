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
#include <assert.h>

#include <vector>
#include <string>
#include <iostream>

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/util.hpp>

unsigned int
fastuidraw::
round_up_to_multiple(unsigned int v, unsigned int alignment)
{
  unsigned int modulas;

  modulas = v % alignment;
  if(modulas == 0)
    {
      return v;
    }
  else
    {
      return v + (alignment - modulas);
    }
}

unsigned int
fastuidraw::
number_blocks(unsigned int alignment, unsigned int sz)
{  
  unsigned int return_value;
  return_value = sz / alignment;
  if(return_value * alignment < sz)
    {
      ++return_value;
    }
  return return_value;
}

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
  if(v == 0)
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
  if(v == 0)
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
assert_fail(const char *str, const char *file, int line)
{
  std::cerr << file << ":" << line << ": Assertion '" << str << "' failed\n";
  std::abort();
}

//////////////////////////////////////
// fastuidraw::noncopyable methods
fastuidraw::noncopyable::
noncopyable(const noncopyable &)
{
  assert(!"noncopyable copy ctor called!");
}

fastuidraw::noncopyable&
fastuidraw::noncopyable::
operator=(const noncopyable &)
{
  assert(!"noncopyable assignment operator called!");
  return *this;
}
