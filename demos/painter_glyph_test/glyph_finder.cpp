#include "glyph_finder.hpp"

///////////////////////////
// PerLine methods
void
PerLine::
insert_glyph(uint32_t idx, fastuidraw::range_type<float> R)
{
  float x0, x1;
  x0 = fastuidraw::t_min(R.m_begin, R.m_end);
  x1 = fastuidraw::t_max(R.m_begin, R.m_end);
  m_glyph_finder[key(x0, true)] = idx;
  m_glyph_finder[key(x1, false)] = idx;
}

unsigned int
PerLine::
glyph_source(float x) const
{
  std::map<key, unsigned int>::const_iterator iter;
  iter = m_glyph_finder.upper_bound(key(x, false));
  if(iter != m_glyph_finder.end())
    {
      return iter->second;
    }
  return GlyphFinder::glyph_not_found;
}

//////////////////////////////////
// GlyphFinder methods
void
GlyphFinder::
init(const std::vector<LineData> &in_data,
     fastuidraw::c_array<const fastuidraw::range_type<float> > glyph_extents)
{
  m_glyph_extents = glyph_extents;
  for(unsigned int i = 0, endi = in_data.size(); i < endi; ++i)
    {
      const LineData &L(in_data[i]);

      FASTUIDRAWassert(m_lines.size() == i);
      m_lines.push_back(L);
      m_line_finder[L.m_vertical_spread.m_end] = i;
      m_line_finder[L.m_vertical_spread.m_begin] = i;

      for(unsigned int g = L.m_range.m_begin; g < L.m_range.m_end; ++g)
        {
          m_lines.back().insert_glyph(g, glyph_extents[g]);
        }
    }
}

unsigned int
GlyphFinder::
glyph_source(fastuidraw::vec2 p) const
{
  std::map<float, unsigned int>::const_iterator iter;

  iter = m_line_finder.upper_bound(p.y());
  if(iter == m_line_finder.end())
    {
      return glyph_not_found;
    }

  const PerLine &L(m_lines[iter->second]);
  if(L.L().m_vertical_spread.m_begin <= p.y()
     && L.L().m_vertical_spread.m_end >= p.y())
    {
      unsigned int G;

      G = L.glyph_source(p.x());
      if(G != glyph_not_found)
        {
          if(m_glyph_extents[G].m_begin <= p.x()
             && m_glyph_extents[G].m_end >= p.x())
            {
              return G;
            }
        }
    }
  return glyph_not_found;
}
