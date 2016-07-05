#pragma once
#include "SkPoint.h"
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

  SkPoint m_bb_min, m_bb_max;

protected:

  SkPoint m_bb_against_parent_min, m_bb_against_parent_max;

  virtual
  void
  pre_paint(void);

};
