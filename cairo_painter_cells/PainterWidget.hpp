#pragma once

#include <list>
#include <boost/utility.hpp>
#include <cairo.h>
#include "vec2.hpp"

class PainterWidget:boost::noncopyable
{
public:

  explicit
  PainterWidget(PainterWidget *parent = NULL);

  virtual
  ~PainterWidget(void);

  void
  paint(cairo_t *painter);

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
  vec2 m_dimensions;

  /* transformation from local coordinate to parent coordinates
   */
  cairo_matrix_t m_parent_matrix_this;

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
  paint_pre_children(cairo_t *painter);

  virtual
  void
  paint_post_children(cairo_t *painter);

private:
  PainterWidget *m_parent;
  std::list<PainterWidget*>::iterator m_iterator_loc;
  std::list<PainterWidget*> m_children;
};
