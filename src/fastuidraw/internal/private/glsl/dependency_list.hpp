/*!
 * \file dependency_list.hpp
 * \brief file dependency_list.hpp
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

#pragma once

#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/string_array.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>

namespace fastuidraw { namespace glsl { namespace detail {

template<typename T>
class DependencyListPrivateT
{
public:
  void
  add_element(c_string name, const reference_counted_ptr<const T> &shader,
              const varying_list *varyings);

  varying_list
  compute_varying_list(const varying_list &combine_with) const;

  string_array
  compute_name_list(void) const;

  std::vector<reference_counted_ptr<const T> >
  compute_shader_list(void) const;

private:
  enum varying_type_t
    {
      varying_float_smooth,
      varying_float_flat,
      varying_float_noperspective,
      varying_int,
      varying_uint,
      varying_alias,

      number_varying_types
    };

  class varying_type
  {
  public:
    varying_type(enum varying_type_t tp = number_varying_types):
      m_tp(tp)
    {}

    varying_type(const std::string &v):
      m_alias(v),
      m_tp(varying_alias)
    {}

    bool
    operator==(const std::string &v) const
    {
      return m_tp == varying_alias
        && m_alias == v;
    }

    std::string m_alias;
    enum varying_type_t m_tp;
  };

  static
  void
  absorb_varyings(std::map<std::string, varying_type> &present_varyings,
                  c_array<const c_string> varying_names, enum varying_type_t tp);

  static
  void
  absorb_varyings(std::map<std::string, varying_type> &present_varyings,
                  const varying_list *varying_names);

  static
  void
  add_varying(varying_list *dst, const std::string &name, varying_type tp);

  std::map<std::string, reference_counted_ptr<const T> > m_shaders;
  std::map<std::string, varying_type> m_varyings;
};


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
      return_value.push_back(v.second);
    }
  return return_value;
}

template<typename T>
void
DependencyListPrivateT<T>::
add_element(c_string pname,
            const reference_counted_ptr<const T> &shader,
            const varying_list *varyings)
{
  FASTUIDRAWassert(pname && shader);

  std::string name(pname);

  FASTUIDRAWassert(name.length() > 0 && m_shaders.find(name) == m_shaders.end());

  absorb_varyings(m_varyings, varyings);
  m_shaders[name] = shader;
}

template<typename T>
void
DependencyListPrivateT<T>::
absorb_varyings(std::map<std::string, varying_type> &present_varyings,
                c_array<const c_string> varying_names, enum varying_type_t tp)
{
  for(c_string nm : varying_names)
    {
      FASTUIDRAWassert(present_varyings.find(nm) == present_varyings.end()
                       || present_varyings[nm].m_tp == tp);
      present_varyings[nm] = tp;
    }
}

template<typename T>
void
DependencyListPrivateT<T>::
absorb_varyings(std::map<std::string, varying_type> &present_varyings,
                const varying_list *varying_names)
{
  if (!varying_names)
    {
      return;
    }

  absorb_varyings(present_varyings, varying_names->varyings(varying_list::interpolator_smooth), varying_float_smooth);
  absorb_varyings(present_varyings, varying_names->varyings(varying_list::interpolator_flat), varying_float_flat);
  absorb_varyings(present_varyings, varying_names->varyings(varying_list::interpolator_noperspective), varying_float_noperspective);
  absorb_varyings(present_varyings, varying_names->varyings(varying_list::interpolator_int), varying_int);
  absorb_varyings(present_varyings, varying_names->varyings(varying_list::interpolator_uint), varying_uint);

  c_array<const c_string> names(varying_names->alias_list_names());
  c_array<const c_string> aliases(varying_names->alias_list_alias_names());
  FASTUIDRAWassert(names.size() == aliases.size());
  for (unsigned int i = 0; i < names.size(); ++i)
    {
      std::string key(aliases[i]);
      std::string value(names[i]);
      FASTUIDRAWassert(present_varyings.find(key) == present_varyings.end()
                       || present_varyings[key] == value);
      present_varyings[key] = value;
    }
}

template<typename T>
varying_list
DependencyListPrivateT<T>::
compute_varying_list(const varying_list &combine_with) const
{
  varying_list return_value;
  std::map<std::string, varying_type> tmp(m_varyings);

  absorb_varyings(tmp, &combine_with);
  for (const auto &v : tmp)
    {
      add_varying(&return_value, v.first, v.second);
    }

  return return_value;
}


template<typename T>
void
DependencyListPrivateT<T>::
add_varying(varying_list *dst,
            const std::string &name, varying_type tp)
{
  c_string cname(name.c_str());

  switch(tp.m_tp)
    {
    case varying_float_smooth:
      dst->add_float(cname);
      break;
    case varying_float_flat:
      dst->add_float_flat(cname);
      break;
    case varying_float_noperspective:
      dst->add_float_noperspective(cname);
      break;
    case varying_int:
      dst->add_int(cname);
      break;
    case varying_uint:
      dst->add_uint(cname);
      break;
    case varying_alias:
      dst->add_alias(tp.m_alias.c_str(), cname);
      break;
    default:
      FASTUIDRAWassert(!"Bad enum value");
    }
}


}}}
