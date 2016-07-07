#pragma once

#include <cairo.h>

template<typename T>
class Tvec2
{
public:
  T m_x, m_y;

  Tvec2(void):
    m_x(0.0), m_y(0.0)
  {}

  Tvec2(T px, T py):
    m_x(px),
    m_y(py)
  {}

  template<typename S>
  explicit
  Tvec2(const Tvec2<S> &obj):
    m_x(obj.m_x),
    m_y(obj.m_y)
  {}

  T
  x(void) const { return m_x; }

  T
  y(void) const { return m_y; }

  T&
  x(void) { return m_x; }

  T&
  y(void) { return m_y; }

  Tvec2
  operator+(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x += rhs.m_x;
    v.m_y += rhs.m_y;
    return v;
  }

  Tvec2
  operator-(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x -= rhs.m_x;
    v.m_y -= rhs.m_y;
    return v;
  }

  Tvec2
  operator*(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x *= rhs.m_x;
    v.m_y *= rhs.m_y;
    return v;
  }

  Tvec2
  operator/(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x /= rhs.m_x;
    v.m_y /= rhs.m_y;
    return v;
  }

  Tvec2
  operator*(T s) const
  {
    Tvec2 v(*this);
    v.m_x *= s;
    v.m_y *= s;
    return v;
  }

  Tvec2
  operator/(T s) const
  {
    Tvec2 v(*this);
    v.m_x /= s;
    v.m_y /= s;
    return v;
  }
};


typedef Tvec2<double> vec2;
typedef Tvec2<int> ivec2;

inline
vec2
operator*(const cairo_matrix_t &lhs, const vec2 &rhs)
{
  vec2 r(rhs);
  cairo_matrix_transform_point(&lhs, &r.m_x, &r.m_y);
  return r;
}

inline
cairo_matrix_t
operator*(const cairo_matrix_t &lhs, const cairo_matrix_t &rhs)
{
  cairo_matrix_t R;
  cairo_matrix_multiply(&R, &lhs, &rhs);
  return R;
}

template<typename T>
inline
void
cairo_matrix_translate(cairo_matrix_t *m, const Tvec2<T> &t)
{
  cairo_matrix_translate(m, t.x(), t.y());
}

template<typename T>
inline
void
cairo_translate(cairo_t *m, const Tvec2<T> &t)
{
  cairo_translate(m, t.x(), t.y());
}
