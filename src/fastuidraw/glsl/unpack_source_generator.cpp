/*!
 * \file unpack_source_generator.cpp
 * \brief file unpack_source_generator.cpp
 *
 * Copyright 2019 by Intel.
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

#include <vector>
#include <string>
#include <sstream>

#include <fastuidraw/glsl/unpack_source_generator.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include "../private/util_private.hpp"

namespace
{
  class UnpackElement
  {
  public:
    UnpackElement(void):
      m_type(fastuidraw::glsl::UnpackSourceGenerator::padding_type),
      m_idx(0)
    {}

    UnpackElement(fastuidraw::c_string name,
                  fastuidraw::glsl::UnpackSourceGenerator::type_t tp,
                  unsigned int idx):
      m_name(name),
      m_type(tp),
      m_idx(idx)
    {}

    std::string m_name;
    fastuidraw::glsl::UnpackSourceGenerator::type_t m_type;
    unsigned int m_idx;
  };

  class UnpackSourceGeneratorPrivate
  {
  public:
    explicit
    UnpackSourceGeneratorPrivate(fastuidraw::c_string nm):
      m_structs(1, nm)
    {}

    explicit
    UnpackSourceGeneratorPrivate(fastuidraw::c_array<const fastuidraw::c_string> nms):
      m_structs(nms.begin(), nms.end())
    {}

    std::vector<std::string> m_structs;
    std::vector<UnpackElement> m_elements;
  };
}

////////////////////////////////////////
// UnpackSourceGenerator methods
fastuidraw::glsl::UnpackSourceGenerator::
UnpackSourceGenerator(c_string name)
{
  m_d = FASTUIDRAWnew UnpackSourceGeneratorPrivate(name);
}

fastuidraw::glsl::UnpackSourceGenerator::
UnpackSourceGenerator(c_array<const c_string> names)
{
  m_d = FASTUIDRAWnew UnpackSourceGeneratorPrivate(names);
}

fastuidraw::glsl::UnpackSourceGenerator::
UnpackSourceGenerator(const UnpackSourceGenerator &rhs)
{
  UnpackSourceGeneratorPrivate *d;
  d = static_cast<UnpackSourceGeneratorPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew UnpackSourceGeneratorPrivate(*d);
}

fastuidraw::glsl::UnpackSourceGenerator::
~UnpackSourceGenerator()
{
  UnpackSourceGeneratorPrivate *d;
  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::glsl::UnpackSourceGenerator)

fastuidraw::glsl::UnpackSourceGenerator&
fastuidraw::glsl::UnpackSourceGenerator::
set(unsigned int offset, c_string field_name, type_t type, unsigned int idx)
{
  UnpackSourceGeneratorPrivate *d;

  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);
  if (offset >= d->m_elements.size())
    {
      d->m_elements.resize(offset + 1);
    }
  d->m_elements[offset] = UnpackElement(field_name, type, idx);
  return *this;
}

void
fastuidraw::glsl::UnpackSourceGenerator::
stream_unpack_function(ShaderSource &dst,
                       c_string function_name,
                       bool has_return_value) const
{
  UnpackSourceGeneratorPrivate *d;
  unsigned int number_blocks;
  std::ostringstream str;
  const fastuidraw::c_string swizzles[] =
    {
      ".x",
      ".xy",
      ".xyz",
      ".xyzw"
    };
  const fastuidraw::c_string ext_components[] =
    {
      ".x",
      ".y",
      ".z",
      ".w"
    };

  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);
  if (has_return_value)
    {
      str << "uint\n";
    }
  else
    {
      str << "void\n";
    }
  str << function_name << "(in uint location, ";

  for (unsigned int s = 0; s < d->m_structs.size(); ++s)
    {
      if (s != 0)
        {
          str << ", ";
        }
      str << "out " << d->m_structs[s] << " out_value" << s;
    }
  str << ")\n"
      << "{\n"
      << "\tuvec4 utemp;\n";

  number_blocks = FASTUIDRAW_NUMBER_BLOCK4_NEEDED(d->m_elements.size());
  FASTUIDRAWassert(4 * number_blocks >= d->m_elements.size());

  for(unsigned int b = 0, i = 0; b < number_blocks; ++b)
    {
      unsigned int cmp, li;
      fastuidraw::c_string swizzle;

      li = d->m_elements.size() - i;
      cmp = fastuidraw::t_min(4u, li);
      FASTUIDRAWassert(cmp >= 1 && cmp <= 4);

      swizzle = swizzles[cmp - 1];
      str << "\tutemp" << swizzle << " = fastuidraw_fetch_data(int(location) + "
          << b << ")" << swizzle << ";\n";

      /* perform bit cast to correct type. */
      for(unsigned int k = 0; k < 4 && i < d->m_elements.size(); ++k, ++i)
        {
          fastuidraw::c_string ext;
          ext = ext_components[k];
          switch(d->m_elements[i].m_type)
            {
            case int_type:
              str << "\tout_value" << d->m_elements[i].m_idx
                  << d->m_elements[i].m_name << " = "
                  << "int(utemp" << ext << ");\n";
              break;
            case uint_type:
              str << "\tout_value" << d->m_elements[i].m_idx
                  << d->m_elements[i].m_name << " = "
                  << "utemp" << ext << ";\n";
              break;
            case float_type:
              str << "\tout_value" << d->m_elements[i].m_idx
                  << d->m_elements[i].m_name << " = "
                  << "uintBitsToFloat(utemp" << ext << ");\n";
              break;
            default:
            case padding_type:
              str << "\t//Padding at component " << ext << "\n";
            }
        }
    }

  if (has_return_value)
    {
      str << "return uint(" << number_blocks << ") + location;\n";
    }
  str << "}\n\n";
  dst.add_source(str.str().c_str(), glsl::ShaderSource::from_string);
}
