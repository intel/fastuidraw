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
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */
#include <fastuidraw/util/util.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cstring>
#ifdef __linux__
#include <execinfo.h>
#include <unistd.h>
#include <cxxabi.h>
#endif

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>

#include <ieeehalfprecision/ieeehalfprecision.hpp>

#ifdef __linux__
std::string
demangled_function_name(const char *backtrace_string)
{
  if (!backtrace_string)
    {
      return "";
    }

  /* backtrace gives the symbol as follows:
   *  library_name(mangled_function_name+offset) [return_address]
   */
  const char *open_paren, *plus, *end;

  end = backtrace_string + std::strlen(backtrace_string);
  open_paren = std::find(backtrace_string, end, '(');
  if (open_paren == end)
    {
      return "";
    }
  ++open_paren;
  plus = std::find(open_paren, end, '+');
  if (plus == end)
    {
      return "";
    }

  char* demangle_c_string;
  int status;
  std::string tmp(open_paren, plus);

  demangle_c_string = abi::__cxa_demangle(tmp.c_str(),
                                          nullptr,
                                          nullptr,
                                          &status);
  if (demangle_c_string == nullptr)
    {
      return "";
    }

  std::string return_value(demangle_c_string);
  free(demangle_c_string);
  return return_value;
}
#endif


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
  std::cerr << "[" << file << "," << line << "]: " << str << "\n";

  #ifdef __linux__
    {
      #define STACK_MAX_BACKTRACE_SIZE 30
      void *backtrace_data[STACK_MAX_BACKTRACE_SIZE];
      char **backtrace_strings;
      size_t backtrace_size;

      std::cerr << "Backtrace:\n" << std::flush;
      backtrace_size = backtrace(backtrace_data, STACK_MAX_BACKTRACE_SIZE);
      backtrace_strings = backtrace_symbols(backtrace_data, backtrace_size);
      for (size_t i = 0; i < backtrace_size; ++i)
        {
          std::cerr << "\t" << backtrace_strings[i]
                    << "{" << demangled_function_name(backtrace_strings[i])
                    << "}\n";
        }
      free(backtrace_strings);
    }
  #endif

  #ifdef FASTUIDRAW_DEBUG
    {
      std::abort();
    }
  #endif
}
