/*!
 * \file static_resource.cpp
 * \brief file static_resource.cpp
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


#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <cstring>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/static_resource.hpp>

#include <private/util_private.hpp>

namespace
{
  class resource_hoard:fastuidraw::noncopyable
  {
  public:
    std::map<std::string, std::vector<uint8_t> > m_data;
    std::mutex m_mutex;
  };

  static
  resource_hoard&
  hoard(void)
  {
    static resource_hoard R;
    return R;
  }
}

void
fastuidraw::
generate_static_resource(c_string presource_label, c_array<const uint8_t> pvalue)
{
  std::string sresource_label(presource_label);
  std::vector<uint8_t> svalue(pvalue.begin(), pvalue.end());
  hoard().m_mutex.lock();
  FASTUIDRAWassert(hoard().m_data.find(sresource_label) == hoard().m_data.end());
  hoard().m_data[sresource_label].swap(svalue);
  hoard().m_mutex.unlock();
}

void
fastuidraw::
generate_static_resource(c_string presource_label, c_string pvalue)
{
  const uint8_t *p;

  FASTUIDRAWassert(pvalue);
  p = reinterpret_cast<const uint8_t*>(pvalue);
  generate_static_resource(presource_label,
                           c_array<const uint8_t>(p, std::strlen(pvalue) + 1));
}

fastuidraw::c_array<const uint8_t>
fastuidraw::
fetch_static_resource(c_string presource_label)
{
  c_array<const uint8_t> return_value;
  std::string sresource_label(presource_label);
  std::map<std::string, std::vector<uint8_t> >::const_iterator iter;

  hoard().m_mutex.lock();
  iter = hoard().m_data.find(sresource_label);
  if (iter != hoard().m_data.end())
    {
      return_value = make_c_array(iter->second);
    }
  hoard().m_mutex.unlock();
  return return_value;
}

///////////////////////////////////////
// fastuidraw::static_resource methods
fastuidraw::static_resource::
static_resource(c_string resource_label, c_array<const uint8_t> value)
{
  generate_static_resource(resource_label, value);
}
