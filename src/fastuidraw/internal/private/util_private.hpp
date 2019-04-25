/*!
 * \file util_private.hpp
 * \brief file util_private.hpp
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

#include <iostream>
#include <vector>
#include <fastuidraw/util/c_array.hpp>

#define FASTUIDRAWwarn_assert(X) do {                           \
    FASTUIDRAWassert(X);                                        \
    if (!(X))                                                   \
      {                                                         \
        std::cerr << __FILE__ << ":" << __LINE__                \
                  << ": Assertion '" << #X << "' failed\n";    \
      }                                                         \
  } while(0)


namespace fastuidraw
{
  template<typename T>
  c_array<T>
  make_c_array(std::vector<T> &p)
  {
    if (p.empty())
      {
        return c_array<T>();
      }
    else
      {
        return c_array<T>(&p[0], p.size());
      }
  }

  template<typename T>
  c_array<const T>
  make_c_array(const std::vector<T> &p)
  {
    if (p.empty())
      {
        return c_array<const T>();
      }
    else
      {
        return c_array<const T>(&p[0], p.size());
      }
  }

  template<typename T>
  c_array<T>
  const_cast_c_array(c_array<const T> p)
  {
    T *q;
    q = const_cast<T*>(p.c_ptr());
    return c_array<T>(q, p.size());
  }
}

#define get_implement(class_name, class_name_private, type_name, member_name) \
  type_name                                                             \
  class_name::                                                          \
  member_name(void) const                                               \
  {                                                                     \
    class_name_private *d;                                              \
    d = static_cast<class_name_private*>(m_d);                          \
    return d->m_##member_name;                                          \
  }

#define get_implement_string(class_name, class_name_private, member_name) \
  fastuidraw::c_string                                                  \
  class_name::                                                          \
  member_name(void) const                                               \
  {                                                                     \
    class_name_private *d;                                              \
    d = static_cast<class_name_private*>(m_d);                          \
    return d->m_##member_name.c_str();                                  \
  }

#define set_implement(class_name, class_name_private, type_name, member_name) \
  class_name&                                                           \
  class_name::                                                          \
  member_name(type_name v)                                              \
  {                                                                     \
    class_name_private *d;                                              \
    d = static_cast<class_name_private*>(m_d);                          \
    d->m_##member_name = v;                                             \
    return *this;                                                       \
  }

#define set_implement_callback(class_name, class_name_private, type_name, member_name, callback) \
  class_name&                                                           \
  class_name::                                                          \
  member_name(type_name v)                                              \
  {                                                                     \
    class_name_private *d;                                              \
    d = static_cast<class_name_private*>(m_d);                          \
    d->m_##member_name = v;                                             \
    callback;                                                           \
    return *this;                                                       \
  }

#define set_implement_string(class_name, class_name_private, member_name) \
  class_name&                                                           \
  class_name::                                                          \
  member_name(c_string v)                                               \
  {                                                                     \
    class_name_private *d;                                              \
    d = static_cast<class_name_private*>(m_d);                          \
    d->m_##member_name = (v) ? v : "";                                  \
    return *this;                                                       \
  }

#define set_implement_string_callback(class_name, class_name_private, member_name, callback) \
  class_name&                                                           \
  class_name::                                                          \
  member_name(c_string v)                                               \
  {                                                                     \
    class_name_private *d;                                              \
    d = static_cast<class_name_private*>(m_d);                          \
    d->m_##member_name = (v) ? v : "";                                  \
    callback;                                                           \
    return *this;                                                       \
  }

#define setget_implement(class_name, class_name_private, type_name, member_name) \
  set_implement(class_name, class_name_private, type_name, member_name) \
  get_implement(class_name, class_name_private, type_name, member_name)

#define setget_implement_callback(class_name, class_name_private, type_name, member_name, callback) \
  set_implement_callback(class_name, class_name_private, type_name, member_name, callback) \
  get_implement(class_name, class_name_private, type_name, member_name)

#define setget_implement_string(class_name, class_name_private, member_name) \
  set_implement_string(class_name, class_name_private, member_name) \
  get_implement_string(class_name, class_name_private, member_name)

#define setget_implement_string_callback(class_name, class_name_private, member_name, callback) \
  set_implement_string_callback(class_name, class_name_private, member_name, callback) \
  get_implement_string(class_name, class_name_private, member_name)

#define copy_ctor(class_name, unscoped_class_name, class_name_private)  \
  class_name::                                                          \
  unscoped_class_name(const unscoped_class_name &obj)                   \
  {                                                                     \
    class_name_private *d;                                              \
    d = static_cast<class_name_private*>(obj.m_d);                      \
    m_d = FASTUIDRAWnew class_name_private(*d);                         \
  }

#define assign_swap_implement(class_name) \
  void                                    \
  class_name::                            \
  swap(class_name &obj)                   \
  {                                       \
    std::swap(m_d, obj.m_d);              \
  }                                       \
  class_name&                             \
  class_name::                            \
  operator=(const class_name &rhs)        \
  {                                       \
    if (this != &rhs)                     \
      {                                   \
        class_name v(rhs);                \
        swap(v);                          \
      }                                   \
    return *this;                         \
  }

#define FASTUIDRAWwarning(X) do {                  \
    std::cerr << "Warning: [" << __FILE__ << ", "  \
              << __LINE__ << "] " << #X << "\n";    \
  } while(0)
