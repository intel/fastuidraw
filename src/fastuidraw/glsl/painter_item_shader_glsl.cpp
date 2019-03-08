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
#include <algorithm>
#include <sstream>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/util.hpp>
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
    operator=(const StringArray &rhs) = delete;

    fastuidraw::c_array<const fastuidraw::c_string>
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
    std::vector<fastuidraw::c_string> m_strings_array;
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

  class PainterShaderGLSLPrivateCommon
  {
  public:
    PainterShaderGLSLPrivateCommon(const fastuidraw::glsl::ShaderSource &vertex_src,
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

  typedef PainterShaderGLSLPrivateCommon PainterItemCoverageShaderGLSLPrivate;

  class PainterItemShaderGLSLPrivate:public PainterShaderGLSLPrivateCommon
  {
  public:
    PainterItemShaderGLSLPrivate(bool uses_discard,
                                 const fastuidraw::glsl::ShaderSource &vertex_src,
                                 const fastuidraw::glsl::ShaderSource &fragment_src,
                                 const fastuidraw::glsl::varying_list &varyings,
                                 const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemCoverageShaderGLSL> &cvg):
      PainterShaderGLSLPrivateCommon(vertex_src, fragment_src, varyings),
      m_uses_discard(uses_discard),
      m_coverage_shader(cvg)
    {}

    bool m_uses_discard;
    fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemCoverageShaderGLSL> m_coverage_shader;
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
  fastuidraw::c_array<const fastuidraw::c_string> src(rhs.string_array());
  add_strings(src.begin(), src.end());
}

StringArray::
~StringArray()
{
  clear();
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
  for(std::string *q : m_strings)
    {
      FASTUIDRAWdelete(q);
    }
  m_strings.clear();
  m_strings_array.clear();
}

fastuidraw::c_array<const fastuidraw::c_string>
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

  FASTUIDRAWassert(slot < m_strings.size());
  *m_strings[slot] = pname;
  m_strings_array[slot] = m_strings[slot]->c_str();
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
  d = static_cast<VaryingListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew VaryingListPrivate(*d);
}

fastuidraw::glsl::varying_list::
~varying_list()
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::glsl::varying_list)

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
floats(enum interpolation_qualifier_t q) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_floats[q].string_array();
}

fastuidraw::c_array<const size_t>
fastuidraw::glsl::varying_list::
float_counts(void) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return c_array<const size_t>(&d->m_float_counts[0], d->m_float_counts.size());
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
uints(void) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_uints.string_array();
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
ints(void) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_ints.string_array();
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
set_float_varying(unsigned int slot, c_string pname,
                  enum interpolation_qualifier_t q)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  d->m_floats[q].implement_set(slot, pname);
  d->m_float_counts[q] = d->m_floats[q].string_array().size();
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_float_varying(c_string pname, enum interpolation_qualifier_t q)
{
  return set_float_varying(floats(q).size(), pname, q);
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
set_uint_varying(unsigned int slot, c_string pname)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  d->m_uints.implement_set(slot, pname);
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_uint_varying(c_string pname)
{
  return set_uint_varying(uints().size(), pname);
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
set_int_varying(unsigned int slot, c_string pname)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  d->m_ints.implement_set(slot, pname);
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_int_varying(c_string pname)
{
  return set_int_varying(ints().size(), pname);
}

/////////////////////////////////////////////
// fastuidraw::glsl::PainterItemShaderGLSL methods
fastuidraw::glsl::PainterItemShaderGLSL::
PainterItemShaderGLSL(bool puses_discard,
                      const glsl::ShaderSource &v_src,
                      const glsl::ShaderSource &f_src,
                      const varying_list &varyings,
                      unsigned int num_sub_shaders,
                      const reference_counted_ptr<PainterItemCoverageShaderGLSL> &cvg):
  PainterItemShader(num_sub_shaders)
{
  m_d = FASTUIDRAWnew PainterItemShaderGLSLPrivate(puses_discard, v_src, f_src, varyings, cvg);
}

fastuidraw::glsl::PainterItemShaderGLSL::
~PainterItemShaderGLSL(void)
{
  PainterItemShaderGLSLPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              const fastuidraw::glsl::varying_list&,
              varyings)

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              vertex_src)

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              fragment_src)

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              bool,
              uses_discard)

///////////////////////////////////////////////
// fastuidraw::glsl::PainterItemCoverageShaderGLSL methods
fastuidraw::glsl::PainterItemCoverageShaderGLSL::
PainterItemCoverageShaderGLSL(const glsl::ShaderSource &v_src,
                              const glsl::ShaderSource &f_src,
                              const varying_list &varyings,
                              unsigned int num_sub_shaders):
  PainterItemCoverageShader(num_sub_shaders)
{
  m_d = FASTUIDRAWnew PainterItemCoverageShaderGLSLPrivate(v_src, f_src, varyings);
}

fastuidraw::glsl::PainterItemCoverageShaderGLSL::
~PainterItemCoverageShaderGLSL(void)
{
  PainterItemCoverageShaderGLSLPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

get_implement(fastuidraw::glsl::PainterItemCoverageShaderGLSL,
              PainterItemCoverageShaderGLSLPrivate,
              const fastuidraw::glsl::varying_list&,
              varyings)

get_implement(fastuidraw::glsl::PainterItemCoverageShaderGLSL,
              PainterItemCoverageShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              vertex_src)

get_implement(fastuidraw::glsl::PainterItemCoverageShaderGLSL,
              PainterItemCoverageShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              fragment_src)
