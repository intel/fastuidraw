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

uint32_t
fastuidraw::
floor_log2(uint32_t x)
{
  uint32_t r(0);
  while(x > 1)
    {
      x >>= 1;
      ++r;
    }
  return r;
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
