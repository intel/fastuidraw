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

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <fastuidraw/util/time.hpp>
#else
#include <sys/time.h>
#endif
#include <stdint.h>

class simple_time
{
public:

  simple_time(void)
  {
    gettimeofday(&m_start_time, nullptr);
  }

  int32_t
  elapsed(void)
  {
    struct timeval current_time;

    gettimeofday(&current_time, nullptr);
    return time_difference_ms(current_time, m_start_time);
  }

  int32_t
  restart(void)
  {
    int32_t return_value;
    struct timeval current_time;

    gettimeofday(&current_time, nullptr);
    return_value = time_difference_ms(current_time, m_start_time);
    m_start_time = current_time;

    return return_value;
  }


  //
  int64_t
  elapsed_us(void)
  {
    struct timeval current_time;

    gettimeofday(&current_time, nullptr);
    return time_difference_us(current_time, m_start_time);
  }

  int64_t
  restart_us(void)
  {
    int64_t return_value;
    struct timeval current_time;

    gettimeofday(&current_time, nullptr);
    return_value = time_difference_us(current_time, m_start_time);
    m_start_time = current_time;

    return return_value;
  }

private:

  static
  int32_t
  time_difference_ms(const struct timeval &end, const struct timeval &begin)
  {
    return (end.tv_sec - begin.tv_sec) * 1000+
      (end.tv_usec - begin.tv_usec) / 1000;
  }

  static
  int64_t
  time_difference_us(const struct timeval &end, const struct timeval &begin)
  {
    int64_t delta_usec, delta_sec;

    delta_usec = int64_t(end.tv_usec) - int64_t(begin.tv_usec);
    delta_sec = int64_t(end.tv_sec) - int64_t(begin.tv_sec);
    return delta_sec * int64_t(1000) * int64_t(1000) + delta_usec;
  }

  struct timeval m_start_time;
};
