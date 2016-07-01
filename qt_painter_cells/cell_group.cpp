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
        || (q->m_bb_against_parent_min.x() > m_bb_max.x())
        || (q->m_bb_against_parent_max.x() < m_bb_min.x())
        || (q->m_bb_against_parent_min.y() > m_bb_max.y())
        || (q->m_bb_against_parent_max.y() < m_bb_min.y());

      m_bb_against_parent_min.rx() = std::max(m_bb_min.x(), q->m_bb_against_parent_min.x());
      m_bb_against_parent_min.ry() = std::max(m_bb_min.y(), q->m_bb_against_parent_min.y());

      m_bb_against_parent_max.rx() = std::min(m_bb_max.x(), q->m_bb_against_parent_max.x());
      m_bb_against_parent_max.ry() = std::min(m_bb_max.y(), q->m_bb_against_parent_max.y());
    }
  else
    {
      m_skip_drawing = false;
      m_bb_against_parent_min = m_bb_min;
      m_bb_against_parent_max = m_bb_max;
    }
}
