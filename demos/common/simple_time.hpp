/*!
 * \file simple_time.hpp
 * \brief file simple_time.hpp
 *
 * Adapted from: WRATHTime.hpp of WRATH:
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
 *
 */



#pragma once

#include <chrono>

#include <stdint.h>

using namespace std::chrono;

class simple_time
{
public:

  simple_time(void)
  {
    m_start_time = steady_clock::now();
  }

  int32_t
  elapsed(void)
  {
    return time_difference_ms(steady_clock::now(), m_start_time);
  }

  int32_t
  restart(void)
  {
    auto current_time = steady_clock::now();

    auto return_value = time_difference_ms(current_time, m_start_time);
    m_start_time = current_time;

    return return_value;
  }


  //
  int64_t
  elapsed_us(void)
  {
    return time_difference_us(steady_clock::now(), m_start_time);
  }

  int64_t
  restart_us(void)
  {
    auto current_time = steady_clock::now();

    auto return_value = time_difference_us(current_time, m_start_time);
    m_start_time = current_time;

    return return_value;
  }

private:

  static
  int32_t
  time_difference_ms(const time_point<steady_clock> &end, const time_point<steady_clock> &begin)
  {
    return duration_cast<milliseconds>(end - begin).count();
  }

  static
  int64_t
  time_difference_us(const time_point<steady_clock> &end, const time_point<steady_clock> &begin)
  {
    return duration_cast<microseconds>(end - begin).count();
  }

  time_point<steady_clock> m_start_time;
};
