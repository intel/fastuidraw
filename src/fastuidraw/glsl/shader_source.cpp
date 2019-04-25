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
#include <private/util_private.hpp>

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

    std::string m_assembled_code;
    std::string m_assembled_code_base;

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
    fastuidraw::c_string
    string_from_extension_t(extension_enable_t tp);
  };

  std::string
  macro_value_as_string(uint32_t v)
  {
    std::ostringstream str;
    str << v << "u";
    return str.str();
  }

  std::string
  macro_value_as_string(int32_t v)
  {
    std::ostringstream str;
    str << v;
    return str.str();
  }

  std::string
  macro_value_as_string(float v)
  {
    std::ostringstream str;
    str << "float(" << v << ")";
    return str.str();
  }

  class MacroSetEntry
  {
  public:
    MacroSetEntry(fastuidraw::c_string name,
                  fastuidraw::c_string value):
      m_name(name),
      m_value(value)
    {}

    MacroSetEntry(fastuidraw::c_string name,
                  uint32_t value):
      m_name(name),
      m_value(macro_value_as_string(value))
    {}

    MacroSetEntry(fastuidraw::c_string name,
                  int32_t value):
      m_name(name),
      m_value(macro_value_as_string(value))
    {}

    MacroSetEntry(fastuidraw::c_string name,
                  float value):
      m_name(name),
      m_value(macro_value_as_string(value))
    {}

    std::string m_name, m_value;
  };

  class MacroSetPrivate
  {
  public:
    std::vector<MacroSetEntry> m_entries;
  };

  std::string
  stripped_macro_name(const std::string &macro_name)
  {
    std::string::size_type pos;
    pos = macro_name.find('(');
    return macro_name.substr(0, pos);
  }
}

//////////////////////////////////////////////////
// SourcePrivate methods
SourcePrivate::
SourcePrivate(void):
  m_dirty(false)
{
}

fastuidraw::c_string
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
      FASTUIDRAWassert(!"Unknown value for extension_enable_t");
      return "";
    }

}

std::string
SourcePrivate::
strip_leading_white_spaces(const std::string &S)
{
  std::string::const_iterator iter, end;

  for(iter = S.begin(), end = S.end(); iter != end && isspace(*iter); ++iter)
    {}

  return (iter != end && *iter == '#')?
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
      if (!label.empty() && (S.empty() || *S.rbegin() != '\\'))
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
      /* combine source lines that end with \ */
      if (S.rbegin() != S.rend() && *S.rbegin() == '\\')
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

          /* now remove the '\\' and put on S. */
          S.clear();
          for(std::string &str : strings)
            {
              if (!str.empty() && *str.rbegin() == '\\')
                {
                  str.resize(str.size() - 1);
                }
              S += str;
            }
        }

      emit_source_line(output_stream, S, line_number, label);
      if (!istr.eof())
        {
          output_stream << "\n";
        }
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

  /* NOTES:
   *   - if source is a file or resource, we add a \n
   *   - if just a string, we do NOT add a \n
   */

  if (v.second == ShaderSource::from_file)
    {
      std::ifstream file(v.first.c_str());

      if (file)
        {
          add_source_code_from_stream(v.first, file, output_stream);
          output_stream << "\n";
        }
      else
        {
          output_stream << "\n//WARNING: Could not open file \""
                        << v.first << "\"\n";
        }
    }
  else
    {
      if (v.second == ShaderSource::from_string)
        {
          std::istringstream istr;
          istr.str(v.first);
          add_source_code_from_stream("", istr, output_stream);
        }
      else
        {
          c_array<const uint8_t> resource_string;

          resource_string = fetch_static_resource(v.first.c_str());
          if (!resource_string.empty() && resource_string.back() == 0)
            {
              std::istringstream istr;
              fastuidraw::c_string s;

              s = reinterpret_cast<fastuidraw::c_string>(resource_string.c_ptr());
              istr.str(std::string(s));

              add_source_code_from_stream(v.first, istr, output_stream);
              output_stream << "\n";
            }
          else
            {
              output_stream << "\n//WARNING: Unable to fetch string resource \"" << v.first
                            << "\"\n";
              return;
            }
        }
    }
}

///////////////////////////////////////////////////
// fastuidraw::glsl::ShaderSource::MacroSet methods
fastuidraw::glsl::ShaderSource::MacroSet::
MacroSet(void)
{
  m_d = FASTUIDRAWnew MacroSetPrivate();
}

fastuidraw::glsl::ShaderSource::MacroSet::
MacroSet(const MacroSet &obj)
{
  MacroSetPrivate *d;
  d = static_cast<MacroSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew MacroSetPrivate(*d);
}

fastuidraw::glsl::ShaderSource::MacroSet::
~MacroSet()
{
  MacroSetPrivate *d;
  d = static_cast<MacroSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::glsl::ShaderSource::MacroSet)

fastuidraw::glsl::ShaderSource::MacroSet&
fastuidraw::glsl::ShaderSource::MacroSet::
add_macro(c_string macro_name, c_string macro_value)
{
  MacroSetPrivate *d;
  d = static_cast<MacroSetPrivate*>(m_d);
  d->m_entries.push_back(MacroSetEntry(macro_name, macro_value));
  return *this;
}

fastuidraw::glsl::ShaderSource::MacroSet&
fastuidraw::glsl::ShaderSource::MacroSet::
add_macro(c_string macro_name, uint32_t macro_value)
{
  MacroSetPrivate *d;
  d = static_cast<MacroSetPrivate*>(m_d);
  d->m_entries.push_back(MacroSetEntry(macro_name, macro_value));
  return *this;
}

fastuidraw::glsl::ShaderSource::MacroSet&
fastuidraw::glsl::ShaderSource::MacroSet::
add_macro(c_string macro_name, int32_t macro_value)
{
  MacroSetPrivate *d;
  d = static_cast<MacroSetPrivate*>(m_d);
  d->m_entries.push_back(MacroSetEntry(macro_name, macro_value));
  return *this;
}

fastuidraw::glsl::ShaderSource::MacroSet&
fastuidraw::glsl::ShaderSource::MacroSet::
add_macro(c_string macro_name, float macro_value)
{
  MacroSetPrivate *d;
  d = static_cast<MacroSetPrivate*>(m_d);
  d->m_entries.push_back(MacroSetEntry(macro_name, macro_value));
  return *this;
}

/////////////////////////////////////////
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

assign_swap_implement(fastuidraw::glsl::ShaderSource)

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
specify_version(c_string v)
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);
  d->m_version = v ? std::string(v) : "";
  d->m_dirty = true;
  return *this;
}

fastuidraw::c_string
fastuidraw::glsl::ShaderSource::
version(void) const
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);
  return d->m_version.c_str();
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_source(c_string str, enum source_t tp, enum add_location_t loc)
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);

  FASTUIDRAWassert(str);
  SourcePrivate::source_code_t v(str, tp);

  if (loc == push_front)
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
add_macro(c_string macro_name, c_string macro_value,
          enum add_location_t loc)
{
  std::ostringstream ostr;

  ostr << "#define " << macro_name << " " << macro_value << "\n";
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_macro(c_string macro_name, uint32_t macro_value,
          enum add_location_t loc)
{
  std::string v(macro_value_as_string(macro_value));
  return add_macro(macro_name, v.c_str(), loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_macro(c_string macro_name, int32_t macro_value,
          enum add_location_t loc)
{
  std::string v(macro_value_as_string(macro_value));
  return add_macro(macro_name, v.c_str(), loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_macro(c_string macro_name, float macro_value,
          enum add_location_t loc)
{
  std::string v(macro_value_as_string(macro_value));
  return add_macro(macro_name, v.c_str(), loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
add_macros(const MacroSet &macros, enum add_location_t loc)
{
  MacroSetPrivate *d;

  d = static_cast<MacroSetPrivate*>(macros.m_d);
  for (const MacroSetEntry &entry : d->m_entries)
    {
      add_macro(entry.m_name.c_str(), entry.m_value.c_str(), loc);
    }
  return *this;
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
remove_macro(c_string macro_name,
             enum add_location_t loc)
{
  std::ostringstream ostr;
  ostr << "#undef " << macro_name << "\n";
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
remove_macros(const MacroSet &macros, enum add_location_t loc)
{
  MacroSetPrivate *d;

  d = static_cast<MacroSetPrivate*>(macros.m_d);
  for (const MacroSetEntry &entry : d->m_entries)
    {
      std::string stripped(stripped_macro_name(entry.m_name));
      remove_macro(stripped.c_str(), loc);
    }
  return *this;
}

fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::ShaderSource::
specify_extension(c_string ext_name, enum extension_enable_t tp)
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
  for(const auto &ext : obj_d->m_extensions)
    {
      d->m_extensions[ext.first] = ext.second;
    }
  return *this;
}

fastuidraw::c_string
fastuidraw::glsl::ShaderSource::
assembled_code(bool code_only) const
{
  SourcePrivate *d;
  d = static_cast<SourcePrivate*>(m_d);

  if (d->m_dirty)
    {
      std::ostringstream output_glsl_source_code;
      std::ostringstream output_glsl_source_code_base;

      if (!d->m_version.empty())
        {
          output_glsl_source_code <<"#version " << d->m_version << "\n";
        }

      for(const auto &ext : d->m_extensions)
        {
          output_glsl_source_code << "#extension " << ext.first << ": "
                                  << SourcePrivate::string_from_extension_t(ext.second)
                                  << "\n";
        }

      #ifdef FASTUIDRAW_DEBUG
        {
          output_glsl_source_code << "#define FASTUIDRAW_DEBUG\n";
        }
      #endif

      for(const SourcePrivate::source_code_t &src : d->m_values)
        {
          SourcePrivate::add_source_entry(src, output_glsl_source_code);
          SourcePrivate::add_source_entry(src, output_glsl_source_code_base);
        }

      /*
       * some GLSL pre-processors do not like to end on a
       * comment or other certain tokens, to make them
       * less grouchy, we emit a few extra \n's
       */
      output_glsl_source_code << "\n\n\n";
      output_glsl_source_code_base << "\n\n\n";

      d->m_assembled_code = output_glsl_source_code.str();
      d->m_assembled_code_base = output_glsl_source_code_base.str();
      d->m_dirty = false;
    }

  return code_only ?
    d->m_assembled_code_base.c_str():
    d->m_assembled_code.c_str();
}
