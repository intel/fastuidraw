#include "glyph_finder.hpp"

//////////////////////////////////
// GlyphFinder methods
GlyphFinder::
~GlyphFinder()
{
  if(m_line_finder)
    {
      FASTUIDRAWdelete(m_line_finder);
    }
  for(auto p : m_lines)
    {
      FASTUIDRAWdelete(p);
    }

}

void
GlyphFinder::
init(const std::vector<LineData> &in_data,
     fastuidraw::c_array<const fastuidraw::range_type<float> > glyph_extents)
{
  m_glyph_extents = glyph_extents;

  if(in_data.empty())
    {
      return;
    }
  float begin, end;

  begin = in_data[0].m_vertical_spread.m_begin;
  end = in_data[0].m_vertical_spread.m_end;
  for(unsigned int i = 0, endi = in_data.size(); i < endi; ++i)
    {
      const LineData &L(in_data[i]);
      begin = fastuidraw::t_min(L.m_vertical_spread.m_begin, begin);
      end = fastuidraw::t_max(L.m_vertical_spread.m_end, end);
    }

  m_line_finder = FASTUIDRAWnew IntervalFinder<PerLine*>(begin, end);
  for(unsigned int i = 0, endi = in_data.size(); i < endi; ++i)
    {
      const LineData &L(in_data[i]);
      PerLine *p;

      FASTUIDRAWassert(m_lines.size() == i);
      p = FASTUIDRAWnew PerLine(L);
      m_lines.push_back(p);
      m_line_finder->add_entry(L.m_vertical_spread, p);

      for(unsigned int g = L.m_range.m_begin; g < L.m_range.m_end; ++g)
        {
          p->insert_glyph(g, glyph_extents[g]);
        }
    }
}

unsigned int
GlyphFinder::
glyph_source(fastuidraw::vec2 p) const
{
  if(!m_line_finder)
    {
      return glyph_not_found;
    }

  std::vector<PerLine*> dst;
  std::vector<unsigned int> gdst;

  m_line_finder->find_entries(p.y(), &dst);
  for(auto ptr : dst)
    {
      unsigned int G;

      ptr->glyph_source(p.x(), &gdst);
      if(!gdst.empty())
        {
          return gdst.front();
        }
    }
  return glyph_not_found;
}
