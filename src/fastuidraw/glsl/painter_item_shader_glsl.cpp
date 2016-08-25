/*!
 * \file painter_item_shader_glsl.cpp
 * \brief file painter_item_shader_glsl.cpp
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


#include <vector>
#include <string>
#include <list>
#include <sstream>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>

#include "../private/util_private.hpp"

namespace
{
  class StringArray
  {
  public:
    StringArray(void);
    StringArray(const StringArray &rhs);
    ~StringArray();

    StringArray&
    operator=(const StringArray &rhs);

    fastuidraw::const_c_array<const char*>
    string_array(void) const;

    void
    implement_set(unsigned int slot, const std::string &pname);

  private:

    void
    add_string(const std::string &str);

    template<typename iterator>
    void
    add_strings(iterator begin, iterator end);

    void
    clear(void);

    std::vector<std::string*> m_strings;
    std::vector<const char*> m_strings_array;
  };

  class VaryingListPrivate
  {
  public:
    enum
      {
        interpolation_number_types = fastuidraw::glsl::varying_list::interpolation_number_types
      };

    fastuidraw::vecN<StringArray, interpolation_number_types> m_floats;
    StringArray m_ints;
    StringArray m_uints;
    fastuidraw::vecN<size_t, interpolation_number_types> m_float_counts;
  };

  class GLSLShaderUnpackValuePrivate
  {
  public:
    GLSLShaderUnpackValuePrivate(const char *p, enum fastuidraw::glsl::shader_unpack_value::type_t t):
      m_name(p),
      m_type(t)
    {}

    std::string m_name;
    enum fastuidraw::glsl::shader_unpack_value::type_t m_type;

    static
    unsigned int
    stream_unpack_code(unsigned int alignment, std::ostream &str,
                       fastuidraw::const_c_array<fastuidraw::glsl::shader_unpack_value> labels,
                       const char *offset_name,
                       const char *prefix);
  };

  class PainterShaderGLSLPrivate
  {
  public:
    PainterShaderGLSLPrivate(const fastuidraw::glsl::ShaderSource &vertex_src,
                           const fastuidraw::glsl::ShaderSource &fragment_src,
                           const fastuidraw::glsl::varying_list &varyings):
      m_vertex_src(vertex_src),
      m_fragment_src(fragment_src),
      m_varyings(varyings)
    {}

    fastuidraw::glsl::ShaderSource m_vertex_src;
    fastuidraw::glsl::ShaderSource m_fragment_src;
    fastuidraw::glsl::varying_list m_varyings;

  };
}

////////////////////////////////////////////////
// StringArray methods
StringArray::
StringArray(void)
{}

StringArray::
StringArray(const StringArray &rhs)
{
  fastuidraw::const_c_array<const char*> src(rhs.string_array());
  add_strings(src.begin(), src.end());
}

StringArray::
~StringArray()
{
  clear();
}

StringArray&
StringArray::
operator=(const StringArray &rhs)
{
  clear();
  fastuidraw::const_c_array<const char*> src(rhs.string_array());
  add_strings(src.begin(), src.end());
  return *this;
}

void
StringArray::
add_string(const std::string &str)
{
  m_strings.push_back(FASTUIDRAWnew std::string(str));
  m_strings_array.push_back(m_strings.back()->c_str());
}

template<typename iterator>
void
StringArray::
add_strings(iterator begin, iterator end)
{
  for(; begin != end; ++begin)
    {
      add_string(*begin);
    }
}

void
StringArray::
clear(void)
{
  for(std::vector<std::string*>::iterator iter = m_strings.begin(),
        end = m_strings.end(); iter != end; ++iter)
    {
      FASTUIDRAWdelete(*iter);
    }
  m_strings.clear();
  m_strings_array.clear();
}

fastuidraw::const_c_array<const char*>
StringArray::
string_array(void) const
{
  return fastuidraw::make_c_array(m_strings_array);
}

void
StringArray::
implement_set(unsigned int slot, const std::string &pname)
{
  while(slot >= m_strings.size())
    {
      add_string("");
    }

  assert(slot < m_strings.size());
  *m_strings[slot] = pname;
  m_strings_array[slot] = m_strings[slot]->c_str();
}

//////////////////////////////////////
// GLSLShaderUnpackValuePrivate methods
unsigned int
GLSLShaderUnpackValuePrivate::
stream_unpack_code(unsigned int alignment, std::ostream &str,
                   fastuidraw::const_c_array<fastuidraw::glsl::shader_unpack_value> labels,
                   const char *offset_name,
                   const char *prefix)
{
  const char *uint_types[5] =
    {
      "",
      "uint",
      "uvec2",
      "uvec3",
      "uvec4"
    };
  const char ext_component[] = "xyzw";

  unsigned int number_blocks;
  number_blocks = labels.size() / alignment;
  if(number_blocks * alignment < labels.size())
    {
      ++number_blocks;
    }

  str << "{\n"
      << uint_types[alignment] << " utemp;\n";

  for(unsigned int b = 0, i = 0; b < number_blocks; ++b)
    {
      /* fetch the data from the store
       */
      str << "utemp.";
      for(unsigned int k = 0, kk = i; k < alignment && kk < labels.size(); ++k, ++kk)
        {
          str << ext_component[k];
        }
      str << " = fastuidraw_fetch_data("
          << "int(" << offset_name << ") + " << b << ").";
      for(unsigned int k = 0, kk = i; k < alignment && kk < labels.size(); ++k, ++kk)
        {
          str << ext_component[k];
        }
      str << ";\n";

      /* perform bit cast to correct type.
       */
      for(unsigned int k = 0; k < alignment && i < labels.size(); ++k, ++i)
        {
          switch(labels[i].type())
            {
            case fastuidraw::glsl::shader_unpack_value::int_type:
              str << prefix << labels[i].name() << " = " << "int(utemp." << ext_component[k] << ");\n";
              break;
            case fastuidraw::glsl::shader_unpack_value::uint_type:
              str << prefix << labels[i].name() << " = " << "utemp." << ext_component[k] << ";\n";
              break;
            default:
            case fastuidraw::glsl::shader_unpack_value::float_type:
              str << prefix << labels[i].name() << " = " << "uintBitsToFloat(utemp." << ext_component[k] << ");\n";
              break;
            }
        }
    }
  str << "}\n";
  return number_blocks;
}

/////////////////////////////////////////////
// fastuidraw::glsl::varying_list methods
fastuidraw::glsl::varying_list::
varying_list(void)
{
  m_d = FASTUIDRAWnew VaryingListPrivate();
}

fastuidraw::glsl::varying_list::
varying_list(const varying_list &rhs)
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew VaryingListPrivate(*d);
}

fastuidraw::glsl::varying_list::
~varying_list()
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
operator=(const varying_list &rhs)
{
  if(this != &rhs)
    {
      VaryingListPrivate *d, *rhs_d;
      d = reinterpret_cast<VaryingListPrivate*>(m_d);
      rhs_d = reinterpret_cast<VaryingListPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

fastuidraw::const_c_array<const char*>
fastuidraw::glsl::varying_list::
floats(enum interpolation_qualifier_t q) const
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  return d->m_floats[q].string_array();
}

fastuidraw::const_c_array<size_t>
fastuidraw::glsl::varying_list::
float_counts(void) const
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  return const_c_array<size_t>(&d->m_float_counts[0], d->m_float_counts.size());
}

fastuidraw::const_c_array<const char*>
fastuidraw::glsl::varying_list::
uints(void) const
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  return d->m_uints.string_array();
}

fastuidraw::const_c_array<const char*>
fastuidraw::glsl::varying_list::
ints(void) const
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  return d->m_ints.string_array();
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
set_float_varying(unsigned int slot, const char *pname,
                  enum interpolation_qualifier_t q)
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  d->m_floats[q].implement_set(slot, pname);
  d->m_float_counts[q] = d->m_floats[q].string_array().size();
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_float_varying(const char *pname, enum interpolation_qualifier_t q)
{
  return set_float_varying(floats(q).size(), pname, q);
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
set_uint_varying(unsigned int slot, const char *pname)
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  d->m_uints.implement_set(slot, pname);
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_uint_varying(const char *pname)
{
  return set_uint_varying(uints().size(), pname);
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
set_int_varying(unsigned int slot, const char *pname)
{
  VaryingListPrivate *d;
  d = reinterpret_cast<VaryingListPrivate*>(m_d);
  d->m_ints.implement_set(slot, pname);
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_int_varying(const char *pname)
{
  return set_int_varying(ints().size(), pname);
}


///////////////////////////////////////////////////
// fastuidraw::glsl::shader_unpack_value methods
fastuidraw::glsl::shader_unpack_value::
shader_unpack_value(const char *pname, type_t ptype)
{
  m_d = FASTUIDRAWnew GLSLShaderUnpackValuePrivate(pname, ptype);
}

fastuidraw::glsl::shader_unpack_value::
shader_unpack_value(const shader_unpack_value &obj)
{
  GLSLShaderUnpackValuePrivate *d;
  d = reinterpret_cast<GLSLShaderUnpackValuePrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew GLSLShaderUnpackValuePrivate(*d);
}

fastuidraw::glsl::shader_unpack_value::
~shader_unpack_value()
{
  GLSLShaderUnpackValuePrivate *d;
  d = reinterpret_cast<GLSLShaderUnpackValuePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::glsl::shader_unpack_value&
fastuidraw::glsl::shader_unpack_value::
operator=(const shader_unpack_value &rhs)
{
  if(this != &rhs)
    {
      GLSLShaderUnpackValuePrivate *d, *rhs_d;
      d = reinterpret_cast<GLSLShaderUnpackValuePrivate*>(m_d);
      rhs_d = reinterpret_cast<GLSLShaderUnpackValuePrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

const char*
fastuidraw::glsl::shader_unpack_value::
name(void) const
{
  GLSLShaderUnpackValuePrivate *d;
  d = reinterpret_cast<GLSLShaderUnpackValuePrivate*>(m_d);
  return d->m_name.c_str();
}

enum fastuidraw::glsl::shader_unpack_value::type_t
fastuidraw::glsl::shader_unpack_value::
type(void) const
{
  GLSLShaderUnpackValuePrivate *d;
  d = reinterpret_cast<GLSLShaderUnpackValuePrivate*>(m_d);
  return d->m_type;
}

unsigned int
fastuidraw::glsl::shader_unpack_value::
stream_unpack_code(unsigned int alignment, glsl::ShaderSource &src,
                   const_c_array<shader_unpack_value> labels,
                   const char *offset_name,
                   const char *prefix)
{
  std::ostringstream str;
  unsigned int return_value;

  return_value = GLSLShaderUnpackValuePrivate::stream_unpack_code(alignment, str, labels, offset_name, prefix);
  src.add_source(str.str().c_str(), glsl::ShaderSource::from_string);

  return return_value;
}


unsigned int
fastuidraw::glsl::shader_unpack_value::
stream_unpack_function(unsigned int alignment, glsl::ShaderSource &src,
                       const_c_array<shader_unpack_value> labels,
                       const char *function_name,
                       const char *out_type,
                       bool has_return_value)
{
  unsigned int number_blocks;
  std::ostringstream str;

  if(has_return_value)
      {
        str << "uint\n";
      }
    else
      {
        str << "void\n";
      }
  str << function_name << "(in uint location, out " << out_type << " out_value)\n"
      << "{";

  number_blocks = GLSLShaderUnpackValuePrivate::stream_unpack_code(alignment, str, labels, "location", "out_value");

  if(has_return_value)
    {
      str << "return uint(" << number_blocks << ") + location;\n";
    }
  str << "}\n\n";
  src.add_source(str.str().c_str(), glsl::ShaderSource::from_string);

  return number_blocks;
}

///////////////////////////////////////////////
// fastuidraw::glsl::PainterItemShaderGLSL methods
fastuidraw::glsl::PainterItemShaderGLSL::
PainterItemShaderGLSL(const glsl::ShaderSource &v_src,
                      const glsl::ShaderSource &f_src,
                      const varying_list &varyings,
                      unsigned int num_sub_shaders):
  PainterItemShader(num_sub_shaders)
{
  m_d = FASTUIDRAWnew PainterShaderGLSLPrivate(v_src, f_src, varyings);
}

fastuidraw::glsl::PainterItemShaderGLSL::
~PainterItemShaderGLSL(void)
{
  PainterShaderGLSLPrivate *d;
  d = reinterpret_cast<PainterShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::glsl::varying_list&
fastuidraw::glsl::PainterItemShaderGLSL::
varyings(void) const
{
  PainterShaderGLSLPrivate *d;
  d = reinterpret_cast<PainterShaderGLSLPrivate*>(m_d);
  return d->m_varyings;
}

const fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::PainterItemShaderGLSL::
vertex_src(void) const
{
  PainterShaderGLSLPrivate *d;
  d = reinterpret_cast<PainterShaderGLSLPrivate*>(m_d);
  return d->m_vertex_src;
}

const fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::PainterItemShaderGLSL::
fragment_src(void) const
{
  PainterShaderGLSLPrivate *d;
  d = reinterpret_cast<PainterShaderGLSLPrivate*>(m_d);
  return d->m_fragment_src;
}
