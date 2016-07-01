#pragma once

#include <QPointF>
#include "PainterWidget.hpp"

class CellGroup:public PainterWidget
{
public:
  CellGroup(CellGroup *qparent):
    PainterWidget(qparent)
  {
    m_clipped = false;
  }

  virtual
  ~CellGroup(void)
  {}

  QPointF m_bb_min, m_bb_max;

protected:

  QPointF m_bb_against_parent_min, m_bb_against_parent_max;

  void
  pre_paint(void);

};
