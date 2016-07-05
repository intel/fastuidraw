#pragma once

#include <assert.h>
#include <list>
#include <boost/utility.hpp>

#include "SkCanvas.h"
#include "SkMatrix.h"
#include "SkRect.h"

class PainterWidget:boost::noncopyable
{
public:

  explicit
  PainterWidget(PainterWidget *parent = NULL);

  virtual
  ~PainterWidget(void);

  void
  paint(SkCanvas *painter);

  PainterWidget*
  parent(void)
  {
    return m_parent;
  }

  void
  parent(PainterWidget *p);

  /* returns true if an ancestor of q
     is this
   */
  bool
  is_ancestor_of(PainterWidget *q);

  /* clipInRect for widget
   */
  SkSize m_dimensions;

  /* transformation from local coordinate to parent coordinates
   */
  SkMatrix m_parent_matrix_this;

  /* if true, content is clipped to m_dimensions
   */
  bool m_clipped;

  /* if true, skip drawing both the widgets
     and alls its descendants
   */
  bool m_skip_drawing;
protected:

  virtual
  void
  pre_paint(void) {}

  virtual
  void
  paint_pre_children(SkCanvas *painter);

  virtual
  void
  paint_post_children(SkCanvas *painter);

private:
  PainterWidget *m_parent;
  std::list<PainterWidget*>::iterator m_iterator_loc;
  std::list<PainterWidget*> m_children;
};
