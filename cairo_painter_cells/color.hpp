#pragma once
#include <cairo.h>

class color_t
{
public:
  explicit
  color_t(double r = 1.0,
          double g = 1.0,
          double b = 1.0,
          double a = 1.0):
    m_r(r), m_g(g), m_b(b), m_a(a)
  {}

  double m_r, m_g, m_b, m_a;
};

inline
void
cairo_set_source_rgba(cairo_t *c, const color_t &rgba)
{
  cairo_set_source_rgba(c, rgba.m_r, rgba.m_g, rgba.m_b, rgba.m_a);
}
