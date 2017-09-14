#pragma once

#include <vector>
#include <map>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include "text_helper.hpp"

class PerLine
{
public:
  PerLine(const LineData &L):
    m_L(L)
  {}

  const LineData&
  L(void) const
  {
    return m_L;
  }

  void
  insert_glyph(uint32_t idx, fastuidraw::range_type<float> R);

  unsigned int
  glyph_source(float x) const;

private:
  typedef std::pair<float, bool> key;

  std::map<key, unsigned int> m_glyph_finder;
  LineData m_L;
};

class GlyphFinder
{
public:
  enum
    {
      glyph_not_found = ~0u
    };

  void
  init(const std::vector<LineData> &in_data,
       fastuidraw::c_array<const fastuidraw::range_type<float> > glyph_extents);

  unsigned int
  glyph_source(fastuidraw::vec2 p) const;

private:
  fastuidraw::c_array<const fastuidraw::range_type<float> > m_glyph_extents;
  std::vector<PerLine> m_lines;
  std::map<float, unsigned int> m_line_finder;
};
