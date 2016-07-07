#pragma once

#include <cairo.h>

class vec2
{
public:
  double m_x, m_y;

  vec2(void):
    m_x(0.0), m_y(0.0)
  {}

  vec2(double px, double py):
    m_x(px),
    m_y(py)
  {}

  vec2
  operator+(const vec2 &rhs) const
  {
    vec2 v(*this);
    v.m_x += rhs.m_x;
    v.m_y += rhs.m_y;
    return v;
  }

  vec2
  operator*(const vec2 &rhs) const
  {
    vec2 v(*this);
    v.m_x *= rhs.m_x;
    v.m_y *= rhs.m_y;
    return v;
  }

  vec2
  operator*(double s) const
  {
    vec2 v(*this);
    v.m_x *= s;
    v.m_y *= s;
    return v;
  }

  friend
  vec2
  operator*(const cairo_matrix_t &lhs, const vec2 &rhs)
  {
    vec2 r(rhs);
    cairo_matrix_transform_point(&lhs, &r.m_x, &r.m_y);
    return r;
  }
};

cairo_matrix_t
operator*(const cairo_matrix_t &lhs, const cairo_matrix_t &rhs)
{
  cairo_matrix_t R;
  cairo_matrix_multiply(&R, &lhs, &rhs);
  return R;
}
