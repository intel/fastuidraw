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
#include <sstream>
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

    void
    add_to_varyings(const std::string &name, varying_list *dst) const;
  };

  std::map<std::string, PerShader> m_shaders;
};

template<typename T>
void
DependencyListPrivateT<T>::PerShader::
add_to_varyings(const std::string &name, varying_list *dst) const
{
  for (unsigned int i = 0; i < varying_list::interpolator_number_types; ++i)
    {
      enum varying_list::interpolator_type_t q;
      q = static_cast<enum varying_list::interpolator_type_t>(i);

      for (c_string v : m_varyings.varyings(q))
        {
          std::ostringstream tmp;
          tmp << name << "_" << v;
          dst->add_varying(tmp.str().c_str(), q);
        }
    }
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
  varying_list return_value(combine_with);
  for (const auto &v : m_shaders)
    {
      v.second.add_to_varyings(v.first, &return_value);
    }
  return return_value;
}

}}}
