/*!
 * \file shader_source.cpp
 * \brief file shader_source.cpp
 *
 * Adapted from: WRATHGLProgram.cpp of WRATH:
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
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <set>
#include <string>
#include <map>
#include <vector>
#include <list>
#include <algorithm>
#include <sstream>
#include <stdint.h>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/static_resource.hpp>
#include <fastuidraw/glsl/shader_source.hpp>

namespace
{
  class SourcePrivate
  {
  public:
    SourcePrivate();

    typedef enum fastuidraw::glsl::ShaderSource::source_t source_t;
    typedef enum fastuidraw::glsl::ShaderSource::extension_enable_t extension_enable_t;
    typedef std::pair<std::string, source_t> source_code_t;

    bool m_dirty;
    std::list<source_code_t> m_values;
    std::map<std::string, extension_enable_t> m_extensions;
    std::string m_version;
    bool m_disable_pre_added_source;

    std::string m_assembled_code;

    static
    std::string
    strip_leading_white_spaces(const std::string &S);

    static
    void
    emit_source_line(std::ostream &output_stream,
                     const std::string &source,
                     int line_number, const std::string &label);

    static
    void
    add_source_code_from_stream(const std::string &label,
                                std::istream &istr,
                                std::ostream &output_stream);

    static
    void
    add_source_entry(const source_code_t &v, std::ostream &output_stream);

    static
    const char*
    string_from_extension_t(extension_enable_t tp);
  };
}
//////////////////////////////////////////////////
// SourcePrivate methods
SourcePrivate::
SourcePrivate(void):
  m_dirty(false),
  m_disable_pre_added_source(false)
{
}


const char*
SourcePrivate::
string_from_extension_t(extension_enable_t tp)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  switch(tp)
    {
    case ShaderSource::enable_extension:
      return "enable";
      break;

    case ShaderSource::require_extension:
      return "require";
      break;

    case ShaderSource::warn_extension:
      return "warn";
      break;

    case ShaderSource::disable_extension:
      return "disable";
      break;

    default:
      assert(!"Unknown value for extension_enable_t");
      return "";
    }

}

std::string
SourcePrivate::
strip_leading_white_spaces(const std::string &S)
{
  std::string::const_iterator iter, end;
  for(iter = S.begin(), end = S.end(); iter != end and isspace(*iter); ++iter)
    {
    }
  return (iter != end and *iter == '#')?
    std::string(iter, end):
    S;
}

void
SourcePrivate::
emit_source_line(std::ostream &output_stream,
                 const std::string &source,
                 int line_number, const std::string &label)
{
  std::string S;
  S = strip_leading_white_spaces(source);
  output_stream << S;

  #ifndef NDEBUG
    {
      if(!label.empty() && (S.empty() || *S.rbegin() != '\\'))
        {
          output_stream << std::setw(80-S.length()) << "  //["
                        << std::setw(3) << line_number
                        << ", " << label
                        << "]";
        }
    }
  #else
    {
      FASTUIDRAWunused(label);
      FASTUIDRAWunused(line_number);
    }
  #endif

  output_stream << "\n";
}


void
SourcePrivate::
add_source_code_from_stream(const std::string &label,
                            std::istream &istr,
                            std::ostream &output_stream)
{
  std::string S;
  int line_number(1);

  while(getline(istr, S))
    {
      /* combine source lines that end with \
       */
      if(*S.rbegin() == '\\')
        {
          std::vector<std::string> strings;

          strings.push_back(S);
          while(!strings.rbegin()->empty() && *strings.rbegin()->rbegin() == '\\')
            {
              getline(istr, S);
              strings.push_back(S);
            }
          getline(istr, S);
          strings.push_back(S);

          /* now remove the '\\' and put on S.
           */
          S.clear();
          for(std::vector<std::string>::iterator
                iter = strings.begin(), end = strings.end(); iter != end; ++iter)
            {
              if(!iter->empty() && *iter->rbegin() == '\\')
                {
                  iter->resize(iter->size() - 1);
                }
              S += *iter;
            }
        }

      emit_source_line(output_stream, S, line_number, label);
      ++line_number;
      S.clear();
    }
}

void
SourcePrivate::
add_source_entry(const source_code_t &v, std::ostream &output_stream)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  if(v.second == ShaderSource::from_file)
    {
      std::ifstream file(v.first.c_str());

      if(file)
        {
          add_source_code_from_stream(v.first, file, output_stream);
        }
      else
        {
          output_stream << "\n//WARNING: Could not open file \""
                        << v.first << "\"\n";
        }
    }
  else
    {
      std::istringstream istr;
      std::string label;

      if(v.second == ShaderSource::from_string)
        {
          istr.str(v.first);
          label.clear();
        }
      else
        {
          const_c_array<uint8_t> resource_string;

          resource_string = fetch_static_resource(v.first.c_str());
          label=v.first;
          if(!resource_string.empty() && resource_string.back() == 0)
            {
              const char *s;
              s = reinterpret_cast<const char*>(resource_string.c_ptr());
              istr.str(std::string(s));
            }
          else
            {
              output_stream << "\n//WARNING: Unable to fetch string resource \"" << v.first
                            << "\"\n";
              return;
            }
        }
      add_source_code_from_stream(label, istr, output_stream);
    }
}

////////////////////////////////////////////////////
// fastuidraw::glsl::ShaderSource methods
fastuidraw::glsl::ShaderSource::
ShaderSource(void)
{
  m_d = FASTUIDRAWnew SourcePrivate();
}

fastuidraw::glsl::ShaderSource::
ShaderSource(const ShaderSource &obj)
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew SourcePrivate(*d);
}

fastuidraw::glsl::ShaderSource::
~ShaderSource()
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::glsl::ShaderSource::
swap(ShaderSource &obj)
{
  std::swap(obj.m_d, m_d);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
operator=(const ShaderSource &obj)
{
  if(this != &obj)
    {
      ShaderSource v(obj);
      swap(v);
    }
  return *this;
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
specify_version(const char *v)
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);
  d->m_version = v ? std::string(v) : "";
  d->m_dirty = true;
  return *this;
}

const char*
fastuidraw::glsl::ShaderSource::
version(void) const
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);
  return d->m_version.c_str();
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_source(const char *str, enum source_t tp, enum add_location_t loc)
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);

  assert(str);
  SourcePrivate::source_code_t v(str, tp);

  if(loc == push_front)
    {
      d->m_values.push_front(v);
    }
  else
    {
      d->m_values.push_back(v);
    }
  d->m_dirty = true;
  return *this;
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_source(const ShaderSource &obj)
{
  SourcePrivate *d, *obj_d;
  d = static_cast<SourcePrivate*>(m_d);
  obj_d = static_cast<SourcePrivate*>(obj.m_d);

  std::copy(obj_d->m_values.begin(), obj_d->m_values.end(),
            std::insert_iterator<std::list<SourcePrivate::source_code_t> >(d->m_values, d->m_values.end()));
  d->m_dirty = true;
  return *this;
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_macro(const char *macro_name, const char *macro_value,
          enum add_location_t loc)
{
  std::ostringstream ostr;
  ostr << "#define " << macro_name << " " << macro_value;
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_macro(const char *macro_name, uint32_t macro_value,
          enum add_location_t loc)
{
  std::ostringstream ostr;
  ostr << "#define " << macro_name << " " << macro_value;
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_macro(const char *macro_name, int32_t macro_value,
          enum add_location_t loc)
{
  std::ostringstream ostr;
  ostr << "#define " << macro_name << " " << macro_value;
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
remove_macro(const char *macro_name)
{
  std::ostringstream ostr;
  ostr << "#undef " << macro_name;
  return add_source(ostr.str().c_str(), from_string);
}


fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
specify_extension(const char *ext_name, enum extension_enable_t tp)
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);
  d->m_extensions[std::string(ext_name)] = tp;
  d->m_dirty = true;
  return *this;
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
specify_extensions(const ShaderSource &obj)
{
  SourcePrivate *d;
  const SourcePrivate *obj_d;
  d = static_cast<SourcePrivate*>(m_d);
  obj_d = static_cast<const SourcePrivate*>(obj.m_d);
  for(std::map<std::string, extension_enable_t>::const_iterator
        iter = obj_d->m_extensions.begin(), end = obj_d->m_extensions.end();
      iter != end; ++iter)
    {
      d->m_extensions[iter->first] = iter->second;
    }
  return *this;
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
disable_pre_added_source(void)
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);
  d->m_dirty = d->m_dirty || !d->m_disable_pre_added_source;
  d->m_disable_pre_added_source = true;
  return *this;
}

const char*
fastuidraw::glsl::ShaderSource::
assembled_code(void) const
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);

  if(d->m_dirty)
    {
      std::ostringstream output_glsl_source_code;

      if(!d->m_version.empty())
        {
          output_glsl_source_code <<"#version " << d->m_version << "\n";
        }

      for(std::map<std::string, enum extension_enable_t>::const_iterator
            iter = d->m_extensions.begin(), end = d->m_extensions.end(); iter != end; ++iter)
        {
          output_glsl_source_code << "#extension " << iter->first
                                  << ": " << SourcePrivate::string_from_extension_t(iter->second) << "\n";
        }

      if(!d->m_disable_pre_added_source)
        {
          output_glsl_source_code << "uint fastuidraw_mask(uint num_bits) { return (uint(1) << num_bits) - uint(1); }\n"
                                  << "uint fastuidraw_extract_bits(uint bit0, uint num_bits, uint src) { return (src >> bit0) & fastuidraw_mask(num_bits); }\n"
                                  << "#define FASTUIDRAW_MASK(bit0, num_bits) (fastuidraw_mask(uint(num_bits)) << uint(bit0))\n"
                                  << "#define FASTUIDRAW_EXTRACT_BITS(bit0, num_bits, src) fastuidraw_extract_bits(uint(bit0), uint(num_bits), uint(src) )\n"
                                  << "void fastuidraw_do_nothing(void) {}\n";
        }

      for(std::list<SourcePrivate::source_code_t>::const_iterator
            iter= d->m_values.begin(), end = d->m_values.end(); iter != end; ++iter)
        {
          SourcePrivate::add_source_entry(*iter, output_glsl_source_code);
        }

      /*
        some GLSL pre-processors do not like to end on a
        comment or other certain tokens, to make them
        less grouchy, we emit a few extra \n's
      */
      output_glsl_source_code << "\n\n\n";

      d->m_assembled_code = output_glsl_source_code.str();
      d->m_dirty = false;
    }
  return d->m_assembled_code.c_str();
}
