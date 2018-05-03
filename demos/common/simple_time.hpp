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

class simple_time
{
public:
  simple_time(void)
  {
    m_start_time = std::chrono::steady_clock::now();
  }

  int32_t
  elapsed(void)
  {
    return time_difference_ms(std::chrono::steady_clock::now(), m_start_time);
  }

  int32_t
  restart(void)
  {
    auto current_time = std::chrono::steady_clock::now();
    int32_t return_value = time_difference_ms(current_time, m_start_time);
    m_start_time = current_time;
    return return_value;
  }

  int64_t
  elapsed_us(void)
  {
    return time_difference_us(std::chrono::steady_clock::now(), m_start_time);
  }

  int64_t
  restart_us(void)
  {
    auto current_time = std::chrono::steady_clock::now();
    int64_t return_value = time_difference_us(current_time, m_start_time);
    m_start_time = current_time;
    return return_value;
  }

private:

  static
  int32_t
  time_difference_ms(const std::chrono::time_point<std::chrono::steady_clock> &end,
                     const std::chrono::time_point<std::chrono::steady_clock> &begin)
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
  }

  static
  int64_t
  time_difference_us(const std::chrono::time_point<std::chrono::steady_clock> &end,
                     const std::chrono::time_point<std::chrono::steady_clock> &begin)
  {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
  }

  std::chrono::time_point<std::chrono::steady_clock> m_start_time;
};
