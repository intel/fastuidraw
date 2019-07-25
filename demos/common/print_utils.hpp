#ifndef FASTUIDRAW_DEMO_PRINT_UTILS_HPP
#define FASTUIDRAW_DEMO_PRINT_UTILS_HPP

#include <iostream>
#include <stdint.h>
#include <fastuidraw/util/util.hpp>

class PrintBytes
{
public:
  enum rounding_mode_t
    {
      round_to_highest_unit = 0,
      round_to_mb_or_highest_unit = 1,
      round_to_kb_or_highest_unit = 2,
      do_not_round= 3,
    };

  explicit
  PrintBytes(uint64_t v, enum rounding_mode_t r = round_to_kb_or_highest_unit):
    m_gb(fastuidraw::uint64_unpack_bits(30u, 34u, v)),
    m_mb(fastuidraw::uint64_unpack_bits(20u, 10u, v)),
    m_kb(fastuidraw::uint64_unpack_bits(10u, 10u, v)),
    m_b(fastuidraw::uint64_unpack_bits(0u, 10u, v)),
    m_rounding_mode(r)
  {}

  uint64_t m_gb, m_mb, m_kb, m_b;
  enum rounding_mode_t m_rounding_mode;
};

inline
std::ostream&
operator<<(std::ostream &str, const PrintBytes &obj)
{
  bool print_spe(false), print(true);
  char spe(' ');

  if (obj.m_gb && print)
    {
      str << obj.m_gb << "GB";
      print_spe = true;
      print = (obj.m_rounding_mode > PrintBytes::round_to_highest_unit);
    }

  if (obj.m_mb && print)
    {
      if (print_spe)
        {
          str << spe;
        }
      str << obj.m_mb << "MB";
      print_spe = true;
      print = (obj.m_rounding_mode > PrintBytes::round_to_mb_or_highest_unit);
    }

  if (obj.m_kb && print)
    {
      if (print_spe)
        {
          str << spe;
        }
      str << obj.m_kb << "KB";
      print_spe = true;
      print = (obj.m_rounding_mode > PrintBytes::round_to_kb_or_highest_unit);
    }

  if (obj.m_b && print)
    {
      if (print_spe)
        {
          str << spe;
        }
      str << obj.m_b << "B";
    }
  return str;
}

#endif
