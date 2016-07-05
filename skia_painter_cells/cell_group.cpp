#include "cell_group.hpp"

void
CellGroup::
pre_paint(void)
{
  PainterWidget *p;
  p = parent();

  if(p != NULL)
    {
      CellGroup *q;
      q = static_cast<CellGroup*>(p);
      m_skip_drawing = q->m_skip_drawing
        || (q->m_bb_against_parent_min.fX > m_bb_max.fX)
        || (q->m_bb_against_parent_max.fX < m_bb_min.fX)
        || (q->m_bb_against_parent_min.fY > m_bb_max.fY)
        || (q->m_bb_against_parent_max.fY < m_bb_min.fY);

      m_bb_against_parent_min.fX = std::max(m_bb_min.fX, q->m_bb_against_parent_min.fX);
      m_bb_against_parent_min.fY = std::max(m_bb_min.fY, q->m_bb_against_parent_min.fY);
      m_bb_against_parent_max.fX = std::min(m_bb_max.fX, q->m_bb_against_parent_max.fX);
      m_bb_against_parent_max.fY = std::min(m_bb_max.fY, q->m_bb_against_parent_max.fY);
    }
  else
    {
      m_skip_drawing = false;
      m_bb_against_parent_min = m_bb_min;
      m_bb_against_parent_max = m_bb_max;
    }
}
