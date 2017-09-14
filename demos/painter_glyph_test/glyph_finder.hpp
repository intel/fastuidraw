#pragma once

#include <vector>
#include <map>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include "text_helper.hpp"
#include "interval_finder.hpp"

class PerLine
{
public:
  PerLine(const LineData &L):
    m_interval_finder(L.m_horizontal_spread.m_begin,
                      L.m_horizontal_spread.m_end),
    m_L(L)
  {}

  const LineData&
  L(void) const
  {
    return m_L;
  }

  void
  insert_glyph(uint32_t idx, fastuidraw::range_type<float> R)
  {
    m_interval_finder.add_entry(R, idx);
  }

  void
  glyph_source(float x, std::vector<unsigned int> *dst) const
  {
    m_interval_finder.find_entries(x, dst);
  }

private:
  IntervalFinder<unsigned int> m_interval_finder;
  LineData m_L;
};

class GlyphFinder:fastuidraw::noncopyable
{
public:
  enum
    {
      glyph_not_found = ~0u
    };

  GlyphFinder(void):
    m_line_finder(nullptr)
  {}

  ~GlyphFinder();

  void
  init(const std::vector<LineData> &in_data,
       fastuidraw::c_array<const fastuidraw::range_type<float> > glyph_extents);

  unsigned int
  glyph_source(fastuidraw::vec2 p) const;

private:
  fastuidraw::c_array<const fastuidraw::range_type<float> > m_glyph_extents;
  std::vector<PerLine*> m_lines;
  IntervalFinder<PerLine*> *m_line_finder;
};
