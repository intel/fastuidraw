/*!
 * \file dependency_list.hpp
 * \brief file dependency_list.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#pragma once

#include <string>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/string_array.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

class CombinedShareableValues
{
public:
  void
  add_element(c_string name, const shareable_value_list &values)
  {
    m_values[name] = values;
  }

  shareable_value_list
  compute_value_list(const shareable_value_list &combine_with) const
  {
    shareable_value_list return_value(combine_with);
    for (const auto &v : m_values)
      {
        for (unsigned int i = 0; i < shareable_value_list::type_number_types; ++i)
          {
            enum shareable_value_list::type_t t;

            t = static_cast<enum shareable_value_list::type_t>(i);
            for (c_string name : v.second.shareable_values(t))
              {
                std::ostringstream str;

                str << v.first << "::" << name;
                return_value.add_shareable_value(str.str().c_str(), t);
              }
          }
      }
    return return_value;
  }

private:
  std::map<std::string, shareable_value_list> m_values;
};

template<typename T>
class DependencyListPrivateT
{
public:
  void
  add_element(c_string name, const reference_counted_ptr<const T> &shader,
              const varying_list *varyings);

  varying_list
  compute_varyings(const varying_list &combine_with) const;

  string_array
  compute_name_list(void) const;

  std::vector<reference_counted_ptr<const T> >
  compute_shader_list(void) const;

private:
  class PerShader
  {
  public:
    reference_counted_ptr<const T> m_shader;
    varying_list m_varyings;
  };

  class EqClass:public reference_counted<EqClass>::non_concurrent
  {
  public:
    EqClass(void):
      m_type(varying_list::interpolator_number_types),
      m_added(false)
    {}

    std::string
    extract_double_colon_value(void);

    std::set<std::string> m_names;
    enum varying_list::interpolator_type_t m_type;
    bool m_added;
  };

  class VaryingTracker
  {
  public:
    void
    add_to_tracker(c_string prefix, const varying_list &src);

    void
    add_varyings_from_tracker(varying_list *dst);

  private:
    typedef reference_counted_ptr<EqClass> EqClassRef;

    static
    std::string
    make_name(c_string prefix, c_string name);

    void
    add_varying(const std::string &name,
                enum varying_list::interpolator_type_t q);

    void
    add_alias(const std::string &name, const std::string &src_name);

    std::map<std::string, EqClassRef> m_data;
  };

  static
  void
  add_varyings(c_string prefix, const varying_list &src, varying_list *dst);

  std::map<std::string, PerShader> m_shaders;
};

template<typename T>
class DependencySymbolListPrivate:public DependencyListPrivateT<T>
{
public:
  void
  add_element(c_string name, const reference_counted_ptr<const T> &shader,
              const symbol_list &symbols)
  {
    DependencyListPrivateT<T>::add_element(name, shader, &symbols.m_varying_list);
    m_vert.add_element(name, symbols.m_vert_shareable_values);
    m_frag.add_element(name, symbols.m_frag_shareable_values);
  }

  symbol_list
  compute_symbols(const symbol_list &combine_with) const
  {
    return symbol_list(DependencyListPrivateT<T>::compute_varyings(combine_with.m_varying_list),
                       m_vert.compute_value_list(combine_with.m_vert_shareable_values),
                       m_frag.compute_value_list(combine_with.m_frag_shareable_values));
  }

private:
  CombinedShareableValues m_vert, m_frag;
};

//////////////////////////////////////////////
// DependencyListPrivateT<T>::EqClass methods
template<typename T>
std::string
DependencyListPrivateT<T>::EqClass::
extract_double_colon_value(void)
{
  std::string return_value;
  std::set<std::string>::iterator iter;

  for (iter = m_names.begin(); iter != m_names.end(); ++iter)
    {
      if (iter->find("::") != std::string::npos)
        {
          return_value = *iter;
          m_names.erase(iter);
          return return_value;
        }
    }

  iter = m_names.begin();
  return_value = *iter;
  m_names.erase(iter);

  return return_value;
}

/////////////////////////////////////////////////////
// DependencyListPrivateT<T>::VaryingTracker methods
template<typename T>
std::string
DependencyListPrivateT<T>::VaryingTracker::
make_name(c_string prefix, c_string name)
{
  FASTUIDRAWassert(name);
  if (!prefix)
    {
      return name;
    }

  std::ostringstream tmp;
  tmp << prefix << "::" << name;
  return tmp.str();
}

template<typename T>
void
DependencyListPrivateT<T>::VaryingTracker::
add_varying(const std::string &name,
            enum varying_list::interpolator_type_t q)
{
  EqClassRef &ref(m_data[name]);
  if (!ref)
    {
      ref = FASTUIDRAWnew EqClass();
    }

  ref->m_names.insert(name);
  FASTUIDRAWmessaged_assert(ref->m_type == varying_list::interpolator_number_types
                            || ref->m_type == q,
                            "Shader aliases merge across different varying types");
  ref->m_type = q;
}

template<typename T>
void
DependencyListPrivateT<T>::VaryingTracker::
add_alias(const std::string &name, const std::string &src_name)
{
  EqClassRef &pref1(m_data[name]);
  EqClassRef &pref2(m_data[src_name]);

  if (!pref1)
    {
      pref1 = FASTUIDRAWnew EqClass();
    }

  if (!pref2)
    {
      pref2 = FASTUIDRAWnew EqClass();
    }

  EqClassRef r1(pref1);
  EqClassRef r2(pref2);

  r1->m_names.insert(name);
  r2->m_names.insert(src_name);
  if (r1 != r2)
    {
      for (const std::string &nm : r2->m_names)
        {
          r1->m_names.insert(nm);

          EqClassRef &ref(m_data[nm]);
          if (ref != r1)
            {
              ref = r1;
            }
        }

      FASTUIDRAWmessaged_assert(r1->m_type == varying_list::interpolator_number_types
                                || r1->m_type == r2->m_type,
                                "Shader aliases merge across different varying types");
      if (r1->m_type == varying_list::interpolator_number_types)
        {
          r1->m_type = r2->m_type;
        }
    }
}

template<typename T>
void
DependencyListPrivateT<T>::VaryingTracker::
add_to_tracker(c_string prefix, const varying_list &src)
{
  for (unsigned int i = 0; i < varying_list::interpolator_number_types; ++i)
    {
      enum varying_list::interpolator_type_t q;
      q = static_cast<enum varying_list::interpolator_type_t>(i);

      for (c_string v : src.varyings(q))
        {
          add_varying(make_name(prefix, v), q);
        }
    }

  c_array<const c_string> names(src.alias_varying_names());
  c_array<const c_string> src_names(src.alias_varying_source_names());
  FASTUIDRAWassert(names.size() == src_names.size());
  for (unsigned int i = 0; i < names.size(); ++i)
    {
      add_alias(make_name(prefix, names[i]).c_str(),
                make_name(prefix, src_names[i]).c_str());
    }
}

template<typename T>
void
DependencyListPrivateT<T>::VaryingTracker::
add_varyings_from_tracker(varying_list *dst)
{
  for (const auto &e : m_data)
    {
      EqClassRef ref(e.second);

      FASTUIDRAWassert(ref);
      if (!ref->m_added)
        {
          FASTUIDRAWassert(!ref->m_names.empty());

          ref->m_added = true;
          FASTUIDRAWmessaged_assert(ref->m_type != varying_list::interpolator_number_types,
                                    "Shader alias chain lacks alias to actual varying");

          /* find an entry that does not have :: in it */
          std::string vname(ref->extract_double_colon_value());

          dst->add_varying(vname.c_str(), ref->m_type);
          for (const std::string &nm : ref->m_names)
            {
              dst->add_varying_alias(nm.c_str(), vname.c_str());
            }
        }
    }
}

/////////////////////////////////////////////
// DependencyListPrivateT<T> methods
template<typename T>
void
DependencyListPrivateT<T>::
add_element(c_string pname,
            const reference_counted_ptr<const T> &shader,
            const varying_list *varyings)
{
  FASTUIDRAWassert(pname && shader);

  std::string name(pname);
  FASTUIDRAWassert(m_shaders.find(name) == m_shaders.end());

  PerShader &sh(m_shaders[name]);
  sh.m_shader = shader;
  if (varyings)
    {
      sh.m_varyings = *varyings;
    }
}

template<typename T>
string_array
DependencyListPrivateT<T>::
compute_name_list(void) const
{
  string_array return_value;
  for (const auto &v : m_shaders)
    {
      return_value.push_back(v.first.c_str());
    }
  return return_value;
}

template<typename T>
std::vector<reference_counted_ptr<const T> >
DependencyListPrivateT<T>::
compute_shader_list(void) const
{
  std::vector<reference_counted_ptr<const T> > return_value;
  for (const auto &v : m_shaders)
    {
      return_value.push_back(v.second.m_shader);
    }
  return return_value;
}

template<typename T>
varying_list
DependencyListPrivateT<T>::
compute_varyings(const varying_list &combine_with) const
{
  VaryingTracker tracker;
  varying_list return_value;

  for (const auto &v : m_shaders)
    {
      tracker.add_to_tracker(v.first.c_str(), v.second.m_varyings);
    }
  tracker.add_to_tracker(nullptr, combine_with);
  tracker.add_varyings_from_tracker(&return_value);

  return return_value;
}

}}}
