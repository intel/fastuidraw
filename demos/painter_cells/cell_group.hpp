#pragma once

#include "ostream_utility.hpp"
#include "PainterWidget.hpp"

using namespace fastuidraw;
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

  vec2 m_bb_min, m_bb_max;

protected:

  vec2 m_bb_against_parent_min, m_bb_against_parent_max;

  void
  pre_paint(void);

};
