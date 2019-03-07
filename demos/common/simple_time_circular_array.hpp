/*!
 * \file simple_time_circular_array.hpp
 * \brief file simple_time_circular_array.hpp
 *
 * Copyright 2019 by Intel.
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

#include "simple_time.hpp"
#include <fastuidraw/util/vecN.hpp>

template<unsigned int N>
class simple_time_circular_array
{
public:
  simple_time_circular_array(void):
    m_current(0),
    m_total(1)
  {
  }

  void
  advance(void)
  {
    m_current = (m_current == static_cast<int>(N)) ? 0 : m_current + 1;
    m_times[m_current].restart();
    ++m_total;
  }

  int32_t
  elapsed(int num_ago) const
  {
    return m_times[get_index(num_ago)].elapsed();
  }

  int64_t
  elapsed_us(int num_ago) const
  {
    return m_times[get_index(num_ago)].elapsed_us();
  }

  int32_t
  oldest_elapsed(int *num_ago) const
  {
    return m_times[get_oldest(num_ago)].elapsed();
  }

  int64_t
  oldest_elapsed_us(int *num_ago) const
  {
    return m_times[get_oldest(num_ago)].elapsed_us();
  }

private:
  int
  get_index(int num_ago) const
  {
    FASTUIDRAWassert(num_ago >= 0);
    FASTUIDRAWassert(num_ago <= static_cast<int>(N));
    FASTUIDRAWassert(num_ago <= m_total);
    int v(m_current - num_ago);
    if (v < 0)
      {
        v += (N + 1u);
      }
    return v;
  }

  int
  get_oldest(int *num_ago) const
  {
    *num_ago = fastuidraw::t_min(N, static_cast<unsigned int>(m_total));
    return get_index(*num_ago);
  }

  int m_current, m_total;
  fastuidraw::vecN<simple_time, N + 1u> m_times;
};
