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
  enum varying_type
    {
      varying_float_smooth,
      varying_float_flat,
      varying_float_noperspective,
      varying_int,
      varying_uint,

      number_varying_types
    };

  static
  void
  absorb_varyings(std::map<std::string, enum varying_type> &present_varyings,
                  c_array<const c_string> varying_names, enum varying_type tp);

  static
  void
  absorb_varyings(std::map<std::string, enum varying_type> &present_varyings,
                  const varying_list *varying_names);

  static
  void
  add_varying(varying_list *dst,
              const std::string &name, enum varying_type tp);

  std::map<std::string, reference_counted_ptr<const T> > m_shaders;
  std::map<std::string, enum varying_type> m_varyings;
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
absorb_varyings(std::map<std::string, enum varying_type> &present_varyings,
                c_array<const c_string> varying_names, enum varying_type tp)
{
  for(c_string nm : varying_names)
    {
      FASTUIDRAWassert(present_varyings.find(nm) == present_varyings.end()
                       || present_varyings[nm] == tp);
      present_varyings[nm] = tp;
    }
}

template<typename T>
void
DependencyListPrivateT<T>::
absorb_varyings(std::map<std::string, enum varying_type> &present_varyings,
                const varying_list *varying_names)
{
  if (!varying_names)
    {
      return;
    }

  absorb_varyings(present_varyings, varying_names->floats(varying_list::interpolation_smooth), varying_float_smooth);
  absorb_varyings(present_varyings, varying_names->floats(varying_list::interpolation_flat), varying_float_flat);
  absorb_varyings(present_varyings, varying_names->floats(varying_list::interpolation_noperspective), varying_float_noperspective);
  absorb_varyings(present_varyings, varying_names->ints(), varying_int);
  absorb_varyings(present_varyings, varying_names->uints(), varying_uint);
}

template<typename T>
varying_list
DependencyListPrivateT<T>::
compute_varying_list(const varying_list &combine_with) const
{
  varying_list return_value;
  std::map<std::string, enum varying_type> tmp(m_varyings);

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
            const std::string &name, enum varying_type tp)
{
  c_string cname(name.c_str());

  switch(tp)
    {
    case varying_float_smooth:
      dst->add_float(cname, varying_list::interpolation_smooth);
      break;
    case varying_float_flat:
      dst->add_float(cname, varying_list::interpolation_flat);
      break;
    case varying_float_noperspective:
      dst->add_float(cname, varying_list::interpolation_noperspective);
      break;
    case varying_int:
      dst->add_int(cname);
      break;
    case varying_uint:
      dst->add_uint(cname);
      break;

    default:
      FASTUIDRAWassert(!"Bad enum value");
    }
}


}}}
